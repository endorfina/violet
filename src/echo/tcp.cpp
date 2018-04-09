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

#include "tcp.hpp"
#include <errno.h>
#include <fcntl.h>
#include <array>
#include <ctime>

#ifdef WIN32
#include <ws2tcpip.h>
#include <cctype>
#ifndef close
	#define close closesocket
#endif
#ifndef SHUT_WR
	#define SHUT_WR SD_SEND
#endif
inline auto _win32_make_nonblocking(int s, bool dontblock) {
	u_long meta = u_long(dontblock);
	return ioctlsocket(s, FIONBIO, &meta);
}
#define SOCKET_MAKENONBLOCKING(s) _win32_make_nonblocking((s), true)
#define SOCKET_MAKEBLOCKING(s) _win32_make_nonblocking((s), false)
#else
#include <unistd.h>
#define SOCKET_MAKENONBLOCKING(s) fcntl((s), F_SETFL, O_NONBLOCK)
#define SOCKET_MAKEBLOCKING(s) fcntl((s), F_SETFL, 0x0)
#endif


#ifndef INVALID_SOCKET
	#define INVALID_SOCKET -1
#endif

#ifndef SOCKET_ERROR
	#define SOCKET_ERROR -1
#endif

#define READ_BUFFER_SIZE 1020

using namespace Violet;
using namespace std::string_view_literals;

std::string addr_to_string(const sockaddr* addr) {
	std::string buff;
	
	if(addr->sa_family == AF_INET)
	{
		// IPv4
		auto u = reinterpret_cast<const uint8_t*>(&(reinterpret_cast<const sockaddr_in*>(addr)->sin_addr));
		buff.resize(18);
		buff.resize(snprintf(&buff[0], buff.capacity(), "%hhu.%hhu.%hhu.%hhu", u[0], u[1], u[2], u[3]));
	}
	else if(addr->sa_family == AF_INET6)
	{
		// IPv6
		size_t s = 0, i = 0;
		buff.resize(50);
		auto u = reinterpret_cast<const uint16_t*>(&(reinterpret_cast<const sockaddr_in6*>(addr)->sin6_addr));
		const auto zeros = static_cast<size_t>(std::search_n(u, u + 8, 2, uint16_t(0x0)) - u);
		if (zeros < 8) {
			while (i < zeros)
				s += sprintf(&buff[s], "%hx:", u[i++]);
			while (i < 8 && u[i] == 0) ++i;
			if (i < 8)
				do {
					s += sprintf(&buff[s], ":%hx", u[i++]);
				} while (i < 8);
			else buff[s++] = ':';
		}
		else {
			for (; i < 8; ++i)
				s += sprintf(&buff[s], i < 8 ? "%hx:" : "%hx", u[i]);
		}
		buff.resize(s);
	}
	return buff;
}

// uint16_t SockAddrGetPort(const sockaddr* addr) {
	
// 	uint16_t port = 0;
// 	if(addr->sa_family == AF_INET) {
// 		// IPv4
// 		port = (reinterpret_cast<const sockaddr_in*>(addr))->sin_port;
// 	} else if(addr->sa_family == AF_INET6) {
// 		// IPv6
// 		port = (reinterpret_cast<const sockaddr_in6*>(addr))->sin6_port;
// 	}
	
// 	// reverse byte order
// 	return ((port & 0xff) << 8) | (port >> 8);
// }

#ifndef VIOLET_NO_COMPILE_SERVER
ListeningSocket::~ListeningSocket() {
	stop();
}

