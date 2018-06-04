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

#include "hash.hpp"

inline unsigned int roll_left(unsigned int x, unsigned int n) {
	return (x << n) | (x >> (32 - n));
}
inline unsigned int reverse_endianness_32(unsigned int a) {
	return (a >> 24) | (a << 24) | ((a & 0xff00) << 8) | ((a & 0xff0000) >> 8);
}

inline void md5_ff(unsigned int& a, unsigned int b, unsigned int c, unsigned int d, unsigned int x, unsigned int s, unsigned int t) {
	a += ((b & c) | ((~b) & d)) + x + t;
	a = roll_left(a, s) + b;
}
inline void md5_gg(unsigned int& a, unsigned int b, unsigned int c, unsigned int d, unsigned int x, unsigned int s, unsigned int t) {
	a += ((b & d) | (c & (~d))) + x + t;
	a = roll_left(a, s) + b;
}
inline void md5_hh(unsigned int& a, unsigned int b, unsigned int c, unsigned int d, unsigned int x, unsigned int s, unsigned int t) {
	a += (b ^ c ^ d) + x + t;
	a = roll_left(a, s) + b;
}
inline void md5_ii(unsigned int& a, unsigned int b, unsigned int c, unsigned int d, unsigned int x, unsigned int s, unsigned int t) {
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
void Violet::MD5::operator()(hash_block& block) {

	unsigned int a = result.w[0];
	unsigned int b = result.w[1];
	unsigned int c = result.w[2];
	unsigned int d = result.w[3];

	md5_ff(a, b, c, d, block.w[0], 7, 0xd76aa478);
	md5_ff(d, a, b, c, block.w[1], 12, 0xe8c7b756);
	md5_ff(c, d, a, b, block.w[2], 17, 0x242070db);
	md5_ff(b, c, d, a, block.w[3], 22, 0xc1bdceee);
	md5_ff(a, b, c, d, block.w[4], 7, 0xf57c0faf);
	md5_ff(d, a, b, c, block.w[5], 12, 0x4787c62a);
	md5_ff(c, d, a, b, block.w[6], 17, 0xa8304613);
	md5_ff(b, c, d, a, block.w[7], 22, 0xfd469501);
	md5_ff(a, b, c, d, block.w[8], 7, 0x698098d8);
	md5_ff(d, a, b, c, block.w[9], 12, 0x8b44f7af);
	md5_ff(c, d, a, b, block.w[10], 17, 0xffff5bb1);
	md5_ff(b, c, d, a, block.w[11], 22, 0x895cd7be);
	md5_ff(a, b, c, d, block.w[12], 7, 0x6b901122);
	md5_ff(d, a, b, c, block.w[13], 12, 0xfd987193);
	md5_ff(c, d, a, b, block.w[14], 17, 0xa679438e);
	md5_ff(b, c, d, a, block.w[15], 22, 0x49b40821);

	md5_gg(a, b, c, d, block.w[1], 5, 0xf61e2562);
	md5_gg(d, a, b, c, block.w[6], 9, 0xc040b340);
	md5_gg(c, d, a, b, block.w[11], 14, 0x265e5a51);
	md5_gg(b, c, d, a, block.w[0], 20, 0xe9b6c7aa);
	md5_gg(a, b, c, d, block.w[5], 5, 0xd62f105d);
	md5_gg(d, a, b, c, block.w[10], 9, 0x02441453);
	md5_gg(c, d, a, b, block.w[15], 14, 0xd8a1e681);
	md5_gg(b, c, d, a, block.w[4], 20, 0xe7d3fbc8);
	md5_gg(a, b, c, d, block.w[9], 5, 0x21e1cde6);
	md5_gg(d, a, b, c, block.w[14], 9, 0xc33707d6);
	md5_gg(c, d, a, b, block.w[3], 14, 0xf4d50d87);
	md5_gg(b, c, d, a, block.w[8], 20, 0x455a14ed);
	md5_gg(a, b, c, d, block.w[13], 5, 0xa9e3e905);
	md5_gg(d, a, b, c, block.w[2], 9, 0xfcefa3f8);
	md5_gg(c, d, a, b, block.w[7], 14, 0x676f02d9);
	md5_gg(b, c, d, a, block.w[12], 20, 0x8d2a4c8a);

	md5_hh(a, b, c, d, block.w[5], 4, 0xfffa3942);
	md5_hh(d, a, b, c, block.w[8], 11, 0x8771f681);
	md5_hh(c, d, a, b, block.w[11], 16, 0x6d9d6122);
	md5_hh(b, c, d, a, block.w[14], 23, 0xfde5380c);
	md5_hh(a, b, c, d, block.w[1], 4, 0xa4beea44);
	md5_hh(d, a, b, c, block.w[4], 11, 0x4bdecfa9);
	md5_hh(c, d, a, b, block.w[7], 16, 0xf6bb4b60);
	md5_hh(b, c, d, a, block.w[10], 23, 0xbebfbc70);
	md5_hh(a, b, c, d, block.w[13], 4, 0x289b7ec6);
	md5_hh(d, a, b, c, block.w[0], 11, 0xeaa127fa);
	md5_hh(c, d, a, b, block.w[3], 16, 0xd4ef3085);
	md5_hh(b, c, d, a, block.w[6], 23, 0x04881d05);
	md5_hh(a, b, c, d, block.w[9], 4, 0xd9d4d039);
	md5_hh(d, a, b, c, block.w[12], 11, 0xe6db99e5);
	md5_hh(c, d, a, b, block.w[15], 16, 0x1fa27cf8);
	md5_hh(b, c, d, a, block.w[2], 23, 0xc4ac5665);

	md5_ii(a, b, c, d, block.w[0], 6, 0xf4292244);
	md5_ii(d, a, b, c, block.w[7], 10, 0x432aff97);
	md5_ii(c, d, a, b, block.w[14], 15, 0xab9423a7);
	md5_ii(b, c, d, a, block.w[5], 21, 0xfc93a039);
	md5_ii(a, b, c, d, block.w[12], 6, 0x655b59c3);
	md5_ii(d, a, b, c, block.w[3], 10, 0x8f0ccc92);
	md5_ii(c, d, a, b, block.w[10], 15, 0xffeff47d);
	md5_ii(b, c, d, a, block.w[1], 21, 0x85845dd1);
	md5_ii(a, b, c, d, block.w[8], 6, 0x6fa87e4f);
	md5_ii(d, a, b, c, block.w[15], 10, 0xfe2ce6e0);
	md5_ii(c, d, a, b, block.w[6], 15, 0xa3014314);
	md5_ii(b, c, d, a, block.w[13], 21, 0x4e0811a1);
	md5_ii(a, b, c, d, block.w[4], 6, 0xf7537e82);
	md5_ii(d, a, b, c, block.w[11], 10, 0xbd3af235);
	md5_ii(c, d, a, b, block.w[2], 15, 0x2ad7d2bb);
	md5_ii(b, c, d, a, block.w[9], 21, 0xeb86d391);

	result.w[0] += a;
	result.w[1] += b;
	result.w[2] += c;
	result.w[3] += d;
}

Violet::MD5::MD5() {
	blocks = 0;
	md5_init(result.w);
}

void Violet::MD5::finish(hash_block& block, const unsigned int used) {
	block.x[used] = 0x80;
	if (64 - used < 8 + 1) {
		memset(block.x + used + 1, 0, 64 - used - 1);
		this->operator()(block);
		memset(block.x, 0, 64 - 8);
	}
	else {
		memset(block.x + used + 1, 0, 64 - used - 1 - 8);
	}
	block.w[14] = (blocks << 9) | (used << 3);
	block.w[15] = blocks >> (32 - 9);
	this->operator()(block);
	memset(block.x, 0, 64);
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
void Violet::SHA1::operator()(hash_block& block) {

	unsigned int w[80];
	for (unsigned int t = 0; t < 16; ++t) {
		w[t] = reverse_endianness_32(block.w[t]);
	}
	for (unsigned int t = 16; t < 80; ++t) {
		w[t] = roll_left(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
	}

	unsigned int a = result.w[0];
	unsigned int b = result.w[1];
	unsigned int c = result.w[2];
	unsigned int d = result.w[3];
	unsigned int e = result.w[4];

	for (unsigned int t = 0; t < 20; ++t) {
		unsigned int temp = roll_left(a, 5) + ((b & c) | ((~b) & d)) + e + w[t] + 0x5a827999;
		e = d;
		d = c;
		c = roll_left(b, 30);
		b = a;
		a = temp;
	}
	for (unsigned int t = 20; t < 40; t++) {
		unsigned int temp = roll_left(a, 5) + (b ^ c ^ d) + e + w[t] + 0x6ed9eba1;
		e = d;
		d = c;
		c = roll_left(b, 30);
		b = a;
		a = temp;
	}
	for (unsigned int t = 40; t < 60; t++) {
		unsigned int temp = roll_left(a, 5) + ((b & c) | (b & d) | (c & d)) + e + w[t] + 0x8f1bbcdc;
		e = d;
		d = c;
		c = roll_left(b, 30);
		b = a;
		a = temp;
	}
	for (unsigned int t = 60; t < 80; t++) {
		unsigned int temp = roll_left(a, 5) + (b ^ c ^ d) + e + w[t] + 0xca62c1d6;
		e = d;
		d = c;
		c = roll_left(b, 30);
		b = a;
		a = temp;
	}

	result.w[0] += a;
	result.w[1] += b;
	result.w[2] += c;
	result.w[3] += d;
	result.w[4] += e;
}

Violet::SHA1::SHA1() {
	blocks = 0;
	sha1_init(result.w);
}

void Violet::SHA1::finish(hash_block& block, const unsigned int used) {
	block.x[used] = 0x80;
	if (64 - used < 8 + 1) {
		memset(block.x + used + 1, 0, 64 - used - 1);
		this->operator()(block);
		memset(block.x, 0, 64 - 8);
	}
	else {
		memset(block.x + used + 1, 0, 64 - used - 1 - 8);
	}
	block.w[14] = reverse_endianness_32(blocks >> (32 - 9));
	block.w[15] = reverse_endianness_32((blocks << 9) | (used << 3));
	this->operator()(block);
	memset(block.x, 0, 64);
	result.w[0] = reverse_endianness_32(result.w[0]);
	result.w[1] = reverse_endianness_32(result.w[1]);
	result.w[2] = reverse_endianness_32(result.w[2]);
	result.w[3] = reverse_endianness_32(result.w[3]);
	result.w[4] = reverse_endianness_32(result.w[4]);
}

inline unsigned int roll_right(unsigned int x, unsigned int n) {
	return (x >> n) | (x << (32 - n));
}

#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

inline unsigned int sha256_ep0(unsigned int x) {
	return roll_right(x, 2) ^ roll_right(x, 13) ^ roll_right(x, 22);
}
inline unsigned int sha256_ep1(unsigned int x) {
	return roll_right(x, 6) ^ roll_right(x, 11) ^ roll_right(x, 25);
}
inline unsigned int sha256_sig0(unsigned int x) {
	return roll_right(x, 7) ^ roll_right(x, 18) ^ (x >> 3);
}
inline unsigned int sha256_sig1(unsigned int x) {
	return roll_right(x, 17) ^ roll_right(x, 19) ^ (x >> 10);
}

void Violet::SHA256::operator()(hash_block& block)
{
	static const unsigned int k[64] = {
		0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
		0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
		0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
		0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
		0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
		0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
		0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
		0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
	};
	unsigned int a, b, c, d, e, f, g, h, w[64];

	for (unsigned int t = 0; t < 16; ++t) {
		w[t] = reverse_endianness_32(block.w[t]);
	}
	for (unsigned int i = 16; i < 64; ++i)
		w[i] = sha256_sig1(w[i - 2]) + w[i - 7] + sha256_sig0(w[i - 15]) + w[i - 16];

	a = result.w[0];
	b = result.w[1];
	c = result.w[2];
	d = result.w[3];
	e = result.w[4];
	f = result.w[5];
	g = result.w[6];
	h = result.w[7];

	for (unsigned int t1 = 0, t2 = 0, i = 0; i < 64; ++i) {
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

	result.w[0] += a;
	result.w[1] += b;
	result.w[2] += c;
	result.w[3] += d;
	result.w[4] += e;
	result.w[5] += f;
	result.w[6] += g;
	result.w[7] += h;
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

void Violet::SHA256::operator++() {
	sha256_inc(blocks1, blocks2, 512);
}

Violet::SHA256::SHA256()
{
	blocks2 = blocks1 = 0;
	sha256_init(result.w);
}

void Violet::SHA256::finish(hash_block& block, const unsigned int used)
{
	block.x[used] = 0x80;
	if (64 - used < 8 + 1) {
		memset(block.x + used + 1, 0, 64 - used - 1);
		this->operator()(block);
		memset(block.x, 0, 64 - 8);
	}
	else {
		memset(block.x + used + 1, 0, 64 - used - 1 - 8);
	}
	sha256_inc(blocks1, blocks2, used * 8);
	block.w[14] = reverse_endianness_32(blocks2);
	block.w[15] = reverse_endianness_32(blocks1);
	this->operator()(block);
	memset(block.x, 0, 64);
	for (unsigned int i = 0; i < 8; ++i)
		result.w[i] = reverse_endianness_32(result.w[i]);
}