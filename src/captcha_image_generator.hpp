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

namespace Captcha
{
	typedef std::array<uint8_t, 140 * 30 * 4> image_t;
	typedef std::array<std::pair<uint8_t, uint8_t>, 4> collection_t;

	template<typename T>
	constexpr inline auto sqr(T a) {
		return a * a;
	}
	
	template<typename T,
		typename = std::enable_if_t<std::is_scalar_v<T>>>
    struct point2 {
        T x, y;
        point2() = default;
        point2(T a, T b) : x(a), y(b) {}

        constexpr point2 &operator+=(const point2 & other) {
			x += other.x;
			y += other.y;
			return *this;
		}

		constexpr point2 & operator-=(const point2 & other) {
			x -= other.x;
			y -= other.y;
			return *this;
		}

        constexpr point2 &operator*=(const point2 & other) {
			x *= other.x;
			y *= other.y;
			return *this;
		}

		constexpr point2 & operator/=(const point2 & other) {
			x /= other.x;
			y /= other.y;
			return *this;
		}

		constexpr friend point2 operator*(point2 first, const point2 & second) { return first *= second; }

		constexpr friend point2 operator/(point2 first, const point2 & second) { return first /= second; }

		constexpr friend point2 operator+(point2 first, const point2 & second) { return first += second; }

		constexpr friend point2 operator-(point2 first, const point2 & second) { return first -= second; }

		constexpr friend point2 operator*(point2 first, T second) { return first *= second; }

		constexpr friend point2 operator/(point2 first, T second) { return first /= second; }

        constexpr point2& operator*=(T f) {
            x *= f;
            y *= f;
            return *this;
        }
        constexpr point2& operator/=(T f) {
            x /= f;
            y /= f;
            return *this;
        }

		constexpr inline T product(const point2 & other) { return x * other.x + y * other.y; }

		// Distance
		friend float operator^(const point2 & first, const point2 & second) {
			return sqrtf(sqr(first.x - second.x) + sqr(first.y - second.y));
		}
    };
	
	template<typename T>
	float DistancePtLine(const point2<T>& a, const point2<T>& b, const point2<T>& p)
	{
		point2<T> n = b - a,
			pa = a - p,
			//c = n * ((pa % n) / (n % n)),
			d = pa - (n *= (pa.product(n) / n.product(n)));
		return sqrtf(static_cast<float>(d.product(d)));
	}

	template<typename T>
	auto SqDistancePtLineSegment(const point2<T>& a, const point2<T>& b, const point2<T>& p)
	{
		point2<T> n = b - a,
			pa = a - p;
	 
		const auto c = n.product(pa);
	 
		// Closest point is a
		if ( c > 0.0f )
			return pa.product(pa);
	 
		point2<T> bp = p - b;
	 
		// Closest point is b
		if (n.product(bp) > 0.0f )
			return bp.product(bp);
	 
		// Closest point is between a and b
		point2<T> e = pa - (n *= (c / n.product(n)));
	 
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
	
		static std::list<std::pair<Signature, time_t>> registry;
	};

	struct Init {
		const std::string seed;

	private:
		template<typename Int>
		static inline char random_char(Int i) { // Int from 0 to 61 = 10 + 2 * 26 + 1
			return static_cast<char>(i + (i < 10 ? '0' : (i < 36 ? 55 : 61)));
		}

		static std::string generate(void);
	
	public:
		Signature Imt;

		Init(void) : seed{generate()} {}
		Init(std::string _S) : seed{std::move(_S)} {}

		void Kickstart(bool gen_filename);
	};
	
	struct line_segment {
		const std::array<point2<float>, 2> p;
		const float d, n;
		
		line_segment(const point2<float> &p1, const point2<float> &p2)
			: p({ p1, p2 }), d(p1 ^ p2), n(std::max(fabsf(p1.x - p2.x), fabsf(p1.y - p2.y) )) {}
	};
	
	struct figure {
		std::vector<point2<float>> v;
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