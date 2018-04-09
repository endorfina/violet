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
#include "captcha_image_generator.hpp"

#include "lodepng.h"

std::list<std::pair<Captcha::Signature, time_t>> Captcha::Signature::registry{};

std::string Captcha::Init::generate(void) {
	std::random_device gen;
	std::minstd_rand rdev{ gen() };
	std::uniform_int_distribution<int> d(0, 61);
	std::string str;
	str.resize(9);
	for (auto &it : str)
		it = random_char(d(rdev));
	return str;
}
void Captcha::Init::Kickstart(bool gen_filename) {
	std::seed_seq ssq(seed.begin(), seed.end());
	std::minstd_rand rdev{ ssq };

	std::uniform_int_distribution<unsigned> d1(0, 3), d2(1, 8);
	for (auto &it : Imt.Collection)
		it = { static_cast<uint8_t>(d1(rdev)), static_cast<uint8_t>(d2(rdev)) };
	Imt.Collection[0].first = 0;

	if (gen_filename) {
		std::uniform_int_distribution<int> d3(0, 61);
		Imt.PicFilename.resize(32);
		for (auto &it : Imt.PicFilename)
			it = random_char(d3(rdev));
		Imt.PicFilename += ".png";
	}
}

void Captcha::Image::create_glyphs(std::map<char, figure> &g) {
		figure w;
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

void Captcha::Image::f(const collection_t &col) {
	const int width = 140, height = 30,
		partwidth = width / 7;
	static std::map<char, figure> glyphs;
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
	std::random_device dev;
	std::mt19937 mt{dev()};
	{
		unsigned i = 0;
		std::uniform_int_distribution<unsigned short int> alpha{10, 175}, noise{26, 255};
		for (auto &it : data)
			it = i++ % 4 == 3 ? alpha(mt) : noise(mt);
	}
	std::uniform_int_distribution<unsigned short int> final_noise{6, 10};

	for (unsigned n = 0; n < 7; ++n)
		if (auto ff = glyphs.find(chrs[n]); ff != glyphs.end()) {
			const auto &fig = ff->second;
			for (int y = 0; y < height; ++y)
				for (int x = 0; x < partwidth; ++x) {
					point2<float> p(static_cast<float>(x) / static_cast<float>(partwidth), static_cast<float>(y) / static_cast<float>(height));
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
						data[4 * (y * width + n * partwidth + x) + 3] = static_cast<uint8_t>((.16f - sqrtf(d)) * (25.5f / .16f)) * final_noise(mt);
				}
		}
}

Captcha::compatibility_memory_keeper Captcha::Image::process(const collection_t col) {
	Captcha::Image im{ col };
	Captcha::compatibility_memory_keeper ef;
	lodepng_encode32(&ef.ptr, &ef.size, im.data.data(), 140, 30);
	return ef;
}