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
		// 登录指令
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("LOGIN", boost::format("a001 LOGIN %1% %2%\r\n"))
			);

		// 上送状态 
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("NOOP", boost::format("a002 NOOP\r\n"))
			);

		// 退出当前邮箱
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("LOGOUT", boost::format("a003 LOGOUT\r\n"))
			);

		// 删除目录
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("DELETE", boost::format("a004 DELETE %1%\r\n"))
			);

		// 修改文件夹名字
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("RENAME", boost::format("a005 RENAME  %1% %2%\r\n"))
			);

		// 上传一个邮件到制定的目录
		// APPEND <folder><attributes><date/time><size><mail data>
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("APPEND", boost::format("a006 APPEND %1% %2% %3% %4% %5%"))
			);

		// 选择一个文件夹
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("SELECT", boost::format("a007 SELECT %1%\r\n"))
			);

		// 获取目录
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("LIST", boost::format("a008 LIST %1% %2%\r\n"))
			);

		// 获取邮件内容
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("FETCH", boost::format("a009 FETCH %1% %2%\r\n"))
			);

		// 创建一个目录 
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("CREATE", boost::format("a010 CREATE %1%\r\n"))
			);


		// 删除邮件
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("STORE-DELETE", boost::format("a011 STORE %1% +FLAGS (\\Deleted)\r\n"))
			);

		// 标记为已读
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("STORE-SEEN", boost::format("a012 STORE %1% +FLAGS (\\Seen)\r\n"))
			);


		// callback
		m_map_handle_event.insert(std::pair<std::string, boost::function<void(const std::string&)>>("a001", boost::bind(&imap_protoc::on_handle_login_event, this, _1)));
		m_map_handle_event.insert(std::pair<std::string, boost::function<void(const std::string&)>>("a002", boost::bind(&imap_protoc::on_handle_noop_event, this, _1)));
		m_map_handle_event.insert(std::pair<std::string, boost::function<void(const std::string&)>>("a003", boost::bind(&imap_protoc::on_handle_logout_event, this, _1)));
		m_map_handle_event.insert(std::pair<std::string, boost::function<void(const std::string&)>>("a004", boost::bind(&imap_protoc::on_handle_delete_event, this, _1)));
		m_map_handle_event.insert(std::pair<std::string, boost::function<void(const std::string&)>>("a007", boost::bind(&imap_protoc::on_handle_select_event, this, _1)));
		m_map_handle_event.insert(std::pair<std::string, boost::function<void(const std::string&)>>("a008", boost::bind(&imap_protoc::on_handle_list_event, this, _1)));
		m_map_handle_event.insert(std::pair<std::string, boost::function<void(const std::string&)>>("a009", boost::bind(&imap_protoc::on_handle_fetch_event, this, _1)));
		m_map_handle_event.insert(std::pair<std::string, boost::function<void(const std::string&)>>("a010", boost::bind(&imap_protoc::on_handle_create_event, this, _1)));
		m_map_handle_event.insert(std::pair<std::string, boost::function<void(const std::string&)>>("a011", boost::bind(&imap_protoc::on_handle_store_delete_event, this, _1)));
		m_map_handle_event.insert(std::pair<std::string, boost::function<void(const std::string&)>>("a012", boost::bind(&imap_protoc::on_handle_store_seen_event, this, _1)));

		// 链接到服务器
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

	void imap_protoc::create_dir(const std::string& floder_name)
	{
		std::string command = boost::str(m_map_command_format["CREATE"] % floder_name);
		send_command(command);
	}

	void imap_protoc::delete_dir(const std::string& floder_name)
	{
		std::string command =  boost::str(m_map_command_format["DELETE"] % floder_name);
		send_command(command);
	}


	// 调整当前邮件属性
	void imap_protoc::set_mail_attribute(const std::string& mail_id, const mail_attribute& attr)
	{
		switch (attr)
		{
		case mail_attribute::mail_attribute_readed:
		{
			std::string command = boost::str(m_map_command_format["STORE-SEEN"] % mail_id);
			send_command(command);
		}
			break;
		case mail_attribute::mail_attribute_delete:
		{
			std::string command = boost::str(m_map_command_format["STORE-DELETE"] % mail_id);
			send_command(command);
		}
			break;
		default:
			break;
		}
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

		std::cout << "链接IMAP服务器成功" << std::endl;

		if (!m_login_account.m_account.empty())
		{
			// 发送登包
			const std::string login_command = boost::str(m_map_command_format["LOGIN"] % m_login_account.m_account % m_login_account.m_password);
			send_command(login_command);
		}

		// 设置常规的呼吸包
		m_send_status_timer.async_wait(boost::bind(&imap_protoc::on_handle_send_status, this));

		boost::shared_ptr<boost::asio::streambuf> cache_stream = boost::make_shared<boost::asio::streambuf>();
		boost::asio::async_read_until(m_mail_socket, *cache_stream, "\r\n", boost::bind(&imap_protoc::on_handle_recv_buf, this, _1, _2, cache_stream));
	}

	void imap_protoc::send_command(const std::string& command)
	{
		// 判断当前socket的状态
		m_mail_socket.async_send(boost::asio::buffer(command.c_str(), command.size()), boost::bind(&imap_protoc::on_handle_send_buf, this, _1, _2, command));
	}


	std::vector<std::string> imap_protoc::split(const std::string& src)
	{
		std::vector<std::string> vec_sp;
		std::string result;
		auto it = src.begin();
		while (it != src.end())
		{
			switch (*it)
			{
			case '\n':
				result.push_back(*it);
				vec_sp.push_back(std::move(result));
				break;
			default:
				result.push_back(*it);
				break;
			}

			++it;
		}
		return vec_sp;
	}

	void imap_protoc::on_handle_login_event(const std::string& stream)
	{
		std::cout << "邮箱登录成功" << std::endl;
		std::cout << stream << std::endl;

		std::string command = boost::str(m_map_command_format["LIST"] % "\"\"" % "\"%\"");
		std::cout << "拉取所有目录" << command << std::endl;
		send_command(command);
	}

	void imap_protoc::on_handle_logout_event(const std::string& stream)
	{
		std::cout << "退出邮箱" << std::endl;
		std::cout << stream << std::endl;

		// 停止呼吸包
		m_send_status_timer.cancel();
		
		// 退出邮箱
		boost::system::error_code ec;
		m_mail_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);

		// 退出io
		m_io.stop();
	}

	void imap_protoc::on_handle_list_event(const std::string& stream)
	{
		std::cout << "获取文件夹" << std::endl;
		std::cout << stream << std::endl;

		std::string command  = boost::str(m_map_command_format["SELECT"] % "INBOX");
		std::cout << "选中收件箱" << command << std::endl;
		send_command(command);
	}

	void imap_protoc::on_handle_fetch_event(const std::string& stream)
	{
		std::cout << "获取邮件内容" << std::endl;
		std::cout << stream << std::endl;
	}

	void imap_protoc::on_handle_create_event(const std::string& stream)
	{
		std::cout << "创建文件夹" << std::endl;
		std::cout << stream << std::endl;
	}


	void imap_protoc::on_handle_store_seen_event(const std::string& stream)
	{
		std::cout << "调整邮件标志--已阅" << std::endl;
		std::cout << stream << std::endl;
	}

	void imap_protoc::on_handle_store_delete_event(const std::string& stream)
	{
		std::cout << "调整邮件标志--删除" << std::endl;
		std::cout << stream << std::endl;
	}

	void imap_protoc::on_handle_delete_event(const std::string& stream)
	{
		std::cout << "删除文件夹" << std::endl;
		std::cout << stream << std::endl;
	}

	void imap_protoc::on_handle_select_event(const std::string& stream)
	{
		std::cout << "选中文件夹" << std::endl;
		std::cout << stream << std::endl;

		const std::string command = boost::str(m_map_command_format["FETCH"] % "*" % "ALL");
		send_command(command);
	}


	void imap_protoc::on_handle_noop_event(const std::string& stream)
	{
		std::cout << "服务器返回NOOP包" <<  std::endl;
		std::cout << stream << std::endl;
#ifdef _DEBUG
		//const std::string command = boost::str(m_map_command_format["LOGOUT"]);
		//send_command(command);
#endif
	}

	void imap_protoc::on_handle_command_error_event(const std::string& stream)
	{
		std::cout << "错误信息:" << std::endl;
		std::cout << stream << std::endl;
	}

	void imap_protoc::on_handle_unset_event(const std::string& stream)
	{
		std::cout << "接收到未处理数据 " << std::endl;
		std::cout << stream << std::endl;
	}

	void imap_protoc::on_handle_send_status()
	{
		std::cout << "上送状态" << std::endl;

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
			// 出错了
			return;
		}
		const int less = send_buf.size() - bytes_transferred;
		if (send_buf.size() < bytes_transferred)
		{
			// 续传
			m_mail_socket.async_send(boost::asio::buffer(send_buf.c_str() + bytes_transferred, less), boost::bind(&imap_protoc::on_handle_send_buf, this, _1, _2, send_buf));
		}
	}

	void imap_protoc::on_handle_recv_buf(const boost::system::error_code& error, std::size_t bytes_transferred, boost::shared_ptr<boost::asio::streambuf> cache_stream)
	{
		if (error == 0 && bytes_transferred > 4)
		{
			boost::shared_ptr<int> finally(new int(0), [&cache_stream, this](int* p){
				cache_stream = boost::make_shared<boost::asio::streambuf>();
				boost::asio::async_read_until(m_mail_socket, *cache_stream, "\r\n", boost::bind(&imap_protoc::on_handle_recv_buf, this, _1, _2, cache_stream));
			});

			boost::asio::streambuf::const_buffers_type cache = cache_stream->data();
			m_recv_buf.append(boost::asio::buffers_begin(cache), boost::asio::buffers_end(cache));

			std::vector<std::string> vec_sp = split(m_recv_buf);

			// 获取固定前缀
			auto it_vec_sp_end = vec_sp.rbegin();
			if (it_vec_sp_end == vec_sp.rend())
				return;

			// 根据前缀进行处理
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
			std::cout << error.message() << std::endl;
		}
	}
}
