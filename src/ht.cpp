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
#include "protocol.hpp"
#include "curly_tags.hpp"

using namespace std::string_view_literals;

inline void GenerateSalt(Violet::UniBuffer &dest, const unsigned _Size = 512u)
{
	std::random_device dev;
	std::minstd_rand gen{ dev() };
	std::uniform_int_distribution<int> uid{ CHAR_MIN, CHAR_MAX };
    dest.resize(_Size);
	auto d = dest.data();
	for (unsigned i = _Size; i > 0; --i)
		(*(d++)) = static_cast<char>(uid(gen));
}

bool DirectoryExists(const char* pzPath)
{
	struct stat st;
	return ::stat(pzPath, &st) == 0 ? (st.st_mode & S_IFDIR != 0) : false;
}

inline bool FileExists(const char * szPath) {
	struct stat buffer;
	return ::stat(szPath, &buffer) == 0;
}

template<class InOutContainer, class ViewT,
	typename = std::enable_if_t<std::is_same_v<ViewT, typename InOutContainer::view_t>>>
void WrapHTML(InOutContainer &f, const ViewT& wrapper_content, const ViewT& wrapper_title)
{
	ViewT src = f.get_string();
	InOutContainer out;
	bool found_content = false;

	while (true)
		if (auto pos = Violet::find_skip_utf8(src, '{'), pose = pos != ViewT::npos ? Violet::find_skip_utf8(src, '}', pos + 1) : ViewT::npos;
				pose == ViewT::npos || src.size() < 3) {
			if (out.size())
				out.write(src);
			break;
		}
		else {
			const ViewT key = src.substr(pos + 1, pose++ - pos - 1);
			if (!found_content && key == ViewT{"content"}) {
				out.write(src.substr(0, pos));
				out.write(wrapper_content);
				found_content = true;
			}
			else if (key == ViewT{"title"}) {
				out.write(src.substr(0, pos));
				out.write(wrapper_title);
			}
			else {
				out.write(src.substr(0, pose));
			}
			if (pose > src.size())
				break;
			src.remove_prefix(pose);
		}
	
	if (found_content)
		f.swap(out);
}

inline bool is_command(const char c) {
	return (c >= '!' && c <= '~' && c != '=' && c != ',' && c != ':' && c != '(' && c != ')');
}

template<class View>
inline size_t find_block_enclosure(const View& src, const size_t start) {
	size_t p1 = start, p2 = start;
	unsigned inner_block_count = 0;
	bool found_exit_node = false;
	do {
		p1 = Violet::find_skip_utf8(src, '{', p2);
		if (p1 != View::npos)
			p2 = Violet::find_skip_utf8q(src, '}', p1);
		else { p2 = View::npos; break; }
		if (p2 == View::npos)
			break;
		++p2;
		if (src.substr(p1 + 1, 6) == View{"block:"})
			++inner_block_count;
		else if (src.substr(p1 + 1, 6) == View{"/block"}) //Change to 7 chars ( "/block}" ) - Optional!
			if (inner_block_count < 1)
				found_exit_node = true;
			else --inner_block_count;
	} while (!found_exit_node && p2 != View::npos && p1 != View::npos);
	return p2;
}

template<typename A>
inline bool is_meaningful(A c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '?' || c == '/';
}

