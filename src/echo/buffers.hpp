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

//#define _USE_STRITZ_ENCODING
#include <cstdio>
#include <memory.h>  // memset
#include <string>
#include <array>
#include <vector>
#include <string_view>
#include <type_traits>
#include <algorithm>	// tolower

#ifndef VIOLET_NO_COMPILE_COMPRESSION
#include <zlib.h>
#endif

namespace Violet
{
	template<typename A>
	void skip_whitespaces(A *s, const size_t size, size_t &it) {
		for (; it < size && !!s[it] && !!std::isspace(static_cast<unsigned char>(s[it])); ++it);
	}

	template<class S>
	constexpr void skip_whitespaces(S&&s, size_t &it) {
		for (; it < s.size() && !!s[it] && !!std::isspace(static_cast<unsigned char>(s[it])); ++it);
	}

	template<class S>
	constexpr void skip_whitespaces(S &it, const S end) {
		while(it != end && *it <= 0x20)
			++it;
	}

	constexpr int __cis_compare(const std::string_view &lhs, const std::string_view &rhs) {
		const auto __s = std::min(lhs.size(), rhs.size());
		int __ret = 0;
		for (std::remove_const_t<decltype(__s)> i = 0; i < __s; ++i)
			if (::tolower(lhs[i]) < ::tolower(rhs[i])) {
				__ret = -1;
				break;
			}
			else if (::tolower(lhs[i]) > ::tolower(rhs[i])) {
				__ret = 1;
				break;
			}
		if (__ret == 0)
			if (lhs.size() < rhs.size())
			__ret = -1;
			else if (lhs.size() > rhs.size())
			__ret = 1;
		return __ret;
	}

	struct functor_less_comparator {
		bool operator()(const std::string &lhs, const std::string &rhs) const {
			return lhs < rhs;
		}
		bool operator()(const std::string_view &lhs, const std::string_view &rhs) const {
			return lhs < rhs;
		}
		using is_transparent = void;
	};
	struct functor_equal_comparator {
		bool operator()(const std::string &lhs, const std::string &rhs) const {
			return lhs == rhs;
		}
		bool operator()(const std::string_view &lhs, const std::string_view &rhs) const {
			return lhs == rhs;
		}
		using is_transparent = void;
	};
	struct cis_functor_equal_comparator {
		bool operator()(std::string_view lhs, std::string_view rhs) const {
			return __cis_compare(lhs, rhs) == 0;
		}
		using is_transparent = void;
	};
	struct cis_functor_less_comparator {
		bool operator()(std::string_view lhs, std::string_view rhs) const {
			return __cis_compare(lhs, rhs) < 0;
		}
		using is_transparent = void;
	};

	namespace internal
	{
		/* Returns true on success */
		template<class Container>
		bool read_from_file(const char* filename, Container &dest, long pos=0, size_t length=size_t(-1)) {
	#ifdef _WIN32
			FILE *f;
			fopen_s(&f, filename, "rb");
	#else
			FILE *f = fopen(filename, "rb");
	#endif
			if(!f)
				return false;
			try {
				fseek(f, 0, SEEK_END);
				auto size = ftell(f);
				if(pos > size || fseek(f, pos, SEEK_SET) != 0) {
					fclose(f);
					return false;
				}
				if(size == -1) {
					fclose(f);
					return false;
				}
				else if (static_cast<size_t>(size - pos) < length)
					length = static_cast<size_t>(size - pos);
				dest.resize(size);
				if(fread(&dest[0], 1, length, f) != length) {
					dest.clear();
					fclose(f);
					return false;
				}
			}
			catch(...) {
				fclose(f);
				throw;
			}
			fclose(f);
			return true;
		}

		template<class Container>
		void write_to_file(const char* filename, const Container &src, const bool append = false) {
			#ifdef _WIN32
				FILE *f;
				fopen_s(&f, filename, append ? "ab" : "wb");
			#else
				FILE *f = fopen(filename, append ? "ab" : "wb");
			#endif
				if(f != nullptr) {
					fwrite(src.data(), 1, src.size(), f);
					fclose(f);
				}
		}

		/* Random access Iter */
		template<typename Iter>
		inline bool compare(Iter p1, std::size_t size1, Iter p2, std::size_t size2)
		{
			if (size1 != size2)
				return false;
			for (const auto end = p1 + size1; p1 != end; ++p1, ++p2)
				if (*p1 != *p2)
					return false;
			return true;
		}

