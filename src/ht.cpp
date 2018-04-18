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
#include "lovely_tags.hpp"

using namespace std::string_view_literals;
using map_t = Protocol::Hi::map_t;

const std::array<unsigned, 6> markers{	// Beware of U+FE0F after some emojis
	Lovely::Codepoints::SuitHeart,
	Lovely::Codepoints::RedHeart,
	Lovely::Codepoints::Tangerine,
	Lovely::Codepoints::Ghost,
	Lovely::Codepoints::CrossMark
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

struct Protocol::Callback {
	Protocol &parent;
	map_t &cb;
	Violet::utf8x::translator<char> &uc;
	Violet::UniBuffer *top;
	size_t read_pos, pos;
	Violet::UniBuffer out;
	const unsigned http_error_code;

	Callback(Protocol &_p, map_t &_m, Violet::utf8x::translator<char> &_u, Violet::UniBuffer *_h, unsigned _hec)
		: parent(_p), cb(_m), uc(_u), top(_h), http_error_code(_hec) { pos = read_pos = uc.get_pos(); }

	

	template<class InOutContainer, class ViewT,
		typename = std::enable_if_t<std::is_same_v<ViewT, typename InOutContainer::view_t>>>
	void wrapHTML(InOutContainer &f, const ViewT& wrapper_content, const ViewT& wrapper_title)
	{
		InOutContainer out;
		Violet::utf8x::translator<char> uc { f.get_string() };
		const std::array<Lovely::Codepoints, 2> hearts { Lovely::Codepoints::RedHeart, Lovely::Codepoints::SuitHeart };
		
		uc.skip_whitespace();
		size_t read_pos = uc.get_pos();
		bool found_content = false;
		
		while (true) {
			size_t pos = uc.find_and_iterate_array(hearts);
			if (pos >= uc.size()) {
				if (pos > read_pos)
					out.write(uc.substr(read_pos, pos - read_pos));
				break;
			}
			auto candy = Lovely::parse(uc);
			if (const auto ptr = std::get_if<Lovely::HeartFunction>(&candy); ptr && (ptr->key == Lovely::Function::Title || ptr->key == Lovely::Function::Content)) {
				if (pos > read_pos)
					out.write(uc.substr(read_pos, pos - read_pos));
				if (!uc.is_at_end() && *uc == 0xfe0f)
					++uc;
				uc.skip_whitespace();
				read_pos = uc.get_pos();
				if (ptr->key == Lovely::Function::Title) {
					out.write(wrapper_title);
				}
				else if (!found_content) {
					out.write(wrapper_content);
					found_content = true;
				}
			}
		}
		if (found_content)
			f.swap(out);
	}


	template<typename A>
	inline std::string_view find_a_proper_watermelon(Violet::utf8x::translator<A>& src) {
		unsigned inner_block_count = 0;
		const size_t start = src.get_pos();
		const std::array<Lovely::Codepoints, 4> tasty_fruits { Lovely::Codepoints::Ghost, Lovely::Codepoints::Tangerine, Lovely::Codepoints::Watermelon, Lovely::Codepoints::CrossMark };
		while (!src.is_at_end()) {
			src.find_and_iterate_array(tasty_fruits);
			switch (src) {
			case Lovely::Codepoints::Ghost:
				if (*++src == Lovely::Codepoints::Grape) {
					do {
						src.find_and_iterate(Lovely::Codepoints::Watermelon);
					} while (!src.is_at_end() && ++src != Lovely::Codepoints::Ghost);
					++src;
				}
				break;
			case Lovely::Codepoints::Watermelon:
				if (inner_block_count < 1) {
					auto tt = src.substr(start, src.get_pos() - start);
					++src;
					return tt;
				}
				else --inner_block_count;
				++src;
				break;
			case Lovely::Codepoints::Tangerine:
				//if (*++src == ':') {
					src.find_and_iterate(Lovely::Codepoints::Grape);
					++inner_block_count;
				//}
				++src;
				break;
			case Lovely::Codepoints::CrossMark:
				++src;
				++src;
				break;
			default:
				++src;
			}
		}
		return {};
	}

	void write_remainder() {
		if (pos > read_pos)
			out.write(uc.substr(read_pos, pos - read_pos));
		//if (!uc.is_at_end() && *uc == 0xfe0f)
		//	++uc;
		//uc.skip_whitespace();	// <- ugly
		read_pos = uc.get_pos();
	}

	void __recursive_stack(Violet::utf8x::translator<char> u)
	{
		write_remainder();
		Callback lets_go_deeper(parent, cb, u, nullptr, http_error_code);
		
		while (true) {
			lets_go_deeper.pos = u.find_and_iterate_array(markers);
			auto candy = Lovely::parse(u);
			std::visit(lets_go_deeper, std::move(candy));

			lets_go_deeper.pos = lets_go_deeper.uc.find_and_iterate_array(markers);

			if (lets_go_deeper.pos >= lets_go_deeper.uc.size()) {
				lets_go_deeper.write_remainder();
				break;
			}
		}
		if (lets_go_deeper.out.size() > 0)
			out << lets_go_deeper.out;
	}

	void operator()(Lovely::HeartFunction &&hf) {
		write_remainder();
		switch (hf.key)
		{
		case Lovely::Function::Echo:
			if (hf.args.size() == 1 || hf.args.size() == 2) {
					const auto& db = hf.args.size() == 2 ? parent.info.list(hf.args[0]) : cb;
					if (auto a = db.find(hf.args[hf.args.size() - 1]); a != db.end())
						out << a->second;
			}
			break;

		case Lovely::Function::StatusErrorCode:
			if (http_error_code) {
				out.write(std::to_string(http_error_code));
			}
			break;

		case Lovely::Function::StatusErrorText:
			switch (http_error_code)
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

		case Lovely::Function::Copyright:
			if (!!parent.shared.var_copyright.length())
				out.write(parent.shared.var_copyright);
			break;

		case Lovely::Function::Battery:

			break;

		case Lovely::Function::Include:
			if (hf.args.size() == 1)
			{
				std::string fn{ dir_html };
				fn += '/';
				fn += hf.args[0];
				fn += ".html";
				Violet::UniBuffer t;
				if (t.read_from_file(fn.c_str())) {
					auto v = t.get_string();
					Violet::utf8x::remove_potential_BOM(v);
					out << v;
				}
			}
			break;
			
		case Lovely::Function::Wrapper:
			if (top != nullptr && (hf.args.size() == 1 || hf.args.size() == 2))
			{
				const std::string fn{ "html/wrapper_" + std::string(hf.args[0]) + ".html" };
				
				Violet::UniBuffer t;
				if (t.read_from_file(fn.c_str())) {
					wrapHTML(t, uc.substr(), hf.args.size() == 2 ? hf.args[1] : "Untitled");
					*top = std::move(t);
					uc = top->get_string();
				}
			}
			break;

		case Lovely::Function::GenerateCaptcha:
			if (!hf.args.size())
			{
				Captcha::Init c;
				c.set_up(true);
				
				cb["captcha_seed"] = std::move(c.seed);
				cb["captcha_image"] = "/captcha." + c.Imt.PicFilename;	// this one is moved in the next step
				c.Imt.Data = std::async(std::launch::async, Captcha::Image::process, c.Imt.Collection);
				parent.shared.captcha_sig.emplace_back(std::move(c.Imt), time(nullptr));
			}
			break;

		case Lovely::Function::StartSession:
			if (const auto argc = hf.args.size(); argc > 1 && parent.ss == nullptr && parent.shared.dir_accounts)
			{
				//const size_t count = tag.GetObjCount();
				std::vector<std::string> data;
				std::string error_msg;
				while (parent.info.method == Hi::Method::Post)
				{
					bool ok = true;
					const char * e = "Invalid username and/or password.<br>";
					data.reserve(argc);
					for (unsigned i = 0; i < argc; ++i)
						if (auto p = parent.info.post.find(hf.args[i]); p != parent.info.post.end())
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
					cb[static_cast<std::string>(hf.args[0])] = data[0];
					{
						auto e = std::find_if(data[0].begin(), data[0].end(), [&](char c) { return !is_username_acceptable(c); });
						if (e != data[0].end())
						{
							error_msg += "Username contains illegal characters!<br>";
							break;
						}
					}
					std::string dir(dir_work);
					ensure_closing_slash(dir += parent.shared.dir_accounts);
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
					parent.CreateSession(stt, &f);
					break;
				}
				if (!!error_msg.length())
					cb["session_error"] = std::move(error_msg);
				
				if (!!data.size()) {
#ifndef WRITE_DATES_ONLY_ON_HEADERS
					WriteDateToLog();
#endif
					std::lock_guard<std::mutex> lock(Protocol::logging.first);
					if (parent.ss) {
						Protocol::logging.second << " * User `"sv << parent.ss->username << "` successfully logged in."sv;
						printf(" > logon: `%s`\r\n", parent.ss->username.c_str());
					}
					else {
						Protocol::logging.second << " * Failed login attempt! Handle used: "sv << data[0];
					}
					Protocol::logging.second.write<uint16_t>(0xa0d);
				}
			}
			break;

		case Lovely::Function::KillSession:
			if (!hf.args.size() && parent.ss != nullptr)
			{
				std::string name;
				auto amt = parent.shared.sessions.size();
				for (auto it = parent.shared.sessions.begin(); it != parent.shared.sessions.end(); ++it)
					if (&it->second == parent.ss)
					{
						name = it->second.username;
						it = parent.shared.sessions.erase(it);
						break;
					}
					
				if (amt > parent.shared.sessions.size())
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
				parent.ss = nullptr;
				parent.info.AddHeader("Set-Cookie", SESSION_KILL_CMD);
			}
			break;

		case Lovely::Function::Register:
			if (const auto argc = hf.args.size(); argc == 6 && parent.ss == nullptr && parent.shared.dir_accounts)
			{
				std::string error_msg;
				if (parent.info.method == Hi::Method::Post)
				{
					bool ok = true;
					std::vector<std::string> data;
					data.reserve(argc);
					for (unsigned i = 0; i < argc; ++i)
						if (auto p = parent.info.post.find(hf.args[i]); p != parent.info.post.end())
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
					cb[static_cast<std::string>(hf.args[0])] = data[0];
					cb[static_cast<std::string>(hf.args[3])] = data[3];
					if (!ok) break;
					else {
						std::string dir(dir_work);
						ensure_closing_slash(dir += parent.shared.dir_accounts);
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

							cb["register_msg"] = "Account `<strong>" + data[0] + "</strong>` has been created.";

							parent.CreateSession(data[0], nullptr);

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
					cb["session_error"] = std::move(error_msg);
			}
			break;

		case Lovely::Function::SessionInfo:
			if (parent.ss != nullptr && hf.args.size() == 1)
			{
				if (hf.args[0] == "name")
					out.write(parent.ss->username);
			}
			break;

		case Lovely::Function::Constant:
			//if ()
		default:
			out.write(u8"\u2764\ufe0f"sv);
		}

		if (bool(hf.chain)) {
			this->operator()(std::move(*hf.chain));
		}
	}

	void operator()(Lovely::TangerineBlock &&tb) {
		bool skip = true;

		do {
			if (tb.sub.empty()) {
				if (tb.name == "Session") {
					if (parent.ss != nullptr)
						skip = false;
					break;
				}
				else if (tb.name == "NoSession") {
					if (parent.ss == nullptr)
						skip = false;
					break;
				}
				else if (tb.name == "Userlevel") {
					if (parent.ss != nullptr) {
						if (tb.op == Lovely::Operator::none || tb.comp_val.valueless_by_exception())
							skip = false;
						else
							if (auto val = std::get_if<long>(&tb.comp_val))
								switch(tb.op) {
									case Lovely::Operator::equality: skip = !(parent.ss->userlevel == *val); break;
									case Lovely::Operator::lessthan: skip = !(parent.ss->userlevel < *val); break;
									case Lovely::Operator::greaterthan: skip = !(parent.ss->userlevel > *val); break;
								}
					}
					break;
				}
			}

			{
				const auto& db = tb.sub.empty() ? cb : parent.info.list(tb.name);
				if (auto g = db.find(tb.sub.empty() ? tb.name : tb.sub); g != db.end())
				{
					if (tb.op == Lovely::Operator::none || tb.comp_val.valueless_by_exception()) {
						skip = false;
					}
					else skip = !std::visit([op = tb.op, &c = g->second](auto && a) {
						using value_type = std::decay_t<decltype(a)>;
						if constexpr (std::is_arithmetic_v<value_type>) {
							long val;
							if (Violet::svtonum(c, val, 0) < c.size())
								switch(op) {
								case Lovely::Operator::equality: return val == a;
								case Lovely::Operator::lessthan: return val < a;
								case Lovely::Operator::greaterthan: return val > a;
								}
						}
						else if (op == Lovely::Operator::equality && a == c)
							return true;
						return false;
					}, tb.comp_val);
				}
			}
		}
		while (false);

		if (skip) {
			find_a_proper_watermelon(uc);
			write_remainder();
		}
		else {
			__recursive_stack(find_a_proper_watermelon(uc));
		}
	}

	void operator()(Lovely::StrategicEscape &&se) {
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




void Protocol::HandleHTML(Violet::UniBuffer &h, uint16_t error)
{
	Violet::utf8x::translator<char> uc { h.get_string() };

	uc.skip_whitespace();
	if (uc.get_and_iterate() == Lovely::Codepoints::ChequeredFlag)
	{
		map_t clipboard;
		Callback call(*this, clipboard, uc, &h, error);
		
		if (call.pos = uc.find_and_iterate_array(markers); call.pos < uc.size())
		{
			call.out.write(std::array<unsigned char, 3>{ 0xef, 0xbb, 0xbf });
			try {
				while (true) {
					auto candy = Lovely::parse(uc);
					std::visit(call, std::move(candy));

					call.pos = call.uc.find_and_iterate_array(markers);

					if (call.pos >= call.uc.size()) {
						call.write_remainder();
						break;
					}
				}
			}
			catch (const char * str) {
				call.out << char('\n') << str;
			}
			catch (const std::string & str) {
				call.out << char('\n') << str;
			}
			catch (const std::string_view & str) {
				call.out << char('\n') << str;
			}
			catch (...) {
				call.out << "\nSomething went horribly awry ;("sv;
			}
			if (call.out.size() > 3)
				call.out.swap(h);
		}
	}
}