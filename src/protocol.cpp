/*
    Copyright © 2018 Emilia "Endorfina" Majewska

    This file is part of Violet.

    Violet is free software: you can study it, redistribute it
    and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    Violet is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Violet.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "pch.h"
//#include <strptime.h>
#include "app_lifetime.h"
#include "protocol.hpp"

std::map<std::string, std::string, Violet::functor_less_comparator> Protocol::content_types;

std::pair<std::mutex, Violet::UniBuffer> Protocol::logging;

const char
* Protocol::dir_html = "html",
* Protocol::dir_log = "log";

std::string Protocol::dir_work;

using namespace std::string_view_literals;

void Protocol::Hi::Clear()
{
	get.clear();
	post.clear();
	cookie.clear();
	//clipboard.clear();
	file.clear();

	table.clear();

	raw_headers.clear();
	content_headers.clear();
}

std::string url_decode(const std::string_view& str) {
	std::string ret;
	for(size_t i = 0; i < str.length(); ++i) {
		unsigned char c = static_cast<unsigned char>(str[i]);
		if(c == '%' && i + 2 < str.length()) {
			unsigned char a = static_cast<unsigned char>(str[++i]);
			unsigned char b = static_cast<unsigned char>(str[++i]);
			ret.push_back
				(((('0' <= a && a <= '9')? a - '0' :
				  ('a' <= a && a <= 'f')? 10 + a - 'a' :
				  ('A' <= a && a <= 'F')? 10 + a - 'A' : 0) << 4) |
				 (('0' <= b && b <= '9')? b - '0' :
				  ('a' <= b && b <= 'f')? 10 + b - 'a' :
				  ('A' <= b && b <= 'F')? 10 + b - 'A' : 0));
		} else {
			ret.push_back(c);
		}
	}
	return ret;
}

std::string ReadUntilSpace(Violet::UniBuffer &src, const char trim_char)
{
	const size_t p1 = src.get_pos();
	const auto data = reinterpret_cast<char*>(src.data());
	for (size_t i = p1; i < src.length(); ++i) {
		if (data[i] == 0x20 && (trim_char == 0 || data[i - 1] == trim_char))
		{
			src.set_pos(i + 1);
			return std::string(data + p1, i - (trim_char ? p1 + 1 : p1));
		}
	}
	return std::string();
}
std::string ReadAlphanumeric(const std::string &src, size_t pos, bool include_underscore)
{
	auto data = src.c_str() + pos;
	char c = *data;
	for (size_t i = 0; i < src.length(); c = data[++i]) {
		if ((c < '0' && c > '9') && (c < 'A' && c > 'Z') && (c < 'a' && c > 'z') && (!include_underscore || c != '_'))
		{
			return std::string(data, i);
		}
	}
	return std::string();
}

std::string ReadUntilSpace(const std::string &src, size_t pos, const char trim_char)
{
	auto data = src.c_str();
	for (auto i = pos; i < src.length(); ++i) {
		if (data[i] <= 0x20 && (trim_char == 0 || data[i - 1] == trim_char))
		{
			return std::string(data + pos, i - (trim_char ? pos + 1 : pos));
		}
	}
	return std::string();
}

// std::string ReadUntilNewLine(Violet::Buffer &src)
// {
// 	const size_t p1 = src.get_pos();
// 	const char * const data = static_cast<char*>(src);
// 	size_t i = p1;
// 	for (i = p1; i < src.length(); ++i)
// 		if (data[i] == 0xa)
// 		{
// 			src.set_pos(i + 1);
// 			return std::string(data + p1, i - p1 - (i > 0 && data[i - 1] == 0xd ? 1 : 0));
// 		}
// 	return std::string(data + p1, i - p1 - 1);
// }

template<class B>
inline bool EndOfHeader(B&&src)
{
	const char * data = src.data() + src.get_pos();
	return (*data == 0xd && *(data + 1) == 0xa);
}

template<typename _Pos>
std::pair<std::string, std::string> __parse_until_post_delimiter(const char * const data, _Pos& pos, const size_t len)
{
	std::pair<std::string, std::string> p;
	auto i = pos;
	for (; i < len; ++i) {
		if (data[i] == '=' || data[i] == '&')
		{
			break;
		}
	}
	p.first.assign(data + pos, i - pos);
	if (data[pos = i] == '=')
	{
		i = ++pos;
		for (; i < len; ++i) {
			if (data[i] == '&')
				break;
		}
		p.second.assign(data + pos, i - pos);
		for (auto &c : p.second)
			if (c == '+')
				c = 0x20;
		p.second = url_decode(p.second);
		pos = i + 1;
	}
	return p;
}

template<typename _Pos>
std::pair<std::string, std::string> __parse_until_cookie_delimiter(const char * const data, _Pos& pos, const size_t len)
{
	std::pair<std::string, std::string> p;
	auto i = pos;
	for (; i < len; ++i)
		if (data[i] < '!' || data[i] > '~' || data[i] == 0x20 || data[i] == ',' || data[i] == ';')
		{
			pos = i + 1;
		}
		else break;
	for (i = pos; i < len; ++i) {
		if (data[i] == '=' || data[i] == ';' || data[i] == 0x20)
		{
			break;
		}
	}
	p.first.assign(data + pos, i - pos);
	if (data[pos = i] == '=')
	{
		i = ++pos;
		for (; i < len; ++i) {
			if (data[i] < '!' || data[i] > '~' || data[i] == 0x20 || data[i] == ',' || data[i] == ';')
				break;
		}
		p.second.assign(data + pos, i - pos);
		pos = i + 1;
	}
	return p;
}

template<typename _Pos>
std::pair<std::string, std::string> __parse_until_POST_disposition_delimiter(const char * const data, _Pos &pos, const size_t len)
{
	std::pair<std::string, std::string> p;
	size_t i;
	for (i = pos; i < len; ++i)
		if (data[i] < '!' || data[i] > '~' || data[i] == 0x20 || data[i] == ',' || data[i] == ';')
		{
			pos = i + 1;
		}
		else break;
	for (i = pos; i < len; ++i)
		if (data[i] == '=' || data[i] == ';' || data[i] == 0x20)
		{
			break;
		}
	p.first.assign(data + pos, i - pos);
	if (data[pos = i] == '=')
	{
		if (data[i = ++pos] == '"')
		{
			for (i = ++pos; i < len; ++i) {
				if (data[i] == '"')
					break;
			}
			p.second.assign(data + pos, i - pos);
			pos = i + 2;
		}
		else {
			for (; i < len; ++i) {
				if (data[i] < '!' || data[i] > '~' || data[i] == 0x20 || data[i] == ',' || data[i] == ';')
					break;
			}
			p.second.assign(data + pos, i - pos);
			pos = i + 1;
		}
	}
	return p;
}

template<class Str, class Map>
void parse_post_content(const Str & src, Map& dest)
{
	size_t pos = 0;
	const auto len = src.length();
	do {
		auto p = __parse_until_POST_disposition_delimiter(src.data(), pos, len);
		if (!!p.first.size())
			dest.emplace(std::move(p));
	} while (pos < len);
}

size_t Protocol::Hi::BuildFromBuffer(Violet::UniBuffer &&src)
{
	unsigned i;
	table.swap(src);
	table.write<uint8_t>(0);
	char * const str = static_cast<char*>(table);
	const size_t len = table.length();
	for (i = 0; i < len && str[i] >= 'A' && str[i] <= 'Z'; ++i);
	str[i++] = 0x0;
	method = Method::Error;
	if (strcmp(str,"GET") == 0)
		method = Method::Get;
	else if (strcmp(str,"POST") == 0)
		method = Method::Post;
	else if (strcmp(str,"HEAD") == 0)
		method = Method::Head;
	else if (strcmp(str,"OPTIONS") == 0)
		method = Method::Options;
	else if (strcmp(str,"PUT") == 0)
		method = Method::Put;
	else if (strcmp(str,"DELETE") == 0)
		method = Method::Delete;
	else if (strcmp(str,"TRACE") == 0)
		method = Method::Trace;

	if (method != Method::Error)
	{
		for (; i < len && str[i] <= 0x20; ++i);
		fetch = str + i;
		for (; i < len && str[i] > 0x20; ++i);
		str[i++] = 0;
		if (i + 8 >= len)
			return 0;
		else {
			std::lock_guard<std::mutex> lock(logging.first);
			logging.second << str << ' ' << fetch << ' ';
		}
		
		for (; i < len && str[i] <= 0x20; ++i);
		const auto protocol = str + i;
		for (; i < len && str[i] > 0x20; ++i);
		if (str[i] != '\r')
			return 0;
		str[i++] = 0;
		if ((strcmp(protocol,"HTTP/1.1") == 0 || strcmp(protocol,"HTTP/1.0") == 0) && str[i++] == '\n')
		{
			raw_headers.clear();
			do {
				auto s1 = str + i;
				for (; i < len && str[i] > 0x20 && str[i] != ':'; ++i);
				if (str[i] != ':')
					return 0;
				std::transform(s1, str + i, s1, ::tolower);
				str[i++] = 0;
				for (; i < len && str[i] <= 0x20; ++i);
				auto s2 = str + i;
				for (; i < len && str[i] >= 0x20; ++i);
				if (str[i] != '\r')
					return 0;
				str[i] = 0;
				i += 2;
				raw_headers.emplace(std::make_pair(s1, s2));
				if (str[i] == 0xd && str[i + 1] == 0xa) {
					*reinterpret_cast<uint16_t*>(str + i) = 0x0;
					table.set_pos(i + 2);
					
					if (method == Method::Post)
						puts(str + i + 2);
					break;
				}
			} while (i < len);
		}
	}
	else {
		std::lock_guard<std::mutex> lock(logging.first);
		logging.second << "Unknown Method "sv;
	}
	return raw_headers.size();
}

size_t Protocol::Hi::ParsePOST(const char * data, const size_t len)
{
	size_t pos = 0;
	if (data[0] == 0xd && data[1] == 0xa)
		data += 2;

	if (auto r = raw_headers.find("content-type"); r != raw_headers.end())
	{
		const std::string_view type2{ r->second };
		bool is_multipart = type2.substr(0, strlen("multipart/form-data")) == "multipart/form-data";
		if (is_multipart) {
			auto pf = type2.find("boundary=", strlen("multipart/form-data"));
			if (pf == std::string::npos)
				return 0;
			pf += strlen("boundary="); // length of 'boundary='
			size_t pfe = std::string::npos;
			for (size_t i = pf, m = type2.length(); i < m; ++i)
				if (type2[i] < '!' || type2[i] > '~' || type2[i] == ';')
				{
					pfe = i - pf;
					break;
				}
			std::string boundary{"--" + static_cast<std::string>(type2.substr(pf, pfe))}, content(data, data + len);
			pos = boundary.length();
			while (pos < len)
			{
				auto limit = *reinterpret_cast<const uint16_t *>(content.c_str() + pos);
				if (limit == 0x2d2d)
					break;
				if (limit == 0xa0d)
					pos += 2u;
				pfe = std::min(content.find(boundary, pos), len);
				Violet::UniBuffer instance;
				instance.write_data(content.c_str() + pos, pfe - pos);
				pos = pfe + boundary.length();
				pf = 0u;
				std::map<std::string_view, std::string_view, Violet::cis_functor_less_comparator> headers;
				do {
					auto s1 = instance.get_string_current();
					s1 = s1.substr(0, s1.find(':'));
					if (s1.size()) {
						instance.set_pos(instance.get_pos() + s1.size());
						s1.remove_suffix(1);
					}
					auto s2 = instance.get_string_current();
					s2 = s2.substr(0, s1.find('\n'));
					instance.set_pos(instance.get_pos() + s2.size());
					while (s2.size() && !!std::isspace(static_cast<unsigned char>(s2.back())))
						s1.remove_suffix(1);
					while (s2.size() && !!std::isspace(static_cast<unsigned char>(s2.front())))
						s1.remove_prefix(1);
					headers.emplace(std::make_pair(s1,s2));
					if (EndOfHeader(instance)) {
						instance.read<uint16_t>();
						break;
					}
				} while (!instance.is_at_end());
				
				if (auto r = headers.find("content-disposition"); r != headers.end())
				{
					std::unordered_map<std::string, std::string> disposition;
					parse_post_content(r->second, disposition);
					auto name = disposition.find("name");
					if (name == disposition.end())
						break;
					r = headers.find("content-type");
					auto isv = instance.get_string_current();
					if (auto rfn = disposition.find("filename"); r != headers.end() || rfn != disposition.end())
					{
						_F newfile;
						newfile.name.assign(name->second);
						if (rfn != disposition.end())
							newfile.filename.assign(rfn->second);
						if (r != headers.end())
							newfile.mime_type.assign(r->second);
						newfile.data.assign(isv.data(), isv.data() + isv.size());
						if (newfile.data.size() > 1 && newfile.data[newfile.data.size() - 2] == '\r' && newfile.data[newfile.data.size() - 1] == '\n')
							newfile.data.resize(newfile.data.size() - 2);
						if (newfile.name.size() > 0)
							file.emplace_back(newfile);
					}
					else {
						isv.remove_suffix(2);
						if (!!name->second.size())
							post[name->second] = static_cast<std::string>(isv);
					}
				}
			}
		}
		else do {
			auto p = __parse_until_post_delimiter(data, pos, len);
			if (p.first.size() > 0)
				post.emplace(std::move(p));
		} while (pos < len);
		return post.size();
	}
	else return 0;
}

size_t Protocol::Hi::ParseGET(const char *src, size_t offset)
{
	const auto len = strlen(src);
	while (offset < len) {
		auto p = __parse_until_post_delimiter(src, offset, len);
		if (p.first.size() > 0)
			get.emplace(std::move(p));
	}
	return get.size();
}

size_t Protocol::Hi::ParseCookies(const char *src)
{
	unsigned int pos = 0;
	const auto len = strlen(src);
	do
	{
		auto p = __parse_until_cookie_delimiter(src, pos, len);
		if (p.first.size() > 0)
			cookie.emplace(std::move(p));
	} while (pos < len);
	return cookie.size();
}

void Protocol::Hi::Print()
{
	printf(" > $Addr = ");
	printf(fetch);
	printf("\n");
#ifdef __PRINT_WHOLE_REQUEST
	for (auto &&it : raw_headers)
	{
		printf(" > ");
		printf(it.first);
		printf(" : ");
		printf(it.second);
		printf("\n");
	}
#endif
}



template<class T>
inline unsigned int ReadUnsignedInt(Violet::buffer<T> &src, uint8_t bytes = 2)
{
	unsigned int i = 0;
	while (bytes > 1)
		i += src.template read<uint8_t>() * static_cast<int>(pow(256, --bytes));
	return i += src.template read<uint8_t>();
}

//#include "file_cache.hpp"

struct file {
	bool is_html = false;
	//std::string_view data;
	Violet::UniBuffer data;
	struct stat attrib;
	
	//template <class S>
	static file search(const std::string_view &_sv, const char * _folder)
	{
		file _f;
		std::string fn { _sv };
		if (_folder != nullptr) {
			if (_f.data.read_from_file((fn = _folder + fn).c_str())) {
				::stat(fn.c_str(), &_f.attrib);
				_f.is_html = false;
			}
			else if (_f.data.read_from_file((fn += ".html").c_str())) {
				_f.is_html = true;
			}
			if (!!_f.data.size())
				return _f;
			fn.assign(_sv);
		}
		if (_f.data.read_from_file((fn = DIRECTORY_SHARED + fn).c_str())) {
			::stat(fn.c_str(), &_f.attrib);
			_f.is_html = false;
		}
		else if (_f.data.read_from_file((fn += ".html").c_str())) {
			_f.is_html = true;
		}
		return _f;
	}
};

//#include <iostream> just for testing

void Protocol::HandleRequest()
{
	if (!received)
	{
		s.update_read();
		auto st = s.get_state();
		if (st == Violet::State::connected)
		{
			if (Violet::UniBuffer b; s >> b)
			{
				received_body = true;
				body_length = 0;
				WriteDateToLog();
				//printf("> Received %zu bytes [id:%lu]\n%s\n", b.GetLength(), id, b.ToString());
				if (info.BuildFromBuffer(std::move(b)))
				{
					auto r = info.raw_headers.find("Cookie");
					if (r != info.raw_headers.end())
					{
						info.ParseCookies(r->second);
						
						if (auto r2 = info.cookie.find("SSID"); r2 != info.cookie.end())
						{
							auto ssid = shared.sessions.find(r2->second);
							if (ssid != shared.sessions.end())
							{
								ss = &(ssid->second);
								ss->last_activity = std::chrono::system_clock::now();
							}
							else info.AddHeader("Set-Cookie", SESSION_KILL_CMD);
						}
					}
					if (info.method == Hi::Method::Post)
					{
						r = info.raw_headers.find("content-length");
						if (r != info.raw_headers.end() && sscanf(r->second, "%zu", &body_length) == 1) {
							if (body_length > info.GetRemainingTable()) {
								received_body = false;
								body_temp.assign(info.GetTableAtPos(), info.GetTableAtPos() + info.GetRemainingTable());
							}
							else info.ParsePOST(info.GetTableAtPos(), body_length);
						}
					}
					r = info.raw_headers.find("Host");
					if (r != info.raw_headers.end()) {
						std::lock_guard<std::mutex> lock(logging.first);
						logging.second << "Host: \""sv << r->second << '\"';
					}
					sent = response_ready = false;
					//printf("> Packet [id:%lu, addr:%s]\n", id, info.fetch.c_str());
				}
				else
				{
					response_ready = true;
					sent = true;
					std::lock_guard<std::mutex> lock(logging.first);
					logging.second << "<BAD REQUEST>?"sv;
				}
				std::string ip{ s.get_peer_address() };
				std::lock_guard<std::mutex> lock(logging.first);
				if (ip != "127.0.0.1")
					logging.second << ' ' << ip << ';';
				logging.second.write_crlf();
				if (logging.second.length() > MAX_HEAP_SIZE)
					SaveLogHeapBuffer();
				received = true;
#ifdef MONITOR_SOCKETS
				++_packets_sent;
#endif
				if (!received_body)
					return;
			}
		}
		else if (st == Violet::State::closed || st == Violet::State::error)
		{
			response_ready = true;
			sent = true;
			return;
		}
	}
	if (!received_body && body_length > 0)
	{
		s.update_read();
		auto st = s.get_state();
		if (st == Violet::State::connected)
		{
			Violet::UniBuffer b;
			if (s.read_message(b))
			{
				const auto pos = body_temp.size();
				body_temp.resize(pos + b.length());
				memcpy(&body_temp[pos], b.data(), b.length());
				if (body_temp.size() >= body_length) {
					body_temp.push_back(0);
					received_body = true;
					info.ParsePOST(body_temp.data(), body_length);
					body_temp.clear();
				}
			}
			return;
		}
		else if (st == Violet::State::closed || st == Violet::State::error)
		{
			response_ready = true;
			sent = true;
			received_body = true;
			return;
		}
	}
	if (!response_ready && received)
	{
		Violet::UniBuffer message;
		file varf;
		Hi::headers_t::iterator key;
		bool modified = true, partial = false, created = false;
		uint16_t error = 0;
		std::string_view filename = info.fetch;
		const auto get_mark = Violet::find_skip_utf8(filename, '?');

		if (get_mark != std::string::npos) {
			info.ParseGET(info.fetch, get_mark + 1);
			filename = filename.substr(0, get_mark);
		}

		if (info.method == Hi::Method::Post && filename == "/savePycBz") {
			auto fd1 = std::find_if(info.file.begin(), info.file.end(), [](const Hi::_F &sp) { return sp.name == "upfile"; });
			if (fd1 != info.file.end()) {
				Violet::UniBuffer ff;
				ff.write(fd1->data);
				ff.write_to_file((getenv("HOME") + ('/' + fd1->filename)).c_str());
			}
			created = true;
			error = 201;
		}
		else if (info.method != Hi::Method::Get && info.method != Hi::Method::Post && info.method != Hi::Method::Head)
		{
			error = 405;
			info.AddHeader("Allow", "GET, HEAD, POST");
		}
		else
		{
			const size_t slashes = std::count(filename.begin(), filename.end(), '/');
			if (std::count(filename.begin(), filename.end(), '\\') > 0 || filename.front() != '/')
				error = 400; // BAD REQUEST
			else if (slashes > 4)
				error = 403;
			else if (filename.length() == 1)
			{
				varf.is_html = true;
				filename = "/root.html";
			}
			if (std::count(filename.begin(), filename.end(), '.') > 1 || slashes > 1)
			{
				auto p = filename.find("..");
				if (p == std::string::npos)
					p = filename.find("//");
				if (p == std::string::npos)
					p = filename.find("/.");
				if (p != std::string::npos)
				{
					error = 400;
				}
			}
		}
		if (!error) {
			if (filename.substr(0, 9) == "/captcha.") {
				auto capf = std::find_if(shared.captcha_sig.begin(), shared.captcha_sig.end(), [sv = filename.substr(9)] (const auto &c) { return c.first.PicFilename == sv; });
				if (capf != shared.captcha_sig.end()) {
					auto ef = capf->first.Data.get();
					shared.captcha_sig.erase(capf);
					created = true;
					if (ef.ptr)
						varf.data.read_from_mem(ef.ptr, ef.size);
				}
				else error = 404;
			}
			else if (varf = file::search(filename, shared.dir_accessible); !varf.is_html) {
				if (!varf.data.length()) {
					std::string efn { dir_html };
					efn += "/error.html";
					varf.data.read_from_file(efn.c_str());
					varf.is_html = true;
					error = 404;
					if (!varf.data.length())
						varf.data << "No file found. :(\n"sv;	// one more check just in case there's no error file
				}
				else if (filename.length() > 5
						&& filename.substr(filename.length() - 5) == ".html")
					varf.is_html = true;
			}
		}
		char dt[64];
		std::unique_ptr<char[]> dtm, s_len, s_range;
		static const char * dtformat = "%a, %d %b %Y %T GMT";
		auto now = time(nullptr);
		tm gmt = *gmtime(&now);
		strftime(dt, 64, dtformat, &gmt);

		if (!error && !varf.is_html && !created)
		{
			gmt = *gmtime(&(varf.attrib.st_ctime));
			dtm.reset(new char[64]);
			strftime(dtm.get(), 64, dtformat, &gmt);
			key = info.raw_headers.find("if-modified-since");
			if (key != info.raw_headers.end())
			{
				tm modt;
				memset(&modt, 0, sizeof(tm));
				strptime(key->second, "%a, %d %b %Y %T", &modt);
				if (difftime(mktime(&modt), mktime(&gmt)) <= 0)
					modified = false;
			}
		}
		info.AddHeader("Date", dt);
		info.AddHeader("Server", VIOLET_CUSTOM_USER_AGENT);
#ifdef ___KEEP_ALIVE_CONNECTION
		key = info.raw_headers.find("Connection");
		if (key == info.raw_headers.end() || Violet::__cis_compare(key->second, "keep-alive") != 0)
			info.AddHeader("Connection", "close");
		else
		{
			info.AddHeader("Connection", "keep-alive");
			info.keepalive = true;
		}
#else
		info.AddHeader("Connection", "close");
#endif
		info.AddHeader("Accept-Ranges", "bytes");
		if (varf.data.length() > 0 && modified)
		{
			if (varf.is_html) {
				if (auto o = HandleHTML(varf.data.get_string(), error); bool(o))
					varf.data = std::move(*o);
				info.AddHeader("Content-Type", "text/html");
			}
			else {
				std::string_view ext;
				if (auto last_period = filename.find_last_of('.'); last_period != std::string::npos)
					ext = filename.substr(last_period);
				const auto skey = Protocol::content_types.find(ext);
				info.AddHeader("Content-Type", skey != Protocol::content_types.end() ? skey->second.c_str() : "text/plain");
			}
			// RANGE
			key = info.raw_headers.find("range");
			if (key != info.raw_headers.end())
			{
				size_t beg = 0, en = 0;
				const size_t s = varf.data.length();
				int amt = sscanf(key->second, "bytes=%zu-%zu", &beg, &en);
				partial = true;
				if (amt > 0) {
					s_range.reset(new char[64]);
					Violet::UniBuffer swap;
					if (amt == 1)
						en = s - 1;
					if (beg >= s || en >= s || beg > en) {
						error = 416;
						varf.data.clear();
					}
					else {
						snprintf(s_range.get(), 64, "bytes %zu-%zu/%zu", beg, en, s);
						info.AddHeader("Content-Range", s_range.get());
						swap.write_buffer(varf.data, beg, en - beg + 1);
						varf.data.swap(swap);
					}
				}
				else {
					error = 416;
					varf.data.clear();
				}
			}

#ifdef USE_PACKET_COMPRESSION
			// CONTENT ENCODING
			key = info.raw_headers.find("accept-encoding");
			if (key != info.raw_headers.end())
			{
				std::map<std::string, float> encodings;
				size_t p1 = 0u, p2 = 0u;
				const size_t len = strlen(key->second);
				if (len > 0) {
					auto func1 = [](char c) { return c > 0x20 && c != ';' && c != ','; };
					while ((p1 = p2) < len) {
						while (func1(key->second[p2]) && p2 < len)
							++p2;
						if (p1 < p2) {
							float &enc_val = encodings[std::string{key->second + p1, p2 - p1}];
							enc_val = 1.f;
							while (key->second[p2] == 0x20)
								++p2;
							if (key->second[p2] == ';')
							{
								sscanf(key->second + ++p2, "q=%f", &enc_val);
								while (key->second[p2] != ',' && p2 < len)
									++p2;
								++p2;
							}
							else if (key->second[p2] == ',')
								++p2;
							while (key->second[p2] == 0x20)
								++p2;
						}
						else break;
					}
					auto deflate = encodings.find("deflate");
					if (deflate != encodings.end()) {
						if (deflate->second > 0.f)
						{
							auto swap = Violet::UniBuffer::zlib_compress(varf.data.get_string(), false);
							if (swap.length() < varf.data.length())
							{
								info.AddHeader("Content-Encoding", "deflate");
								varf.data.swap(swap);
							}
						}
					}
				}
			}
#endif
			// TRANSFER LENGTH
			s_len.reset(new char[32]);
			snprintf(s_len.get(), 32, "%zu", varf.data.length());
			info.AddHeader("Content-Length", s_len.get());
			info.AddHeader("Last-Modified", varf.is_html || !bool(dtm) ? dt : dtm.get());
			/* // md5???
			info.AddHeader("Content-MD5", ...);*/
		}
		info.raw_headers.clear();
		message << "HTTP/1.1 "sv;
		if (error > 0)
		{
			message << std::to_string(static_cast<int>(error)) << char(0x20);
			switch (error)
			{
			case 400:
				message << "Bad Request"sv;
				break;
			case 403:
				message << "Forbidden"sv;
				break;
			case 404:
				message << "Not Found"sv;
				break;
			case 405:
				message << "Method Not Allowed"sv;
				break;
			case 416:
				message << "Requested Range Not Satisfiable"sv;
				break;
			case 501:
				message << "Not Implemented"sv;
				break;
			}
		}
		else if (!modified)
			message << "304 Not Modified"sv;
		else if (partial)
			message << "206 Partial Content"sv;
		else
			message << "200 OK"sv;
		message.write_crlf();
		for (auto &&a : info.content_headers) {
			message << a.first << ": "sv << a.second << "\r\n"sv;
		}
		info.content_headers.clear();

		message.write_crlf();
		if (varf.data.length() > 0 && modified && info.method != Hi::Method::Head)
			message << varf.data;
		s << message;
		response_ready = true;
		sent = false;
		last_used = std::chrono::system_clock::now();
	}
	
	if (!sent && response_ready)
	{
		if (info.keepalive)
		{
			received = false;
			info.Clear();
			ss = nullptr;
		}
		else s.to_be_closed();
		s.update_write();
		if (s.get_state() != Violet::State::connected) {
			sent = true;
		}
	}
}

