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
					async_read_until(*m_mail_socket, *m_cache_stream, "a014", boost::bind(*this, _1, _2));

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
						async_read_until(*m_mail_socket, *m_cache_stream, "a009", boost::bind(*this, _1, _2));

					if (ec)
						return;


					InternetMailFormat& imf = m_mail_ctx[m_current_mail_summary_index];
					std::stringstream ss;
					ss << std::string(boost::asio::buffers_begin(m_cache_stream->data()), boost::asio::buffers_end(m_cache_stream->data()));
					imf_read_stream(imf, ss);

				}

				// ��ȡ�����ʼ���BODT-TEXT
				m_current_mail_summary_index = 1;
				for (; m_current_mail_summary_index <= m_server_mail_count; ++m_current_mail_summary_index)
				{
					send_command = boost::str(m_map_command_format["FETCH-BODY"] % m_current_mail_summary_index % "RFC822.TEXT");
					BOOST_ASIO_CORO_YIELD
						m_mail_socket->async_send(boost::asio::buffer(send_command.c_str(), send_command.size()), boost::bind(*this, _1, _2, send_command));

					if (length != command.size())
						return;

					m_cache_stream.reset(new boost::asio::streambuf());
					BOOST_ASIO_CORO_YIELD
						async_read_until(*m_mail_socket, *m_cache_stream, "a013", boost::bind(*this, _1, _2));

					if (ec)
						return;

					BOOST_ASIO_CORO_YIELD[this](){
						InternetMailFormat& imf = m_mail_ctx[m_current_mail_summary_index];
						mailcontent mail_ctx;
						std::stringstream ss;

						ss << std::string(boost::asio::buffers_begin(m_cache_stream->data()), boost::asio::buffers_end(m_cache_stream->data()));
						imf_read_stream(imf, ss);

						select_content(mail_ctx.content_type, mail_ctx.content, imf);

						mail_ctx.from = imf.header["from"];
						mail_ctx.to = imf.header["to"];
						mail_ctx.subject = imf.header["subject"];

						 m_io.post(boost::bind(&imap::broadcast_signal, this, m_sig_gotmail, mail_ctx, call_to_continue_function(boost::bind(*this, _1))));
					}();

					if (ec)
						return;
				}


				// <TODO>: У�����еĸ���

				// <TODO>: ����IMAP NOOP����״̬
				m_cache_stream.reset(new boost::asio::streambuf());
				async_read_until(*m_mail_socket, *m_cache_stream, "\r\n", boost::bind(&imap::on_handle_command, this, _1, _2));
			}
		}

	private:
		void send_command(const std::string&);
		std::vector<std::string> split(const std::string& src);
		std::string get_from_name(const std::string& mail_form)
		{
			std::string result;
			for (auto it(mail_form.begin()); it != mail_form.end(); ++it)
			{
				switch (*it)
				{
				case '\"':
						break;
				case '<':
					return result;
				default:
					result.push_back(*it);
					break;
				}

			}
			return result;

		}
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

		std::string find_mimetype(std::string contenttype)
		{
			if (!contenttype.empty()) {
				std::vector<std::string> splited;
				boost::split(splited, contenttype, boost::is_any_of("; "));
				return splited[0].empty() ? contenttype : splited[0];
			}

			return "text/plain";
		}

		std::string decode_content_charset(std::string body, std::string content_type)
		{
			boost::cmatch what;
			boost::regex ex("(.*)?;[\t \r\a]?charset=(.*)?");

			if (boost::regex_search(content_type.c_str(), what, ex)) {
				// text/plain; charset="gb18030" ���֣�Ȼ������ UTF-8
				std::string charset = what[2];
				return detail::ansi_utf8(body, charset);
			}

			return body;
		}

		// �С�text/plain���ľ�ѡ��text/plain, û�Ĳ�ѡ��text/html
		std::pair<std::string, std::string>
			select_best_mailcontent(InternetMailFormat & imf)
		{
			if (imf.have_multipart) {
				return select_best_mailcontent(boost::get<MIMEcontent>(imf.body));
			}
			else {
				std::string content_type = find_mimetype(imf.header["content-type"]);
				std::string content = decode_content_charset(boost::get<std::string>(imf.body), imf.header["content-type"]);
				return std::make_pair(content_type, content);
			}
		}

		// �С�text/plain���ľ�ѡ��text/plain, û�Ĳ�ѡ��text/html
		std::pair<std::string, std::string>
			select_best_mailcontent(MIMEcontent mime)
		{
			BOOST_FOREACH(InternetMailFormat & v, mime) {
				if (v.have_multipart) {
					return select_best_mailcontent(v);
				}

				// �� v.first aka contenttype �ҵ�����.
				std::string mimetype = find_mimetype(v.header["content-type"]);

				if (mimetype == "text/plain") {
					return std::make_pair(v.header["content-type"], boost::get<std::string>(v.body));
				}
			}
			BOOST_FOREACH(InternetMailFormat & v, mime) {
				if (v.have_multipart) {
					return select_best_mailcontent(v);
				}

				// �� v.first aka contenttype �ҵ�����.
				std::string mimetype = find_mimetype(v.header["content-type"]);

				if (mimetype == "text/html")
					return std::make_pair(v.header["content-type"], boost::get<std::string>(v.body));
			}
			return std::make_pair("", "");
		}

		void broadcast_signal(std::shared_ptr<imap::on_mail_function> sig_gotmail, mailcontent thismail, imap::call_to_continue_function handler)
		{
			if (sig_gotmail) {
				(*sig_gotmail)(thismail, handler);
			}
			else {
				handler(0);
			}
		}

		// �� IMF �еݹ����ѡ���һ����õ� mail content
		void select_content(std::string& content_type, std::string& content, InternetMailFormat& imf)
		{
			std::pair<std::string, std::string> mc = select_best_mailcontent(imf);
			content_type = find_mimetype(mc.first);
			content = decode_content_charset(mc.second, mc.first);
		}

		void on_handle_command(const boost::system::error_code & ec, std::size_t bytes_transferred)
		{
			if (ec == 0 && bytes_transferred > 4)
			{
				boost::shared_ptr<int> finally(new int(0), [this](int* p){
					if (p)
						delete p;
					p = nullptr;

					m_cache_stream = boost::make_shared<boost::asio::streambuf>();
					boost::asio::async_read_until(m_mail_socket, *m_cache_stream, "\r\n", boost::bind(&imap::on_handle_command, this, _1, _2, m_cache_stream));
				});

				boost::asio::streambuf::const_buffers_type cache = m_cache_stream->data();
				m_recv_buf.append(boost::asio::buffers_begin(cache), boost::asio::buffers_end(cache));

				std::vector<std::string> vec_sp = split(m_recv_buf);

				auto it_vec_sp_end = vec_sp.rbegin();
				if (it_vec_sp_end == vec_sp.rend())
					return;

				const std::string prefix_string(it_vec_sp_end->substr(0, 4));
				auto it_map_event_handle = m_map_handle_event.find(prefix_string);
				if (it_map_event_handle != m_map_handle_event.end())
				{
					m_recv_buf = boost::locale::conv::between(m_recv_buf, "GBK", "UTF-8");
					it_map_event_handle->second(m_recv_buf);
					m_recv_buf.clear();
				}
			}
			else
			{
				std::cout << ec.message() << std::endl;
			}



			// 
			m_cache_stream.reset(new boost::asio::streambuf());
			async_read_until(*m_mail_socket, *m_cache_stream, "a002", boost::bind(&imap::on_handle_command, this, _1, _2));
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

		boost::unordered::unordered_map<int, InternetMailFormat> m_mail_ctx;
		std::shared_ptr<on_mail_function>		m_sig_gotmail;

		boost::shared_ptr<boost::asio::streambuf> m_cache_stream;
		std::string m_recv_buf;

		std::map<std::string, boost::format> m_map_command_format;


		boost::unordered::unordered_map<std::string, boost::function<void(std::string)>> m_map_handle_event;
	};

}