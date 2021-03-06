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
#include "bluescript.hpp"
#include "echo/hash.hpp"

using namespace std::string_view_literals;
using map_t = Protocol::Hi::map_t;

const std::array<unsigned, 5> markers{	// Beware of U+FE0F after some emojis
	Blue::Codepoints::SuitHeart,
	Blue::Codepoints::RedHeart,
	Blue::Codepoints::Tangerine,
	Blue::Codepoints::Ghost,
	Blue::Codepoints::CrossMark
};

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

template<typename A>
inline bool is_meaningful(A c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '?' || c == '/';
}


struct Protocol::Callback
{
	struct Reusable
	{
		Protocol &parent;
		map_t cb;
		const unsigned http_error_code;

		Reusable(Protocol &_p, unsigned _hec)
			: parent(_p), http_error_code(_hec) {}
	};

	Reusable & re;
	Violet::utf8x::translator<char> &uc;
	size_t read_pos, pos;
	Violet::UniBuffer &out;

	using wrapped_content_t = std::pair<Violet::UniBuffer, Violet::utf8x::translator<char>>;
	std::unique_ptr<wrapped_content_t> wrapped_content;

	Callback(Reusable &_re, Violet::utf8x::translator<char> &_u, Violet::UniBuffer &_o)
		: re(_re), uc(_u), out(_o) { pos = read_pos = uc.get_pos(); }

	void magic() {
		while (true) {
			pos = uc.find_and_iterate_array(markers);
			if (pos < uc.size()) {
				std::visit(*this, Blue::parse(uc));
			}
			else {
				write_remainder();
				if (bool(wrapped_content) && !wrapped_content->second.is_at_end()) {
					uc = std::move(wrapped_content->second);
					wrapped_content.reset();
				}
				else break;
			}
		}
	}

	// template<class InOutContainer, class ViewT,
	// 	typename = std::enable_if_t<std::is_same_v<ViewT, typename InOutContainer::view_t>>>
	// void wrapHTML(InOutContainer &f, const ViewT& wrapper_content, const ViewT& wrapper_title)
	// {
	// 	InOutContainer out;
	// 	Violet::utf8x::translator<char> uc { f.get_string() };
	// 	const std::array<Blue::Codepoints, 2> hearts { Blue::Codepoints::RedHeart, Blue::Codepoints::SuitHeart };
		
	// 	uc.skip_whitespace();
	// 	size_t read_pos = uc.get_pos();
	// 	bool found_content = false;
		
	// 	while (true) {
	// 		size_t pos = uc.find_and_iterate_array(hearts);
	// 		if (pos >= uc.size()) {
	// 			if (pos > read_pos)
	// 				out.write(uc.substr(read_pos, pos - read_pos));
	// 			break;
	// 		}
	// 		auto candy = Blue::parse(uc);
	// 		if (const auto ptr = std::get_if<Blue::HeartFunction>(&candy); ptr && (ptr->key == Blue::Function::Title || ptr->key == Blue::Function::Content)) {
	// 			if (pos > read_pos)
	// 				out.write(uc.substr(read_pos, pos - read_pos));
	// 			if (!uc.is_at_end() && *uc == 0xfe0f)
	// 				++uc;
	// 			uc.skip_whitespace();
	// 			read_pos = uc.get_pos();
	// 			if (ptr->key == Blue::Function::Title) {
	// 				out.write(wrapper_title);
	// 			}
	// 			else if (!found_content) {
	// 				out.write(wrapper_content);
	// 				found_content = true;
	// 			}
	// 		}
	// 	}
	// 	if (found_content)
	// 		f.swap(out);
	// }

	inline void write_remainder() {
		if (pos > read_pos)
			out.write(uc.substr(read_pos, pos - read_pos));
		read_pos = uc.get_pos();
	}

	void __recursive_stack(Violet::utf8x::translator<char> u, bool check_for_init_tag)
	{
		write_remainder();
		if (check_for_init_tag) {
			u.skip_whitespace();
			if (*u != Blue::Codepoints::ChequeredFlag) {
				out.write(u.substr());
				return;
			}
			(++u).skip_whitespace();
		}
		Callback lets_go_deeper(re, u, out);
		lets_go_deeper.magic();
	}

	void __recursive_stack(Violet::UniBuffer &buff, Blue::HeartFunction &&hf)
	{
		write_remainder();
		Callback lets_go_deeper(re, uc, buff);
		lets_go_deeper(std::move(hf));
	}

