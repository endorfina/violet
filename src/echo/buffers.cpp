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

using namespace Violet;

void internal::rc4_crypt(unsigned char *out, std::size_t size, const unsigned char *key, std::size_t keysize) {
	std::array<unsigned char, 256> table;
	unsigned char i = 0, j = 0;
	for (auto&&c : table)
		c = i++;
	for (unsigned k = 0; k < 256; ++k) {
		j += table[k] + key[k % keysize];
		std::swap(table[k], table[j]);
	}
	i = 0;
	j = 0;
	for (unsigned k = 0; k < size; ++k) {
		j += table[++i];
		std::swap(table[i], table[j]);
		out[k] ^= table[static_cast<unsigned char>(table[i] + table[j])];
	}
}

#ifdef _USE_STRITZ_ENCODING
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

#ifndef VIOLET_NO_COMPILE_HASHES

inline uint32_t roll_left(uint32_t x, uint32_t n) {
	return (x << n) | (x >> (32 - n));
}
inline uint32_t reverse_endianness_32(uint32_t a) {
	return (a >> 24) | (a << 24) | ((a & 0xff00) << 8) | ((a & 0xff0000) >> 8);
}

inline void md5_ff(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t t) {
	a += ((b & c) | ((~b) & d)) + x + t;
	a = roll_left(a, s) + b;
}
inline void md5_gg(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t t) {
	a += ((b & d) | (c & (~d))) + x + t;
	a = roll_left(a, s) + b;
}
inline void md5_hh(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t t) {
	a += (b ^ c ^ d) + x + t;
	a = roll_left(a, s) + b;
}
inline void md5_ii(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t t) {
	a += (c ^ (b | (~d))) + x + t;
	a = roll_left(a, s) + b;
}

// initialize a md5 hash
void md5_init(unsigned int result[4]) {
	result[0] = 0x67452301;
	result[1] = 0xefcdab89;
	result[2] = 0x98badcfe;
	result[3] = 0x10325476;
}

