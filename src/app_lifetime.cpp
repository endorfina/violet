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

#include "app_lifetime.h"
#include "protocol.hpp"

#ifdef VIOLET_SOCKET_USE_OPENSSL	/// hmm... should probably switch to GNUTLS instead
void init_openssl()
{ 
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl()
{
    EVP_cleanup();
}

SSL_CTX *create_context()
{
    const SSL_METHOD * method = SSLv23_server_method(); //TLSv1_2_server_method();
    SSL_CTX * ctx = SSL_CTX_new(method);
	
    if (!ctx) {
		perror("Unable to create SSL context");
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
    }

    return ctx;
}

void configure_context(SSL_CTX *ctx)
{
	SSL_CTX_set_ecdh_auto(ctx, 1);
	
	/* Set the key and cert */
	if (SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) < 0)
	{
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}
	if (SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) < 0 )
	{
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}
	//OpenSSL_add_all_algorithms();
	//int rc = SSL_CTX_set_cipher_list(ctx, "ALL:!ECDH");
	//assert(0 != rc);
}
#endif

#include <csignal>
#include <thread>

std::vector<std::pair<const Application::Server&, Violet::ListeningSocket>> ls;
std::vector<std::thread> thread_stack;
bool killswitch = false;

void ConsoleHandlerRoutine(int sig)
{
	killswitch = true;
	for (auto &l : ls)
		l.second.stop();
	for (auto &t : thread_stack)
		t.join();
	ls.clear();
	thread_stack.clear();
#ifdef VIOLET_SOCKET_USE_OPENSSL
    cleanup_openssl();
#endif
	Protocol::SaveLogHeapBuffer();
	puts("\nShutting down correctly.");
	std::exit(EXIT_SUCCESS);
}

template<class Map>
void ComposeMIMEs(Map & ct, const char * fn) {
	Violet::UniBuffer f;
	f.read_from_file(fn);
	std::string keyword;
	while (!f.is_at_end()) {
		size_t p1 = f.get_pos(), p2, len = f.length();
		char * c = reinterpret_cast<char*>(f.data());
		for (p2 = p1; p2 < len; ++p2)
			if (c[p2] == 0x20 || c[p2] == 0x0d)
				break;
		std::string s{ c + p1, c + p2 };

		// I guess, why not???
		s.shrink_to_fit();

		if (c[p2] == 0x20) {
			keyword = std::move(s);
			f.set_pos(p2 + 1);
		}
		else {
			ct.emplace(std::move(keyword), std::move(s));
			f.set_pos(p2 + 2);
		}
	}
}

void RoutineA(std::pair<const Application::Server&, Violet::ListeningSocket> &l
#ifdef VIOLET_SOCKET_USE_OPENSSL
, SSL_CTX * const ctx
#endif
) {
	std::list<Protocol> http;
	Protocol::Shared shared_registry {
		l.first.dir.size() ? l.first.dir.c_str() : nullptr,
		l.first.dir_meta.size() ? l.first.dir_meta.c_str() : nullptr,
		l.first.copyright
	};
	unsigned int tick = 0;
	bool block = true;

	while (!killswitch) {
		time_t now;
		if (block) {
			http.emplace_back(l.second.block_once(
#ifdef VIOLET_SOCKET_USE_OPENSSL
				l.first.ssl ? ctx : nullptr
#endif
				), shared_registry);
#ifdef MONITOR_SOCKETS
			auto &h = http.back();
			printf("> New connection [id:%lu, s:%i, p:%hu]\n", h.id, h.s.s, l.first.port);
#endif
			block = false;
		}
		for (unsigned n = 0; l.second.acceptable() && n < 32; ++n) {
			http.emplace_back(l.second.accept(
#ifdef VIOLET_SOCKET_USE_OPENSSL
				l.first.ssl ? ctx : nullptr
#endif
				), shared_registry);
#ifdef MONITOR_SOCKETS
			auto &h = http.back();
			printf("> New connection [id:%lu, s:%i, p:%hu]\n", h.id, h.s.s, l.first.port);
#endif
		}

		if (!!http.size())
		{
			for (auto &it : http)
				it.HandleRequest();
			now = time(nullptr);
			for (auto e = http.begin(); e != http.end();)
				if (e->sent || !e->s.is_functional() || (now - e->last_used > 25))
					e = http.erase(e);
				else
					++e;
		}
		else {
			block = true;
		}

		if (++tick > 500)
		{
			tick = 0;
			if (auto amt = shared_registry.sessions.size(); !!amt || !!shared_registry.captcha_sig.size())
			{
				now = time(nullptr);
				for (auto it = shared_registry.sessions.begin(); it != shared_registry.sessions.end();)
					if (now - it->second.last_activity > 1000)
						it = shared_registry.sessions.erase(it);
					else
						++it;
				if (amt > shared_registry.sessions.size())
				{
					char str[64];
					snprintf(str, 64, " > %zu unused session(s) have been closed.", amt - shared_registry.sessions.size());
					puts(str);
					std::lock_guard<std::mutex> lock(Protocol::logging.first);
					Protocol::logging.second << str << '\n';
				}
				for (auto it = shared_registry.captcha_sig.begin(); it != shared_registry.captcha_sig.end();)
					if (now - it->second > 20)
						it = shared_registry.captcha_sig.erase(it);
					else
						++it;
			}
		}
	}
}