		template<typename Iter>
		inline bool ciscompare(Iter p1, std::size_t size1, Iter p2, std::size_t size2)
		{
			if (size1 != size2)
				return false;
			for (const auto end = p1 + size1; p1 != end; ++p1, ++p2)
				if (::tolower(*p1) != ::tolower(*p2))
					return false;
			return true;
		}

		void rc4_crypt(unsigned char *out, std::size_t size, const unsigned char *key, std::size_t keysize);

		std::string base64_encode(const std::string_view &str);
		std::string base64_decode(const std::string_view &str);

#ifndef VIOLET_NO_COMPILE_COMPRESSION
		template <class Container>
		bool zlib(const char *in, const size_t in_size, Container &out, const bool compress, const bool gzip)
		{
			size_t blocksize = in_size + 1000;
			z_stream strm;
			out.resize(blocksize);
			strm.zalloc = Z_NULL;
			strm.zfree = Z_NULL;
			strm.opaque = Z_NULL;
			strm.next_in = (z_const Bytef*)(in); // There were issues with z_const, switching to C style cast
			strm.avail_in = static_cast<unsigned int>(in_size);
			strm.next_out = reinterpret_cast<Bytef*>(out.data());
			strm.avail_out = static_cast<unsigned int>(blocksize);

			int ret = compress ?
						(gzip ? deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) : deflateInit(&strm, Z_DEFAULT_COMPRESSION)) :
						(gzip ? inflateInit2(&strm, 15 + 16) : inflateInit(&strm));

			if (ret != Z_OK)
				throw std::bad_alloc();
			try {
				while(true) {
					ret = compress ? deflate(&strm, Z_FINISH) : inflate(&strm, Z_FINISH);
					if(ret == Z_STREAM_ERROR || ret == Z_MEM_ERROR)
						throw std::bad_alloc();
					if(ret == Z_STREAM_END) {
						if (compress)
							deflateEnd(&strm);
						else
							inflateEnd(&strm);
						out.resize(out.size() - strm.avail_out);
						break;
					}
					if(!compress && (ret == Z_NEED_DICT || ret == Z_DATA_ERROR)) {
						inflateEnd(&strm);
						return false;
					}
					if(out.size() > (1 << 30) || blocksize > (1 << 30))
						throw std::bad_alloc();
					blocksize += blocksize / 2;
					out.resize(out.size() + blocksize);
					strm.next_out = reinterpret_cast<Bytef*>(out.data() + out.size() - blocksize);
					strm.avail_out = static_cast<unsigned int>(blocksize);
				}
			} catch(...) {
				if (compress)
					deflateEnd(&strm);
				else
					inflateEnd(&strm);
				throw;
			}
			return true;
		}

		template<class Container>
		auto zlibf(Container&&in, const bool compress, const bool gzip) {
			std::remove_reference_t<Container> out;
			zlib<Container>(in.data(), in.size(), out, compress, gzip);
			return out;
		}
#endif

		namespace utf8x {
			template <typename Char>
			constexpr inline auto scoop(Char c) {
				return static_cast<uint8_t>(c) & 0x3f; // Scoop dat 10xxxxxx, yo
			}

			template <typename Char>
			constexpr inline bool check(Char c) {
				return static_cast<uint8_t>(c) >> 6 == 0x2;
			}

			template <typename CharIt>
			constexpr inline unsigned char sequence_length(CharIt it)
			{
				const auto byte = static_cast<uint8_t>(*it);
				// 0xxxxxxx
				if (byte < 0x80) return 1;
				// 110xxxxx
				else if ((byte >> 5) == 0x6) return 2;
				// 1110xxxx
				else if ((byte >> 4) == 0xe) return 3;
				// 11110xxx
				else if ((byte >> 3) == 0x1e) return 4;
				// 111110xx
				else if ((byte >> 2) == 0x3e) return 5;
				// 1111110x
				else if ((byte >> 1) == 0x7e) return 6;
				return 0; // It ain't no utf8, fren. what do?
			}

			template <typename CharIt>
			constexpr inline uint32_t get_2(CharIt it) // Can be inlined?
			{
				if (check(it[1]))
					return ((static_cast<uint32_t>(*it) & 0x1f) << 6) + scoop(it[1]);
				return 0x0;
			}

