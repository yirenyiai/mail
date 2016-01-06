#pragma once
#include <string>
#include <vector>
#include <boost/locale.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/bind.hpp>
#include <boost/regex.hpp>
#include <boost/foreach.hpp>
namespace mail
{
	namespace body_type_select
	{
		static std::string find_mimetype(std::string contenttype)
		{
			if (!contenttype.empty()) {
				std::vector<std::string> splited;
				boost::split(splited, contenttype, boost::is_any_of("; "));
				return splited[0].empty() ? contenttype : splited[0];
			}

			return "text/plain";
		}

		static std::string decode_content_charset(std::string body, std::string content_type)
		{
			boost::cmatch what;
			boost::regex ex("(.*)?;[\t \r\a]?charset=(.*)?");

			if (boost::regex_search(content_type.c_str(), what, ex)) {
				// text/plain; charset="gb18030" 这种，然后解码成 UTF-8
				std::string charset = what[2];

				boost::to_upper(charset);
				return detail::ansi2utf8(body, charset);
			}

			return body;
		}

		// 从 IMF 中递归进行选择出一个最好的 mail content
		static void select_content(std::string& content_type, std::string& content, InternetMailFormat& imf);

		// 有　text/plain　的就选　text/plain, 没的才选　text/html
		static std::pair<std::string, std::string>
			select_best_mailcontent(MIMEcontent mime);

		// 有　text/plain　的就选　text/plain, 没的才选　text/html
		static std::pair<std::string, std::string>
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

		// 有　text/plain　的就选　text/plain, 没的才选　text/html
		static std::pair<std::string, std::string>
			select_best_mailcontent(MIMEcontent mime)
		{
			BOOST_FOREACH(InternetMailFormat & v, mime) {
				if (v.have_multipart) {
					return select_best_mailcontent(v);
				}

				// 从 v.first aka contenttype 找到编码.
				std::string mimetype = find_mimetype(v.header["content-type"]);

				if (mimetype == "text/plain") {
					return std::make_pair(v.header["content-type"], boost::get<std::string>(v.body));
				}
			}
			BOOST_FOREACH(InternetMailFormat & v, mime) {
				if (v.have_multipart) {
					return select_best_mailcontent(v);
				}

				// 从 v.first aka contenttype 找到编码.
				std::string mimetype = find_mimetype(v.header["content-type"]);

				if (mimetype == "text/html")
					return std::make_pair(v.header["content-type"], boost::get<std::string>(v.body));
			}
			return std::make_pair("", "");
		}
	}
}



// 从 IMF 中递归进行选择出一个最好的 mail content
static void select_content(std::string& content_type, std::string& content, InternetMailFormat& imf)
{
	using namespace mail::body_type_select;
	std::pair<std::string, std::string> mc = select_best_mailcontent(imf);
	content_type = find_mimetype(mc.first);
	content = decode_content_charset(mc.second, mc.first);
}