// add 16 words to the md5 hash
void MD5::do_hash() {

	unsigned int a = m_result.w[0];
	unsigned int b = m_result.w[1];
	unsigned int c = m_result.w[2];
	unsigned int d = m_result.w[3];

	md5_ff(a, b, c, d, m_block.w[0], 7, 0xd76aa478);
	md5_ff(d, a, b, c, m_block.w[1], 12, 0xe8c7b756);
	md5_ff(c, d, a, b, m_block.w[2], 17, 0x242070db);
	md5_ff(b, c, d, a, m_block.w[3], 22, 0xc1bdceee);
	md5_ff(a, b, c, d, m_block.w[4], 7, 0xf57c0faf);
	md5_ff(d, a, b, c, m_block.w[5], 12, 0x4787c62a);
	md5_ff(c, d, a, b, m_block.w[6], 17, 0xa8304613);
	md5_ff(b, c, d, a, m_block.w[7], 22, 0xfd469501);
	md5_ff(a, b, c, d, m_block.w[8], 7, 0x698098d8);
	md5_ff(d, a, b, c, m_block.w[9], 12, 0x8b44f7af);
	md5_ff(c, d, a, b, m_block.w[10], 17, 0xffff5bb1);
	md5_ff(b, c, d, a, m_block.w[11], 22, 0x895cd7be);
	md5_ff(a, b, c, d, m_block.w[12], 7, 0x6b901122);
	md5_ff(d, a, b, c, m_block.w[13], 12, 0xfd987193);
	md5_ff(c, d, a, b, m_block.w[14], 17, 0xa679438e);
	md5_ff(b, c, d, a, m_block.w[15], 22, 0x49b40821);

	md5_gg(a, b, c, d, m_block.w[1], 5, 0xf61e2562);
	md5_gg(d, a, b, c, m_block.w[6], 9, 0xc040b340);
	md5_gg(c, d, a, b, m_block.w[11], 14, 0x265e5a51);
	md5_gg(b, c, d, a, m_block.w[0], 20, 0xe9b6c7aa);
	md5_gg(a, b, c, d, m_block.w[5], 5, 0xd62f105d);
	md5_gg(d, a, b, c, m_block.w[10], 9, 0x02441453);
	md5_gg(c, d, a, b, m_block.w[15], 14, 0xd8a1e681);
	md5_gg(b, c, d, a, m_block.w[4], 20, 0xe7d3fbc8);
	md5_gg(a, b, c, d, m_block.w[9], 5, 0x21e1cde6);
	md5_gg(d, a, b, c, m_block.w[14], 9, 0xc33707d6);
	md5_gg(c, d, a, b, m_block.w[3], 14, 0xf4d50d87);
	md5_gg(b, c, d, a, m_block.w[8], 20, 0x455a14ed);
	md5_gg(a, b, c, d, m_block.w[13], 5, 0xa9e3e905);
	md5_gg(d, a, b, c, m_block.w[2], 9, 0xfcefa3f8);
	md5_gg(c, d, a, b, m_block.w[7], 14, 0x676f02d9);
	md5_gg(b, c, d, a, m_block.w[12], 20, 0x8d2a4c8a);

	md5_hh(a, b, c, d, m_block.w[5], 4, 0xfffa3942);
	md5_hh(d, a, b, c, m_block.w[8], 11, 0x8771f681);
	md5_hh(c, d, a, b, m_block.w[11], 16, 0x6d9d6122);
	md5_hh(b, c, d, a, m_block.w[14], 23, 0xfde5380c);
	md5_hh(a, b, c, d, m_block.w[1], 4, 0xa4beea44);
	md5_hh(d, a, b, c, m_block.w[4], 11, 0x4bdecfa9);
	md5_hh(c, d, a, b, m_block.w[7], 16, 0xf6bb4b60);
	md5_hh(b, c, d, a, m_block.w[10], 23, 0xbebfbc70);
	md5_hh(a, b, c, d, m_block.w[13], 4, 0x289b7ec6);
	md5_hh(d, a, b, c, m_block.w[0], 11, 0xeaa127fa);
	md5_hh(c, d, a, b, m_block.w[3], 16, 0xd4ef3085);
	md5_hh(b, c, d, a, m_block.w[6], 23, 0x04881d05);
	md5_hh(a, b, c, d, m_block.w[9], 4, 0xd9d4d039);
	md5_hh(d, a, b, c, m_block.w[12], 11, 0xe6db99e5);
	md5_hh(c, d, a, b, m_block.w[15], 16, 0x1fa27cf8);
	md5_hh(b, c, d, a, m_block.w[2], 23, 0xc4ac5665);

	md5_ii(a, b, c, d, m_block.w[0], 6, 0xf4292244);
	md5_ii(d, a, b, c, m_block.w[7], 10, 0x432aff97);
	md5_ii(c, d, a, b, m_block.w[14], 15, 0xab9423a7);
	md5_ii(b, c, d, a, m_block.w[5], 21, 0xfc93a039);
	md5_ii(a, b, c, d, m_block.w[12], 6, 0x655b59c3);
	md5_ii(d, a, b, c, m_block.w[3], 10, 0x8f0ccc92);
	md5_ii(c, d, a, b, m_block.w[10], 15, 0xffeff47d);
	md5_ii(b, c, d, a, m_block.w[1], 21, 0x85845dd1);
	md5_ii(a, b, c, d, m_block.w[8], 6, 0x6fa87e4f);
	md5_ii(d, a, b, c, m_block.w[15], 10, 0xfe2ce6e0);
	md5_ii(c, d, a, b, m_block.w[6], 15, 0xa3014314);
	md5_ii(b, c, d, a, m_block.w[13], 21, 0x4e0811a1);
	md5_ii(a, b, c, d, m_block.w[4], 6, 0xf7537e82);
	md5_ii(d, a, b, c, m_block.w[11], 10, 0xbd3af235);
	md5_ii(c, d, a, b, m_block.w[2], 15, 0x2ad7d2bb);
	md5_ii(b, c, d, a, m_block.w[9], 21, 0xeb86d391);

	m_result.w[0] += a;
	m_result.w[1] += b;
	m_result.w[2] += c;
	m_result.w[3] += d;
}

