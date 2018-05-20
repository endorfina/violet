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

#ifndef F_TAU
#define	F_TAU		6.28318530717958647692f	/* tau */
#endif

#include "echo/buffers.hpp"
#include "echo/math.hpp"

namespace Captcha
{
	typedef std::array<uint8_t, 140 * 30 * 4> image_t;
	typedef std::array<std::pair<uint8_t, uint8_t>, 4> collection_t;

	template<typename T>
	constexpr inline auto sqr(T a) {
		return a * a;
	}
	
	template<typename T>
	float DistancePtLine(const Violet::point2<T>& a, const Violet::point2<T>& b, const Violet::point2<T>& p)
	{
		Violet::point2<T> n = b - a,
			pa = a - p,
			//c = n * ((pa % n) / (n % n)),
			d = pa - (n *= (pa.product(n) / n.product(n)));
		return sqrtf(static_cast<float>(d.product(d)));
	}

	template<typename T>
	auto SqDistancePtLineSegment(const Violet::point2<T>& a, const Violet::point2<T>& b, const Violet::point2<T>& p)
	{
		Violet::point2<T> n = b - a,
			pa = a - p;
	 
		const auto c = n.product(pa);
	 
		// Closest point is a
		if ( c > 0.0f )
			return pa.product(pa);
	 
		Violet::point2<T> bp = p - b;
	 
		// Closest point is b
		if (n.product(bp) > 0.0f )
			return bp.product(bp);
	 
		// Closest point is between a and b
		Violet::point2<T> e = pa - (n *= (c / n.product(n)));
	 
		return e.product(e);
	}

	struct compatibility_memory_keeper {
		unsigned char * ptr = nullptr;
		size_t size;

		compatibility_memory_keeper() = default;
		compatibility_memory_keeper(const compatibility_memory_keeper &) = delete;
		compatibility_memory_keeper(compatibility_memory_keeper &&other)
			: ptr(other.ptr), size(other.size) {
			other.ptr = nullptr;
		}
		compatibility_memory_keeper(unsigned char * _p, size_t _s)
			: ptr(_p), size(_s) {}
		~compatibility_memory_keeper() {
			if (ptr)
				free(ptr);
		}
	};
	
	struct Signature {
		typedef std::pair<uint8_t, uint8_t> collectible_type;
		std::array<collectible_type, 4> Collection;
		std::string PicFilename;
		std::future<compatibility_memory_keeper> Data;
	};

	struct Init {
		const std::string seed;
		Signature Imt;

	private:
		static inline std::string generate(void) {
			std::string s;
			s.resize(13);
			Violet::generate_random_string(s.data(), s.size());
			return s;
		}

	public:

		Init(void) : seed{generate()} {}
		Init(std::string _S) : seed{std::move(_S)} {}

		void set_up(bool gen_filename);
	};
	
	struct line_segment {
		const std::array<Violet::point2<float>, 2> p;
		const float d, n;
		
		line_segment(const Violet::point2<float> &p1, const Violet::point2<float> &p2)
			: p({ p1, p2 }), d(p1 ^ p2), n(std::max(fabsf(p1.x - p2.x), fabsf(p1.y - p2.y) )) {}
	};
	
	struct figure {
		std::vector<Violet::point2<float>> v;
		bool is_strip = true;
	};

	class Image {
		image_t data;
		
		static inline char g(uint8_t i) {
			switch (i) {
				case 0:
					return '+';
				case 1:
					return '-';
				case 2:
					return '*';
				default:
					return 0x0;
			}
		}

		static void create_glyphs(std::map<char, figure> &g);
		
		void f(const collection_t &col);

	public:
		static compatibility_memory_keeper process(const collection_t col);

		Image() = default;
		Image(const collection_t &col) { f(col); }
	};
}