bool Protocol::CheckRegistrationData(std::vector<std::string> &data, std::string &error_msg)
{
	if(data[0].empty())
	{
		error_msg += "Username field is empty!<br>";
		return false;
	}
	if (data[4].empty() || data[5].empty())
	{
		error_msg += "Control question is empty!<br>";
		return false;
	}
	else
	{
			Captcha::Init captcha{ std::move(data[4]) };
			captcha.set_up(false);

			std::vector<Captcha::Signature::collectible_type> q(captcha.Imt.Collection.begin(), captcha.Imt.Collection.end());
			for (int n = 3; n >= 0; --n)
				for (unsigned i = 1u; i < q.size(); ++i)
					if (q[i].first == n)
					{
						switch (n) {
						case 0: q[i - 1].second += q[i].second; break;
						case 1: q[i - 1].second -= q[i].second; break;
						case 2: q[i - 1].second *= q[i].second; break;
						case 3: q[i - 1].second /= q[i].second; break;
						}
						q.erase(q.begin() + i--);
					}
			long correct_answer = q[0].second, answer;
			if (sscanf(data[5].c_str(), "%ld", &answer) == 1 && answer != correct_answer)
			{
				error_msg += "Control answer is wrong!<br>Correct answer was <b>";
				error_msg += std::to_string(correct_answer);
				error_msg += "</b><br>";
				return false;
			}
	}
	if (data[0].back() <= 0x20)
		data[0].resize(data[0].length() - 1);
	if (!data[3].empty())
		while (data[3].back() <= 0x20)
			data[3].resize(data[3].length() - 1);
	if (data[0].length() < 3)
	{
		error_msg += "Username too short!<br>";
		return false;
	}
	if (data[0].length() > 20)
	{
		error_msg += "Username can't be longer than 20 characters.<br>";
		return false;
	}
	else
	{
		if (std::find_if_not(data[0].begin(), data[0].end(), is_username_acceptable) != data[0].end())
		{
			error_msg += "Username can only contain letters, digits and an underscore.<br>";
			return false;
		}
		auto c = data[0][0];
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
		{
			error_msg += "Username must start with a letter.<br>";
			return false;
		}
	}
	if (data[1].length() < 6)
	{
		error_msg += "Password must be at least 6 characters long.<br>";
		return false;
	}
	if (data[1] != data[2])
	{
		error_msg += "Passwords do not match.<br>";
		return false;
	}
	return true;
}

