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

//#pragma once

// C RunTime Header Files
#include <cstdio>
#include <cstdlib>

#include <unistd.h>
#include <cmath>
#include <cstdio>
#include <memory>
#include <chrono>
#include <ctime>
#include <type_traits>
#include <list>
#include <locale.h>
#include <vector>
#include <array>
#include <map>
#include <unordered_map>
#include <atomic>
#include <algorithm>
#include <exception>
#include <string>
#include <future>
#include <string_view>
#include <optional>
#include <random>

#include <sys/stat.h>

//#define _min(a,b) ((a) < (b) ? (a) : (b))
//#define _max(a,b) ((a) < (b) ? (b) : (a))

//#define CreateDir(x) mkdir((x), 0750);

#ifdef VIOLET_SOCKET_USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif