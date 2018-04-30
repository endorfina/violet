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

int main(int argc, char *argv[])
{
	Application app;
	try {
		app.CheckConfigFile(argc > 1 && !!strlen(argv[1]) ? argv[1] : "violet.conf");
	}
	catch (std::exception &e) {
		//puts("EXIT_FAILURE");
		puts(e.what());
		return EXIT_FAILURE;
	}
	if (app.stack.empty()) {
		puts("No servers were declared.");
		return EXIT_FAILURE;
	}
	
	return ApplicationLifetime(app);
}