int ApplicationLifetime(const Application &app)
{
#ifdef VIOLET_SOCKET_USE_OPENSSL
	SSL_CTX * ctx = nullptr;

	for (auto& s : app.stack)
		if (s.ssl)
		{
			init_openssl();
			ctx = create_context();
			configure_context(ctx);
			break;
		}
#endif
	(Protocol::dir_work = "werk/").shrink_to_fit();

	for (auto& s : app.stack) {
		ls.emplace_back(s, Violet::ListeningSocket());
	}
	for (auto &l : ls) {
		l.second.start(false, l.first.port, false);
		for (int tries = 30; !l.second.is_listening(); --tries) {
			sleep(1);
			l.second.start(false, l.first.port, false);
		}
		if (l.second.is_listening())
		{
			printf("Listening on port %hu\n", l.first.port);
			l.second.use_safety_header(false);
		}
		else
		{
			printf("Port %hu is jammed!\n", l.first.port);
			break;
		}
	}
	
	if (ls.empty()) {
		ConsoleHandlerRoutine(0);
		return 0;
	}
	
	for (auto &l : ls)
		if (!l.second.is_listening())
		{
			puts("One or more sockets aren't working.");
			ConsoleHandlerRoutine(0);
			return 0;
		}
	
	puts("HTTP is ready!");
	if constexpr (sizeof(time_t) < 8) {
		printf("WARNING: time_t is only %zu bits long\n", sizeof(time_t) * 8);
	}
	
	if (app.daemon) {
		pid_t pid, sid;
		pid = fork();
		if (pid < 0)
			return EXIT_FAILURE;
		if (pid > 0) {
			puts("Daemon created.");
			return EXIT_SUCCESS;
		}
		umask(0);
		sid = setsid();
		if (sid < 0)
			return EXIT_FAILURE;
		// if (chdir("/") < 0)
		// 	return EXIT_FAILURE;
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
	}

	std::signal(SIGINT, ConsoleHandlerRoutine);
	std::signal(SIGTERM, ConsoleHandlerRoutine);
	
	ComposeMIMEs(Protocol::content_types, "content_types.txt");

	if (ls.size() == 1) { // no need for threads if there's just one.
#ifdef VIOLET_SOCKET_USE_OPENSSL
		RoutineA(ls[0], ctx);
#else
		RoutineA(ls[0]);
#endif
	}
	else {
		for (auto &l : ls)
#ifdef VIOLET_SOCKET_USE_OPENSSL
			thread_stack.emplace_back(RoutineA, std::ref(l), ctx);
#else
			thread_stack.emplace_back(RoutineA, std::ref(l));
#endif

		for (auto &t : thread_stack)
			t.join();
	}
	ls.clear();
	thread_stack.clear();
#ifdef VIOLET_SOCKET_USE_OPENSSL
	SSL_CTX_free(ctx);
    cleanup_openssl();
#endif
	Protocol::SaveLogHeapBuffer();
	return 0;
}

