#include "stdafx.h"
#include <iostream>
#include "IMAP.h"


namespace mail
{
	imap_protoc::imap_protoc(boost::asio::io_service& io, const std::string& server_name, const std::string& server_port, const std::string& account, const std::string& password)
		: m_io(io)
		, m_mail_socket(m_io)
		, m_server(server_name, server_port)
		, m_login_account(account, password)
		, m_send_status_timer(io, boost::posix_time::seconds(30))
	{
		// ��¼ָ��
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("LOGIN", boost::format("a001 LOGIN %1% %2%\r\n"))
			);

		// ����״̬ 
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("NOOP", boost::format("a002 NOOP\r\n"))
			);

		// �˳���ǰ����
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("LOGOUT", boost::format("a003 LOGOUT\r\n"))
			);

		// ɾ��Ŀ¼
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("DELETE", boost::format("a004 DELETE %1%"))
			);

		// �޸��ļ�������
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("RENAME", boost::format("a005 RENAME  %1% %2%\r\n"))
			);

		// �ϴ�һ���ʼ����ƶ���Ŀ¼
		// APPEND <folder><attributes><date/time><size><mail data>
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("APPEND", boost::format("a006 APPEND %1% %2% %3% %4% %5%"))
			);

		// ѡ��һ���ļ���
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("SELECT", boost::format("a007 SELECT %1%"))
			);

		// ��ȡĿ¼
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("LIST", boost::format("a008 LIST %1% %2%\r\n"))
			);


		m_map_handle_event.insert(std::pair<std::string, boost::function<void(const std::string&)>>("a001", boost::bind(&imap_protoc::on_handle_login_event, this, _1)));
		m_map_handle_event.insert(std::pair<std::string, boost::function<void(const std::string&)>>("a002", boost::bind(&imap_protoc::on_handle_command_error_event, this, _1)));
		m_map_handle_event.insert(std::pair<std::string, boost::function<void(const std::string&)>>("a003", boost::bind(&imap_protoc::on_handle_logout_event, this, _1)));
		m_map_handle_event.insert(std::pair<std::string, boost::function<void(const std::string&)>>("a008", boost::bind(&imap_protoc::on_handle_fetch_event, this, _1)));


		
		// ���ӵ�������
		boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(server_name), boost::lexical_cast<int>(server_port));
		m_mail_socket.async_connect(endpoint, boost::bind(&imap_protoc::on_handle_connect_server, this, _1));
	}

	imap_protoc::~imap_protoc()
	{
		if (m_mail_socket.is_open())
		{
			m_mail_socket.close();
		}
	}

	// ɾ���ʼ�
	void imap_protoc::delete_mail(const std::string& mail_title)
	{
	}

	void imap_protoc::delete_dir(const std::string& floder_name)
	{
		std::string command =  boost::str(m_map_command_format["DELETE"] % floder_name);
		send_command(command);
	}


	// ������ǰ�ʼ�����
	void imap_protoc::set_mail_attribute(const mail_attribute&)
	{
	}

	void imap_protoc::exit_mail()
	{
		std::string command = boost::str(m_map_command_format["LOGOUT"]);
		send_command(command);
	}

	void imap_protoc::on_handle_connect_server(const boost::system::error_code& error)
	{
		if (error != 0)
			return;

		// ���͵ǰ�
		const std::string login_command = boost::str(m_map_command_format["LOGIN"] % m_login_account.m_account % m_login_account.m_password);
		send_command(login_command);

		// ���ó���ĺ�����
		m_send_status_timer.async_wait(boost::bind(&imap_protoc::on_handle_send_status, this));

		boost::shared_ptr<boost::asio::streambuf> cache_stream = boost::make_shared<boost::asio::streambuf>();
		boost::asio::async_read_until(m_mail_socket, *cache_stream, "\r\n", boost::bind(&imap_protoc::on_handle_recv_buf, this, _1, _2, cache_stream));
	}

	void imap_protoc::send_command(const std::string& command)
	{
		// �жϵ�ǰsocket��״̬
		m_mail_socket.async_send(boost::asio::buffer(command.c_str(), command.size()), boost::bind(&imap_protoc::on_handle_send_buf, this, _1, _2, command));
	}

	void imap_protoc::on_handle_login_event(const std::string& stream)
	{
		std::cout << "�����¼�ɹ�, ��ȡ�����ļ���" << std::endl;

		const std::string command = boost::str(m_map_command_format["LIST"] % "\"\"" % "\"%\"");
		send_command(command);

	}

	void imap_protoc::on_handle_logout_event(const std::string& stream)
	{
	}

	void imap_protoc::on_handle_fetch_event(const std::string& stream)
	{
		std::cout << "��ȡ�ļ���" << stream << std::endl;
	}

	void imap_protoc::on_handle_noop_event(const std::string& stream)
	{
		std::cout << "����������NOOP��" << stream << std::endl;
	}

	void imap_protoc::on_handle_command_error_event(const std::string& stream)
	{}

	void imap_protoc::on_handle_unset_event(const std::string& stream)
	{
		std::cout << "���յ����� " << stream << std::endl;
	}

	void imap_protoc::on_handle_send_status()
	{
		std::cout << "����״̬" << std::endl;

		auto it_command = m_map_command_format.find("NOOP");
		assert(it_command != m_map_command_format.end());
		send_command(boost::str(it_command->second));

		m_send_status_timer.expires_at(m_send_status_timer.expires_at() + boost::posix_time::seconds(30));
		m_send_status_timer.async_wait(boost::bind(&imap_protoc::on_handle_send_status, this));
	}

	void imap_protoc::on_handle_send_buf(const boost::system::error_code& error, std::size_t bytes_transferred, const std::string& send_buf)
	{
		if (error != 0)
		{
			// ������
			return;
		}
		const int less = send_buf.size() - bytes_transferred;
		if (send_buf.size() < bytes_transferred)
		{
			// ����
			m_mail_socket.async_send(boost::asio::buffer(send_buf.c_str() + bytes_transferred, less), boost::bind(&imap_protoc::on_handle_send_buf, this, _1, _2, send_buf));
		}
	}

	void imap_protoc::on_handle_recv_buf(const boost::system::error_code& error, std::size_t bytes_transferred, boost::shared_ptr<boost::asio::streambuf> cache_stream)
	{
		if (error == 0 && bytes_transferred > 4)
		{
			boost::asio::streambuf::const_buffers_type cache = cache_stream->data();
			const std::string recv_buf(boost::asio::buffers_begin(cache), boost::asio::buffers_end(cache));

			// ��ȡ�̶�ǰ׺
			const std::string prefix_string(recv_buf.substr(0, 4));

			// ����ǰ׺���д���
			auto it_map_event_handle = m_map_handle_event.find(prefix_string);
			if (it_map_event_handle != m_map_handle_event.end())
			{
				it_map_event_handle->second(recv_buf);
			}
			else
			{
				on_handle_unset_event(recv_buf);
			}

			cache_stream = boost::make_shared<boost::asio::streambuf>();
			boost::asio::async_read_until(m_mail_socket, *cache_stream, "\r\n", boost::bind(&imap_protoc::on_handle_recv_buf, this, _1, _2, cache_stream));
		}
		else
		{
			std::cout << error.message() << std::endl;
		}
	}



}