			template <typename CharIt>
			constexpr uint32_t get_3(CharIt it)
			{
				uint32_t code_point = static_cast<uint32_t>(*it) & 0xf;
				if (check(it[1]) && check(it[2])) {
					code_point = (code_point << 12) + (scoop(*++it) << 6);
					code_point += scoop(*++it);
				}
				return code_point;
			}

			template <typename CharIt>
			constexpr uint32_t get_4(CharIt it)
			{
				uint32_t code_point = static_cast<uint32_t>(*it) & 0x7;
				if (check(it[1]) && check(it[2]) && check(it[3])) {
					code_point = (code_point << 18) + (scoop(*++it) << 12);
					code_point += scoop(*++it) << 6;
					code_point += scoop(*++it);
				}
				return code_point;
			}

			/* 5 and 6 byte encoding are an extension of the standard */
			template <typename CharIt>
			constexpr uint32_t get_5(CharIt it)
			{
				uint32_t code_point = static_cast<uint32_t>(*it) & 0x3;
				if (check(it[1]) && check(it[2]) && check(it[3]) && check(it[4])) {
					code_point = (code_point << 24) + (scoop(*++it) << 18);
					code_point += scoop(*++it) << 12;
					code_point += scoop(*++it) << 6;
					code_point += scoop(*++it);
				}
				return code_point;
			}

			template <typename CharIt>
			constexpr uint32_t get_6(CharIt it)
			{
				uint32_t code_point = static_cast<uint32_t>(*it) & 0x1;
				if (check(it[1]) && check(it[2]) && check(it[3]) && check(it[4]) && check(it[5])) {
					code_point = (code_point << 30) + (scoop(*++it) << 24);
					code_point += scoop(*++it) << 18;
					code_point += scoop(*++it) << 12;
					code_point += scoop(*++it) << 6;
					code_point += scoop(*++it);
				}
				return code_point;
			}

			template <typename CharIt>
			constexpr inline uint32_t get_switch(CharIt it, unsigned size)
			{
				switch(size) {
					case 1:
						return static_cast<uint8_t>(*it);
					case 2:
						return internal::utf8x::get_2(it);
					case 3:
						return internal::utf8x::get_3(it);
					case 4:
						return internal::utf8x::get_4(it);
					case 5:
						return internal::utf8x::get_5(it);
					case 6:
						return internal::utf8x::get_6(it);
					default:
						//assert(false); // perhaps throw an error
						return 0;
				}
			}

