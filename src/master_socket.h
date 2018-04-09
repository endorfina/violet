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
#include "tcp.hpp"
#include "blog.h"

#ifdef _DEBUG
#define MAX_HEAP_SIZE 16
#else
#define MAX_HEAP_SIZE 3200
#endif

//#define MONITOR_SOCKETS

inline void GetTimeGMT(char * dest, size_t buffSize, const char *_format, bool localTime = false);

//std::string ReadUntilSpace(Violet::Buffer &src, const char trim_char = 0);

std::string ReadAlphanumeric(const std::string &src, size_t pos = 0, bool include_underscore = false);

std::string ReadUntilSpace(const std::string &src, size_t pos = 0, const char trim_char = 0);

//std::string ReadUntilNewLine(Violet::Buffer &src);


// inline std::string str_convert(std::string_view str) {
//     if constexpr (std::is_convertible<std::string_view, std::string>::value) {
//         return std::string(str);
//     }
//     else {
//         return std::string(str.data(), str.size());
//     }
// }


struct MasterSocket {

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
	
	struct CaptchaImageType {
		typedef std::pair<uint8_t, uint8_t> collectible_type;
		std::array<collectible_type, 4> Collection;
		std::array<char, 24> PicFilename;
		std::future<Violet::UniBuffer> Data;
	};

	struct Hi {
		std::vector<ConstStringPair> raw_headers, content_headers;
		bool keepalive = false;
		const char * fetch = nullptr;//, table;

		enum Method { Get, Head, Post, Put, Delete, Trace, Options, Error };

		Method method = Method::Error;

		std::vector<StringPair> get, post, cookie, clipboard;

		struct _F {
			std::string name, filename, mime_type;
			std::vector<char> data;
		};

		std::list<_F> file;
		std::list<std::string> temp_strs;
		
	protected:
		Violet::UniBuffer table;

	private:
		static size_t ReadUntilPOSTDemiliter(const char * const data, size_t pos, const size_t len, StringPair &p);

		static unsigned int ReadUntilCookieDemiliter(const char * const data, unsigned int pos, const size_t len, StringPair &p);

		static unsigned int ReadUntilPOSTContentDispositionDelimiter(const char * const data, unsigned int pos, const size_t len, StringPair &p);

		static size_t ParsePOSTContentDisposition(std::string &src, std::vector<StringPair> &dest);

	public:
		size_t BuildFromBuffer(Violet::UniBuffer &&src);

		size_t ParsePOST(const char * src, const size_t len);

		size_t ParseGET(const char *src, size_t offset = 0);

		size_t ParseCookies(const char * src);

		void Print();

		void Clear();

		std::vector<StringPair> * FetchList(const std::string_view &f);

		inline void AddHeader(const char *s1, const char *s2);
		
		inline const char * GetTableAtPos();
		
		inline size_t GetRemainingTable();

		//inline void AddHeader(const char *s1, std::string &&s2);

		//inline void AddHeader(const char *s1, std::string &s2);
	};

	Violet::Socket<std::vector<char>> s;
	bool received = false, response_ready = false, sent = false, received_body = false;
	size_t body_length = 0;
	std::vector<char> body_temp;
	
	time_t last_used = time(nullptr);
	
	Hi info;
	Session *ss = nullptr;
	
	const char * dir_accessible, * dir_accounts;

	static unsigned long id_range;
	const unsigned long id;
	
	template<class T>
	MasterSocket(T&&_sock, const char * access, const char * accounts)
		: s{std::forward<T>(_sock)}, dir_accessible(access), dir_accounts(accounts), id(++id_range) {}
	
#ifdef MONITOR_SOCKETS
	unsigned _packets_sent = 0;
	~MasterSocket() { printf("> ~Destroyed [id:%lu, packets sent:%u]\n", id, _packets_sent); }
#endif

	void HandleRequest();

	void HandleHTML(Violet::UniBuffer &file, uint16_t error);

	void CreateSession(std::string_view name, Violet::UniBuffer *loaded_file);
	
	
		////   GLOBALS   ////

	static std::vector<StringPair> content_types;

	static std::unordered_map<std::string, Session> sessions;
	
	static std::list<Blog> active_blogs;
	
	static std::list<std::pair<CaptchaImageType, time_t>> active_captchas;

	static const char *dir_html, *dir_log;

	static std::string dir_work;

	static Violet::UniBuffer log_heap;

	static void WriteDateToLog();

	static void SaveLogHeapBuffer();
};