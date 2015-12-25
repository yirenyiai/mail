#include <iostream>
#include "imap.hpp"
#include "internet_mail_format.hpp"

namespace mx
{
	imap::imap(boost::asio::io_service& io, const std::string& account, const std::string& password, const std::string& server_addr_string, bool ssl)
		: m_io(io)
		, m_mail_socket(boost::make_shared<boost::asio::ip::tcp::socket>(io))
		, m_server_addr_string(server_addr_string)
		, m_server_port_string(ssl ? "993" : "143")
		, m_login_account(account, password)
		, m_current_mail_summary_index(1)
		, m_server_mail_count(0)
		, m_select_floder_index(0)
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
			std::pair<std::string, boost::format>("APPEND", boost::format("a006 APPEND %1% %2% %3% %4% %5%\r\n"))
			);

		// 选择一个文件夹
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("SELECT", boost::format("a007 SELECT \"%1%\"\r\n"))
			);

		// 获取目录
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("LIST", boost::format("a008 LIST %1% %2%\r\n"))
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

		// 获取邮件摘要
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("FETCH-SUMMARY", boost::format("a009 FETCH %1% BODY[HEADER]\r\n"))
			);

		// 获取邮件内容
		m_map_command_format.insert(
			std::pair<std::string, boost::format>("FETCH-BODY", boost::format("a013 FETCH %1% BODY[TEXT]\r\n"))
			);
	}

	imap::~imap()
	{
	}

	void imap::create_dir(const std::string& floder_name)
	{
		std::string command = boost::str(m_map_command_format["CREATE"] % floder_name);
	}

	void imap::delete_dir(const std::string& floder_name)
	{
		std::string command = boost::str(m_map_command_format["DELETE"] % floder_name);
	}

	void imap::rename_dir(const std::string& old_floder_name, const std::string& new_floder_name)
	{
		std::string command = boost::str(m_map_command_format["RENAME"] % old_floder_name % new_floder_name);
	}

	void imap::get_mail_ctx(const std::string& mail_id)
	{
		const std::string command = boost::str(m_map_command_format["FETCH-BODY"] % mail_id % "BODY[TEXT]");
	}

	// 调整当前邮件属性
	void imap::set_mail_attribute(const std::string& mail_id, const mail_attribute& attr)
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

	void imap::exit_mail()
	{
		std::string command = boost::str(m_map_command_format["LOGOUT"]);
		send_command(command);
	}

	void imap::send_command(const std::string& command)
	{
		if (m_mail_socket && m_mail_socket->is_open())
			m_mail_socket->async_send(boost::asio::buffer(command.c_str(), command.size()), boost::bind(&imap::on_handle_send_buf, this, _1, _2, command));
	}

	std::vector<std::string> imap::split(const std::string& src)
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
}
