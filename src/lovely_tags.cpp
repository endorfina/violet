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
#include "lovely_tags.hpp"
#include "echo/buffers.hpp"

using namespace Lovely;

const std::unordered_map <std::string_view, Function> sweets {
	{ "wrapper", Function::Wrapper },
	{ "title", Function::Title },
	{ "content", Function::Content },
	{ "blog_load", Function::BlogLoad },
	{ "posts", Function::Posts },
	{ "http_error_code", Function::StatusErrorCode },
	{ "print_http_error_text", Function::StatusErrorText },
	{ "copyright", Function::Copyright },
	{ "include", Function::Include },
	{ "startsession", Function::StartSession },
	{ "endsession", Function::KillSession },
	{ "register", Function::Register },
	{ "session", Function::SessionInfo },
	{ "echo", Function::Echo },
	{ "generate_captcha", Function::GenerateCaptcha }
};

const std::unordered_map <unsigned int, Function> super_sweets {
	{ Codepoints::WrappedGift, Function::Wrapper },
	{ Codepoints::Scroll, Function::Include },
	{ Codepoints::GrinningFace, Function::Echo },
	{ Codepoints::IceCream, Function::Constant }
};

const std::unordered_map <std::string_view, Operator> ops {
	{ "==", Operator::equality },
	{ "=", Operator::equality },
	{ "<", Operator::lessthan },
	{ ">", Operator::greaterthan }
};

std::optional<std::pair<size_t,size_t>> find_markers(const std::string_view& sv, char a1, char a2) {
	size_t r1 = sv.find(a1), r2 = sv.find(a2);
	if (r1 != std::string_view::npos && r2 != std::string_view::npos && r2 > r1)
		return std::make_pair(r1, r2);
	return {};
}

bool cik_strcmp(const char * s, const char * k) {
	while (!!*s && !!*k)
		if (::tolower(*(s++)) != *(k++))
			return false;
	return true;
}

template<typename A>
inline bool _is_punct_lv(const A c) {
	return c < 0x80 && c != '_' && (!!std::ispunct(static_cast<unsigned char>(c)));
}

template<class T = std::string_view>
inline T __split(std::string_view &_l) {
	size_t len;
	T out;
	const bool is_name = !_is_punct_lv(_l.front());
	if (!is_name && (_l.front() == '\'' || _l.front() == '\"')) {
		if (_l.size() < 2) {
			_l.remove_suffix(1);
		}
		else {
			len = _l.find(_l.front(), 1);
			if (len > 0)
				out = _l.substr(1, len - 1);
			_l.remove_prefix(std::min(len + 1, _l.length()));
			Violet::remove_prefix_whitespace(_l);
		}
	}
	else {
		len = 1;
		if (is_name) {
			size_t s = Violet::utf8x::sequence_length(_l.data());
			if (s == 1)
				for(;len < _l.length() && Violet::utf8x::sequence_length(_l.data() + len) == 1 && !_is_punct_lv(_l[len]) && !std::isspace(static_cast<unsigned char>(_l[len])); ++len);
			else if (s <= _l.length()) {
				//for(len = s; len < _l.length() && (s = Violet::utf8x::sequence_length(_l.data() + len)) > 1 && len + s <= _l.length(); len += s);
				if constexpr (!std::is_same_v<T, std::string_view>) {
					out = Violet::utf8x::get_switch(_l.data(), s);
					_l.remove_prefix(s);
					Violet::remove_prefix_whitespace(_l);
					return out;
				}
				else len = s;
			}
		}
		else if (_l.length() > 1 && (_l[0] == '<' || _l[0] == '>' || _l[0] == '=') && _l[1] == '=')
			++len;

		out = _l.substr(0, len);

		_l.remove_prefix(len);

		Violet::remove_prefix_whitespace(_l);
	}
	return out;
}

std::vector<std::string_view> split(std::string_view _l) {
	std::vector<std::string_view> out;
	while(!_l.empty()) {
		out.emplace_back(__split<std::string_view>(_l));
	}
	return out;
}

