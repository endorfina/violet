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
#include "buffers.hpp"

#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <chrono>
#include <ctime>

#ifdef WIN32
#include <Winsock2.h>
char * strptime(const char *s, const char *format, tm *tm);

namespace Violet {
	class WSALifetimeHelper {
		WSADATA m_wsaData;
		const int init_state;
	public:
		WSALifetimeHelper()
			: init_state{ WSAStartup(MAKEWORD(2,2), &m_wsaData) } {}
		
		~WSALifetimeHelper(void) {
			if (init_state == 0) // init was successful
				WSACleanup();
		}
	};
}
#else
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#endif

#ifdef VIOLET_SOCKET_USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif
//#include <chrono>

#ifndef VIOLET_CUSTOM_USER_AGENT
	#define VIOLET_CUSTOM_USER_AGENT "VioletEcho/0.4"
#endif

namespace Violet
{
	enum class State {
		notconnected,
		connecting,
		connected,
		closed,
		error
#ifndef VIOLET_NO_COMPILE_HTTP
		, wait_statuscode,
		wait_headers,
		wait_messagebody,
		finished
#endif
	};
#ifndef VIOLET_NO_COMPILE_SERVER
	struct __RwSocket;

	class ListeningSocket
	{
		int mSocket = 0;
		bool mListening = false;
		bool mUseSafeHeader = false;

	public:
		ListeningSocket() = default;
		~ListeningSocket();

		void start(bool ipv6, uint16_t port, bool local);
		void stop();
		bool acceptable();
	#ifdef VIOLET_SOCKET_USE_OPENSSL
		__RwSocket accept(SSL_CTX * ctx = nullptr);
		__RwSocket block_once(SSL_CTX * ctx = nullptr);
	#else
		__RwSocket accept();
		__RwSocket block_once();
	#endif

		inline bool is_listening() const { return mListening; }
		inline void use_safety_header(bool _use) { mUseSafeHeader = _use; }
	};
#endif

	struct __RwSocket {
		friend class ListeningSocket;
	protected:
		State mState = State::notconnected;
		int mSocket = 0;
	#ifdef VIOLET_SOCKET_USE_OPENSSL
		SSL * mSsl_s = nullptr;
	#endif
		bool mShouldClose = false;
		bool mUseSafeHeader = false;
	};

	class BaseSocket : public __RwSocket
	{
	protected:
		addrinfo *mFirst_addrinfo = nullptr, *mCurrent_addrinfo = nullptr;

		virtual void append_read(const char *in, size_t size) = 0;
		virtual bool can_write() const = 0;
		virtual std::pair<const char *, size_t> get_write() = 0;
		virtual void write_confirm_sent(size_t bytes) = 0;

	public:
		BaseSocket() = default;
		BaseSocket(const BaseSocket&) = default;
		BaseSocket(BaseSocket&&) = default;
		BaseSocket(const __RwSocket& _rw) : __RwSocket{_rw} {}
		BaseSocket(__RwSocket&&_rw) : __RwSocket{_rw} {}
		//using __RwSocket::__RwSocket;
		~BaseSocket();

	public:
		virtual void reset();
	#ifdef VIOLET_SOCKET_USE_OPENSSL
		void connect(const char * address, uint16_t port, SSL_CTX * ctx = nullptr);
	#else
		void connect(const char * address, uint16_t port);
	#endif
		void update_read();
		void update_write();
		void to_be_closed();

		std::string get_peer_address() const;

		inline State get_state() const { return mState; }
		inline bool is_functional() const { return mState == State::connecting || mState == State::connected; }
		inline void use_safety_header(bool _use) { mUseSafeHeader = _use; }
	};

	template<class TempStorage>
	struct Socket : public BaseSocket {
		using BaseSocket::BaseSocket; // making the constructor visible is mandatory
		using buffer_t = Violet::buffer<TempStorage>;
		buffer_t mReadbuffer, mWritebuffer;

		void reset() {
			mReadbuffer.clear();
			mWritebuffer.clear();
			BaseSocket::reset();
		}
	private:	
		void append_read(const char *in, size_t size) { mReadbuffer.write_data(in, size); }

		bool can_write() const { return !mWritebuffer.is_at_end(); }

		std::pair<const char *, size_t> get_write() { return std::make_pair<const char *, size_t>(mWritebuffer.data(), mWritebuffer.size() * sizeof(typename buffer_t::value_type)); }

		void write_confirm_sent(size_t bytes) {
			mWritebuffer.set_pos(mWritebuffer.get_pos() + bytes);
			const size_t p = mWritebuffer.get_pos();
			if(p > mWritebuffer.length() / 4) {
				memmove(mWritebuffer.data(), mWritebuffer.data() + p, mWritebuffer.length() - p);
				mWritebuffer.set_pos(0);
				mWritebuffer.resize(mWritebuffer.length() - p);
			}
		}
	public:

		auto get_read_data() const { return mReadbuffer.get_string_current(); }

		size_t get_read_data_length() const { return mReadbuffer.length() - mReadbuffer.get_pos(); }