	void __recursive_stack_ref(Violet::utf8x::translator<char> &u)
	{
		write_remainder();
		Callback lets_go_deeper(re, u, out);
		lets_go_deeper.magic();
	}

	void operator()(Blue::HeartFunction &&hf) {
		write_remainder(); // has to be written whether or not function outputs anything 
		switch (hf.key)
		{
		case Blue::Function::Echo:
			if (hf.arg.size() == 1 || hf.arg.size() == 2) {
					const auto& db = hf.arg.size() == 2 ? re.parent.info.list(hf.arg[0]) : re.cb;
					if (auto a = db.find(hf.arg[hf.arg.size() - 1]); a != db.end())
						out << a->second;
			}
			break;

		case Blue::Function::Constant:
			if (hf.arg.size() == 1 && bool(hf.chain)) {
				Violet::UniBuffer tbuff;
				__recursive_stack(tbuff, std::move(*hf.chain));
				hf.chain.release();
				re.cb[std::string(hf.arg[0])] = std::string(tbuff.get_string());
			}
			else if (hf.arg.size() == 2) {
				re.cb[std::string(hf.arg[0])] = std::string(hf.arg[1]);
			}
			break;

		case Blue::Function::StatusErrorCode:
			if (re.http_error_code) {
				out.write(std::to_string(re.http_error_code));
			}
			break;

		case Blue::Function::StatusErrorText:
			switch (re.http_error_code)
				{
				case 400:
					out.write("Bad Request"sv);
					break;
				case 403:
					out.write("Forbidden"sv);
					break;
				case 404:
					out.write("Not Found"sv);
					break;
				case 405:
					out.write("Method Not Allowed"sv);
					break;
				case 416:
					out.write("Requested Range Not Satisfiable"sv);
					break;
				case 501:
					out.write("Not Implemented"sv);
					break;
				}
			break;

		case Blue::Function::Copyright:
			if (!!re.parent.shared.var_copyright.length())
				out.write(re.parent.shared.var_copyright);
			break;

		case Blue::Function::Battery:

			break;

		case Blue::Function::Include:
			if (hf.arg.size() == 1)
			{
				std::string fn{ dir_html };
				fn += '/';
				fn += hf.arg[0];
				fn += ".html";
				Violet::UniBuffer t;
				if (t.read_from_file(fn.c_str())) {
					__recursive_stack(t.get_string(), true);
				}
			}
			break;
			
		case Blue::Function::Wrapper:
			if ((hf.arg.size() == 1 || hf.arg.size() == 2) && !bool(wrapped_content)) {
				Violet::UniBuffer t;
				const std::string fn{ "html/wrapper_" + std::string(hf.arg[0]) + ".html" };
				if (t.read_from_file(fn.c_str())) {
					Violet::utf8x::translator<char> uc_copy(uc);
					wrapped_content = std::make_unique<wrapped_content_t>(std::move(t), std::move(uc));
					uc = wrapped_content->first.get_string();
					uc.skip_whitespace();
				}
			}
			break;

		case Blue::Function::Content:
			if (bool(wrapped_content)) {
				if (!wrapped_content->second.is_at_end())
					__recursive_stack_ref(wrapped_content->second);
			}
			break;

		case Blue::Function::GenerateCaptcha:
			if (!hf.arg.size())
			{
				Captcha::Init c;
				c.set_up(true);
				
				re.cb["captcha_seed"] = std::move(c.seed);
				re.cb["captcha_image"] = "/captcha." + c.Imt.PicFilename;	// this one is moved in the next step
				c.Imt.Data = std::async(std::launch::async, Captcha::Image::process, c.Imt.Collection);
				re.parent.shared.captcha_sig.emplace_back(std::move(c.Imt), std::chrono::system_clock::now());
			}
			break;

		case Blue::Function::StartSession:
			if (const auto argc = hf.arg.size(); argc > 1 && re.parent.ss == nullptr && re.parent.shared.dir_accounts)
			{
				//const size_t count = tag.GetObjCount();
				std::vector<std::string> data;
				std::string error_msg;
				while (re.parent.info.method == Hi::Method::Post)
				{
					bool ok = true;
					const char * e = "Invalid username and/or password.<br>";
					data.reserve(argc);
					for (unsigned i = 0; i < argc; ++i)
						if (auto p = re.parent.info.post.find(hf.arg[i]); p != re.parent.info.post.end())
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
					re.cb[static_cast<std::string>(hf.arg[0])] = data[0];
					{
						auto e = std::find_if(data[0].begin(), data[0].end(), [&](char c) { return !is_username_acceptable(c); });
						if (e != data[0].end())
						{
							error_msg += "Username contains illegal characters!<br>";
							break;
						}
					}
					std::string dir(dir_work);
					ensure_closing_slash(dir += re.parent.shared.dir_accounts);
					auto pos = dir.length();
					dir += data[0];
					std::transform(dir.begin() + pos, dir.end(), dir.begin() + pos, ::tolower);
					Violet::UniBuffer f;
					auto llfile = dir + USERFILE_LASTLOGIN;
					if (f.read_from_file(llfile.c_str()))
					{
						std::chrono::system_clock::time_point last_login(std::chrono::microseconds(f.read<std::chrono::microseconds::rep>())),
							now = std::chrono::system_clock::now();
						auto d = std::chrono::duration_cast<std::chrono::seconds>(now - last_login).count();
						if (d <= 10)
						{
							error_msg += "Last login attempt on this account took place ";
							error_msg += std::to_string(static_cast<int64_t>(d));
							error_msg += " seconds ago. For security reasons, users have to wait at least 10 seconds between consecutive login attempts.";
							break;
						}
						f.clear();
						f.write(std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count());
						f.write_to_file(llfile.c_str());
					}
					if (!f.read_from_file((dir + USERFILE_ACC).c_str()))
					{
						error_msg += e;
						break;
					}
					{
						Violet::hash<Violet::SHA1> sha1;
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
					re.parent.CreateSession(stt, &f);
					break;
				}
				if (!!error_msg.length())
					re.cb["session_error"] = std::move(error_msg);
				
				if (!!data.size()) {
#ifndef WRITE_DATES_ONLY_ON_HEADERS
					WriteDateToLog();
#endif
					std::lock_guard<std::mutex> lock(Protocol::logging.first);
					if (re.parent.ss) {
						Protocol::logging.second << " * User `"sv << re.parent.ss->username << "` successfully logged in."sv;
						printf(" > logon: `%s`\r\n", re.parent.ss->username.c_str());
					}
					else {
						Protocol::logging.second << " * Failed login attempt! Handle used: "sv << data[0];
					}
					Protocol::logging.second.write<uint16_t>(0xa0d);
				}
			}
			break;

		case Blue::Function::KillSession:
			if (!hf.arg.size() && re.parent.ss != nullptr)
			{
				std::string name;
				auto amt = re.parent.shared.sessions.size();
				for (auto it = re.parent.shared.sessions.begin(); it != re.parent.shared.sessions.end(); ++it)
					if (&it->second == re.parent.ss)
					{
						name = it->second.username;
						it = re.parent.shared.sessions.erase(it);
						break;
					}
					
				if (amt > re.parent.shared.sessions.size())
				{
					char str[65];
					snprintf(str, 64, " > Session `%s` closed by user.", name.c_str());
#ifndef WRITE_DATES_ONLY_ON_HEADERS
					WriteDateToLog();
#endif
					puts(str);
					std::lock_guard<std::mutex> lock(Protocol::logging.first);
					Protocol::logging.second << str << "\r\n"sv;
				}
				re.parent.ss = nullptr;
				re.parent.info.AddHeader("Set-Cookie", SESSION_KILL_CMD);
			}
			break;

		case Blue::Function::Register:
			if (const auto argc = hf.arg.size(); argc == 6 && re.parent.ss == nullptr && re.parent.shared.dir_accounts)
			{
				std::string error_msg;
				if (re.parent.info.method == Hi::Method::Post)
				{
					bool ok = true;
					std::vector<std::string> data;
					data.reserve(argc);
					for (unsigned i = 0; i < argc; ++i)
						if (auto p = re.parent.info.post.find(hf.arg[i]); p != re.parent.info.post.end())
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
					re.cb[static_cast<std::string>(hf.arg[0])] = data[0];
					re.cb[static_cast<std::string>(hf.arg[3])] = data[3];
					if (!ok) break;
					else {
						std::string dir(dir_work);
						ensure_closing_slash(dir += re.parent.shared.dir_accounts);
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

							Violet::hash<Violet::SHA1> sha1;
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
							w.write<std::chrono::microseconds::rep>( 0x0 );
							w.write_to_file((dir + USERFILE_LASTLOGIN).c_str());

							re.cb["register_msg"] = "Account `<strong>" + data[0] + "</strong>` has been created.";

							re.parent.CreateSession(data[0], nullptr);

#ifndef WRITE_DATES_ONLY_ON_HEADERS
							WriteDateToLog();
#endif
							printf(" > register: `%s`\r\n", data[0].c_str());
							std::lock_guard<std::mutex> lock(Protocol::logging.first);
							Protocol::logging.second << " * Account `"sv << data[0] << "` registered.\r\n"sv;
						}
					}
				}
				if (error_msg.length())
					re.cb["session_error"] = std::move(error_msg);
			}
			break;

		case Blue::Function::SessionInfo:
			if (re.parent.ss != nullptr && hf.arg.size() == 1)
			{
				if (hf.arg[0] == "name")
					out.write(re.parent.ss->username);
			}
			break;
			
		default:
			out.write(u8"\u2764\ufe0f"sv);
		}

		if (bool(hf.chain)) {
			this->operator()(std::move(*hf.chain));
		}
	}