			/* WARNING: May overflow, check sequence length first */
			template <typename CharIt>
			constexpr CharIt put(CharIt it,  uint32_t code)
			{
				if (code < 0x80)
					*it++ = static_cast<uint8_t>(code);
				else if (code < 0x800) {
					*it++ = static_cast<uint8_t>(code >> 6) | 0xc0;
					*it++ = static_cast<uint8_t>(code) & 0x3f | 0x80;
				}
				else if (code < 0x10000) {
					*it++ = static_cast<uint8_t>(code >> 12) | 0xe0;
					*it++ = static_cast<uint8_t>(code >> 6) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code) & 0x3f | 0x80;
				}
				else if (code < 0x200000) { // Should end at 0x110000 according to the RFC 3629
					*it++ = static_cast<uint8_t>(code >> 18) | 0xf0;
					*it++ = static_cast<uint8_t>(code >> 12) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code >> 6) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code) & 0x3f | 0x80;
				}
				else if (code < 0x4000000) {
					*it++ = static_cast<uint8_t>(code >> 24) | 0xf8;
					*it++ = static_cast<uint8_t>(code >> 18) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code >> 12) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code >> 6) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code) & 0x3f | 0x80;
				}
				else if (code < 0x80000000) {
					*it++ = static_cast<uint8_t>(code >> 30) | 0xfc;
					*it++ = static_cast<uint8_t>(code >> 24) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code >> 18) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code >> 12) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code >> 6) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code) & 0x3f | 0x80;
				}
				//else return put(it, 0xfffd); // The replacement character?
				// rather, don't write anything
				return it;
			}

			template <typename CharIt>
			constexpr CharIt put_switch(CharIt it, const unsigned size, uint32_t code)
			{
				switch(size)
				{
				case 1:
					*it++ = static_cast<uint8_t>(code);
					break;
					
				case 2:
					*it++ = static_cast<uint8_t>(code >> 6) | 0xc0;
					*it++ = static_cast<uint8_t>(code) & 0x3f | 0x80;
					break;
					
				case 3:
					*it++ = static_cast<uint8_t>(code >> 12) | 0xe0;
					*it++ = static_cast<uint8_t>(code >> 6) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code) & 0x3f | 0x80;
					break;
					
				case 4:
					*it++ = static_cast<uint8_t>(code >> 18) | 0xf0;
					*it++ = static_cast<uint8_t>(code >> 12) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code >> 6) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code) & 0x3f | 0x80;
					break;
					
				case 5:
					*it++ = static_cast<uint8_t>(code >> 24) | 0xf8;
					*it++ = static_cast<uint8_t>(code >> 18) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code >> 12) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code >> 6) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code) & 0x3f | 0x80;
					break;
					
				case 6:
					*it++ = static_cast<uint8_t>(code >> 30) | 0xfc;
					*it++ = static_cast<uint8_t>(code >> 24) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code >> 18) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code >> 12) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code >> 6) & 0x3f | 0x80;
					*it++ = static_cast<uint8_t>(code) & 0x3f | 0x80;
					break;
				}
				return it;
			}

			/* In accordance with the previous function */
			constexpr inline unsigned char code_length(const uint32_t code) {
				if (code < 0x80)
					return 1;
				else if (code < 0x800)
					return 2;
				else if (code < 0x10000)
					return 3;
				else if (code < 0x200000)
					return 4;
				else if (code < 0x4000000)
					return 5;
				else if (code < 0x80000000)
					return 6;
				return 0;
			}
		}
	}

	template<class Container = std::vector<char>,
		typename = std::enable_if_t<sizeof(typename Container::value_type) == 1>>
	class Buffer
	{
	public:
		using container_type = Container;
		using value_type = typename container_type::value_type;
		using string_t = std::basic_string<value_type>;
		using view_t = std::basic_string_view<value_type>;
    public:
        enum class Error { BadFormat, NoNULL, OutOfReach };

		//allocator_type 	Allocator
		using size_type = typename container_type::size_type;
		//difference_type 	Signed integer type (usually std::ptrdiff_t)
		//reference 	Allocator::reference 	(until C++11)
		//value_type& 	(since C++11)
		//const_reference 	Allocator::const_reference 	(until C++11)
		//const value_type& 	(since C++11)
		//pointer 	Allocator::pointer 	(until C++11)
		//std::allocator_traits<Allocator>::pointer 	(since C++11)
		//const_pointer 	Allocator::const_pointer 	(until C++11)
		//std::allocator_traits<Allocator>::const_pointer 	(since C++11)
		//iterator 	RandomAccessIterator
		//const_iterator 	Constant RandomAccessIterator
		//reverse_iterator 	std::reverse_iterator<iterator>
		//const_reverse_iterator 	std::reverse_iterator<const_iterator>

    private:
		container_type mData;
		size_type mPos = 0;

	public:
		// Buffer() =default;
        // Buffer(Buffer &&other) { Swap(other); };  // defualt is perfectly performant here

		void swap(Buffer& other) {
			mData.swap(other.mData);
			std::swap(mPos, other.mPos);
		}

		void swap(container_type& other) {
			mData.swap(other);
			mPos = 0;
		}

        void clear() {
			mData.clear();
			mPos = 0;
		}
		inline void set_pos(size_t newpos) { mPos = newpos > mData.size() ? mData.size() : newpos; }
        inline void resize(size_t newlength) { mData.resize(newlength); }

		void read_from_mem(const void* mem_start, size_t mem_size) {
			clear();
			if (mem_size > 0)
				mData.assign(reinterpret_cast<typename container_type::const_pointer>(mem_start), reinterpret_cast<typename container_type::const_pointer>(mem_start) + mem_size);
		}
		bool read_from_file(const char* filename) { clear(); return internal::read_from_file(filename, mData); }
		bool read_from_file(const char* filename, size_t _pos, size_t _len) { clear(); return internal::read_from_file(filename, mData, _pos, _len); }
		inline void write_to_file(const char* filename) { internal::write_to_file(filename, mData, false); }
		inline void append_to_file(const char* filename) { internal::write_to_file(filename, mData, true); }

		void rc4_crypt(const view_t& key) { internal::rc4_crypt(reinterpret_cast<unsigned char*>(mData.data()), mData.size(), reinterpret_cast<const unsigned char*>(key.data()), key.size()); }
#ifdef _USE_STRITZ_ENCODING
		void spritz_algoritm(const char* key, unsigned keylen);
#endif

#ifndef VIOLET_NO_COMPILE_COMPRESSION
		void zlib_compress() {
			container_type out;
			internal::zlib(mData.data(), mData.size(), out, true, false);
			clear();
			mData.swap(out);
		}

		bool zlib_uncompress() {
			container_type out;
			internal::zlib(mData.data(), mData.size(), out, false, false);
			clear();
			mData.swap(out);
			return true;
		}

		static Buffer zlib_compress(const view_t &data, bool gzip) {
			Buffer retn;
			internal::zlib(data.data(), data.size(), retn.mData, true, gzip);
			return retn;
		}

		static Buffer zlib_uncompress(const view_t& data, bool gzip) {
			Buffer retn;
			internal::zlib(data.data(), data.size(), retn.mData, false, gzip);
			return retn;
		}
#endif //VIOLET_NO_COMPILE_COMPRESSION

		template<typename A>
		A read() {
			static_assert(sizeof(A) != 0);
			static_assert(std::is_arithmetic_v<A>);
			if (mPos + sizeof(A) > mData.size()) {
				throw Error::OutOfReach;
			}
			mPos += sizeof(A);
			return *reinterpret_cast<A*>(&mData[mPos]);
		}

		template<typename A,
			typename = std::enable_if_t<
				std::is_scalar_v<A> ||
				std::is_convertible_v<A, const char *>
			>>
		void write(A x) {
			static_assert(sizeof(A) != 0);
			if constexpr (std::is_scalar_v<A>) {
				size_type p = mData.size();
				mData.resize(mData.size() + sizeof(A));
				*reinterpret_cast<A*>(&mData[p]) = x;
			}
			else {
				while (*x)
					mData.push_back(*(x++));
			}
		}

		// template<typename A,
		// 	typename = std::enable_if_t<std::is_same_v<A, char>>>
		// void write(const A *x) {
		// 		while (*x)
		// 			mData.push_back(*(x++));
		// }

		template<class A,
			typename = typename std::enable_if<std::is_class<A>::value>::type>
		void write(const A &x) {
			static_assert(sizeof(typename A::value_type) != 0);
			if (const std::size_t size = x.size() * sizeof(typename A::value_type); size != 0) {
				const size_type p = mData.size();
				mData.resize(p + size);
				memcpy(&mData[p], x.data(), size);
			}
		}

#ifndef VIOLET_NO_COMPILE_BUFFER_UTF8
		uint32_t read_utfx() {
			if (is_at_end()) {
				throw Error::BadFormat;
			}
			else if (const size_t s = internal::utf8x::sequence_length(&mData[mPos]); mPos + s <= mData.size()) {
				const auto p = &mData[mPos];
				mPos += s;
				internal::utf8x::get_switch(p, s);
			}
			else if(mPos + 1 > mData.size()) {
				throw Error::BadFormat;
			}
			return 0;
		}

		void write_utfx(uint32_t x) {
			const size_t s = internal::utf8x::code_length(x), p = mData.size();
			mData.resize(p + s);
			if (x > 0x7fffffff) {
				x = 0x7fffffff; // TODO: This is really unsafe
			}
			internal::utf8x::put_switch(&mData[p], s, x);
		}

		int32_t read_utfxs() {
			uint32_t b = internal::utf8x::sequence_length(&mData[mPos]);
			b = 1 << (b < 2 ? 6 : 10 + (b - 2) * 5);
			const uint32_t u = read_utfx();
			// check the most significant bit
			return !!(u & b) ?
				static_cast<int32_t>((u - b) ^ uint32_t(-1)):
				static_cast<int32_t>(u);
		}

		void write_utfxs(int32_t x) {
			const size_t p = mData.size();
			uint32_t b, u = static_cast<uint32_t>(x);
			if (x < 0)
				u ^= uint32_t(-1);
			b = internal::utf8x::code_length(u << 1);
			mData.resize(p + b);
			b = 1 << (b < 2 ? 6 : 10 + (b - 2) * 5);
			if (x < 0 && !(u & b))
				u |= b;
			else
				u &= ~b;
			write_utfx(u);
		}
#endif
        view_t read_data(size_t bytes) {
			if(bytes == 0) {
				return {};
			}
			if(mPos + bytes > mData.size()) {
				throw Error::OutOfReach;
			}
			else {
				const auto p = &mData[mPos];
				mPos += bytes;
				return std::string_view(p, bytes);
			}
		}

		void write_data(const void* data, size_t len) {
			if (len) {
				const size_t p = mData.size();
				mData.resize(p + len);
				memcpy(&mData[p], data, len);
			}
		}

		void write_buffer(const Buffer& b, size_t _pos, size_t _len) {
			if(_pos > b.mData.size()) _pos = b.mData.size();
			if(_len > b.mData.size() - _pos) _len = b.mData.size() - _pos;
			if(_len != 0) {
				const size_t p = mData.size();
				mData.resize(p + _len);
				memcpy(&mData[p], &b.mData[_pos], _len);
			}
		}
		
		inline void write_crlf() { write(view_t{"\r\n"}); };
		
        inline view_t get_string() const { return { mData.data(), mData.size() }; }
        inline view_t get_string_current() const { return { &mData[mPos], mData.size() - mPos }; }
        inline const value_type* data() const { return &mData[0]; }
        inline value_type* data() { return &mData[0]; }
		inline value_type& operator[](size_type _pos) { return mData[_pos]; };
		inline const value_type& operator[](size_type _pos) const { return mData[_pos]; };
        explicit inline operator value_type *() { return &mData[0]; }
        inline operator view_t() { return { mData.data(), mData.size() }; }
		inline size_type get_pos() const { return mPos; }
		inline size_type length() const { return mData.size(); }
		inline size_type size() const { return mData.size(); }
		inline bool is_at_end() const { return (mPos >= mData.size()); }

		// template<typename A,
		// 	typename = std::enable_if_t<
		// 		std::is_scalar_v<A> ||
		// 		std::is_convertible_v<A, const char *>
		// 	>>
		// Buffer &operator<<(A x) {
		// 	if constexpr (std::is_array_v<A>)
		// 		write<std::add_pointer_t<std::remove_extent_t<A>>>(x);
		// 	else
		// 		write<A>(x);
		// 	return *this;
		// }

		template<class A>
		Buffer &operator<<(A&&x) {
			if constexpr (std::is_convertible_v<A, std::string_view>)
				write<std::string_view>(x);
			else
				write<A>(x);
			return *this;
		}

		template<class A>
		Buffer &operator>>(A &x) {
			x = read<A>();
			return *this;
		}
	};

	using UniBuffer = Buffer<std::vector<char>>;

