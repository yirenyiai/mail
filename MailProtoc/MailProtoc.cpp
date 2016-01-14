#pragma once


#include "protoc\pop3.hpp"
#include "protoc\imap.hpp"
#include "protoc\smtp.hpp"

void imap_fetch_mail(const mailcontent& mail_ctx, mx::imap::call_to_continue_function handler)
{
	handler(0);
}

void pop_fetch_mail(const mailcontent& mail_ctx, mx::pop3::call_to_continue_function handler)
{
	handler(0);
}


int main(int argc, char* argv[])
{
	if (argc == 5)
	{
		boost::asio::io_service io;

		//mx::imap imap(io, argv[1], argv[2], argv[3], true);

		//imap.async_fetch_mail(imap_fetch_mail);

		mx::pop3 pop(io, argv[1], argv[2], argv[4], false);
		pop.async_fetch_mail(pop_fetch_mail);

		io.run();
	}
	return 0;
}