		template<class A>
		void write(A && data) {
			if(!mShouldClose && mState != State::error) {
				if(mUseSafeHeader)
					mWritebuffer.write_utfx(data.size());
				mWritebuffer.template write<A>(data);
			}
		}

		size_t get_write_data_length() const { return mWritebuffer.length() - mWritebuffer.get_pos(); }
		
		void dump_data(size_t length) {
			mReadbuffer.set_pos(mReadbuffer.get_pos() + length);
			size_t p = mReadbuffer.get_pos();
			if (mReadbuffer.is_at_end())
				mReadbuffer.clear();
			else if(p > mReadbuffer.length() / 4) {
				memmove(mReadbuffer.data(), mReadbuffer.data() + p, mReadbuffer.length() - p);
				memset(mReadbuffer.data() + (mReadbuffer.length() - p), 0, p);
				mReadbuffer.set_pos(0);
				mReadbuffer.resize(mReadbuffer.length() - p);
			}
		}

		bool read_message(buffer_t &b) {
			if (mUseSafeHeader) {
				const size_t p1 = mReadbuffer.get_pos();
				const auto len = mReadbuffer.read_utfx();
				if (len == 0) return false;
				const size_t p2 = mReadbuffer.get_pos();
				mReadbuffer.set_pos(p1);
				if(len <= mReadbuffer.length() - p2) {
					b.clear();
					b.resize(len);
					memcpy(b.data(), mReadbuffer.data() + p2, len);
					dump_data(p2 - p1 + len);
					return true;
				}
			}
			else if (mReadbuffer.length() != 0) {
				b.clear();
				b.swap(mReadbuffer);
				return true;
			}
			return false;
		}

		buffer_t pop_read_message() {
			buffer_t tbuf;
			mReadbuffer.swap(tbuf);
			return tbuf;
		}

		template<class A>
		buffer_t &operator<<(A&&x) {
			mWritebuffer.template write<std::decay_t<A>>(x);
			return mWritebuffer;
		}
		
		// Returns true if a message was read
		bool operator>>(buffer_t &b) {
			return read_message(b);
		}
	};

	template<typename A>
	inline bool caseinsensitive_equal(const std::basic_string_view<A>& a, const std::basic_string_view<A>& b) {
		return Violet::internal::ciscompare(a.data(), a.size(), b.data(), b.size());
	}

	template<class S>
	constexpr inline auto insure_str(S && s) noexcept {
		static_assert(std::is_convertible_v<std::remove_reference_t<S>, std::string_view>);
		if constexpr (std::is_same_v<std::remove_reference_t<S>, std::string>) {
			return std::forward<S>(s);
		}
		else {
			return static_cast<std::string_view>(s);
		}
	}

	template<class V>
	inline std::string_view variant_to_view(const V& v) {
		return std::visit([](auto&&a) { return static_cast<std::string_view>(a); }, v);
	}

#ifndef VIOLET_NO_COMPILE_HTTP

#define DTFORMAT "%a, %d %b %Y %T GMT"

	class HttpRequest {
		struct HttpSocket : public BaseSocket {
			using BaseSocket::BaseSocket;
			std::string response, request;

			void reset() {
				response.clear();
				request.clear();
				BaseSocket::reset();
			}
		private:	
			void append_read(const char *in, size_t size) { response.append(in, size); }

			bool can_write() const { return request.size() > 0; }

			std::pair<const char *, size_t> get_write() { return std::make_pair<const char *, size_t>(request.c_str(), request.length()); }

			void write_confirm_sent(size_t bytes) {
				if (bytes >= request.length())
					request.clear();
				else
					request.erase(0, bytes);
			}

		public:
			const std::string& get_read_data() const { return response; }

			size_t get_read_data_length() const { return response.length(); }

			void dump_data(size_t characters) {
				if (characters >= response.length())
					response.clear();
				else
					response.erase(0, characters);
			}

			std::string pop_read_message() { return std::move(response); }

			template<class A>
			HttpSocket &operator<<(A&&x) {
				if(!mShouldClose && mState != State::error) {
					if constexpr (std::is_convertible_v<A, const char *>)
						request.append(x);
					else
						request.append(x.data(), x.length());
				}
				return *this;
			}
			
			// Returns true if a message was read
			bool operator>>(std::string &str) {
				str = pop_read_message();
				return str.length() > 0;
			}
		};
	
		struct Cookie {
			std::string value;
			std::chrono::system_clock::time_point expi;
			bool persistent = false;
		};

		State mState = State::notconnected;
		unsigned int statuscode;
		bool mSsl;
		std::string response;
	public:
		HttpSocket mSock;
	private:
		enum Encoding { NoEncoding, Deflate, Gzip };
		Encoding encoding = Encoding::NoEncoding;
		bool transfer_chunked = false;

