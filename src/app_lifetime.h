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
#include "pch.h"

#define DIRECTORY_SHARED "shared"

struct Application {

	struct Server {
		uint16_t port;
		bool ssl = false;
		std::string dir, dir_meta, copyright;
		Server(uint16_t _p) : port(_p) {}
	};
	std::list<Server> stack;
	bool daemon = false;
	void CheckConfigFile(const char *filename);
};

int ApplicationLifetime(const Application &app);

typedef std::pair<std::string, std::string> StringPair;
typedef std::pair<const char *, const char *> ConstStringPair;