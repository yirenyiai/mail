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
#include <boost/enable_shared_from_this.hpp>

#include "boost/timedcall.hpp"
#include "boost/avproxy.hpp"

#include <string>
#include <map>
#include "internet_mail_format.hpp"

namespace mx
{
	enum class mail_attribute 
	{
		mail_attribute_begin = 0,
		mail_attribute_readed, 
		mail_attribute_delete,
		mail_attribute_end
	};

	class imap;

	class imap_main_loop
		: public boost::asio::coroutine
	{
	public:
		typedef void result_type;
	public:
		imap_main_loop(boost::shared_ptr<boost::asio::ip::tcp::socket> s)
			: m_socket(s)
		{
			s->get_io_service().post(boost::bind(*this, boost::system::error_code(), 0));
		}
		~imap_main_loop()
		{}

		void operator()(const boost::system::error_code & ec, std::size_t length)
		{
			BOOST_ASIO_CORO_REENTER(this)
			{
				while (true)
				{
					m_cache_stream.reset(new boost::asio::streambuf());
					BOOST_ASIO_CORO_YIELD
						async_read_until(*(m_socket), *(m_cache_stream), "\r\n", boost::bind(*this, _1, _2));

					if (!ec)
					{
						std::cout << std::string(boost::asio::buffers_begin(m_cache_stream->data()), boost::asio::buffers_end(m_cache_stream->data())) << std::endl;
					}
				}
			}
		}

	public:
		boost::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
		boost::shared_ptr<boost::asio::streambuf> m_cache_stream;
	};