#ifndef VIOLET_NO_COMPILE_HASHES
	template<std::size_t IntCount>
    class HashFunction {
	protected:
        union {
            unsigned int w[IntCount];
            unsigned char x[IntCount * sizeof(int)];
        } m_result;
        union {
            unsigned int w[16];
            unsigned char x[16 * sizeof(int)];
        } m_block;

		unsigned int used = 0; // 0 <= used < 64
	private:
		bool has_result = false;
	protected:
		virtual void do_hash(void) = 0;
		virtual void finish(void) = 0;
		virtual void increment(void) = 0;
    public:
        inline const char* result() {
			if (!has_result) {
				finish();
				has_result = true;
			}
			return reinterpret_cast<const char*>(m_result.x);
		}

		void push(const uint8_t *in, size_t len) {
			if (len > 0 && !has_result) {
				uint32_t j = 0;
				while (len - j >= 64 - used) {
					memcpy(m_block.x + used, in + j, 64 - used);
					j += 64 - used;
					do_hash();
					increment();
					used = 0;
				}
				if (len - j > 0) {
					memcpy(m_block.x + used, in + j, len - j);
					used += len - j;
				}
			}
		}

		template<class Container>
		HashFunction &operator<<(const Container &src) {
			this->push(reinterpret_cast<const uint8_t *>(src.data()), src.size() * sizeof(typename Container::value_type));
			return *this;
		}
    };

	class MD5 : public HashFunction<4>
	{
		unsigned int blocks;

	public:
		MD5();
	
	private:
		void do_hash();
		void finish();
		void increment();
	};

	class SHA1 : public HashFunction<5>
	{
		unsigned int blocks;

	public:
		SHA1();
	
	private:
		void do_hash();
		void finish();
		void increment();
	};

	class SHA256 : public HashFunction<8>
	{
		unsigned int blocks1, blocks2;

	public:
		SHA256();
	
	private:
		void do_hash();
		void finish();
		void increment();
	};
}
#endif