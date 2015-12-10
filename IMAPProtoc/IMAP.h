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
#include <string>
#include <map>

namespace mail 
{
	enum class mail_attribute 
	{
		mail_attribute_begin = 0,
		mail_attribute_readed, 
		mail_attribute_delete,
		mail_attribute_end

	};

	class imap_protoc
	{
	public:
		// �ӵ�һ�������Ļظ�
		boost::signals2::signal<void(const std::string)> m_signal_recv_result;
	public:
		imap_protoc(boost::asio::io_service& io, const std::string& server_name, const std::string& server_port, const std::string& account, const std::string& password);
		~imap_protoc();

		// ����Ŀ¼
		void create_dir(const std::string& floder_name);
		// ɾ��ĳ��Ŀ¼
		void delete_dir(const std::string& floder_name);
		// ������ǰ�ʼ�����
		void set_mail_attribute(const std::string& mail_id, const mail_attribute&);
		// �޸��ļ�������

		// �˳���ǰ����
		void exit_mail();
	private:
		void on_handle_connect_server(const boost::system::error_code& error);
		void on_handle_send_status();
		void on_handle_send_buf(const boost::system::error_code& error, std::size_t bytes_transferred, const std::string& send_buf);
		void on_handle_recv_buf(const boost::system::error_code& error, std::size_t bytes_transferred, boost::shared_ptr<boost::asio::streambuf> cache_stream);

		void send_command(const std::string&);


		std::vector<std::string> split(const std::string& src);

	protected:
		void on_handle_login_event(const std::string& stream);
		void on_handle_logout_event(const std::string& stream);

		void on_handle_store_seen_event(const std::string& stream);
		void on_handle_store_delete_event(const std::string& stream);
		void on_handle_delete_event(const std::string& stream);
		void on_handle_select_event(const std::string& stream);
		void on_handle_list_event(const std::string& stream);
		void on_handle_fetch_event(const std::string& stream);
		void on_handle_create_event(const std::string& stream);

		void on_handle_noop_event(const std::string& stream);

		void on_handle_command_error_event(const std::string& stream);
		void on_handle_unset_event(const std::string& stream);
	private:
		// ��������Ϣ
		struct imap_server
		{
			std::string m_server_name;
			std::string m_server_port;
			imap_server(const std::string& server_name, const std::string& server_port)
				: m_server_name(server_name)
				, m_server_port(server_port)
			{}
		};

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

	private:
		boost::asio::io_service& m_io;
		boost::asio::ip::tcp::socket m_mail_socket;
		
		// imap ָ��̶�4�ֽ�ǰ׺
		std::string m_pre_string;


		// ��������Ϣ
		imap_server m_server;
		imap_login_account m_login_account;

		// ��ʱ��
		boost::asio::deadline_timer m_send_status_timer;

		std::string m_recv_buf;


		std::map<std::string, boost::function<void(const std::string&)>> m_map_handle_event;
		std::map<std::string, boost::format> m_map_command_format;
	};

}