	void operator()(Blue::TangerineBlock &&tb) {
		bool skip = true;

		do {
			if (tb.sub.empty()) {
				if (tb.name == "Session") {
					if (re.parent.ss != nullptr)
						skip = false;
					break;
				}
				else if (tb.name == "NoSession") {
					if (re.parent.ss == nullptr)
						skip = false;
					break;
				}
				else if (tb.name == "Userlevel") {
					if (re.parent.ss != nullptr) {
						if (tb.op == Blue::Operator::none)
							skip = false;
						else
							if (auto val = std::get_if<long>(&tb.comp_val))
								switch(tb.op) {
									case Blue::Operator::equality: skip = !(re.parent.ss->userlevel == *val); break;
									case Blue::Operator::lessthan: skip = !(re.parent.ss->userlevel < *val); break;
									case Blue::Operator::greaterthan: skip = !(re.parent.ss->userlevel > *val); break;
								}
					}
					break;
				}
			}

			{
				const auto& db = tb.sub.empty() ? re.cb : re.parent.info.list(tb.name);
				if (auto g = db.find(tb.sub.empty() ? tb.name : tb.sub); g != db.end())
				{
					if (tb.op == Blue::Operator::none) {
						skip = false;
					}
					else skip = !std::visit([op = tb.op, &c = g->second](auto && a) {
						using value_type = std::decay_t<decltype(a)>;
						if constexpr (std::is_arithmetic_v<value_type>) {
							long val;
							if (Violet::svtonum(c, val, 0) < c.size())
								switch(op) {
								case Blue::Operator::equality: return val == a;
								case Blue::Operator::lessthan: return val < a;
								case Blue::Operator::greaterthan: return val > a;
								}
						}
						else if (op == Blue::Operator::equality && a == c)
							return true;
						return false;
					}, tb.comp_val);
				}
			}
		}
		while (false);

		if (skip) {
			write_remainder();
			if (bool(tb.elseblock)) {
				if (auto strawberry = std::get_if<std::string_view>(tb.elseblock.get())) {
					__recursive_stack(*strawberry, false);
				}
				else if (auto lemon = std::get_if<Blue::TangerineBlock>(tb.elseblock.get())) {
					this->operator()(std::move(*lemon));
				}
			}
		}
		else {
			__recursive_stack(tb.content, false);
		}
	}

	void operator()(Blue::StrategicEscape &&se) {
		write_remainder();
		out.write_utfx(se.val);
	}

	void operator()(bool keep_body) {
		if (keep_body) {
			pos = uc.get_pos();
		}
		else {
			write_remainder();
		}
	}
};




std::optional<Violet::UniBuffer> Protocol::HandleHTML(const std::string_view &h, uint16_t error)
{
	Violet::utf8x::translator<char> uc { h };

	uc.skip_whitespace();
	if (uc.get_and_iterate() == Blue::Codepoints::ChequeredFlag)
	{
		Violet::UniBuffer output;
		Callback::Reusable re(*this, error);
		Callback call(re, uc, output);
		output.write(std::array<unsigned char, 3>{ 0xef, 0xbb, 0xbf });

		try {
			call.magic();
		}
		catch (std::exception &e) {
			output << char('\n') << e.what();
		}
		catch (...) {
			output << "\nSomething went horribly awry ;("sv;
		}
		if (output.size() > 3)
			return output;
	}
	return {};
}