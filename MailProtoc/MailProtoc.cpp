#pragma once

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "protoc\pop3.hpp"
#include "protoc\imap.hpp"
#include "protoc\smtp.hpp"


int main(int argc, char* argv[])
{
		boost::asio::io_service io;
#ifdef IMAP_TEST
		std::string mail, password, mail_server;
		po::options_description desc("mail protoc options");
		desc.add_options()
			("version,v", "output version")
			("help,h", "produce help message")
			("mail,m", po::value<std::string>(&mail), "send mail by this address")
			("password,p", po::value<std::string>(&password), "password of mail")
			("server,s", po::value<std::string>(&mail_server), "server to use")
			;

		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

		if (vm.size() == 0)
			return 0;


		if (vm.count("help"))
		{
			std::cerr << desc << std::endl;
			return 1;
		}

		if (vm.count("version"))
		{
			std::cerr << "beat 0.0.1 mail protoc" << std::endl;
			return 1;
		}


		mx::imap imap(io, mail, password, mail_server, true);
		imap.async_fetch_mail([](const mailcontent& mail_ctx, mx::imap::call_to_continue_function handler){
			handler(0);
		});
#endif

#ifdef POP3_TEST
		std::string mail, password, mail_server;
		po::options_description desc("mail protoc options");
		desc.add_options()
			("version,v", "output version")
			("help,h", "produce help message")
			("mail,m", po::value<std::string>(&mail), "send mail by this address")
			("password,p", po::value<std::string>(&password), "password of mail")
			("server,s", po::value<std::string>(&mail_server), "server to use")
			;

		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

		if (vm.size() == 0)
			return 0;


		if (vm.count("help"))
		{
			std::cerr << desc << std::endl;
			return 1;
		}

		if (vm.count("version"))
		{
			std::cerr << "beat 0.0.1 mail protoc" << std::endl;
			return 1;
		}

		mx::pop3 pop(io, mail, password, mail_server, false);
		pop.async_fetch_mail([](const mailcontent& mail_ctx, mx::pop3::call_to_continue_function handler){
			handler(0);
		});
#endif


#ifdef SMTP_TEST		
		std::string mail_from, mail_to, mail_passwd, mail_server;
		po::options_description desc("mail protoc options");
		desc.add_options()
			("version,v", "output version")
			("help,h", "produce help message")
			("from,f", po::value<std::string>(&mail_from), "send mail by this address")
			("to,t", po::value<std::string>(&mail_to), "recv mail by this address")
			("password,p", po::value<std::string>(&mail_passwd), "password of mail")
			("server,s", po::value<std::string>(&mail_server), "server to use")
			;

		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

		if (vm.size() == 0)
			return 0;


		if (vm.count("help")) 
		{
			std::cerr << desc << std::endl;
			return 1;
		}
		
		if (vm.count("version"))
		{
			std::cerr << "beat 0.0.1 mail protoc" << std::endl;
			return 1;
		}


		mx::smtp smtp(io, mail_from, mail_passwd, mail_server);
		InternetMailFormat imf;

		imf.header["from"] = "mail_smtp_protoc_test";
		imf.header["to"] = boost::str(boost::format("\"Mr. \" <%1%>") % mail_to);
		imf.header["subject"] = "test mail";
		imf.header["content-type"] = "text/plain; charset=utf8";

		imf.body = "this mail send by mail smtp protoc test";
		std::stringstream maildata;
		boost::asio::streambuf buf;
		std::ostream os(&buf);
		imf_write_stream(imf, os);

		std::string _mdata = boost::asio::buffer_cast<const char*>(buf.data());

		smtp.async_sendmail(imf, [&io](const boost::system::error_code & ec){
			std::cout << ec.message() << std::endl;
			io.stop();
		});

		boost::asio::io_service::work work(io);
#endif
		avloop_run(io);
	return 0;
}