void ListeningSocket::start(bool ipv6, uint16_t port, bool local)
{	
	stop();
	
	// initialize addrinfo hints
	addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = ipv6 ? AF_INET6 : AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	if(!local)
		hints.ai_flags = AI_PASSIVE;
	
	// get addrinfo
	addrinfo *first_addrinfo = nullptr;
	char portstring[6];
	snprintf(portstring, 6, "%hu", port);
	int result = getaddrinfo(local ? "localhost" : nullptr, portstring, &hints, &first_addrinfo);
	if(result != 0 || first_addrinfo == nullptr) {
		return;
	}
	
	// create a socket
	mSocket = ::socket(first_addrinfo->ai_family, first_addrinfo->ai_socktype, first_addrinfo->ai_protocol);
	if(mSocket == INVALID_SOCKET) {
		freeaddrinfo(first_addrinfo);
		return;
	}
	
	// bind the socket
	result = bind(mSocket, first_addrinfo->ai_addr, static_cast<int>(first_addrinfo->ai_addrlen));
	freeaddrinfo(first_addrinfo);
	if(result == SOCKET_ERROR) {
		printf("bind() error: %d\n", errno);
		close(mSocket);
		return;
	}
	
	// start listening
	result = listen(mSocket, SOMAXCONN);
	if(result == SOCKET_ERROR) {
		close(mSocket);
		mSocket = INVALID_SOCKET;
		return;
	}
	
	// make the socket nonblocking
	SOCKET_MAKENONBLOCKING(mSocket);
	
	mListening = true;
}

void ListeningSocket::stop() {
	if(mListening) {
		close(mSocket);
		mListening = false;
	}
}

bool ListeningSocket::acceptable() {
	
	if(!mListening) {
		return false;
	}
	
	fd_set read;
	FD_ZERO(&read);
	FD_SET(mSocket, &read);
	
	timeval t{ 0, 0 };
	if(::select(FD_SETSIZE, &read, nullptr, nullptr, &t) == SOCKET_ERROR) {
		close(mSocket);
		mListening = false;
		return false;
	}
	
	return (FD_ISSET(mSocket, &read));
	
}