void Protocol::HandleHTML(Violet::UniBuffer &h, uint16_t error)
{
	auto src = h.get_string();
	Violet::skip_potential_bom(src);
	
	if (Violet::find_skip_utf8(src, '{') != std::string_view::npos)
	{
		Violet::UniBuffer f;
		size_t read_pos = 0, end_read_pos = 0;
		while (read_pos < src.length())
		{
			if (src[read_pos] == 0xa)
				++read_pos;
			else if (src[read_pos] == 0xd && src[read_pos + 1] == 0xa)
				if ((read_pos += 2) >= src.length())
					break;
			size_t pos, pose = read_pos;

			std::string_view c;
			do {
				pos = Violet::find_skip_utf8(src, '{', pose);
				if (pos == std::string::npos) break;
				c = src.substr(std::min(src.length(), pos + 1));
				if (is_meaningful(c.front())) break;
				pose = pos + 1;
			} while(pose < src.length());
			if (pos != std::string::npos) {
				pose = Violet::find_skip_utf8q(src, '}', pos);
				if (pose == std::string::npos)
					pos = std::string::npos;
				else
					c.remove_suffix(src.length() - pose++);
			}
			else pose = std::string::npos;

			end_read_pos = std::min(src.length(), pos);
			if (end_read_pos - read_pos > 0)
				f.write(src.substr(read_pos, end_read_pos - read_pos));
			if (pos != std::string::npos && c.front() != '/' && c.front() != '?')
			{
				const Curly::Tag tag{c};
				if(tag.GetType() != Curly::modError)
				switch (tag.GetTag())
				{
				case Curly::tagBlock:
					{
						bool skip = true;
						const auto name = tag.GetObj(0).str;
						if (name == "Session")
						{
							if (ss != nullptr)
								skip = false;
						}
						else if (name == "NoSession")
						{
							if (ss == nullptr)
								skip = false;
						}
						else if (name == "Plain") {
							const size_t start = pose;
							pose = find_block_enclosure(src, start);
							if (pose != std::string::npos) {
								const size_t encl = src.find_last_of('{', pose - 1);
								if (encl > start) {
									std::string w(src.data() + start, encl - start);
									for (auto c : w)
										if (c >= 0x20)
											f.write(c);
										else if (c == 0xa)
											f.write("<br>"sv);
								}
							}
							break;
						}
						else if (tag.GetObjCount() == 3 && name == "Userlevel" && !tag.GetObj(1).is_string && tag.GetObj(2).is_string)
						{
							if (ss != nullptr) {
								unsigned val;
								if (sscanf(tag.GetObj(2).str.data(), "%u", &val) == 1)
									switch (tag.GetObj(1).val) {
									case '=': case 0x3d3d: // "=="
										if (val == ss->userlevel) skip = false;
										break;
									case '>':
										if (val < ss->userlevel) skip = false;
										break;
									case '<':
										if (val > ss->userlevel) skip = false;
										break;
									}
							}
						}
						if (skip) {
							pose = find_block_enclosure(src, pose);
						}
					}
					break;
					
					
				case Curly::tagBlockArray:
					{
						bool skip = true;
						if (tag.GetObjCount() > 1 && tag.GetObj(0).is_string && tag.GetObj(1).is_string)
						{
							const Hi::map_t& db = info.list(tag.GetObj(0).str);
							if (auto f = db.find(tag.GetObj(1).str); f != db.end())
							{
								if (tag.GetObjCount() <= 2) {
									skip = false;
								}
								else if (tag.GetObjCount() == 4 && !tag.GetObj(2).is_string && tag.GetObj(3).is_string)
								{
									long v1, v2;
									if (sscanf(f->second.c_str(), "%ld", &v1) == 1 && sscanf(tag.GetObj(3).str.data(), "%ld", &v2) == 1)
										switch (tag.GetObj(2).val) {
										case '=': case 0x3d3d: // "=="
											if (v1 == v2) skip = false; break;
										case '>':
											if (v1 > v2) skip = false; break;
										case '<':
											if (v1 < v2) skip = false; break;
										}
									else switch (tag.GetObj(2).val) {
										case '=': case 0x3d3d: // "=="
											if (f->second == tag.GetObj(3).str) skip = false; break;
										}
								}
							}
						}
						if (skip) {
							pose = find_block_enclosure(src, pose);
						}
					}
					break;


				case Curly::tagEcho:
					if(tag.GetObjCount() == 1) {
							const Hi::map_t& db = tag.GetObj(0).child ? info.list(tag.GetObj(0).str) : info.clipboard;
							if (auto a = db.find(tag.GetObj(0).child->str); a != db.end())
								f << a->second;
					}
					break;

				case Curly::tagStatusErrorCode:
					if (error) {
						f.write(std::to_string(static_cast<int>(error)));
					}
					break;

				case Curly::tagStatusErrorText:
					switch (error)
						{
						case 400:
							f.write("Bad Request"sv);
							break;
						case 403:
							f.write("Forbidden"sv);
							break;
						case 404:
							f.write("Not Found"sv);
							break;
						case 405:
							f.write("Method Not Allowed"sv);
							break;
						case 416:
							f.write("Requested Range Not Satisfiable"sv);
							break;
						case 501:
							f.write("Not Implemented"sv);
							break;
						}
					break;

				case Curly::tagCopyright:
					f.write("&copy;&nbsp;2017&nbsp;0x65"sv);
					break;

				case Curly::tagBitcoin:
					f.write("<a href=\"http://bitcoin.org\" target=_blank>BitCoin</a>:&nbsp;<a class=\"bitcoin_link\" href=\"bitcoin:1JGduM5un6Q6LqGQckkTH2FUs8p36NF3t9\">1JGduM5un6Q6LqGQckkTH2FUs8p36NF3t9</a>"sv);
					break;

				case Curly::tagBattery:

					break;

				case Curly::tagInclude:
					if (tag.GetObjCount() == 1 && tag.GetObj(0).is_string)
					{
						std::string fn(dir_html);
						fn += '/';
						fn += tag.GetObj(0).str;
						fn += ".html";
						Violet::UniBuffer t;
						if (t.read_from_file(fn.c_str())) {
							auto v = t.get_string();
							Violet::skip_potential_bom(v);
							f << v;
						}
					}
					break;
					
				case Curly::tagWrapper:
					if (tag.GetObjCount() >= 1)
					{
						const std::string fn{ "html/wrapper_" + std::string(tag.GetObj(0).str.data(), tag.GetObj(0).str.length()) + ".html" };
						
						Violet::UniBuffer t;
						if (t.read_from_file(fn.c_str())) {
							WrapHTML(t, src.substr(pose), tag.GetObjCount() > 1 ? tag.GetObj(1).str : "Untitled");
							t.swap(h);
							src = h.get_string();
							Violet::skip_potential_bom(src);
							pose = 0;
						}
					}
					break;

				case Curly::tagGenerateCaptcha:
					if (tag.GetType() == Curly::modTrigger)
					{
						Captcha::Init c;
						c.set_up(true);
						
						info.clipboard["captcha_seed"] = std::move(c.seed);
						info.clipboard["captcha_image"] = "/captcha." + c.Imt.PicFilename;	// this one is moved in the next step
						c.Imt.Data = std::async(std::launch::async, Captcha::Image::process, c.Imt.Collection);
						shared.captcha_sig.emplace_back(std::move(c.Imt), time(nullptr));
					}
					break;

				case Curly::tagStartSession:
					if (ss == nullptr && shared.dir_accounts)
					{
						const size_t count = tag.GetObjCount();
						std::vector<std::string> data;
						std::string error_msg;
						while (info.method == Hi::Method::Post && tag.GetType() == Curly::modFunction && count > 1)
						{
							bool ok = true;
							const char * e = "Invalid username and/or password.<br>";
							data.reserve(count);
							for (unsigned i = 0; i < count; ++i)
								if (auto p = info.post.find(tag.GetObj(i).str); p != info.post.end())
									data.emplace_back(p->second);
								else
									ok = false;
									
							if (!ok) break;
							if (data[0].empty())
							{
								error_msg += "Username field is empty!<br>";
								break;
							}
							else if (data[0].back() <= 0x20)
								data[0].resize(data[0].length() - 1);
							auto ct = std::remove_if(data[0].begin(), data[0].end(), [](char c) { return (c == '\"' || c == '\''); });
							data[0].erase(ct, data[0].end());
							info.clipboard[static_cast<std::string>(tag.GetObj(0).str)] = data[0];
							{
								auto e = std::find_if(data[0].begin(), data[0].end(), [&](char c) { return !is_username_acceptable(c); });
								if (e != data[0].end())
								{
									error_msg += "Username contains illegal characters!<br>";
									break;
								}
							}
							std::string dir(dir_work);
							ensure_closing_slash(dir += shared.dir_accounts);
							auto pos = dir.length();
							dir += data[0];
							std::transform(dir.begin() + pos, dir.end(), dir.begin() + pos, ::tolower);
							Violet::UniBuffer f;
							auto llfile = dir + USERFILE_LASTLOGIN;
							if (f.read_from_file(llfile.c_str()))
							{
								time_t last_login = f.read<time_t>(), now = time(nullptr);
								auto d = difftime(now, last_login);
								if (d <= 10)
								{
									error_msg += "Last login attempt on this account took place ";
									error_msg += std::to_string(static_cast<int64_t>(d));
									error_msg += " seconds ago. For security reasons, users have to wait at least 10 seconds between consecutive login attempts.";
									break;
								}
								f.clear();
								f.write<time_t>(now);
								f.write_to_file(llfile.c_str());
							}
							if (!f.read_from_file((dir + USERFILE_ACC).c_str()))
							{
								error_msg += e;
								break;
							}
							{
								Violet::SHA1 sha1;
								Violet::UniBuffer salt;
								salt.read_from_file((dir + USERFILE_SALT).c_str());
								sha1 << data[1];
								sha1 << salt;
								const char *pass1 = sha1.result(), *pass2 = reinterpret_cast<char*>(f.data());
								for (unsigned i = 0; i < 20; ++i)
									if (*(pass1++) != *(pass2++))
									{
										error_msg += e;
										ok = false;
										break;
									}
							}
							if (!ok) break;
							f.set_pos(20);
							auto stt = f.read_data(f.read<uint16_t>());
							CreateSession(stt, &f);
							break;
						}
						if (error_msg.length())
							info.clipboard["session_error"] = std::move(error_msg);
							
						if (data.size() > 0) {
#ifndef WRITE_DATES_ONLY_ON_HEADERS
							WriteDateToLog();
#endif
							std::lock_guard<std::mutex> lock(logging.first);
							if (ss) {
								logging.second << " * User `"sv << ss->username << "` successfully logged in."sv;
								printf(" > logon: `%s`\r\n", ss->username.c_str());
							}
							else {
								logging.second << " * Failed login attempt! Handle used: "sv << data[0];
							}
							logging.second.write<uint16_t>(0xa0d);
						}
					}
					break;

				case Curly::tagKillSession:
					if (ss != nullptr)
					{
						std::string name;
						auto amt = shared.sessions.size();
						for (auto it = shared.sessions.begin(); it != shared.sessions.end(); ++it)
							if (&it->second == ss)
							{
								name = it->second.username;
								it = shared.sessions.erase(it);
								break;
							}
							//else
								//++it;
						if (amt > shared.sessions.size())
						{
							char str[64];
							snprintf(str, 64u, " > Session `%s` closed by user.", name.c_str());
#ifndef WRITE_DATES_ONLY_ON_HEADERS
							WriteDateToLog();
#endif
							puts(str);
							std::lock_guard<std::mutex> lock(logging.first);
							logging.second << str << "\r\n"sv;
						}
						ss = nullptr;
						info.AddHeader("Set-Cookie", SESSION_KILL_CMD);
					}
					break;

				case Curly::tagRegister:
					if (ss == nullptr && shared.dir_accounts)
					{
						const size_t count = tag.GetObjCount();
						std::string error_msg;
						if (info.method == Hi::Method::Post && tag.GetType() == Curly::modFunction && count > 5 && count <= 6)
						{
							bool ok = true;
							std::vector<std::string> data;
							data.reserve(count);
							for (unsigned i = 0; i < count; ++i)
								if (auto p = info.post.find(tag.GetObj(i).str); p != info.post.end())
									data.emplace_back(p->second);
								else
									ok = false;
									
							if (!ok) break;
							if (!CheckRegistrationData(data, error_msg))
								ok = false;
							auto has_quotes = [](char c) { return (c == '\"' || c == '\''); };
							auto ct = std::remove_if(data[0].begin(), data[0].end(), has_quotes);
							data[0].erase(ct, data[0].end());
							ct = std::remove_if(data[3].begin(), data[3].end(), has_quotes);
							data[3].erase(ct, data[3].end());
							info.clipboard[static_cast<std::string>(tag.GetObj(0).str)] = data[0];
							info.clipboard[static_cast<std::string>(tag.GetObj(3).str)] = data[3];
							if (!ok) break;
							else {
								std::string dir(dir_work);
								ensure_closing_slash(dir += shared.dir_accounts);
								auto pos = dir.length();
								dir += data[0];
								std::transform(dir.begin() + pos, dir.end(), dir.begin() + pos, ::tolower);
								if (DirectoryExists(dir.c_str()))
								{
									error_msg += "Username <em>";
									error_msg += data[0];
									error_msg += "</em> is already taken.<br>";
									break;
								}
								else
								{
									Violet::UniBuffer w;
									GenerateSalt(w);
									mkdir(dir.c_str(), 0700);
									w.write_to_file((dir + USERFILE_SALT).c_str());

									Violet::SHA1 sha1;
									sha1 << data[1] << w;

									w.clear();
									w.write_data(sha1.result(), 20u);
									//w.WriteType<uint16_t>(0xa0d);
									w.write(static_cast<uint16_t>(data[0].length()));
									w.write(data[0]);
									w.write<uint16_t>(0xa0d);
									w.write<uint8_t>('A');
									w.write<uint8_t>('0');
									w.write<uint16_t>(0xa0d);
									w.write(static_cast<uint16_t>(data[3].length()));
									w.write(data[3]);
									w.write<uint16_t>(0xa0d);
									w.write_to_file((dir + USERFILE_ACC).c_str());

									w.clear();
									w.write<time_t>( 0x0 );
									w.write_to_file((dir + USERFILE_LASTLOGIN).c_str());

									info.clipboard["register_msg"] = "Account `<strong>" + data[0] + "</strong>` has been created.";

									CreateSession(data[0], nullptr);

#ifndef WRITE_DATES_ONLY_ON_HEADERS
									WriteDateToLog();
#endif
									printf(" > register: `%s`\r\n", data[0].c_str());
									std::lock_guard<std::mutex> lock(logging.first);
									logging.second << " * Account `"sv << data[0] << "` registered.\r\n"sv;
								}
							}
						}
						if (error_msg.length())
							info.clipboard["session_error"] = std::move(error_msg);
					}
					break;

				case Curly::tagSessionInfo:
					if (ss != nullptr && tag.GetObjCount() == 1)
					{
						if (tag.GetObj(0).str == "name")
							f.write(ss->username);
					}
					break;

				default:
					f.write<char>('{');
					f.write<std::string_view>(c);
					f.write<char>('}');
				}
			}

			read_pos = std::min(src.length() + 1, pose);
		}
		f.swap(h);
	}
}