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

#pragma once

namespace Curly
{
	enum Mode { modFunction, modTrigger, modValue, modBlock, modError };
	enum Key { tagUnknown, tagBlock, tagBlockArray, tagWrapper, tagTitle, tagContent, tagBlogLoad, tagPosts, tagStatusErrorCode, tagStatusErrorText, tagCopyright, tagBitcoin, tagBattery, tagInclude, tagStartSession, tagKillSession, tagRegister, tagSessionInfo, tagEcho, tagGenerateCaptcha, tagCatch_recv_error };

	//static_assert(std::is_trivially_destructible_v<std::optional<std::string>>);

	struct Child {
		bool is_string;
		std::string_view str;
		unsigned val;
		//std::vector<Child> child;
		std::unique_ptr<Child> child;
		
		Child(unsigned _v) : is_string(false), str(), val(_v) {}
		Child(const std::string_view &_s) : is_string(true), str(_s), val(0) {}
		//template<typename... Args>
		// Child(const std::string_view &_s, Args&&... args) : is_string(true), str(_s), val(0) {
		// 	(child.push_back(Child(args)), ...);
		// }
		template<typename T>
		Child(const std::string_view &_s, T&& _c) : is_string(true), str(_s), val(0), child(std::make_unique<Child>(_c)) {}
	};
	
	class Tag
	{
		Mode type;
		Key tag;
		std::vector<Child> children;

		// struct mapViewCICompare {
		// 	bool operator() (const std::string_view &a, const std::string_view &b) const;
		// };

		static const std::map <std::string_view, Key> map;

	public:

		Mode GetType() const;

		Key GetTag() const;

		inline const Child &GetObj(std::vector<Child>::size_type n) const {
			return children.at(n);
		}

		inline size_t GetObjCount() const {
			return children.size();
		}

		Tag(char * src);

		Tag(std::string_view src);

		template<typename A>
		static inline bool is_operator(const A c) {
			return (c == '!' || c == '=' || c == ',' || c == ':' || c == '(' || c == ')' || c == '<' || c == '>');
		}
		
		template<typename A>
		static inline bool is_acceptable(const A c)
		{
			return (c >= '!' && c <= '~' && c != '=' && c != ',' && c != ':' && c != '(' && c != ')' && c != '{' && c != '}');
		}
	};
}

//inline void SkipWhiteSpaces(const char *data, size_t &it);

size_t strfind(const char * s, const char c, size_t start = 0, const size_t end = std::string::npos);

size_t strfind_mindquotes(const char * s, const char c, size_t start = 0, const size_t end = std::string::npos);

bool cik_strcmp(const char * s, const char * k);