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
#include "echo/tcp.hpp"
#include "error.hpp"
#include "captcha_image_generator.hpp"
#include "blog.h"

#ifdef _DEBUG
#define MAX_HEAP_SIZE 16
#else
#define MAX_HEAP_SIZE 3200
#endif

//#define MONITOR_SOCKETS

inline void GetTimeGMT(char * dest, size_t buffSize, const char *_format, bool localTime = false);

std::string ReadAlphanumeric(const std::string &src, size_t pos = 0, bool include_underscore = false);

std::string ReadUntilSpace(const std::string &src, size_t pos = 0, const char trim_char = 0);


struct Protocol {

	struct Session {
		std::string username, email;
		time_t last_activity;
		uint8_t userlevel;

		struct CharacterInfo {
			std::string name;
		};

		CharacterInfo * character;

		Session(const std::string &name);

		~Session(void);
	};

	// struct __session_functor_comparator {

	// };
	using sessions_t = std::unordered_map<std::string, Session>;

	struct Hi {
		using headers_t = std::unordered_map<std::string_view, const char *, std::hash<std::string_view>, Violet::cis_functor_equal_comparator>;
		headers_t raw_headers, content_headers;
		bool keepalive = false;
		const char * fetch = nullptr;//, table;

		enum class Method { Get, Head, Post, Put, Delete, Trace, Options, Error }

		method = Method::Error;

		using map_t = std::map<std::string, std::string, Violet::functor_less_comparator>;
		map_t get, post, cookie, clipboard;

		struct _F {
			std::string name, filename, mime_type;
			std::vector<char> data;
		};

		std::list<_F> file;
		std::list<std::string> temp_strs;
		
	protected:
		Violet::UniBuffer table;

	public:
		size_t BuildFromBuffer(Violet::UniBuffer &&src);

		size_t ParsePOST(const char * src, const size_t len);

		size_t ParseGET(const char *src, size_t offset = 0);

		size_t ParseCookies(const char * src);

		void Print();

		void Clear();

		inline void AddHeader(const char *s1, const char *s2) { content_headers.emplace(s1, s2); }

		inline const char * GetTableAtPos() const { return table.data() + table.get_pos(); }

		inline size_t GetRemainingTable() const { return table.length() - table.get_pos(); }

		template<class S>
		inline map_t &list(const S & _name) {
			using namespace std::string_view_literals;
			if (_name == "post"sv)
				return post;
			else if (_name == "get"sv)
				return get;
			else if (_name == "cookie"sv)
				return cookie;
			else if (_name == "clipboard"sv || _name == "cb"sv)
				return clipboard;
			throw violet_exception::wrong_enum;
		}
	};

	Violet::Socket<std::vector<char>> s;
	bool received = false, response_ready = false, sent = false, received_body = false;
	size_t body_length = 0;
	std::vector<char> body_temp;
	
	time_t last_used = time(nullptr);
	
	Hi info;
	Session *ss = nullptr;
	
	const char * dir_accessible, * dir_accounts;

	// deprecated
	//static unsigned long id_range;
	//const unsigned long id;

	sessions_t & sessions;

	Protocol(void) = delete;
	
	template<class T>
	Protocol(T&&_sock, sessions_t &_se, const char * access, const char * accounts)
		: s{std::forward<T>(_sock)}, dir_accessible(access), dir_accounts(accounts), sessions(_se) /*id(++id_range)*/ {}
	
#ifdef MONITOR_SOCKETS
	unsigned _packets_sent = 0;
	~Protocol() { printf("> ~Destroyed [id:%lu, packets sent:%u]\n", id, _packets_sent); }
#endif

	void HandleRequest();

private:
	void HandleHTML(Violet::UniBuffer &file, uint16_t error);

	void CreateSession(std::string_view name, Violet::UniBuffer *loaded_file);

public:
	
		////   thread-safe GLOBALS   ////

	static std::map<std::string, std::string, Violet::functor_less_comparator> content_types;
	
	static std::list<Blog> active_blogs;

	static const char *dir_html, *dir_log;

	static std::string dir_work;

	static std::pair<std::mutex, Violet::UniBuffer> logging;

	static void WriteDateToLog();

	static void SaveLogHeapBuffer();
};