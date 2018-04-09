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
#include "curly_tags.h"
#include <buffers.hpp>

enum class Error {
	bad_format
};

using namespace Curly;

const std::map <std::string_view, Key> Tag::map {
	{ "block", Key::tagBlock },
	{ "wrapper", Key::tagWrapper },
	{ "title", Key::tagTitle },
	{ "content", Key::tagContent },
	{ "blog_load", Key::tagBlogLoad },
	{ "posts", Key::tagPosts },
	{ "error_code", Key::tagStatusErrorCode },
	{ "print_error_text", Key::tagStatusErrorText },
	{ "copyright", Key::tagCopyright },
	{ "include", Key::tagInclude },
	{ "startsession", Key::tagStartSession },
	{ "endsession", Key::tagKillSession },
	{ "register", Key::tagRegister },
	{ "session", Key::tagSessionInfo },
	{ "echo", Key::tagEcho },
	{ "generate_captcha", Key::tagGenerateCaptcha }
};

Mode Tag::GetType() const
{
	return type;
}

Key Tag::GetTag() const
{
	return tag;
}

// template<typename T, typename _ = void>
// struct is_container : std::false_type {};

// template<typename... Ts>
// struct is_container_helper {};

// template<typename T>
// struct is_container<
//         T,
//         std::conditional_t<
//             false,
//             is_container_helper<
//                 typename T::size_type,
//                 typename T::iterator,
//                 typename T::const_iterator,
//                 decltype(std::declval<T>().size()),
//                 decltype(std::declval<T>().begin()),
//                 decltype(std::declval<T>().end()),
//                 decltype(std::declval<T>().cbegin()),
//                 decltype(std::declval<T>().cend())
//                 >,
//             void
//             >
//         > : public std::true_type {};

unsigned GetOperator(const char *s) {
	unsigned ret = 0;
	for (unsigned i = 0; i < sizeof(unsigned) && Tag::is_operator(*s); ++i, ++s)
		ret |= static_cast<unsigned>(*s) << (8 * i);
	return ret;
}

unsigned GetOperator(const char *s, size_t &it) {
	unsigned ret = 0;
	s += it;
	for (unsigned i = 0; i < sizeof(unsigned) && Tag::is_operator(*s); ++i, ++s, ++it)
		ret |= static_cast<unsigned>(*s) << (8 * i);
	return ret;
}

unsigned GetOperator(std::string_view::iterator &it, const std::string_view::iterator end) {
	unsigned ret = 0;
	for (unsigned i = 0; i < sizeof(unsigned) && it != end && Tag::is_operator(*it); ++i, ++it)
		ret |= static_cast<unsigned>(*it) << (8 * i);
	return ret;
}

template <typename Lambda>
inline std::string_view scoop_iterate(std::string_view::iterator &it, const std::string_view::iterator end, Lambda&& l) {
	const auto beg = it;
	it = std::find_if_not(beg, end, l);
	return { beg, static_cast<size_t>(it - beg) };
}

void DealWithBlockSpecial(std::string_view data, std::vector<Child> &children) {
	for(auto it = data.begin(); it != data.end();) {
		Violet::skip_whitespaces(it, data.end());
		if (Tag::is_operator(*it)) {
			children.emplace_back(GetOperator(it, data.end()));
			Violet::skip_whitespaces(it, data.end());
			children.emplace_back(scoop_iterate(it, data.end(), [](char c) { return Tag::is_acceptable(c) && !Tag::is_operator(c); }));
		}
		else {
			children.emplace_back(scoop_iterate(it, data.end(), [](char c) { return Tag::is_acceptable(c) && !Tag::is_operator(c); }));
		}
	}
}

std::optional<std::pair<size_t,size_t>> find_markers(const std::string_view& sv, char a1, char a2) {
	size_t r1 = sv.find(a1), r2 = sv.find(a2);
	if (r1 != std::string_view::npos && r2 != std::string_view::npos && r2 > r1)
		return std::make_pair(r1, r2);
	return {};
}


