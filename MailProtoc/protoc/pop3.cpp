
#include <boost/locale.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/bind.hpp>
#include <boost/regex.hpp>
#include <boost/foreach.hpp>

#include "boost/base64.hpp"

#include "pop3.hpp"
#include "ctx_type_select.hpp"

namespace mx {
static void broadcast_signal( std::shared_ptr<pop3::on_mail_function> sig_gotmail, mailcontent thismail, pop3::call_to_continue_function handler )
{
	if( sig_gotmail ) {
		( *sig_gotmail )( thismail, handler );
	} else {
		handler( 0 );
	}
}


template<class Handler>
void pop3::process_mail( std::istream &mail, Handler handler )
{
	InternetMailFormat imf;
	imf_read_stream( imf, mail );

	mailcontent thismail;
	thismail.m_floder = "INBOX";
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