void Protocol::CreateSession(std::string_view name, Violet::UniBuffer *loaded_file = nullptr)
{
	std::string cookie;
	auto r = std::find_if(shared.sessions.begin(), shared.sessions.end(), [&name](std::pair<const std::string, Session> &p) { return p.second.username == name; });
	if (r != shared.sessions.end())
	{
		cookie = r->first;
		ss = &r->second;
		ss->last_activity = std::chrono::system_clock::now();
	}
	else
	{
		for (auto c : name)
			if (is_username_acceptable(c))
				cookie += c;
		const auto _rpos = cookie.size();
		cookie.resize(_rpos + 30);
		Violet::generate_random_string(&cookie[_rpos], 30);
		auto ssid = shared.sessions.emplace(cookie, std::string(name.data(), name.size()));
		ss = &(ssid.first->second);

		bool buffer_is_private = false;
		if (loaded_file == nullptr) {
			std::string dir(dir_work);
			loaded_file = new Violet::UniBuffer;
			buffer_is_private = true;
			ensure_closing_slash(dir += shared.dir_accounts);
			auto pos = dir.length();
			dir.append(name.data(), name.size());
			std::transform(dir.begin() + pos, dir.end(), dir.begin() + pos, ::tolower);
			loaded_file->read_from_file((dir + USERFILE_ACC).c_str());
		}
		loaded_file->set_pos(20u);
		loaded_file->set_pos(loaded_file->read<uint16_t>() + 24u);

		ss->userlevel = loaded_file->read<uint8_t>() - 'A';
		ss->userlevel = std::min<decltype(ss->userlevel)>(ss->userlevel, 10);

		size_t pos = loaded_file->get_pos(), len = loaded_file->length();
		for (auto c = loaded_file->data(); pos < len && (c[pos] != 0xd || c[pos + 1] != 0xa); ++pos);
		loaded_file->set_pos(pos + 2u);
		if (!loaded_file->is_at_end()) {
			ss->email = static_cast<std::string>(loaded_file->read_data(loaded_file->read<uint16_t>()));
		}
		if (buffer_is_private)
			delete loaded_file;
	}
	cookie = "SSID=" + cookie;
	info.ParseCookies(cookie.c_str());
	info.temp_strs.emplace_back(cookie);
	info.AddHeader("Set-Cookie", info.temp_strs.back().c_str());
}

void Protocol::WriteDateToLog()
{
	char dt[50];
	GetTimeGMT(dt, sizeof(dt), "[%j %T]: ");
	std::lock_guard<std::mutex> lock(logging.first);
	logging.second << dt;
}

void Protocol::SaveLogHeapBuffer()
{
	if (logging.second.length() > 0)
	{
		char dt[32];
		std::string fn(dir_log);
		fn += "/activity.";
		GetTimeGMT(dt, sizeof(dt), "%yw%j.txt");
		fn += dt;
		logging.second.append_to_file(fn.c_str());
		logging.second.clear();
	}
}

inline void GetTimeGMT(char * dest, size_t buffSize, const char * _format, bool localTime)
{
	auto now = time(nullptr);
	tm gmt;
	if (localTime)
		gmt = *localtime(&now);
	else
		gmt = *gmtime(&now);
	strftime(dest, buffSize, _format, &gmt);
}