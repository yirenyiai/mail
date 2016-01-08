#pragma once


#include "protoc\pop3.hpp"
#include "protoc\imap.hpp"


void fetch_mail(const mailcontent& mail_ctx, mx::imap::call_to_continue_function handler)
{


	handler(0);

}


int main(int argc, char* argv[])
{
	if (argc == 4)
	{
		boost::asio::io_service io;

		mx::imap imap(io, argv[1], argv[2], argv[3]);

		imap.async_fetch_mail(fetch_mail);


		io.run();
	}
	return 0;
}