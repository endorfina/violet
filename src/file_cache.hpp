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
#include <memory>
#include <string_view>
#include "echo/buffers.hpp"

class cached_file
{
public:
	using value_type = char;
	using view_t = std::basic_string_view<value_type>;
public:
	using size_type = std::size_t;

private:
	std::unique_ptr<value_type[]> data;
	size_type size = 0;

public:
	void clear() {
		data.reset(nullptr);
		size = 0;
	}
	inline void resize(size_t newlength) {
		data.reset(new value_type[newlength]); // no memmove, at least for now
		size = bool(data) ? newlength : 0;
	}

	inline bool read_from_file(const char* filename) { return Violet::internal::read_from_file(filename, *this); }

	inline view_t get_string() const { return { data.get(), size }; }
	inline const value_type* data() const { return data.get(); }
	inline value_type* data() { return data.get(); }
	inline value_type& operator[](size_type _pos) { return data[_pos]; };
	inline const value_type& operator[](size_type _pos) const { return data[_pos]; };
	explicit inline operator value_type *() { return data.get(); }
	inline operator view_t() { return { data.get(), size }; }
	inline size_type length() const { return size; }
	inline size_type size() const { return size; }
	
};