	public:
		struct PostParameters {
			using variant_type = std::variant<std::string, std::string_view>;
			struct value_type {
				variant_type name, filename, value;
				value_type() = default;
				template<class V1, class V2, class V3>
				value_type(V1&&a, V2&&b, V3&&c)
						: name(std::forward<V1>(a)), filename(std::forward<V2>(b)), value(std::forward<V3>(c)) {}
			};
			struct return_type {
				std::string_view name, filename, value;
				return_type() = default;
				return_type(const std::string_view &a, const std::string_view &b, const std::string_view &c)
						: name(a), filename(b), value(c) {}
			};
			using container = std::vector<value_type>;
		private:
			container data;
		public:
			template<class S1, class S2>
			value_type &emplace(S1 && name, S2 && value) {
				return data.emplace_back(insure_str(name), std::string_view{}, insure_str(value));
			}
			template<class S1, class S2, class S3>
			value_type &emplace(S1 && name, S2 && filename, S3 && value) {
				return data.emplace_back(insure_str(name), insure_str(filename), insure_str(value));
			}

			template<class S>
			std::optional<return_type> strs(S &&name) const {
				const std::string_view cn = name;
				for(auto&&it : data)
					if (std::visit([&cn](auto&&v) { return caseinsensitive_equal<char>(cn, v); }, it.name))
						return { { cn, variant_to_view(it.filename), variant_to_view(it.value) } };
				return {};
			}

			std::vector<return_type> clone(void) const {
				std::vector<return_type> v;
				for (auto&&it : data)
					v.emplace_back(variant_to_view(it.name), variant_to_view(it.filename), variant_to_view(it.value));
				return v;
			}

			inline void clear() { data.clear(); }
			inline auto size() const { return data.size(); }
			inline const container &operator*() const { return data; }
		}
		postparameters;


		struct pairs_of_strings {
			using variant_type = std::variant<std::string, std::string_view>;
			using value_type = std::pair<variant_type, variant_type>;
			using container = std::vector<value_type>;
		private:
			container data;
		public:
			template<class S1, class S2>
			void emplace(S1 && name, S2 && value, bool replace = true) {
				if(replace) {
					const std::string_view cn = name;
					for(auto&&it : data)
						if (std::visit([&cn](auto&&v) { return caseinsensitive_equal<char>(cn, v); }, it.first)) {
							it.second = insure_str(value);
							return;
						}
				}
				data.emplace_back(insure_str(name), insure_str(value));
			}
			template<class S1, class S2>
			bool try_emplace(S1 && name, S2 && value) {
				const std::string_view cn = name;
				for(auto&&it : data)
					if (std::visit([&cn](auto&&v) { return caseinsensitive_equal<char>(cn, v); }, it.first))
						return false;
				data.emplace_back(insure_str(name), insure_str(value));
				return true;
			}

			template<class S>
			std::string_view str(S &&name) const {
				const std::string_view cn = name;
				for(auto&&it : data)
					if (std::visit([&cn](auto&&v) { return caseinsensitive_equal<char>(cn, v); }, it.first))
						return std::visit([](auto&&v) { return static_cast<std::string_view>(v); }, it.second);
				return "";
			}

			template<class S>
			bool exists(const S &name) const {
				const std::string_view cn = name;
				for(auto&&it : data)
					if (std::visit([&cn](auto&&v) { return caseinsensitive_equal<char>(cn, v); }, it.first))
						return true;
				return false;
			}

			template<class S>
			variant_type &operator[](S&&name) {
				const std::string_view cn = name;
				for(auto&&it : data)
					if (std::visit([&cn](auto&&v) { return caseinsensitive_equal<char>(cn, v); }, it.first))
						return it.second;
				return data.emplace_back(insure_str(name), std::string_view{}).second;
			}

			template<class S>
			void remove(const S &name) {
				data.erase(std::remove_if(data.begin(), data.end(),
				[name = std::string_view{name}](const value_type& t) { return std::visit([&name](auto&&v) { return caseinsensitive_equal<char>(name, v); }, t); }), data.end());
			}

			inline void clear() { data.clear(); }
			inline auto size() const { return data.size(); }
			inline const container &operator*() const { return data; }
		}
		requestheaders, responseheaders;
		std::map<std::string, Cookie> cookies;

		static std::string url_encode(const std::string_view& str, bool keepspecialchars = false);
		static std::string url_decode(const std::string_view& str);

		void reset();
		bool update();
	#ifdef VIOLET_SOCKET_USE_OPENSSL
		void connect(const std::string_view& url, SSL_CTX * = nullptr);
	#else
		void connect(const std::string_view& url);
	#endif

		inline unsigned get_status_code() const { return statuscode; }

		const std::string &get_message_body() const { return response; };
		size_t get_message_body_length() const;

	private:
		void parse_cookies(std::string sv);
		void parse_cookies(char *c, size_t len);

	public:
		template<class B>
		void save_cookies(B &b) const {
			char dt[64];
			for (auto&&it : cookies)
				if (it.second.persistent) {
					const auto tt = std::chrono::system_clock::to_time_t(it.second.expi);
					tm gmt = *gmtime(&tt);
					strftime(dt, 64, DTFORMAT, &gmt);
					b << it.first << char('=') << it.second.value << "; EXPIRES=";
					b.write_data(dt, strlen(dt));
					b.write_crlf();
				}
		}
		void load_cookies(char *c, size_t len);
		inline State get_state() const { return mState; }
	};
#endif
}