#ifdef VIOLET_SOCKET_USE_OPENSSL
__RwSocket ListeningSocket::accept(SSL_CTX * ctx) {
#else
__RwSocket ListeningSocket::accept() {
#endif
	__RwSocket socket;
	
	if(!mListening) {
		socket.mState = State::error;
		return socket;
	}

	socket.mSocket = ::accept(mSocket, nullptr, nullptr);
	if(socket.mSocket == INVALID_SOCKET) {
		socket.mState = State::error;
	} else {
		SOCKET_MAKENONBLOCKING(socket.mSocket);
		socket.mState = State::connected;
		socket.mUseSafeHeader = mUseSafeHeader;
#ifdef VIOLET_SOCKET_USE_OPENSSL
		if (ctx) {
			socket.mSsl_s = SSL_new(ctx);
			if (socket.mSsl_s != nullptr) {
				SSL_set_fd(socket.mSsl_s, socket.mSocket);
				SSL_set_accept_state(socket.mSsl_s);
				SSL_accept(socket.mSsl_s);
				ERR_print_errors_fp(stderr);
			}
			else ERR_print_errors_fp(stderr);
		}
#endif
	}
	return socket;
}

#ifdef VIOLET_SOCKET_USE_OPENSSL
__RwSocket ListeningSocket::block_once(SSL_CTX * ctx) {
#else
__RwSocket ListeningSocket::block_once() {
#endif
	SOCKET_MAKEBLOCKING(mSocket);
#ifdef VIOLET_SOCKET_USE_OPENSSL
	auto s = accept(ctx);
#else
	auto s = accept();
#endif
	SOCKET_MAKENONBLOCKING(mSocket);
	return s;
}
#endif

BaseSocket::~BaseSocket() {
	reset();
}

void BaseSocket::reset() {
	
	if(mState == State::connecting || mState == State::connected || mState == State::closed) {
		close(mSocket);
	}
	if(mState == State::connecting) {
		freeaddrinfo(mFirst_addrinfo);
	}
	
	mState = State::notconnected;
	mShouldClose = false;
	mUseSafeHeader = false;
#ifdef VIOLET_SOCKET_USE_OPENSSL
	if (mSsl_s != nullptr) {
		SSL_free(mSsl_s);
		mSsl_s = nullptr;
	}
#endif
}

#ifdef VIOLET_SOCKET_USE_OPENSSL
	void BaseSocket::connect(const char * address, uint16_t port, SSL_CTX * ctx)
#else
	void BaseSocket::connect(const char * address, uint16_t port)
#endif
{	
	// reset the socket (only if already connected, otherwise the buffers would be cleared)
	if(mState != State::notconnected) {
		reset();
	}
	
	// initialize addrinfo hints
	addrinfo hints;
	memset(&hints, 0x0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	
	// get addrinfo list
	char portstring[6];
	snprintf(portstring, 6, "%hu", port);
	int result = getaddrinfo(address, portstring, &hints, &mFirst_addrinfo);
	if(result != 0 || mFirst_addrinfo == nullptr) {
		mState = State::error;
		return;
	}
	
	// try to connect
	// connect fails because this is a nonblocking socket, this is normal
	for(mCurrent_addrinfo = mFirst_addrinfo; mCurrent_addrinfo != nullptr; mCurrent_addrinfo = mCurrent_addrinfo->ai_next) {
		mSocket = ::socket(mCurrent_addrinfo->ai_family, mCurrent_addrinfo->ai_socktype, mCurrent_addrinfo->ai_protocol);
		if(mSocket == INVALID_SOCKET) {
			continue;
		}
		SOCKET_MAKENONBLOCKING(mSocket);
		::connect(mSocket, mCurrent_addrinfo->ai_addr, static_cast<int>(mCurrent_addrinfo->ai_addrlen));
		mState = State::connecting;
#ifdef VIOLET_SOCKET_USE_OPENSSL
		if (ctx) {
			mSsl_s = SSL_new(ctx);
			if (mSsl_s != nullptr) {
				SSL_set_fd(mSsl_s, mSocket);
				SSL_set_connect_state(mSsl_s);
				SSL_set_tlsext_host_name(mSsl_s, address); 
				SSL_connect(mSsl_s);
				ERR_print_errors_fp(stderr);
			}
			else ERR_print_errors_fp(stderr);
		}
#endif
		return;
	}
	
	// connecting failed
	freeaddrinfo(mFirst_addrinfo);
	mState = State::error;
}

#define SOCKET_EXCEPT_CONNECT

void BaseSocket::update_read() {
	// connecting
	if(mState == State::connecting) {
		
		fd_set write, except;
		FD_ZERO(&write);
		FD_SET(mSocket, &write);
#ifdef SOCKET_EXCEPT_CONNECT
		FD_ZERO(&except);
		FD_SET(mSocket, &except);
#endif
		
		// call select()
		timeval t = {0, 0};
		if(::select(mSocket + 1, nullptr, &write, &except, &t) == SOCKET_ERROR) {
			freeaddrinfo(mFirst_addrinfo);
			mState = State::error;
			return;
		}
		
		// error?
#ifdef SOCKET_EXCEPT_CONNECT
		if(FD_ISSET(mSocket, &except)) {
            if (errno == EINPROGRESS)
                return;
			// connection attempt has failed, try the next address
			close(mSocket);
			for(mCurrent_addrinfo = mCurrent_addrinfo->ai_next; mCurrent_addrinfo != nullptr; mCurrent_addrinfo = mCurrent_addrinfo->ai_next) {
				mSocket = ::socket(mCurrent_addrinfo->ai_family, mCurrent_addrinfo->ai_socktype, mCurrent_addrinfo->ai_protocol);
				if(mSocket == INVALID_SOCKET) {
					continue;
				}
				SOCKET_MAKENONBLOCKING(mSocket);
				::connect(mSocket, mCurrent_addrinfo->ai_addr, static_cast<int>(mCurrent_addrinfo->ai_addrlen));
#ifdef VIOLET_SOCKET_USE_OPENSSL
				if (mSsl_s != nullptr) {
					SSL_set_fd(mSsl_s, mSocket);
					SSL_set_connect_state(mSsl_s);
					SSL_connect(mSsl_s);
					ERR_print_errors_fp(stderr);
				}
#endif
				return;
			}
			
			// connecting failed
			freeaddrinfo(mFirst_addrinfo);
			mState = State::error;
			return;
			
		}
#endif
		// success?
		if(FD_ISSET(mSocket, &write)) {
			// connection attempt has succeeded
			freeaddrinfo(mFirst_addrinfo);
			mState = State::connected;
		}
	}
	
	// reading
	if(mState == State::connected) {
		char buffer[READ_BUFFER_SIZE];
		while(true) {
			// try to receive data
			int result;
#ifdef VIOLET_SOCKET_USE_OPENSSL
			if (mSsl_s) {
				result = SSL_read(mSsl_s, buffer, sizeof(buffer));
				if (result < 0) {
					auto e = SSL_get_error(mSsl_s, result);
					if (e == SSL_ERROR_WANT_WRITE || e == SSL_ERROR_WANT_READ) {
						break;
					}
					printf("SSL_read() error: %d\n", e);
					ERR_print_errors_fp(stderr);
					close(mSocket);
					mState = State::error;
					break;
				}
			}
			else
#endif
			{
				result = recv(mSocket, buffer, sizeof(buffer), 0);
				if(result == SOCKET_ERROR) {
					auto e = errno;
					if(e == EWOULDBLOCK) {
						break;
					}
					printf("recv() error: %d\n", e);
					close(mSocket);
					mState = State::error;
					break;
				}
			}
			
			if(result > 0) {
				append_read(buffer, result);
				
				if(static_cast<size_t>(result) < sizeof(buffer))
					break;
			}
			else {
				mState = State::closed;
				break;
			}
		}
	}
}

void BaseSocket::update_write() {
	if((mState == State::connected || mState == State::closed) && can_write()) {
		// try to send data
		int result;
		const auto [out, size] = get_write();
#ifdef VIOLET_SOCKET_USE_OPENSSL
		if (mSsl_s) {
			result = SSL_write(mSsl_s, out, static_cast<int>(size));
			if (result < 0) {
				auto e = SSL_get_error(mSsl_s, result);
				if (e == SSL_ERROR_WANT_WRITE || e == SSL_ERROR_WANT_READ) {
					return;
				}
				printf("SSL_write() error: %d\n", e);
				ERR_print_errors_fp(stderr);
				close(mSocket);
				mState = State::error;
				return;
			}
		}
		else
#endif
		{
			result = send(mSocket, out, size, 0);
			if(result == SOCKET_ERROR) {
				if(errno == EWOULDBLOCK) {
					return;
				}
				close(mSocket);
				mState = State::error;
				return;
			}
		}
		write_confirm_sent(result);
		
		// shut down?
		if(mShouldClose && !can_write()) {
			shutdown(mSocket, SHUT_WR);
			mState = State::closed;
		}
		
	}
	
}

void BaseSocket::to_be_closed() {
	mShouldClose = true;
}

std::string BaseSocket::get_peer_address() const {
	
	if(mState != State::connected && mState != State::closed) {
		return std::string();
	}
	
	sockaddr_in6 addr;
	socklen_t size = sizeof(addr);
	if(getpeername(mSocket, reinterpret_cast<sockaddr*>(&addr), &size) == SOCKET_ERROR) {
		return std::string();
	}
	
	return addr_to_string(reinterpret_cast<sockaddr*>(&addr));
}




#ifndef VIOLET_NO_COMPILE_HTTP

typedef std::array<unsigned char, 256> UrlEncodeTable;

constexpr UrlEncodeTable Create_HttpRequest_UrlEncodeTable() {
	UrlEncodeTable table { 0x0 };
	//const UrlEncodeTable &fake_const_table = table;
	for(const char *p = "!*'();:@&=+$,/?#[]%"; *p != '\0'; ++p) { table[static_cast<unsigned char>(*p)] = 1; }
	for(const char *p = "-_.~"; *p != '\0'; ++p) { table[static_cast<unsigned char>(*p)] = 2; }
	for(char c = '0'; c <= '9'; ++c) { table[static_cast<unsigned char>(c)] = 2; }
	for(char c = 'a'; c <= 'z'; ++c) { table[static_cast<unsigned char>(c)] = 2; }
	for(char c = 'A'; c <= 'Z'; ++c) { table[static_cast<unsigned char>(c)] = 2; }
	return table;
}

std::string HttpRequest::url_encode(const std::string_view & str, bool keepspecialchars) {
	constexpr static const auto httprequest_urlencodetable = Create_HttpRequest_UrlEncodeTable();
	const char *hextable = "0123456789abcdef";
	unsigned char threshold = keepspecialchars ? 1 : 2;
	std::string ret;
	ret.reserve(str.size() * 2);
	for(auto c : str)
		if(httprequest_urlencodetable[static_cast<unsigned char>(c)] >= threshold) {
			ret.push_back(c);
		} else {
			ret.push_back('%');
			ret.push_back(hextable[static_cast<unsigned char>(c) >> 4]);
			ret.push_back(hextable[static_cast<unsigned char>(c) & 15]);
		}
	return ret;
}

//template<class T, typename = std::enable_if_t<std::is_convertible_v<T, std::string_view>>>
std::string HttpRequest::url_decode(const std::string_view& str) {
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

void HttpRequest::parse_cookies(std::string s) {
	parse_cookies(s.data(), s.length());
}

void HttpRequest::parse_cookies(char *s, size_t len)
{
	std::vector<std::pair<std::string_view, std::string_view>> l;
	for (unsigned j = 0; j < len; ++j)
	{
		unsigned i;
		std::string_view name, val;
		for (i = j; i < len; ++i) {
			if (s[i] < '!' || s[i] > '~' || std::isspace(static_cast<unsigned char>(s[i])) != 0 || s[i] == ',' || s[i] == ';')
			{
				j = i + 1;
			}
			else break;
		}
		for (i = j; i < len; ++i) {
			if (s[i] == '=' || s[i] == ';' || std::isspace(static_cast<unsigned char>(s[i])) != 0)
			{
				break;
			}
		}
		name = std::string_view(s + j, i - j);//s.substr(j, i - j);
		if (s[j = i] == '=')
		{
			s[j++] = 0;
			for (i = j; i < len; ++i) {
				if (s[i] == ';')
					break;
			}
			val = std::string_view(s + j, i - j); //s.substr(j, i - j);
			j = i;
		}
		s[j] = 0;
		if (name.size() > 0)
			l.emplace_back(std::make_pair(name, val));
	}
	if (l.size() == 0)
		return;
	auto &r = cookies[static_cast<std::string>(l.front().first)];
	r.value = static_cast<std::string>(l.front().second);
	if (l.size() > 1) {
		bool max_age_has_prec = false;
		//auto f = std::find_if(l.begin() + 1, l.end(), [](const std::pair<std::string_view, std::string_view> &p) { return caseinsensitive_equal(p.first, "Expires"); });
		for(auto it = l.begin() + 1; it != l.end(); ++it) {
			if (caseinsensitive_equal<char>(it->first, "Max-Age")) {
				//auto sdt = static_cast<std::string>(it.second);
				max_age_has_prec = true;
				int val;
				if (sscanf(it->second.data(), "%d", &val) == 1) {
					r.persistent = true;
					r.expi = time(nullptr) + (val > 0 ? val : -1);
				}
				break; // for now
			}
			else if (!max_age_has_prec && caseinsensitive_equal<char>(it->first, "Expires")) {
				//auto sdt = static_cast<std::string>(it.second);
				tm modt;
				memset(&modt, 0, sizeof(tm));
				strptime(it->second.data(), DTFORMAT, &modt);
				r.expi = mktime(&modt);
				r.persistent = true;
			}
		}
	}
}

#include <random>

void HttpRequest::load_cookies(char *c, size_t len) {
	size_t i, j = 0;
	while (j < len) {
		for (i = j; i < len && std::isprint(static_cast<unsigned char>(c[i])) != 0; ++i);
		parse_cookies(c + j, i - j);
		for (j = i + 1; j < len && std::isspace(static_cast<unsigned char>(c[j])) != 0; ++j);
	}
}
void HttpRequest::reset()
{
	mSock.reset();
	requestheaders.clear();
	postparameters.clear();
	statuscode = 0;
	responseheaders.clear();
	mState = State::notconnected;
	mSsl = false;
	response.clear();
	encoding = Encoding::None;
	transfer_chunked = false;
}

#ifdef VIOLET_SOCKET_USE_OPENSSL
void HttpRequest::connect(const std::string_view& url, SSL_CTX * ctx)
#else
void HttpRequest::connect(const std::string_view& url)
#endif
{
	// disconnect
	if(mState != State::notconnected) {
		mSock.reset();
		statuscode = 0;
		responseheaders.clear();
		mSsl = false;
	}
	
	// get host, port and file
	std::string_view host, file;
	uint16_t port;
	{
		size_t pos_host = url.find("//");
		if (pos_host == std::string::npos)
			pos_host = 0;
		else
			pos_host += 2;
		mSsl = url.substr(0, 8) == "https://";
		size_t pos_slash = url.find('/', pos_host);
		if(pos_slash == std::string::npos) {
			pos_slash = url.length();
			file = "/";
		} else {
			file = url.substr(pos_slash);
		}
		size_t pos_colon = url.find_last_of(':', pos_slash);
		if(pos_colon > pos_slash || pos_colon < pos_host) {
			port = mSsl ? 443 : 80;
			host = url.substr(pos_host, pos_slash - pos_host);
		} else {
			sscanf(url.substr(pos_colon).data(), ":%hu", &port);
			host = url.substr(pos_host, pos_colon - pos_host);
		}
	}
	
	const bool post_request = postparameters.size() > 0;

	// write HTTP request
	mSock << (post_request ? "POST " : "GET ")
		<< url_encode(file, true) << " HTTP/1.1\r\n";

	requestheaders.try_emplace("Host"sv, host);

	requestheaders.emplace("Connection", "close", true);
	requestheaders.emplace("User-Agent", VIOLET_CUSTOM_USER_AGENT, true);
	requestheaders.emplace("Accept-Encoding", "gzip, deflate", true);

	if (cookies.size() > 0) {
		auto now = time(nullptr);
		for(auto it = cookies.begin(); it != cookies.end();) {
			if(it->second.persistent && difftime(now, it->second.expi) > 0)
				it = cookies.erase(it);
			else
				++it;
		}
		if (cookies.size() > 0) {
			std::string cs;
			auto it = cookies.begin();
			cs += it->first;
			cs += '=';
			cs += it->second.value;
			for (++it; it != cookies.end(); ++it)
			{
				cs += "; ";
				cs += it->first;
				cs += '=';
				cs += it->second.value;
			}
			requestheaders["Cookie"] = std::move(cs);
		}
	}

	// write post_request data
	std::string sendmessagebody;
	if(post_request)
	{
		bool use_multipart = false;
		auto params = postparameters.clone();
		for(auto&&it : params)
			if(!it.filename.empty()) {
				use_multipart = true;
				break;
			}
		
		std::string content_type;
		if(use_multipart)
		{
			std::random_device dev;
			std::minstd_rand mr(dev());
			std::uniform_int_distribution<int> rand{'a', 'z'};

			std::string boundary;
			bool needle_found;
			do {
				needle_found = false;
				boundary.assign(25, '-');
				for(unsigned i = 0; i < 15; ++i) {
					boundary.push_back(static_cast<char>(rand(mr)));
				}
				for(auto&&it : params) {
					if(it.name.find(boundary) != std::string::npos
						|| it.filename.find(boundary) != std::string::npos
						|| it.value.find(boundary) != std::string::npos) {
						needle_found = true;
						break;
					}
				}
			} while (needle_found);
			
			// generate 'multipart' message body
			for(auto&&it : params) {
				sendmessagebody += "--";
				sendmessagebody += boundary;
				sendmessagebody += "\r\nContent-Disposition: form-data; name=\"";
				sendmessagebody += it.name;
				if(!it.filename.empty()) {
					sendmessagebody += "\"; filename=\"";
					sendmessagebody += it.filename;
				}
				sendmessagebody += "\"\r\n\r\n";
				sendmessagebody += it.value;
				sendmessagebody += "\r\n";
			}
			sendmessagebody += "--";
			sendmessagebody += boundary;
			sendmessagebody += "--\r\n";
			
			// set the content type
			content_type = "multipart/form-data; boundary=";
			content_type += boundary;
			
		} else {
			bool nfirst = false;
			// generate 'normal' message body
			for(auto&&it : params) {
				if(nfirst) sendmessagebody += '&';
				else nfirst = true;
				sendmessagebody += it.name;
				sendmessagebody += '=';
				sendmessagebody += url_encode(it.value);
			}
			
			// set the content type
			content_type = "application/x-www-form-urlencoded";	
		}
		
		char lenstring[21];
		snprintf(lenstring, 21, "%zu", sendmessagebody.size());
		requestheaders.emplace("Content-Type", content_type, true);
		requestheaders.emplace("Content-Length", lenstring, true);
	}
	
	// write headers
	auto fetch = [](auto&&v) { return static_cast<std::string_view>(v); };
	for(auto&&it : *requestheaders) {
		mSock << std::visit(fetch, it.first) << ": " << std::visit(fetch, it.second) << "\r\n";
	}
	mSock << "\r\n";
	if (sendmessagebody.length() > 0)
		mSock << sendmessagebody;
	
	// connect
#ifdef VIOLET_SOCKET_USE_OPENSSL
	mSock.connect(static_cast<std::string>(host).c_str(), port, ctx);
#else
	mSock.connect(static_cast<std::string>(host).c_str(), port);
#endif
	mState = State::wait_statuscode;
}

bool HttpRequest::update() {
	
	if(mState == State::notconnected || mState == State::closed || mState == State::error) {
		return false;
	}
	
	mSock.update_read();
	mSock.update_write();
	
	if(mState == State::wait_statuscode) {
		auto &data = mSock.get_read_data();
		for(size_t linelen = 0; linelen + 1 < data.size(); ++linelen) {
			if(data[linelen] == '\r' && data[linelen + 1] == '\n') {
				
				// skip http version
				size_t i;
				for(i = 0; i < linelen; ++i) {
					if(data[i] == ' ') break;
				}
				++i;
				
				// read status code
				size_t j;
				for(j = i; j < linelen; ++j) {
					if(data[j] == ' ') break;
				}
				sscanf(data.data() + i, "%u", &statuscode);
				mSock.dump_data(linelen + 2);
				mState = State::wait_headers;
				break;
			}
		}
	}
	
	if(mState == State::wait_headers) {
		const std::string_view data{ mSock.get_read_data() };
		size_t headerslen;
		for(headerslen = 0; headerslen + 3 < data.size(); ++headerslen) {
			if(data[headerslen] == '\r' && data[headerslen + 1] == '\n'
				&& data[headerslen + 2] == '\r' && data[headerslen + 3] == '\n') {
				
				size_t i = 0;
				while(i < headerslen) {
					
					// find end of line
					size_t j;
					for(j = i; j + 1 < headerslen; ++j) {
						if(data[j] == '\r' && data[j + 1] == '\n') {
							if(j + 2 >= headerslen) break;
							if(data[j + 2] != ' ' && data[j + 2] != '\t') break;
						}
					}
					if(j + 1 >= headerslen) {
						j = headerslen;
					}
					
					// find colon
					size_t k;
					for(k = i; k < j; ++k) {
						if(data[k] == ':') break;
					}
					
					// read name
					std::string_view hname = data.substr(i, k - i);
					std::string hval;
					++k;
					
					// read value (and replace whitespace with one space character)
					{
						size_t s = 0;
						bool whitespace = true;
						for(size_t l = k; l < j; ++l) {
							if(std::isspace(static_cast<unsigned char>(data[l])) != 0) {
								if(!whitespace) {
									++s;
								}
								whitespace = true;
							} else {
								++s;
								whitespace = false;
							}
						}
						hval.resize(s, '\0');
					}
					{
						size_t s = 0;
						bool whitespace = true;
						for(size_t l = k; l < j; ++l) {
							if(std::isspace(static_cast<unsigned char>(data[l])) != 0) {
								if(!whitespace) {
									hval[s++] = ' ';
								}
								whitespace = true;
							} else {
								hval[s++] = data[l];
								whitespace = false;
							}
						}
					}
					if (caseinsensitive_equal<char>(hname, "Set-Cookie"))
						parse_cookies(std::move(hval));
					else if (caseinsensitive_equal<char>(hname ,"Content-Encoding")) {
						if (hval == "gzip")
							encoding = Encoding::Gzip;
						else if (hval == "deflate")
							encoding = Encoding::Deflate;
						else {
							mState = State::error;
							break;
						}
					}
					else if (caseinsensitive_equal<char>(hname ,"Transfer-Encoding")) {
						if (hval == "chunked")
							transfer_chunked = true;
					}
					else responseheaders.emplace(hname, std::move(hval), true);
					
					// skip \r\n characters
					i = j + 2;
					
				}
				
				mSock.dump_data(headerslen + 4);
				mState = State::wait_messagebody;
				return true;
			}
		}
	}

	if(mState == State::wait_messagebody) {
		std::string_view data{ mSock.get_read_data() };
		if (transfer_chunked) {
			size_t d = 0;
			while (d < data.length()) {
				size_t j = d;
				for (; j < data.length() && std::isxdigit(static_cast<unsigned char>(data[j])) != 0; ++j);
				if (size_t val; d != j && j + 2 < data.length() && data[j] == '\r' && data[j + 1] == '\n' && sscanf(&data[d], "%zx", &val) == 1) {
					j += 2;
					if (val == 0) {
						mState = State::finished;
						d = data.length();
					}
					else if (j + val + 2 < data.length()) {
						response.append(&data[j], val);
						d = j + val + 2;
					}
					else break;
				}
				else {
					mState = State::error;
					return false;
				}
			}
			mSock.dump_data(d);
		}
		else {
			response.append(data.data(), data.length());
			mSock.dump_data(data.length());
			mState = State::finished;
		}
		return true;
	}
	
	switch(mSock.get_state()) {
		case State::closed: {
			mSock.to_be_closed();
			if(mState == State::finished) {
				mState = State::closed;
				switch (encoding) {
					case Encoding::Deflate:
						response = internal::zlibf(response, false, false);
						break;
					case Encoding::Gzip:
						response = internal::zlibf(response, false, true);
						break;
				}
			} else {
				mState = State::error;
			}
            return false;
		}
		case State::error: {
			mState = State::error;
            return false;
		}
        default:
            return true;
	}
}

size_t HttpRequest::get_message_body_length() const {
	if(mState != State::wait_messagebody && mState != State::closed) {
		return 0;
	}
	return response.length();//mSock.GetReadDataLength();
}
#endif

#ifdef WIN32
#undef close
#include "strptimex.h"
#endif