	class imap
		: public boost::asio::coroutine
		, public boost::enable_shared_from_this<imap>
	{
	public:
		typedef void result_type;
		typedef std::function< void(int) >  call_to_continue_function;
		typedef std::function< void(mailcontent, call_to_continue_function)>  on_mail_function;
	public:
		imap(boost::asio::io_service& io, const std::string& account, const std::string& password, const std::string& server_addr_string, bool ssl = false);
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
			std::string send_command;

			auto cache_to_string= [this](boost::shared_ptr<boost::asio::streambuf> cache)->std::string{
				std::string recv;
				recv.append(boost::asio::buffers_begin(cache->data()), boost::asio::buffers_end(cache->data()));
				return recv;
			};

			BOOST_ASIO_CORO_REENTER(this) 
			{
				// ��¼��������
				do
				{
					// dns ����������.
					BOOST_ASIO_CORO_YIELD avproxy::async_proxy_connect(
						avproxy::autoproxychain(*m_mail_socket, boost::asio::ip::tcp::resolver::query(m_server_addr_string, "143")), *this);

					// ʧ������ʱ 10s
					if (ec)
					{
						std::cout << "LOGIN FAILE ��WAIT FOR 10'S RECONNECT : " <<  ec.value() << std::endl;
						BOOST_ASIO_CORO_YIELD boost::delayedcallsec(m_io, 10, boost::bind(*this, ec, 0));
					}
				} while (ec); // һֱ���Ե����ӳɹ�Ϊֹ


				// ���շ��������ص�����ȷ��
				m_cache_stream.reset(new boost::asio::streambuf());
				BOOST_ASIO_CORO_YIELD boost::asio::async_read_until(*m_mail_socket, *m_cache_stream, "\r\n", boost::bind(*this, _1, _2));
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
				BOOST_ASIO_CORO_YIELD async_read_until(*m_mail_socket, *m_cache_stream, "a001", boost::bind(*this, _1, _2));
				if (ec)
					return;

				std::cout << "LOGIN IMAP SERVER �� " << std::endl
					<< cache_to_string(m_cache_stream) << std::endl;

				// ���ͻ�ȡ�����ļ���ָ��
				send_command = boost::str(m_map_command_format["LIST"] % "\"\"" % "%");
				BOOST_ASIO_CORO_YIELD
					m_mail_socket->async_send(boost::asio::buffer(send_command.c_str(), send_command.size()), boost::bind(*this, _1, _2, send_command));
				if (length != command.size())
					return;

				// ���ջ�ȡ�����ļ���ָ��
				m_cache_stream.reset(new boost::asio::streambuf());
				BOOST_ASIO_CORO_YIELD async_read_until(*m_mail_socket, *m_cache_stream, "a008", boost::bind(*this, _1, _2));
				if (ec)
					return;

				std::cout << "LIST MAIL FLODER �� " << std::endl
					<< cache_to_string(m_cache_stream) << std::endl;

				parse_floder_format(cache_to_string(m_cache_stream));

				// ����ѡ���ռ���ָ��
				for (m_select_floder_index = 0; m_select_floder_index < m_vec_floder.size(); ++m_select_floder_index)
				{
					send_command = boost::str(m_map_command_format["SELECT"] % m_vec_floder[m_select_floder_index]);
					BOOST_ASIO_CORO_YIELD
						m_mail_socket->async_send(boost::asio::buffer(send_command.c_str(), send_command.size()), boost::bind(*this, _1, _2, send_command));

					if (length != command.size())
						return;
					std::cout << command << std::endl;

					// ����ѡ���ռ���ָ��
					m_cache_stream.reset(new boost::asio::streambuf());
					BOOST_ASIO_CORO_YIELD async_read_until(*m_mail_socket, *m_cache_stream, "a007", boost::bind(*this, _1, _2));
					if (ec)
						return;

					std::cout << "SELECT MAIL FLODER �� " << std::endl 
						<< cache_to_string(m_cache_stream) << std::endl << std::endl;
					m_server_mail_count = floder_mail_count(std::string(boost::asio::buffers_begin(m_cache_stream->data()), boost::asio::buffers_end(m_cache_stream->data())));
					if (m_server_mail_count == 0)
						continue;

					// ��ȡ��ǰ�ļ��е��ʼ�UID
					send_command = boost::str(m_map_command_format["FETCH-UID"] % m_server_mail_count);
					BOOST_ASIO_CORO_YIELD 
						m_mail_socket->async_send(boost::asio::buffer(send_command.c_str(), send_command.size()), boost::bind(*this, _1, _2, send_command));
					if (length != command.size())
						return;

					m_cache_stream.reset(new boost::asio::streambuf());
					BOOST_ASIO_CORO_YIELD async_read_until(*m_mail_socket, *m_cache_stream, "a014", boost::bind(*this, _1, _2));
					if (ec)
						return;

					std::cout << "MAIL UID�� " << std::endl
						<< cache_to_string(m_cache_stream) << std::endl << std::endl;

					//// ��ȡ�����ʼ���MIME
					for (m_current_mail_summary_index = 1; m_current_mail_summary_index <= m_server_mail_count; ++m_current_mail_summary_index)
					{
						send_command = boost::str(m_map_command_format["FETCH-SUMMARY"] % m_current_mail_summary_index);
						BOOST_ASIO_CORO_YIELD
							m_mail_socket->async_send(boost::asio::buffer(send_command.c_str(), send_command.size()), boost::bind(*this, _1, _2, send_command));

						if (length != command.size())
							return;

						m_cache_stream.reset(new boost::asio::streambuf());
						BOOST_ASIO_CORO_YIELD
							async_read_until(*m_mail_socket, *m_cache_stream, "a009", boost::bind(*this, _1, _2));

						if (ec)
							return;

						std::cout << std::string(boost::asio::buffers_begin(m_cache_stream->data()), boost::asio::buffers_end(m_cache_stream->data())) << std::endl;
					}

					// ��ȡ��ǰ�ʼ�������
					for (m_current_mail_summary_index = 1; m_current_mail_summary_index <= m_server_mail_count; ++m_current_mail_summary_index)
					{
						send_command = boost::str(m_map_command_format["FETCH-BODY"] % m_current_mail_summary_index % "BODY[TEXT]");
						BOOST_ASIO_CORO_YIELD
							m_mail_socket->async_send(boost::asio::buffer(send_command.c_str(), send_command.size()), boost::bind(*this, _1, _2, send_command));

						if (length != command.size())
							return;

						m_cache_stream.reset(new boost::asio::streambuf());
						BOOST_ASIO_CORO_YIELD
							async_read_until(*m_mail_socket, *m_cache_stream, "a013", boost::bind(*this, _1, _2));

						if (ec)
							return;

						std::cout << std::string(boost::asio::buffers_begin(m_cache_stream->data()), boost::asio::buffers_end(m_cache_stream->data())) << std::endl;
					}
				}

				m_main_loop.reset(new imap_main_loop(m_mail_socket));

				// �������ݽ��յ���ѭ��
				m_send_status_timer = boost::make_shared<boost::asio::deadline_timer>(m_io, boost::posix_time::seconds(20));
				do
				{
					// ����һ����ʱѭ��
					BOOST_ASIO_CORO_YIELD[this, &ec](){
						m_send_status_timer->expires_at(m_send_status_timer->expires_at() + boost::posix_time::seconds(20));
						m_send_status_timer->async_wait(boost::bind(*this, ec, 0));
					}();

					// ����NOOPָ��
					BOOST_ASIO_CORO_YIELD[this](){
						const std::string send_command = boost::str(m_map_command_format["NOOP"]);
						m_mail_socket->async_send(boost::asio::buffer(send_command.c_str(), send_command.size()), boost::bind(*this, _1, _2, send_command));
					}();


					if (length != command.size())
						return;
					else
					{
						std::cout << "NOOP SUCCESS" << std::endl;
					}

				} while (true);
			}
		}