MD5::MD5() {
	blocks = 0;
	used = 0;
	md5_init(m_result.w);
}

void MD5::increment() {
	++blocks;
}

void MD5::finish() {
	m_block.x[used] = 0x80;
	if (64 - used < 8 + 1) {
		memset(m_block.x + used + 1, 0, 64 - used - 1);
		do_hash();
		memset(m_block.x, 0, 64 - 8);
	}
	else {
		memset(m_block.x + used + 1, 0, 64 - used - 1 - 8);
	}
	m_block.w[14] = (blocks << 9) | (used << 3);
	m_block.w[15] = blocks >> (32 - 9);
	do_hash();
	memset(m_block.x, 0, 64);
}

// initialize a sha1 hash
void sha1_init(unsigned int result[5]) {
	result[0] = 0x67452301;
	result[1] = 0xefcdab89;
	result[2] = 0x98badcfe;
	result[3] = 0x10325476;
	result[4] = 0xc3d2e1f0;
}

// add 16 words to the sha1 hash
void SHA1::do_hash() {

	uint32_t w[80];
	for (uint32_t t = 0; t < 16; ++t) {
		w[t] = reverse_endianness_32(m_block.w[t]);
	}
	for (uint32_t t = 16; t < 80; ++t) {
		w[t] = roll_left(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
	}

	uint32_t a = m_result.w[0];
	uint32_t b = m_result.w[1];
	uint32_t c = m_result.w[2];
	uint32_t d = m_result.w[3];
	uint32_t e = m_result.w[4];

	for (uint32_t t = 0; t < 20; ++t) {
		uint32_t temp = roll_left(a, 5) + ((b & c) | ((~b) & d)) + e + w[t] + 0x5a827999;
		e = d;
		d = c;
		c = roll_left(b, 30);
		b = a;
		a = temp;
	}
	for (uint32_t t = 20; t < 40; t++) {
		uint32_t temp = roll_left(a, 5) + (b ^ c ^ d) + e + w[t] + 0x6ed9eba1;
		e = d;
		d = c;
		c = roll_left(b, 30);
		b = a;
		a = temp;
	}
	for (uint32_t t = 40; t < 60; t++) {
		uint32_t temp = roll_left(a, 5) + ((b & c) | (b & d) | (c & d)) + e + w[t] + 0x8f1bbcdc;
		e = d;
		d = c;
		c = roll_left(b, 30);
		b = a;
		a = temp;
	}
	for (uint32_t t = 60; t < 80; t++) {
		uint32_t temp = roll_left(a, 5) + (b ^ c ^ d) + e + w[t] + 0xca62c1d6;
		e = d;
		d = c;
		c = roll_left(b, 30);
		b = a;
		a = temp;
	}

	m_result.w[0] += a;
	m_result.w[1] += b;
	m_result.w[2] += c;
	m_result.w[3] += d;
	m_result.w[4] += e;
}

SHA1::SHA1() {
	blocks = 0;
	used = 0;
	sha1_init(m_result.w);
}

void SHA1::increment() {
	++blocks;
}

void SHA1::finish() {
	m_block.x[used] = 0x80;
	if (64 - used < 8 + 1) {
		memset(m_block.x + used + 1, 0, 64 - used - 1);
		do_hash();
		memset(m_block.x, 0, 64 - 8);
	}
	else {
		memset(m_block.x + used + 1, 0, 64 - used - 1 - 8);
	}
	m_block.w[14] = reverse_endianness_32(blocks >> (32 - 9));
	m_block.w[15] = reverse_endianness_32((blocks << 9) | (used << 3));
	do_hash();
	memset(m_block.x, 0, 64);
	m_result.w[0] = reverse_endianness_32(m_result.w[0]);
	m_result.w[1] = reverse_endianness_32(m_result.w[1]);
	m_result.w[2] = reverse_endianness_32(m_result.w[2]);
	m_result.w[3] = reverse_endianness_32(m_result.w[3]);
	m_result.w[4] = reverse_endianness_32(m_result.w[4]);
}

inline uint32_t roll_right(uint32_t x, uint32_t n) {
	return (x >> n) | (x << (32 - n));
}

#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

inline uint32_t sha256_ep0(uint32_t x) {
	return roll_right(x, 2) ^ roll_right(x, 13) ^ roll_right(x, 22);
}
inline uint32_t sha256_ep1(uint32_t x) {
	return roll_right(x, 6) ^ roll_right(x, 11) ^ roll_right(x, 25);
}
inline uint32_t sha256_sig0(uint32_t x) {
	return roll_right(x, 7) ^ roll_right(x, 18) ^ (x >> 3);
}
inline uint32_t sha256_sig1(uint32_t x) {
	return roll_right(x, 17) ^ roll_right(x, 19) ^ (x >> 10);
}

void SHA256::do_hash()
{
	static const uint32_t k[64] = {
		0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
		0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
		0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
		0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
		0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
		0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
		0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
		0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
	};
	uint32_t a, b, c, d, e, f, g, h, w[64];

	for (uint32_t t = 0; t < 16; ++t) {
		w[t] = reverse_endianness_32(m_block.w[t]);
	}
	for (uint32_t i = 16; i < 64; ++i)
		w[i] = sha256_sig1(w[i - 2]) + w[i - 7] + sha256_sig0(w[i - 15]) + w[i - 16];

	a = m_result.w[0];
	b = m_result.w[1];
	c = m_result.w[2];
	d = m_result.w[3];
	e = m_result.w[4];
	f = m_result.w[5];
	g = m_result.w[6];
	h = m_result.w[7];

	for (uint32_t t1 = 0, t2 = 0, i = 0; i < 64; ++i) {
		t1 = h + sha256_ep1(e) + CH(e, f, g) + k[i] + w[i];
		t2 = sha256_ep0(a) + MAJ(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}

	m_result.w[0] += a;
	m_result.w[1] += b;
	m_result.w[2] += c;
	m_result.w[3] += d;
	m_result.w[4] += e;
	m_result.w[5] += f;
	m_result.w[6] += g;
	m_result.w[7] += h;
}

inline void sha256_init(unsigned int result[8]) {
	result[0] = 0x6a09e667;
	result[1] = 0xbb67ae85;
	result[2] = 0x3c6ef372;
	result[3] = 0xa54ff53a;
	result[4] = 0x510e527f;
	result[5] = 0x9b05688c;
	result[6] = 0x1f83d9ab;
	result[7] = 0x5be0cd19;
}

inline void sha256_inc(unsigned &b1, unsigned &b2, unsigned amt) {
	if (b1 > 0xffffffff - (amt))
		++b2;
	b1 += amt;
}

void SHA256::increment() {
	sha256_inc(blocks1, blocks2, 512);
}

SHA256::SHA256()
{
	blocks2 = blocks1 = used = 0;
	sha256_init(m_result.w);
}

void SHA256::finish()
{
	m_block.x[used] = 0x80;
	if (64 - used < 8 + 1) {
		memset(m_block.x + used + 1, 0, 64 - used - 1);
		do_hash();
		memset(m_block.x, 0, 64 - 8);
	}
	else {
		memset(m_block.x + used + 1, 0, 64 - used - 1 - 8);
	}
	sha256_inc(blocks1, blocks2, used * 8);
	m_block.w[14] = reverse_endianness_32(blocks2);
	m_block.w[15] = reverse_endianness_32(blocks1);
	do_hash();
	memset(m_block.x, 0, 64);
	for (uint32_t i = 0; i < 8; ++i)
		m_result.w[i] = reverse_endianness_32(m_result.w[i]);
}

#endif // no compile hashes

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

std::string internal::base64_encode(const std::string_view &str) {
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

std::string internal::base64_decode(const std::string_view &str) {
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
