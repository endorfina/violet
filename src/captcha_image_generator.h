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
#include <future>

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
    struct Point2 {
        T x, y;
        Point2() = default;
        Point2(T a, T b) : x(a), y(b) {}

        constexpr Point2 &operator+=(const Point2 & other) {
			x += other.x;
			y += other.y;
			return *this;
		}

		constexpr Point2 & operator-=(const Point2 & other) {
			x -= other.x;
			y -= other.y;
			return *this;
		}

        constexpr Point2 &operator*=(const Point2 & other) {
			x *= other.x;
			y *= other.y;
			return *this;
		}

		constexpr Point2 & operator/=(const Point2 & other) {
			x /= other.x;
			y /= other.y;
			return *this;
		}

		constexpr friend Point2 operator*(Point2 first, const Point2 & second) { return first *= second; }

		constexpr friend Point2 operator/(Point2 first, const Point2 & second) { return first /= second; }

		constexpr friend Point2 operator+(Point2 first, const Point2 & second) { return first += second; }

		constexpr friend Point2 operator-(Point2 first, const Point2 & second) { return first -= second; }

		constexpr friend Point2 operator*(Point2 first, T second) { return first *= second; }

		constexpr friend Point2 operator/(Point2 first, T second) { return first /= second; }

        constexpr Point2& operator*=(T f) {
            x *= f;
            y *= f;
            return *this;
        }
        constexpr Point2& operator/=(T f) {
            x /= f;
            y /= f;
            return *this;
        }

		constexpr inline T product(const Point2 & other) { return x * other.x + y * other.y; }

		// Distance
		friend float operator^(const Point2 & first, const Point2 & second) {
			return sqrtf(sqr(first.x - second.x) + sqr(first.y - second.y));
		}
    };
	
	float DistancePtLine(const Point2<float>& a, const Point2<float>& b, const Point2<float>& p)
	{
		Point2<float> n = b - a,
			pa = a - p,
			//c = n * ((pa % n) / (n % n)),
			d = pa - (n *= (pa.product(n) / n.product(n)));
		return sqrtf(d.product(d));
	}
	float SqDistancePtLineSegment(const Point2<float>& a, const Point2<float>& b, const Point2<float>& p)
	{
		Point2<float> n = b - a,
			pa = a - p;
	 
		const float c = n.product(pa);
	 
		// Closest point is a
		if ( c > 0.0f )
			return pa.product(pa);
	 
		Point2<float> bp = p - b;
	 
		// Closest point is b
		if (n.product(bp) > 0.0f )
			return bp.product(bp);
	 
		// Closest point is between a and b
		Point2<float> e = pa - (n *= (c / n.product(n)));
	 
		return e.product(e);
	}

	struct Init {
		const std::string seed;

	private:
		template<typename Int>
		static inline char random_char(Int i) { // Int from 0 to 61 = 10 + 2 * 26 + 1
			return static_cast<char>(i + (i < 10 ? '0' : (i < 36 ? 55 : 61)));
		}

		static std::string generate(void) {
			std::random_device gen;
			std::minstd_rand rdev{ gen() };
			std::uniform_int_distribution<int> d(0, 61);
			std::string str;
			str.resize(9);
			for (auto &it : str)
				it = random_char(d(rdev));
			return str;
		}
	
	public:
		MasterSocket::CaptchaImageType Imt;

		Init(void) : seed{generate()} {}
		Init(std::string _S) : seed{std::move(_S)} {}

		void Kickstart(void) {
			std::seed_seq ssq(seed.begin(), seed.end());
			std::minstd_rand rdev{ ssq };

			std::uniform_int_distribution<unsigned> d1(0, 3), d2(1, 8);
			for (auto &it : Imt.Collection)
				it = { static_cast<uint8_t>(d1(rdev)), static_cast<uint8_t>(d2(rdev)) };
			Imt.Collection[0].first = 0;

			std::uniform_int_distribution<int> d3(0, 61);
			for (auto &it : Imt.PicFilename)
				it = random_char(d3(rdev));
			Imt.PicFilename.back() = 0x0;
		}
	};
	
	struct LineSegment {
		const std::array<Point2<float>, 2> p;
		const float d, n;
		
		LineSegment(const Point2<float> &p1, const Point2<float> &p2)
			: p({ p1, p2 }), d(p1 ^ p2), n(std::max(fabsf(p1.x - p2.x), fabsf(p1.y - p2.y) )) {}
	};
	
	struct Figure {
		std::vector<Point2<float>> v;
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
					throw;
			}
		}

		static void create_glyphs(std::map<char, Figure> &g) {
				Figure w;
				int i;
				for (i = 0; i < 6; ++i)
					w.v.emplace_back(.5f + cosf(i * F_TAU / 6) * .4f, .5f + sinf(i * F_TAU / 6) * .4f);
				g.emplace('0', w);
				w.v.clear();
				w.v.emplace_back(.55f, .15f);
				w.v.emplace_back(.45f, .85f);
				g.emplace('1', w);
				w.v.clear();
				for (i = 0; i < 4; ++i)
					w.v.emplace_back(.5f - cosf(i * F_TAU / 6) * .33f, .45f - sinf(i * F_TAU / 6) * .33f);
				w.v.emplace_back(.1f, .75f);
				w.v.emplace_back(.8f, .9f);
				g.emplace('2', w);
				w.v.clear();
				for (i = 0; i < 4; ++i)
					w.v.emplace_back(.5f - cosf(i * F_TAU / 6) * .4f, .25f - sinf(i * F_TAU / 6) * .2f);
				for (i = 0; i < 4; ++i)
					w.v.emplace_back(.5f + sinf(i * F_TAU / 6) * .4f, .75f - cosf(i * F_TAU / 6) * .2f);
				g.emplace('3', w);
				w.v.clear();
				for (i = 3; i >= 0; --i)
					w.v.emplace_back(.5f - cosf(i * F_TAU / 10) * .4f, .6f - sinf(i * F_TAU / 10) * .5f);
				w.v.emplace_back(.9f, .7f);
				w.v.emplace_back(.6f, .4f);
				w.v.emplace_back(.5f, .9f);
				g.emplace('4', w);
				w.v.clear();
				w.v.emplace_back(.65f, .2f);
				w.v.emplace_back(.3f, .1f);
				for (i = 1; i < 6; ++i)
					w.v.emplace_back(.5f - cosf(i * F_TAU / 6) * .4f, .65f - sinf(i * F_TAU / 6) * .25f);
				g.emplace('5', w);
				w.v.clear();
				for (i = 0; i < 3; ++i)
					w.v.emplace_back(.5f - sinf(i * F_TAU / 6) * .4f, .5f - cosf(i * F_TAU / 6) * .4f);
				for (i = 0; i < 4; ++i)
					w.v.emplace_back(.5f + sinf(i * F_TAU / 6) * .4f, .7f + cosf(i * F_TAU / 6) * .2f);
				g.emplace('6', w);
				w.v.clear();
				w.v.emplace_back(.1f, .1f);
				w.v.emplace_back(.9f, .35f);
				w.v.emplace_back(.2f, .9f);
				g.emplace('7', w);
				w.v.clear();
				for (i = 0; i < 3; ++i)
					w.v.emplace_back(.5f - cosf(i * F_TAU / 6) * .4f, .3f - sinf(i * F_TAU / 6) * .2f);
				for (i = 0; i < 4; ++i)
					w.v.emplace_back(.5f - cosf(i * F_TAU / 6) * .3f, .66f + sinf(i * F_TAU / 6) * .2f);
				w.v.emplace_back(.5f -.4f, .3f);
				g.emplace('8', w);
				w.v.clear();
				for (i = 0; i < 3; ++i)
					w.v.emplace_back(.5f + sinf(i * F_TAU / 6) * .4f, .5f + cosf(i * F_TAU / 6) * .4f);
				for (i = 0; i < 4; ++i)
					w.v.emplace_back(.5f - sinf(i * F_TAU / 6) * .4f, .25f + cosf(i * F_TAU / 6) * .15f);
				g.emplace('9', w);
				w.v.clear();
				w.is_strip = false;
				w.v.emplace_back(.2f, .45f);
				w.v.emplace_back(.8f, .55f);
				w.v.emplace_back(.5f, .3f);
				w.v.emplace_back(.45f, .75f);
				g.emplace('+', w);
				w.v.clear();
				w.v.emplace_back(.15f, .55f);
				w.v.emplace_back(.85f, .48f);
				g.emplace('-', w);
				w.v.clear();
				for (i = 0; i < 3; ++i) {
					w.v.emplace_back(.5f - cosf(i * F_TAU / 6 + .2f) * .3f, .5f - sinf(i * F_TAU / 6 + .2f) * .3f);
					w.v.emplace_back(.5f + cosf(i * F_TAU / 6 + .2f) * .3f, .5f + sinf(i * F_TAU / 6 + .2f) * .3f);
				}
				g.emplace('*', w);
				for (auto&&it : g)
					it.second.v.shrink_to_fit();
		}
		
		void f(const collection_t &col) {
			const int width = 140, height = 30,
				partwidth = width / 7;
			static std::map<char, Figure> glyphs;
			if (glyphs.size() == 0) 
				create_glyphs(glyphs);
			std::array<char, 7> chrs = {
				static_cast<char>(col[0].second + '0'),
				g(col[1].first),
				static_cast<char>(col[1].second + '0'),
				g(col[2].first),
				static_cast<char>(col[2].second + '0'),
				g(col[3].first),
				static_cast<char>(col[3].second + '0')
				};
			unsigned i = 0;
			for (auto &it : data)
				it = i++ % 4 == 3 ? rand() % 165 + 10 : rand() % 230 + 26;
			// for (int y = 0; y < height; ++y)
			// 	for (int x = 0; x < width; ++x)
			// 		if(auto d = std::min({abs(x), abs(y), abs(y - height), abs(x - height)}); d < 5)
			// 			data[4 * (y * width + x) + 3] *= static_cast<float>(d + 1) / 6.2f;
			for (unsigned n = 0; n < 7; ++n)
				if (auto ff = glyphs.find(chrs[n]); ff != glyphs.end()) {
					const auto &fig = ff->second;
					for (int y = 0; y < height; ++y)
						for (int x = 0; x < partwidth; ++x) {
							Point2<float> p(static_cast<float>(x) / static_cast<float>(partwidth), static_cast<float>(y) / static_cast<float>(height));
							float d = 10.f;
							if (fig.v.size() > 1) {
								int b = static_cast<int>(fig.v.size()) - 1;
								if (fig.is_strip)
									do d = std::min(SqDistancePtLineSegment(fig.v[b], fig.v[b - 1], p), d);
									while(--b > 0);
								else do d = std::min(SqDistancePtLineSegment(fig.v[b], fig.v[b - 1], p), d);
									while((b -= 2) > 0);
							}
							if (d < .015f) // .04
								data[4 * (y * width + n * partwidth + x) + 3] = static_cast<uint8_t>((.16f - sqrtf(d)) * (25.5f / .16f)) * (rand() % 5 + 6);
						}
				}
		}

	public:
		static Violet::UniBuffer proc(const collection_t col) {
			Captcha::Image im;
			Violet::UniBuffer b;
			unsigned char * buff;
			size_t bsize;
			im.f(col);
			lodepng_encode32(&buff, &bsize, im.data.data(), 140, 30);
			b.read_from_mem(buff, bsize);
			free(buff);
			return b;
		}
	};
}