std::pair<Function, std::vector<std::string_view>> split2(std::string_view _l) {
	std::vector<std::string_view> out;
	Function ff = Function::Unknown;
	if(!_l.empty()) {
		auto key = __split<std::variant<std::string_view, unsigned int>>(_l);
		ff = std::visit([](auto &&a) {
			if constexpr (std::is_same_v<std::decay_t<decltype(a)>, std::string_view>) {
				const auto k = sweets.find(a);
				return k != sweets.end() ? k->second : Lovely::Function::Unknown;
			} else {
				const auto k = super_sweets.find(a);
				return k != super_sweets.end() ? k->second : Lovely::Function::Unknown;
			}
		}, key);
		if (ff != Function::Unknown)
			while(!_l.empty())
				out.emplace_back(__split(_l));
	}
	return { ff, std::move(out) };
}

template<class Str>
void print_tags_2(std::FILE* stream, const std::vector<Str> &v) {
	bool append = false;
	fprintf(stream, "[%zu]: ", v.size());
	for (auto &&it : v) {
		if (append) {
			std::putc(',', stream);
			std::putc(' ', stream);
		}
		else append = true;

		std::putc('\"', stream);
		for (auto c : it) {
			std::putc(c, stream);
		}
		std::putc('\"', stream);
	}
	fprintf(stream, "\n");
}

HeartFunction parse_function(Violet::utf8x::translator<char> &uc) {
	Violet::utf8x::translator<char> uc_copy{ uc };
	uc_copy.skip_whitespace();
	if (auto str = uc_copy.pop_substr_until([](unsigned c) { return c == '\n' || c == Codepoints::RedHeart || c == Codepoints::SuitHeart; }); str.size() > 0 ) {
		auto [key, tags] = split2(str);

		if (key != Function::Unknown)
		{
			uc = uc_copy;
		
			HeartFunction hf { key };
			for (auto it = tags.begin(); it != tags.end(); ++it)
				if (it->size() && _is_punct_lv(it->front())) {
					if (it->front() != ',')
						break;
				}
				else hf.args.emplace_back(*it);

			if (*uc == '\n') {
				uc.skip_whitespace();
			}
			else {
				while (*++uc == 0xfe0f);
				if (auto chf = parse_function(uc); chf.key != Function::Unknown)
					hf.chain = std::make_unique<HeartFunction>(std::move(chf));
			}
			return hf;
		}
	}
	return { Function::Unknown };
}

Candy Lovely::parse(Violet::utf8x::translator<char> &uc)
{
	const auto main = uc.get_and_iterate();
	while (!uc.is_at_end() && uc == 0xfe0f)
		++uc;
	
	switch (main) {
	case Codepoints::Ghost:
		if (*uc == Codepoints::Grape) {
			do {
				uc.find_and_iterate(Codepoints::Watermelon);
			} while (!uc.is_at_end() && ++uc != Codepoints::Ghost);
			++uc;
			return bool(false);
		}
		break;

	case Codepoints::Tangerine:
		if (*uc == ':')
			++uc;
		uc.skip_whitespace();
		{
			std::string_view str = uc.pop_substr_until([](unsigned c) { return c == Codepoints::Grape; });
			if (!str.size() || uc.is_at_end())
				break;
			++uc;
			Violet::remove_suffix_whitespace(str);

			std::vector<std::string_view> tags;

			tags = split(str);
			if (!tags.size())
				break;
			//print_tags_2(stderr, tags);
			
			TangerineBlock tb;
			unsigned shift = 0;

			tb.name = tags[0];
			if (tags.size() > 3 && tags[1] == "[" && tags[3] == "]") {
				tb.sub = tags[2];
				shift = 3;
			}
			if (tags.size() == 3 + shift) {
				if (const auto o = ops.find(tags[1 + shift]); o != ops.end()) {
					tb.op = o->second;
					long il;
					if (tags[2 + shift].size() > 0 && Violet::svtonum(tags[2 + shift], il, 0) < tags[2 + shift].size()) {
						tb.comp_val = il;
					}
					else tb.comp_val = tags[2 + shift];
				}
				else break;
			}
			else if (tags.size() != 1 + shift)
				break;
			return std::move(tb);
		}
		break;
	
	case Codepoints::RedHeart:	// generic function structure
	case Codepoints::SuitHeart:
		if (auto hf = parse_function(uc); hf.key != Function::Unknown)
			return std::move(hf);
		break;

	case Codepoints::CrossMark:
		return StrategicEscape{ uc++ };
	}
	return bool(true);
}