	private:
		void send_command(const std::string&);
		std::vector<std::string> parse_floder_format(const std::string& str)
		{
			auto it = str.begin();
			std::string floder_name;
			bool begin = false;
			while (it != str.end())
			{
				switch (*it)
				{
				case  '\"':
					if (!floder_name.empty() && begin)
					{
						if (floder_name != "/")
						{
							auto it_find = std::find(m_vec_floder.begin(), m_vec_floder.end(), floder_name);
							if (it_find == m_vec_floder.end())
							{
								m_vec_floder.push_back(std::move(floder_name));
							}
						}
						begin = false;
					}
					else
					{
						begin = true;
						floder_name.clear();
					}
					break;
				default:
					if (begin)
						floder_name.push_back(*it);
					break;
				}
				++it;
			}

			return m_vec_floder;
		}
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

		void on_handle_send_buf(const boost::system::error_code& error, std::size_t bytes_transferred, const std::string& send_buf)
		{
			if (error != 0)
			{
				return;
			}
			const int less = send_buf.size() - bytes_transferred;
			if (send_buf.size() < bytes_transferred)
			{
				m_mail_socket->async_send(boost::asio::buffer(send_buf.c_str() + bytes_transferred, less), boost::bind(&imap::on_handle_send_buf, this, _1, _2, send_buf));
			}
		}

		void on_handle_recv_command(const boost::system::error_code& error, std::size_t bytes_transferred)
		{
			m_recv_buf.append(boost::asio::buffers_begin(m_cache_stream->data()), boost::asio::buffers_end(m_cache_stream->data()));

			std::vector<std::string> vec_sp = split(m_recv_buf);
			auto it_vec_sp_end = vec_sp.rbegin();
			if (it_vec_sp_end == vec_sp.rend())
				return;

			const std::string prefix_string(it_vec_sp_end->substr(0, 4));
			auto it_map_event_handle = m_map_handle_event.find(prefix_string);
			if (it_map_event_handle != m_map_handle_event.end())
			{
				it_map_event_handle->second(m_recv_buf);
				m_recv_buf.clear();
			}
			else
			{
				std::cout << error.message() << std::endl;
			}
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
		imap_login_account m_login_account;
		std::string m_server_addr_string;
		std::string m_server_port_string;

		// �������ʼ�����
		int m_server_mail_count;
		int m_current_mail_summary_index;		// ����������������ѵ�������ʼ�

		boost::unordered::unordered_map<int, InternetMailFormat> m_mail_ctx;
		std::shared_ptr<on_mail_function>	m_sig_gotmail;

		// ����
		boost::shared_ptr<boost::asio::streambuf> m_cache_stream;

		// �������ӵĶ�ʱ��
		boost::shared_ptr<boost::asio::deadline_timer> m_send_status_timer;

		// 
		boost::shared_ptr<imap_main_loop> m_main_loop;


		// һ�����������ݰ�
		std::string m_recv_buf;
		std::vector<std::string>::iterator m_it_vec_floder;

		// �����ʽ
		boost::unordered::unordered_map<std::string, boost::format> m_map_command_format;

		// <TODO>: ��ʱ��֧�ֶ����˵�
		// ��ǰ����������ļ���
		std::vector<std::string> m_vec_floder;
		std::size_t m_select_floder_index;

		boost::unordered::unordered_map<std::string, boost::function<void(std::string)>> m_map_handle_event;
	};
}