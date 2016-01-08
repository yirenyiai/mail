#pragma once


#include "protoc\pop3.hpp"
#include "protoc\imap.hpp"


void fetch_mail(const mailcontent& mail_ctx, mx::imap::call_to_continue_function handler)
{


	handler(0);

}


int main()
{
	boost::asio::io_service io;

	mx::imap imap(io, "", "", "imap.qq.com");

	imap.async_fetch_mail(fetch_mail);


	io.run();
	return 0;
}