template<typename A>
inline bool is_punct(const A c) {
	return !! std::ispunct(static_cast<unsigned char>(c));
}

template<class Str>
void print_tags(std::FILE* stream, const std::vector<Str> &v) {
	bool append = false;
	for (auto &&it : v) {
		if (append) {
			//std::putc(',', stream);
			std::putc(' ', stream);
		}
		else append = true;

		std::putc('\"', stream);
		for (auto c : it) {
			std::putc(c, stream);
		}
		std::putc('\"', stream);
	}
}

template<class Str>
Str & remove_last_char(Str & str, const typename Str::value_type c) {
	if (!!str.length() && str.back() == c) {
		if constexpr (Violet::is_view<Str>::value) {
			str.remove_suffix(1);
		}
		else {
			str.resize(str.size() - 1);
		}
	}
	return str;
}

void Application::CheckConfigFile(const char *filename) {
	std::string __src;
	if (Violet::internal::read_from_file(filename, __src)) {
		std::string_view src{ __src };
		while (!!src.length())
		{
			Violet::remove_prefix_whitespace(src);
			if (src.empty())
				break;
			std::string_view l{ src.substr(0, src.find('\n')) };
			
			src.remove_prefix(l.length());

			if (l.empty() || l.front() == '#')
				continue;
			Violet::remove_suffix_whitespace(l);

			std::vector<std::string_view> tags;
			while(!!l.length() && l.front() != '#') {
				size_t len;
				const bool is_name = !is_punct(l.front());
				if (!is_name && (l.front() == '\'' || l.front() == '\"')) {
					len = l.find(l.front(), 1);
					if (len > 0)
						tags.emplace_back(l.substr(1, len - 1));
					l.remove_prefix(std::min(len + 1, l.length()));
					Violet::remove_prefix_whitespace(l);
				}
				else {
					for(len = 1; len < l.length() && (is_punct(l[len]) ^ is_name); ++len);
					tags.emplace_back(l.substr(0, len));

					l.remove_prefix(len);

					if (is_name)
						Violet::remove_suffix_whitespace(tags.back());
					else
						Violet::remove_prefix_whitespace(l);
				}
			}

			if (!tags.size())	// just in case I forgot something
				break;

			if (tags[0] == "@") {
				uint16_t port;
				if (tags.size() == 2 && !!std::isdigit(tags[1].front()) && sscanf(tags[1].data(), "%hu", &port) == 1)
					stack.emplace_back(port);
				else {
					printf("Wrong server declaration: ");
					print_tags(stdout, tags);
					puts("\nCorrect syntax is\n@ <port_number>");
					std::exit(EXIT_FAILURE);
				}
				continue;
			}
			
			if (stack.empty()) {
				return;
			}
			if ((tags.size() != 3 && (tags.size() != 4 || !is_punct(tags[3].front()))) || tags[1] != "=") {
				puts("Configuration error at:");
				print_tags(stdout, tags);
				puts("\nCorrect syntax is\n<variable_name> = <string_value>[;]");
				std::exit(EXIT_FAILURE);
			}

			if (tags[0] == "AccessDir") {
				stack.back().dir.assign(remove_last_char(tags[2], '/'));
			}
			else if (tags[0] == "MetaDir") {
				stack.back().dir_meta.assign(remove_last_char(tags[2], '/'));
			}
			else if (tags[0] == "CopyrightNotice") {
				stack.back().copyright.assign(tags[2]);
			}
			else if (tags[0] == "SSL") {
#ifndef VIOLET_SOCKET_USE_OPENSSL
				puts("WARNING: Application has been built without SSL support");
				printf("Server on port number %hu will be unsecure\n", stack.back().port);
#else
				std::string val { static_cast<std::string>(tags[2]) };
				std::transform(val.begin(), val.end(), val.begin(), ::tolower);
				if (val == "true" || val == "1" || val == "on")
		 			stack.back().ssl = true;
		 		else if (val == "false" || val == "0" || val == "off")
		 			stack.back().ssl = false;
				else {
					puts("ERROR: Value assigned to \'SSL\' must be a boolean");
					std::exit(EXIT_FAILURE);
				}
#endif
			}
		}
		printf("Sockets declared: %zu\n", stack.size());
	}
}