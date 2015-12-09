// IMAPProtoc.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <vector>
#include "IMAP.h"
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>

std::string get_ip_from_host()
{
	WORD wVersion;
	WSADATA WSAData;
	wVersion = MAKEWORD(2, 2);
	WSAStartup(wVersion, &WSAData);
	struct addrinfo hints;
	struct addrinfo *res(nullptr), *cur(nullptr);
	int ret = 0;
	struct sockaddr_in *addr = NULL;
	char m_ipaddr[16] = {0};

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;     /* Allow IPv4 */
	hints.ai_flags = AI_PASSIVE;/* For wildcard IP address */
	hints.ai_protocol = 0;         /* Any protocol */
	hints.ai_socktype = SOCK_STREAM;

	ret = getaddrinfo("imap.qq.com", NULL, &hints, &res);

	if (ret == -1) {
		perror("getaddrinfo");
		exit(1);
	}
	for (cur = res; cur != NULL; cur = cur->ai_next) {
		addr = (struct sockaddr_in *)cur->ai_addr;
		sprintf_s(m_ipaddr, 15, "%d.%d.%d.%d", (*addr).sin_addr.S_un.S_un_b.s_b1, (*addr).sin_addr.S_un.S_un_b.s_b2, (*addr).sin_addr.S_un.S_un_b.s_b3, (*addr).sin_addr.S_un.S_un_b.s_b4);
	}
	freeaddrinfo(res);
	WSACleanup();
	return std::string(m_ipaddr);
}

//域名解析为IP  
//入参：域名，端口  
//返回：ip地址  
std::vector<std::string> domain2ip(boost::asio::io_service& io, const char *domain, int port)
{
	//创建resolver对象  
	boost::asio::ip::tcp::resolver slv(io);
	//创建query对象  
	boost::asio::ip::tcp::resolver::query qry(domain, boost::lexical_cast<std::string>(port));//将int型端口转换为字符串  
	//使用resolve迭代端点  
	boost::asio::ip::tcp::resolver::iterator it = slv.resolve(qry);
	boost::asio::ip::tcp::resolver::iterator end;
	std::vector<std::string> ip;
	for (; it != end; it++)
	{
		ip.push_back(it->endpoint().address().to_string());
	}
	return ip;
}

int _tmain(int argc, _TCHAR* argv[])
{
	boost::asio::io_service io;

	//std::vector<std::string> vec_ip = domain2ip(io, "imap.qq.com", 143);

	mail::imap_protoc imap(io, get_ip_from_host(), "143", "", "");

	io.run();
	return 0;
}

