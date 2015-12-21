
#include <boost/locale.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/bind.hpp>
#include <boost/regex.hpp>
#include <boost/foreach.hpp>

#include "boost/base64.hpp"

#include "pop3.hpp"

namespace mx {

static std::string find_mimetype( std::string contenttype )
{
	if( !contenttype.empty() ) {
		std::vector<std::string> splited;
		boost::split( splited, contenttype, boost::is_any_of( "; " ) );
		return splited[0].empty() ? contenttype : splited[0];
	}

	return "text/plain";
}

static std::string decode_content_charset( std::string body, std::string content_type )
{
	boost::cmatch what;
	boost::regex ex( "(.*)?;[\t \r\a]?charset=(.*)?" );

	if( boost::regex_search( content_type.c_str(), what,  ex ) ) {
		// text/plain; charset="gb18030" 这种，然后解码成 UTF-8
		std::string charset = what[2];
		return detail::ansi_utf8( body, charset );
	}

	return body;
}

// 从 IMF 中递归进行选择出一个最好的 mail content
static void select_content( std::string& content_type, std::string& content, InternetMailFormat& imf );

// 有　text/plain　的就选　text/plain, 没的才选　text/html
static std::pair<std::string, std::string>
select_best_mailcontent( MIMEcontent mime );

// 有　text/plain　的就选　text/plain, 没的才选　text/html
static std::pair<std::string, std::string>
select_best_mailcontent( InternetMailFormat & imf )
{
	if( imf.have_multipart ) {
		return select_best_mailcontent( boost::get<MIMEcontent>( imf.body ) );
	} else {
		std::string content_type = find_mimetype( imf.header["content-type"] );
		std::string content = decode_content_charset( boost::get<std::string>( imf.body ), imf.header["content-type"] );
		return std::make_pair( content_type, content );
	}
}

// 有　text/plain　的就选　text/plain, 没的才选　text/html
static std::pair<std::string, std::string>
select_best_mailcontent( MIMEcontent mime )
{
	BOOST_FOREACH( InternetMailFormat & v, mime ) {
		if( v.have_multipart ) {
			return select_best_mailcontent( v );
		}

		// 从 v.first aka contenttype 找到编码.
		std::string mimetype = find_mimetype( v.header["content-type"] );

		if( mimetype == "text/plain" ) {
			return std::make_pair( v.header["content-type"], boost::get<std::string>( v.body ) );
		}
	}
	BOOST_FOREACH( InternetMailFormat & v, mime ) {
		if( v.have_multipart ) {
			return select_best_mailcontent( v );
		}

		// 从 v.first aka contenttype 找到编码.
		std::string mimetype = find_mimetype( v.header["content-type"] );

		if( mimetype == "text/html" )
			return std::make_pair( v.header["content-type"], boost::get<std::string>( v.body ) );
	}
	return std::make_pair( "", "" );
}

static void broadcast_signal( std::shared_ptr<pop3::on_mail_function> sig_gotmail, mailcontent thismail, pop3::call_to_continue_function handler )
{
	if( sig_gotmail ) {
		( *sig_gotmail )( thismail, handler );
	} else {
		handler( 0 );
	}
}

// 从 IMF 中递归进行选择出一个最好的 mail content
static void select_content( std::string& content_type, std::string& content, InternetMailFormat& imf )
{
	std::pair<std::string, std::string> mc = select_best_mailcontent( imf );
	content_type = find_mimetype( mc.first );
	content = decode_content_charset( mc.second, mc.first );

}

template<class Handler>
void pop3::process_mail( std::istream &mail, Handler handler )
{
	InternetMailFormat imf;
	imf_read_stream( imf, mail );

	mailcontent thismail;
	thismail.from = imf.header["from"];
	thismail.to = imf.header["to"];
	thismail.subject = imf.header["subject"];

	select_content( thismail.content_type, thismail.content, imf );

	io_service.post( boost::bind( broadcast_signal, m_sig_gotmail, thismail, call_to_continue_function( handler ) ) );
}

pop3::pop3( boost::asio::io_service& _io_service, std::string user, std::string passwd, std::string _mailserver )
	: io_service( _io_service ),
	  m_mailaddr( user ), m_passwd( passwd ),
	  m_mailserver( _mailserver ),
	  m_writebuf( new boost::asio::streambuf )
{
	if( m_mailserver.empty() ) { // 自动从　mailaddress 获得.
		if( m_mailaddr.find( "@" ) == std::string::npos )
			m_mailserver = "pop.qq.com"; // 如果　邮箱是 qq 号码（没@），就默认使用 pop.qq.com .
		else
			m_mailserver =  std::string( "pop." ) + m_mailaddr.substr( m_mailaddr.find_last_of( "@" ) + 1 );
	}
}

void pop3::async_fetch_mail( pop3::on_mail_function handler )
{
	m_sig_gotmail.reset( new on_mail_function( handler ) );
	io_service.post( boost::asio::detail::bind_handler( *this, boost::system::error_code(), 0 ) );
}

}
