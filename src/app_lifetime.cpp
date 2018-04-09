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
#include "master_socket.h"

void init_openssl()	/// hmm... should probably switch to GNUTLS instead
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
    cleanup_openssl();
	MasterSocket::SaveLogHeapBuffer();
	puts("\nShutting down correctly.");
	std::exit(EXIT_SUCCESS);
}

void ComposeMIMEs(std::vector<StringPair> & ct, const char * fn) {
	Violet::UniBuffer f;
	f.read_from_file(fn);
	while (!f.is_at_end()) {
		size_t p1 = f.get_pos(), p2, len = f.length();
		char * c = reinterpret_cast<char*>(f.data());
		for (p2 = p1; p2 < len; ++p2)
			if (c[p2] == 0x20 || c[p2] == 0x0d)
				break;
		std::string s(c + p1, c + p2);
		if (c[p2] == 0x20)
		{
			ct.emplace_back();
			ct.back().first = std::move(s);
			f.set_pos(p2 + 1);
		}
		else
		{
			ct.back().second = std::move(s);
			f.set_pos(p2 + 2);
		}
	}
	ct.shrink_to_fit();
}

void RoutineA(std::pair<const Application::Server&, Violet::ListeningSocket> &l, SSL_CTX * const ctx) {
	std::list<MasterSocket> http;
	unsigned int tick = 0;
	bool block = true;

	while (!killswitch) {
		time_t now;
		if (block) {
			http.emplace_back(l.second.block_once(l.first.ssl ? ctx : nullptr),
				l.first.dir.c_str(),
				l.first.dir_meta.size() ? l.first.dir_meta.c_str() : nullptr
				);
#ifdef MONITOR_SOCKETS
			auto &h = http.back();
			printf("> New connection [id:%lu, s:%i, p:%hu]\n", h.id, h.s.s, l.first.port);
#endif
			block = false;
		}
		for (unsigned n = 0; l.second.acceptable() && n < 32; ++n) {
			http.emplace_back(l.second.accept(l.first.ssl ? ctx : nullptr),
				l.first.dir.c_str(),
				l.first.dir_meta.size() ? l.first.dir_meta.c_str() : nullptr
				);
#ifdef MONITOR_SOCKETS
			auto &h = http.back();
			printf("> New connection [id:%lu, s:%i, p:%hu]\n", h.id, h.s.s, l.first.port);
#endif
		}

		if (http.size() > 0)
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
			if (auto amt = MasterSocket::sessions.size(); amt > 0 || MasterSocket::active_captchas.size())
			{
				now = time(nullptr);
				for (auto it = MasterSocket::sessions.begin(); it != MasterSocket::sessions.end();)
					if (now - it->second.last_activity > 1000)
						it = MasterSocket::sessions.erase(it);
					else
						++it;
				if (amt > MasterSocket::sessions.size())
				{
					char str[64];
					snprintf(str, 64, " > %zu unused session(s) have been closed.\n", amt - MasterSocket::sessions.size());
					MasterSocket::log_heap.write<std::string_view>(str);
					printf(str);
				}
				for (auto it = MasterSocket::active_captchas.begin(); it != MasterSocket::active_captchas.end();)
					if (now - it->second > 20)
						it = MasterSocket::active_captchas.erase(it);
					else
						++it;
			}
		}
	}
}

int ApplicationLifetime(const Application &app)
{
	SSL_CTX * ctx = nullptr;
	(MasterSocket::dir_work = "werk/").shrink_to_fit();

	puts("Starting ~");
	
	for (auto& s : app.stack)
		if (s.ssl)
		{
			init_openssl();
			ctx = create_context();
			configure_context(ctx);
			break;
		}

	for (auto& s : app.stack) {
		ls.emplace_back(s, Violet::ListeningSocket());
	}
	for (auto &l : ls) {
		l.second.start(false, l.first.port, false);
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
			printf("One or more sockets aren't working.");
			ConsoleHandlerRoutine(0);
			return 0;
		}
	

	printf("HTTP ready to go! [time_t = %zu]\n", sizeof(time_t) * 8);
	
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
	
	ComposeMIMEs(MasterSocket::content_types, "content_types.txt");

	if (ls.size() == 1) {
		RoutineA(ls[0], ctx); // no need for threads if there's just one.
	}
	else {
		for (auto &l : ls)
			thread_stack.emplace_back(RoutineA, std::ref(l), ctx);

		for (auto &t : thread_stack)
			t.join();
	}
	ls.clear();
	thread_stack.clear();
	SSL_CTX_free(ctx);
    cleanup_openssl();
	MasterSocket::SaveLogHeapBuffer();
	return 0;
}

void Application::CheckConfigFile(const char *filename) {
	Violet::UniBuffer src;
	if (src.read_from_file(filename)) {
		while (!src.is_at_end())
		{
			std::string_view l{ src.get_string_current() };
			while (l.size() && !!std::isspace(static_cast<unsigned char>(l.front())))
				l.remove_prefix(1);
			if (!l.size())
				break;
			l = l.substr(0, l.find('\n'));
			src.set_pos(src.get_pos() + l.length());
			while (l.size() && !!std::isspace(static_cast<unsigned char>(l.back())))
				l.remove_suffix(1);
			if (l.empty() || l.front() == '#')
				continue;
			size_t p1 = 0;
			Violet::skip_whitespaces(l, p1);
			if (l[p1] == '@') {
				++p1;
				Violet::skip_whitespaces(l, p1);
				uint16_t port;
				if (sscanf(l.data() + p1, "%hu", &port) != 1)
					throw;
				stack.emplace_back(port);
			}
			else {
				size_t p2 = p1;
				for (; p2 < l.size() && l[p2] != 0x20 && l[p2] != '='; ++p2);
				std::string name{ l.substr(p1, p2) };
				p1 = p2;
				Violet::skip_whitespaces(l, p1);
				if (l[p1++] != '=' || stack.empty())
					continue;
				Violet::skip_whitespaces(l, p1);
				std::string val{ l.substr(p1) };
				if (val.empty() || val.front() == '#')
					continue;
				if (name == "SSL")
				{
					std::transform(val.begin(), val.end(), val.begin(), ::tolower);
					if (val == "true" || val == "1")
						stack.back().ssl = true;
					else if (val == "false" || val == "0")
						stack.back().ssl = false;
					else throw;
				}
				else if (name == "AccessDir")
				{
					stack.back().dir = val;
				}
				else if (name == "MetaDir")
				{
					stack.back().dir_meta = val;
				}
			}
		}
		printf("Configuration file parsed successfully.\n");
		printf("Raw servers declared: %zu\n", stack.size());
	}
}