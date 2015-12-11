#pragma once
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <boost/format.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/signals2.hpp>
#include <boost/coroutine/coroutine.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/locale.hpp>
#include "boost/timedcall.hpp"

#include "internet_mail_format.hpp"

#include <string>
#include <map>

namespace mx
{
	enum class mail_attribute 
	{
		mail_attribute_begin = 0,
		mail_attribute_readed, 
		mail_attribute_delete,
		mail_attribute_end
	};

	class imap
		: public boost::asio::coroutine
	{
	public:
		typedef void result_type;
		typedef std::function< void(int) >  call_to_continue_function;
		typedef std::function< void(mailcontent, call_to_continue_function)>  on_mail_function;
	public:
		imap(boost::asio::io_service& io, const std::string& account, const std::string& password, const std::string& server_name, const std::string& server_port);
		~imap();

		// ����IMAP
		void async_fetch_mail(imap::on_mail_function handler)
		{
			m_sig_gotmail.reset(new on_mail_function(handler));
			m_io.post(boost::bind(*this, boost::system::error_code(), 0));
		}

		// ����Ŀ¼
		void create_dir(const std::string& floder_name);

		// ɾ��ĳ��Ŀ¼(QQ���䲻֧�ָ�ָ��)
		void delete_dir(const std::string& floder_name);

		// �޸��ļ�������(QQ���䲻֧�ָ�ָ��)
		void rename_dir(const std::string& old_floder_name, const std::string& new_floder_name);

		// ��ȡĳ���ʼ�����ϸ����
		void get_mail_ctx(const std::string& mail_id);

		// ������ǰ�ʼ�����
		void set_mail_attribute(const std::string& mail_id, const mail_attribute&);

		// �˳���ǰ����
		void exit_mail();

		void operator()(const boost::system::error_code & ec, std::size_t length = 0, const std::string& command = "")
		{
			using namespace boost::asio;
			std::string send_command;

			BOOST_ASIO_CORO_REENTER(this) 
			{
				// ��¼��������
				do
				{
					BOOST_ASIO_CORO_YIELD m_mail_socket->async_connect(m_endpoint, *this);

					// ʧ������ʱ 10s
					if (ec)
					{
						std::cout << "LOGIN FAILE ��WAIT FOR 10'S RECONNECT" << std::endl;
						BOOST_ASIO_CORO_YIELD boost::delayedcallsec(m_io, 10, boost::bind(*this, ec, 0));
					}
				} while (ec); // һֱ���Ե����ӳɹ�Ϊֹ


				// ���շ��������ص�����ȷ��
				m_cache_stream.reset(new boost::asio::streambuf());
				BOOST_ASIO_CORO_YIELD async_read_until(*m_mail_socket, *m_cache_stream, "\r\n", boost::bind(*this, _1, _2));
				if (ec)
					return;

				std::cout << "���� IMAP SERVER �� " << std::string(boost::asio::buffers_begin(m_cache_stream->data()), boost::asio::buffers_end(m_cache_stream->data())) << std::endl;
				// ���͵�¼ָ�������
				send_command = boost::str(m_map_command_format["LOGIN"] % m_login_account.m_account % m_login_account.m_password);
				BOOST_ASIO_CORO_YIELD 
					m_mail_socket->async_send(boost::asio::buffer(send_command.c_str(), send_command.size()), boost::bind(*this, _1, _2, send_command));

				if (length != command.size())
					return;

				// ���յ�¼ָ��
				m_cache_stream.reset(new boost::asio::streambuf());
				BOOST_ASIO_CORO_YIELD async_read_until(*m_mail_socket, *m_cache_stream, "\r\n", boost::bind(*this, _1, _2));
				if (ec)
					return;

				std::cout << "LOGIN IMAP SERVER �� " << std::endl
						  << std::string(boost::asio::buffers_begin(m_cache_stream->data()), boost::asio::buffers_end(m_cache_stream->data())) << std::endl;

				// ����ѡ���ռ���ָ��
				send_command = boost::str(m_map_command_format["SELECT"] % "INBOX");
				BOOST_ASIO_CORO_YIELD
					m_mail_socket->async_send(boost::asio::buffer(send_command.c_str(), send_command.size()), boost::bind(*this, _1, _2, send_command));
				if (length != command.size())
					return;

				// ����ѡ���ռ���ָ��
				m_cache_stream.reset(new boost::asio::streambuf());
				BOOST_ASIO_CORO_YIELD async_read_until(*m_mail_socket, *m_cache_stream, "\r\n", boost::bind(*this, _1, _2));
				if (ec)
					return;

				std::cout << "SELECT MAIL FLODER �� " << std::endl 
					      << std::string(boost::asio::buffers_begin(m_cache_stream->data()), boost::asio::buffers_end(m_cache_stream->data())) << std::endl;

				m_server_mail_count = floder_mail_count(std::string(boost::asio::buffers_begin(m_cache_stream->data()), boost::asio::buffers_end(m_cache_stream->data())));

				// ��ȡ�������ʼ���ID
				send_command = boost::str(m_map_command_format["FETCH-UID"] % m_server_mail_count);
				BOOST_ASIO_CORO_YIELD
					m_mail_socket->async_send(boost::asio::buffer(send_command.c_str(), send_command.size()), boost::bind(*this, _1, _2, send_command));
				if (length != command.size())
					return;

				m_cache_stream.reset(new boost::asio::streambuf());
				BOOST_ASIO_CORO_YIELD 
					async_read_until(*m_mail_socket, *m_cache_stream, "\r\n", boost::bind(*this, _1, _2));

				if (ec)
					return;
				std::cout << "FETCH MAIL UID�� " << std::endl
						  << std::string(boost::asio::buffers_begin(m_cache_stream->data()), boost::asio::buffers_end(m_cache_stream->data())) << std::endl;


				// ��ȡ�����ʼ���MIME
				for (; m_current_mail_summary_index <= m_server_mail_count; ++m_current_mail_summary_index)
				{
					send_command = boost::str(m_map_command_format["FETCH-SUMMARY"] % m_current_mail_summary_index);
					BOOST_ASIO_CORO_YIELD
						m_mail_socket->async_send(boost::asio::buffer(send_command.c_str(), send_command.size()), boost::bind(*this, _1, _2, send_command));

					if (length != command.size())
						return;

					m_cache_stream.reset(new boost::asio::streambuf());
					BOOST_ASIO_CORO_YIELD
						async_read_until(*m_mail_socket, *m_cache_stream, "\r\n", boost::bind(*this, _1, _2));

					if (ec)
						return;

					// std::cout << "FETCH MAIL MIME�� " << std::string(boost::asio::buffers_begin(m_cache_stream->data()), boost::asio::buffers_end(m_cache_stream->data())) << std::endl;


					InternetMailFormat imf;
					mailcontent mail_ctx;

					std::stringstream ss;
					ss << std::string(boost::asio::buffers_begin(m_cache_stream->data()), boost::asio::buffers_end(m_cache_stream->data()));
					imf_read_stream(imf, ss);

					mail_ctx.from = imf.header["from"];
					mail_ctx.to = imf.header["to"];
					mail_ctx.subject = imf.header["subject"];
					std::cout
						<< "�� " << m_current_mail_summary_index << " ���ʼ�" << std::endl
						<< "from :" << mail_ctx.from << std::endl
						<< "to :" << mail_ctx.to << std::endl
						<< "subject :" << mail_ctx.subject << std::endl
						<< std::endl;

					on_mail_function(m_mail_ctx);
				}

				m_current_mail_summary_index = 0;

				// <TODO>: У�����еĸ���
				// <TODO>: ����IMAP NOOP����״̬
			}
		}

	private:
		void send_command(const std::string&);
		std::vector<std::string> split(const std::string& src);
		int floder_mail_count(const std::string& str)
		{
			const std::vector<std::string> vec_sp = split(str);
			if (vec_sp.empty())
				return 0;
			auto it_first_line = vec_sp.begin();


			auto C(it_first_line->begin());
			std::string strCount;
			for (; C != it_first_line->end(); ++C)
			{
				switch (*C)
				{
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				case '0':
					strCount.push_back(*C);
					break;
				default:
					break;
				}
			}
			return boost::lexical_cast<int>(strCount);
		}

	private:
		// �ʺ���Ϣ
		struct imap_login_account
		{
			std::string m_account;
			std::string m_password;
			imap_login_account(const std::string account, const std::string password)
				: m_account(account)
				, m_password(password)
			{
			}
		};
	public:
		// 
		boost::asio::io_service& m_io;
		boost::shared_ptr<boost::asio::ip::tcp::socket> m_mail_socket;

		// ��������Ϣ
		boost::asio::ip::tcp::endpoint m_endpoint;
		imap_login_account m_login_account;

		// �������ʼ�����
		int m_current_mail_summary_index;
		int m_server_mail_count;

		boost::shared_ptr<on_mail_function>		m_sig_gotmail;

		boost::shared_ptr<boost::asio::streambuf> m_cache_stream;
		std::string m_recv_buf;

		std::map<std::string, boost::format> m_map_command_format;
	};

}