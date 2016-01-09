
#pragma once

#ifdef __llvm__
#pragma GCC diagnostic ignored "-Wdangling-else"
#endif


#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include "boost/timedcall.hpp"
#include "boost/avproxy.hpp"

#include "internet_mail_format.hpp"

namespace mx {

class pop3 : boost::asio::coroutine {
public:
	typedef void result_type;
	typedef std::function< void ( int ) >  call_to_continue_function;
	typedef std::function< void ( mailcontent, call_to_continue_function )>  on_mail_function;
public:
	pop3( ::boost::asio::io_service & _io_service, std::string user, std::string passwd, std::string _mailserver = "", bool use_ssl = false );

	void operator()( const boost::system::error_code & ec, std::size_t length = 0 ) {
		using namespace boost::asio;

		std::string		status;
		std::string		maillength;
		std::istream	inbuffer( m_readbuf.get() );
		std::ostream	outbuffer( m_writebuf.get() );
		std::string		msg;

		BOOST_ASIO_CORO_REENTER( this ) {
			m_socket.reset( new ip::tcp::socket( io_service ) );

			do {
#ifndef DEBUG
				// 延时 60s
				BOOST_ASIO_CORO_YIELD ::boost::delayedcallsec( io_service, 10, boost::bind( *this, ec, 0 ) );
#endif

				// dns 解析并连接.
				BOOST_ASIO_CORO_YIELD avproxy::async_proxy_connect(
					avproxy::autoproxychain(*m_socket, ip::tcp::resolver::query(m_mailserver, m_port)),
					*this );

				// 失败了延时 10s
				if( ec )
					BOOST_ASIO_CORO_YIELD ::boost::delayedcallsec( io_service, 10, boost::bind( *this, ec, 0 ) );
			} while( ec );  // 尝试到连接成功为止!

			// 好了，连接上了.
			m_readbuf.reset( new streambuf );
			// "+OK QQMail POP3 Server v1.0 Service Ready(QQMail v2.0)"
			BOOST_ASIO_CORO_YIELD	async_read_until( *m_socket, *m_readbuf, "\n", *this );
			inbuffer >> status;

			if( status != "+OK" ) {
				// 失败，重试.
				return;
			}

			// 发送用户名.
			BOOST_ASIO_CORO_YIELD async_write( std::string("user ") + m_mailaddr + "\n" , *this );

			if ( ec ) 
			{ 
				std::cout << ec.message() << std::endl;
				return;
			}

			// 接受返回状态.
			m_readbuf.reset( new streambuf );
			BOOST_ASIO_CORO_YIELD	async_read_until( *m_socket, *m_readbuf, "\n", *this );
			inbuffer >> status;

			// 解析是不是　OK.
			if( status != "+OK" ) {
				// 失败，重试.
				return;
			}

			// 发送密码.
			outbuffer << "pass " <<  m_passwd <<  "\r\n";
			BOOST_ASIO_CORO_YIELD async_write(*this );
			// 接受返回状态.
			m_readbuf.reset( new streambuf );
			BOOST_ASIO_CORO_YIELD	async_read_until( *m_socket, *m_readbuf, "\n", *this );
			inbuffer >> status;

			// 解析是不是　OK.
			if( status != "+OK" ) {
				// 失败，重试.
				return;
			}

			// 完成登录. 开始接收邮件.

			// 发送　list 命令.
			BOOST_ASIO_CORO_YIELD async_write( "list\n", *this );
			// 接受返回的邮件.
			m_readbuf.reset( new streambuf );
			BOOST_ASIO_CORO_YIELD	async_read_until( *m_socket, *m_readbuf, "\n", *this );
			inbuffer >> status;

			// 解析是不是　OK.
			if( status != "+OK" ) {
				// 失败，重试.
				return;
			}

			// 开始进入循环处理邮件.
			maillist.clear();
			BOOST_ASIO_CORO_YIELD	m_socket->async_read_some( m_readbuf->prepare( 8192 ), *this );
			m_readbuf->commit( length );

			while( status != "." ) {
				maillength.clear();
				status.clear();
				inbuffer >> status;
				inbuffer >> maillength;

				// 把邮件的编号push到容器里.
				if( maillength.length() )
					maillist.push_back( status );

				if( inbuffer.eof() && status != "." )
					BOOST_ASIO_CORO_YIELD	m_socket->async_read_some( m_readbuf->prepare( 8192 ), *this );
			}

			// 获取邮件.
			while( !maillist.empty() ) {
				// 发送　retr #number 命令.
				outbuffer << "retr " << maillist[0] <<  "\r\n";
				BOOST_ASIO_CORO_YIELD async_write(*this );
				// 获得　+OK
				m_readbuf.reset( new streambuf );
				BOOST_ASIO_CORO_YIELD	async_read_until( *m_socket, *m_readbuf, "\n", *this );
				inbuffer >> status;

				// 解析是不是　OK.
				if( status != "+OK" ) {
					// 失败，重试.
					return;
				}

				// 获取邮件内容，邮件一单行的 . 结束.
				BOOST_ASIO_CORO_YIELD	async_read_until( *m_socket, *m_readbuf, "\r\n.\r\n", *this );
				// 然后将邮件内容给处理.
				BOOST_ASIO_CORO_YIELD process_mail( inbuffer ,  boost::bind( *this, ec, _1 ) );

				// 如果返回的是 1,  也就是 length != 0 ,  就删除邮件.
				if( length ) {
#	ifndef DEBUG
					// 删除邮件啦.
					outbuffer << "dele " << maillist[0] <<  "\r\n";
					BOOST_ASIO_CORO_YIELD async_write(*this);

					// 获得　+OK
					m_readbuf.reset( new streambuf );
					BOOST_ASIO_CORO_YIELD	async_read_until( *m_socket, *m_readbuf, "\n", *this );
					inbuffer >> status;

					// 解析是不是　OK.
					if( status != "+OK" ) {
						// 失败，但是并不是啥大问题.
						// but 如果是连接出问题那还是要重启的.
						if( ec ) 
							return;
					}

#	 endif
				}

				maillist.erase( maillist.begin() );

			}

			// 处理完毕.
			BOOST_ASIO_CORO_YIELD async_write("quit\n" , *this );
			BOOST_ASIO_CORO_YIELD ::boost::delayedcallsec( io_service, 1, boost::bind( *this, ec, 0 ) );

			if( m_socket->is_open() ) {
				try {
					m_socket->shutdown( ip::tcp::socket::shutdown_both );
					m_socket.reset();
				} catch( const boost::system::system_error & ec ) {
					std::cout << ec.what() << std::endl;
					m_socket.reset();
				}
			}

			BOOST_ASIO_CORO_YIELD ::boost::delayedcallsec( io_service, 10, boost::bind( *this, ec, 0 ) );
		}
	}

	void async_fetch_mail( on_mail_function handler );

private:
	template <typename WriteHandler>
	void async_write(BOOST_ASIO_MOVE_ARG( WriteHandler ) handler )
	{
		boost::asio::async_write( *m_socket, *m_writebuf, handler );
	}

	template <typename WriteHandler>
	void async_write(const std::string & str, BOOST_ASIO_MOVE_ARG( WriteHandler ) handler )
	{
		std::ostream writebuf(m_writebuf.get());
		writebuf <<  str ;
		boost::asio::async_write( *m_socket, *m_writebuf, handler );
	}

	template<class Handler>
	void process_mail( std::istream &mail, Handler handler );
private:
	::boost::asio::io_service & io_service;

	std::string m_mailaddr, m_passwd, m_mailserver, m_port;
	// 必须是可拷贝的，所以只能用共享指针.
	std::shared_ptr<boost::asio::ip::tcp::socket>	m_socket;
	std::shared_ptr<boost::asio::streambuf>	m_readbuf;
	std::shared_ptr<boost::asio::streambuf>	m_writebuf;
	std::shared_ptr<on_mail_function>		m_sig_gotmail;
	std::vector<std::string>	maillist;
};

}
