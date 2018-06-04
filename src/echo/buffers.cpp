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

#include "buffers.hpp"

#ifdef YYY_USE_STRITZ_ENCODING
void Buffer::spritz_algoritm(const char* key, unsigned keylen = 256)
{
	struct SpritzState {
		uint8_t s[256];
		uint8_t a, i, j, k, w, z;
		
		void initialize_state() {
			for (unsigned v = 0; v < 256; ++v)
				s[v] = static_cast<uint8_t>(v);
			a = 0;
			i = 0;
			j = 0;
			k = 0;
			w = 1;
			z = 0;
		};
		void update() {
			uint8_t t, y;
			i += w;
			y = j + s[i];
			j = k + s[y];
			k = i + k + s[j];
			t = s[i];
			s[i] = s[j];
			s[j] = t;
		};
		uint8_t output() {
			const uint8_t y1 = z + k;
			const uint8_t x1 = i + s[y1];
			const uint8_t y2 = j + s[x1];
			z = s[y2];
			return z;
		};
		void crush() {
			for (uint8_t v = 0, x1, x2, y; v < 128; ++v) {
				y = 255u - v;
				x1 = s[v];
				x2 = s[y];
				if (x1 > x2) {
					s[v] = x2;
					s[y] = x1;
				}
				else {
					s[v] = x1;
					s[y] = x2;
				}
			}
		};
		void whip() {
			for (unsigned v = 0; v < 512; ++v)
				update();
			w += 2;
		};
		void shuffle() {
			whip();
			crush();
			whip();
			crush();
			whip();
			a = 0;
		};
		void absorb_stop() {
			if (a == 128)
				shuffle();
			a;
		};
		void absorb_nibble(const uint8_t x) {
			uint8_t t, y;
			if (a == 128)
				shuffle();
			y = 128 + x;
			t = s[a];
			s[a] = s[y];
			s[y] = t;
			++a;
		};
		void absorb_byte(const uint8_t b) { absorb_nibble(LOW(b)); absorb_nibble(HIGH(b)); };
		void absorb(const uint8_t * msg, const size_t length) { for (size_t v = 0; v < length; ++v) absorb_byte(msg[v]); };
		uint8_t drip() { if (a > 0) shuffle(); update(); return output(); };
		void squeeze(uint8_t * out, const size_t outlen) { if (a > 0) shuffle(); for (size_t v = 0; v < outlen; ++v) out[v] = drip(); }; // Not used.
		void key_setup(const uint8_t * key, const size_t keylen) { initialize_state(); absorb(key, keylen); };
	};

	////// START ///////
	SpritzState st;
	st.key_setup(reinterpret_cast<const uint8_t *>(key), keylen);
	st.absorb_stop();
	st.absorb(reinterpret_cast<const uint8_t *>(key), keylen);
	for (size_t v = 0; v < length; ++v)
		data[v] ^= st.drip(); // I'm not sure and I don't care anymore
	/*if (decode)
		for (size_t v = 0; v < length; ++v)
			data[v] = data[v] - drip(st);
	else
		for (size_t v = 0; v < length; ++v)
			data[v] = data[v] + drip(st);*/
}
#endif

inline char Base64EncodeChar(unsigned n) {
	if(n < 26)
		return 'A' + n;
	if(n < 52)
		return 'a' + n - 26;
	if(n < 62)
		return '0' + n - 52;
	if(n < 63)
		return '+';
	return '/';
}

inline unsigned Base64DecodeChar(char c) {
	if('A' <= c && c <= 'Z')
		return c - 'A';
	if('a' <= c && c <= 'z')
		return c - 'a' + 26;
	if('0' <= c && c <= '9')
		return c - '0' + 52;
	if(c == '+' || c == '-')
		return 62;
	if(c == '/' || c == '_')
		return 63;
	if(c == '=')
		return 64; // special value: padding character
	return 65; // special value: other character
}

std::string Violet::internal::base64_encode(const std::string_view &str) {
	std::string out;
	size_t i;
	for(i = 0; i + 3 <= str.size(); i += 3) {
		unsigned num = (static_cast<unsigned char>(str[i]) << 16) | (static_cast<unsigned char>(str[i + 1]) << 8) | static_cast<unsigned char>(str[i + 2]);
		char outchars[4] = {
			Base64EncodeChar((num >> 18) & 63), Base64EncodeChar((num >> 12) & 63),
			Base64EncodeChar((num >> 6) & 63), Base64EncodeChar(num & 63),
		};
		if(i % 48 == 0 && i != 0)
			out.append("\r\n", 2);
		out.append(outchars, 4);
	}
	if(i < str.size()) {
		if(str.size() - i == 1) {
			unsigned num = static_cast<unsigned char>(str[i + 1]) << 16;
			char outchars[4] = {
				Base64EncodeChar((num >> 18) & 63), Base64EncodeChar((num >> 12) & 63),
				'=', '=',
			};
			if(i % 48 == 0 && i != 0)
				out.append("\r\n", 2);
			out.append(outchars, 4);
		} else {
			unsigned num = (static_cast<unsigned char>(str[i]) << 16) | (static_cast<unsigned char>(str[i + 1]) << 8);
			char outchars[4] = {
				Base64EncodeChar((num >> 18) & 63), Base64EncodeChar((num >> 12) & 63),
				Base64EncodeChar((num >> 6) & 63), '=',
			};
			if(i % 48 == 0 && i != 0)
				out.append("\r\n", 2);
			out.append(outchars, 4);
		}
	}
	return out;
}

std::string Violet::internal::base64_decode(const std::string_view &str) {
	std::string out;
	unsigned num = 0;
	int chars_read = 0, padding_read = 0;
	for(auto c : str) {
		unsigned n = Base64DecodeChar(c);
		if(n < 64) {
			num = (num << 6) | n;
			++chars_read;
		} else if(n == 64) { // padding character
			num = num << 6;
			++chars_read;
			++padding_read;
		} else { // skip other characters
			continue;
		}
		if(chars_read == 4) {
			char outchars[3] = { static_cast<char>(num >> 16), static_cast<char>(num >> 8), static_cast<char>(num) };
			if(padding_read == 0)
				out.append(outchars, 3);
			else if(padding_read == 1)
				out.append(outchars, 2);
			else if(padding_read == 2)
				out.append(outchars, 1);
			num = 0;
			chars_read = 0;
			padding_read = 0;
		}
	}
	if(chars_read != 0) {
		// simulate extra padding
		while(chars_read != 4) {
			num = num << 6;
			++chars_read;
			++padding_read;
		}
		char outchars[3] = { static_cast<char>(num >> 16), static_cast<char>(num >> 8), static_cast<char>(num) };
		if(padding_read == 0)
			out.append(outchars, 3);
		else if(padding_read == 1)
			out.append(outchars, 2);
		else if(padding_read == 2)
			out.append(outchars, 1);
	}
	return out;
}
