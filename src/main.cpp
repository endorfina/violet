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
#include "app_lifetime.h"
#include <bitset>

//#define THROW_PAIR(a, b) (throw std::make_pair<const std::string, const std::string>((a), (b))) 

// int handle_arguments(Application &app, const int argc, char **argv)
// {
// 	try {
// 		bool occur_d = false;
// 		std::bitset<256U> occurs;
// 		for (unsigned i = 1; i < argc; ++i) {
// 			std::string arg{ argv[i] };
// 			occurs.reset();
// 			if (arg.length() < 2 || arg[0] != '-')
// 			{
// 				THROW_PAIR("Proper usage: %s [-ARGUMENT]\n", argv[0]);
// 			}
// 			if (arg[1] == '-')
// 			{
// 				if (strcmp(arg.c_str() + 2, "daemon") == 0) {
// 					if (occur_d)
// 						throw "-d or --daemon used more than once.";
// 					occur_d = app.daemon = true;
// 				}
// 				else if (strcmp(arg.c_str() + 2, "ssl") == 0) {
// 					if (occurs.test('s'))
// 						throw 's';
// 					occurs.set('s', true);
// 				}
// 			}
// 			else for (auto it = arg.begin() + 1; it != arg.end(); ++it)
// 			{
// 				if (occurs.test(*it))
// 					throw *it;
// 				else if (*it == 'd') {
// 					if (occur_d)
// 						throw "-d or --daemon used more than once.";
// 					occur_d = app.daemon = true;
// 				}
// 				else occurs.set(*it, true);
// 			}
			
// 			/// Do the things!
// 			if (occurs.test('p')) {
// 				uint16_t port;
// 				if (++i >= argc || sscanf(argv[i], "%hu", &port) != 1)
// 					throw "Argument p requires port number\n-p <val>";
// 				app.stack.emplace_back(port);
// 				occurs.set('p', false);
// 			}
// 			if (occurs.test('s')) {
// 				if (app.stack.empty())
// 					throw "-p must be used first";
// 				app.stack.back().ssl = true;
// 				occurs.set('s', false);
// 			}
// 			if (occurs.test('l')) {
// 				if (app.stack.empty())
// 					throw "-p must be used first";
// 				if (i + 2 >= argc)
// 					throw "Argument l requires two folder names\n-l <access dir> <meta dir>";
// 				app.stack.back().dir = argv[++i];
// 				app.stack.back().dir_meta = argv[++i];
// 				occurs.set('l', false);
// 			}
			
// 			// Unknown functions
// 			if (occurs.count() > 0)
// 			{
// 				arg.clear();
// 				for (unsigned n = 0; n < occurs.size(); ++n)
// 					if (occurs.test(n))
// 						arg += static_cast<char>(n);
// 				THROW_PAIR("Unknown arguments used: %s", arg.c_str());
// 			}
// 		}
// 		// argv iteration end.
// 	}
// 	catch (const char e) {
// 		printf("EXIT_FAILURE\n");
// 		printf("-%c was used more than once in the same server declaration.", e);
// 		return EXIT_FAILURE;
// 	}
// 	catch (const char * estr) {
// 		printf("EXIT_FAILURE\n");
// 		printf(estr);
// 		return EXIT_FAILURE;
// 	}
// 	catch (std::pair<const std::string, const std::string> estr) {
// 		printf("EXIT_FAILURE\n");
// 		printf(estr.first.c_str(), estr.second.c_str());
// 		return EXIT_FAILURE;
// 	}
// 	return EXIT_SUCCESS;
// }

//#undef THROW_PAIR

int main(int argc, char *argv[])
{
	Application app;
	try {
		app.CheckConfigFile(argc > 1 && !!strlen(argv[1]) ? argv[1] : "violet.conf");
	}
	catch (...) {
		printf("EXIT_FAILURE\n");
		printf("Wrong configuration file.\n");
		return EXIT_FAILURE;
	}
	if (app.stack.empty()) {
		printf("No servers were declared.\n");
		return EXIT_FAILURE;
	}
	
	return ApplicationLifetime(app);
}