size_t strfind(const char * s, const char c, size_t i, const size_t end) {
	for (s += i; i < end && *s != 0x0; ++i, ++s)
		if (*s == c)
			return i;
	return std::string::npos;
}

size_t strfind_mindquotes(const char * s, const char c, size_t i, const size_t end) {
	for (s += i; i < end && *s != 0x0; ++i, ++s) {
		const char q = *s;
		if (q == c)
			return i;
		else if (q == '\"' || q == '\'') {
			for (++i, ++s; *s != q && i < end && *s != 0x0; ++i, ++s);
		}
	}
	return std::string::npos;
}

bool cik_strcmp(const char * s, const char * k) {
	while (*s != '\0' && *k != '\0')
		if (::tolower(*(s++)) != *(k++))
			return false;
	return true;
}

Tag::Tag(std::string_view data) : type(Mode::modTrigger), tag(Key::tagUnknown)
{
	try
	{
		size_t i = 0, pos = 0;
		while (is_acceptable(data[i])) ++i;
		
		auto r = map.find(data.substr(0, i));
		if (r != map.end()) {
			tag = r->second;
			Violet::skip_whitespaces(data, i);
			data.remove_prefix(i);
			i = 0;
		}
		else return;
		
		if (tag == tagBlock)
		{
			if (data.front() != ':')
				throw Error::bad_format;
			data.remove_prefix(1);
			i = std::find_if_not(data.begin(), data.end(), [](char c) { return is_acceptable(c); }) - data.begin();
			
			if (auto e = find_markers(data.substr(0, i), '[', ']'); e)
			{
				children.emplace_back(data.substr(0, e->first));
				children.emplace_back(data.substr(e->first + 1, e->second - e->first - 1));
				DealWithBlockSpecial(data.substr(e->second + 1), children);
				tag = tagBlockArray;
			}
			else
			{
				DealWithBlockSpecial(data, children);
			}
			if (children.size() == 0) throw Error::bad_format;
			type = Mode::modBlock;
		}
		else
		{
			Violet::skip_whitespaces(data, i);
			if (i < data.size())
			{
				switch (data[i++])
				{
				case '=':
					data = data.substr(i);
					type = Mode::modValue;
					break;
				case '(':
					if (auto f = strfind_mindquotes(data.data(), ')', i, data.size()); f != std::string::npos) {
						for (auto c = f + 1; c < data.size(); ++c)
							if (data[c] > 0x20)
								throw Error::bad_format;
						data = data.substr(i, f - i);
						type = Mode::modFunction;
						break;
					}
					else throw Error::bad_format;
				default:
					tag = tagUnknown;
					return;
				}
				i = 0;
				for (unsigned limit = type == Mode::modFunction ? 16 : 1; limit > 0; --limit)
				{
					Violet::skip_whitespaces(data, i);
					if (data[i] == '\"' || data[i] == '\'')
					{
						const char q = data[i++];
						for (pos = i; data[i] != q && i < data.length()/*&& data[i] > 0x0 && i < len*/; ++i);
						children.emplace_back(data.substr(pos, i++ - pos));
					}
					else
					{
						for (pos = i; is_acceptable(data[i]) && i < data.length(); ++i);
						
						if (auto e = find_markers(data.substr(pos, i - pos), '[', ']'); e)
							children.emplace_back(data.substr(pos, e->first), data.substr(pos + e->first + 1, e->second - e->first - 1));
						else
							children.emplace_back(data.substr(pos, i - pos));
					}
					if (limit > 1) {
						pos = i;
						Violet::skip_whitespaces(data, i);
						if (i >= data.size())
							break;
						else if (data[i++] != ',')
							throw Error::bad_format;
					}
				}
				
			}
		}
	}
	catch (Error e)
	{
		children.clear();
		type = Mode::modError;
	}
}