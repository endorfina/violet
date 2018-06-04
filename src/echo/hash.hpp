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
#include <memory.h>

namespace Violet
{
    union hash_block {
        unsigned int w[16];
        unsigned char x[16 * sizeof(unsigned int)];
    };

	template<class hash_t>
    class hash {
	private:
        hash_block mblock;
		unsigned int used = 0; // 0 <= used < 64
		bool has_result = false;
        hash_t __hash;
        
    public:
        inline const char* result() {
			if (!has_result) {
				__hash.finish(mblock, used);
				has_result = true;
			}
			return reinterpret_cast<const char*>(__hash.result.x);
		}

		void push(const unsigned char *in, size_t len) {
			if (len > 0 && !has_result) {
				unsigned int j = 0;
				while (len - j >= 64 - used) {
					memcpy(mblock.x + used, in + j, 64 - used);
					j += 64 - used;
					__hash(mblock);
					++__hash;
					used = 0;
				}
				if (len - j > 0) {
					memcpy(mblock.x + used, in + j, len - j);
					used += len - j;
				}
			}
		}

		template<class Container>
		hash &operator<<(const Container &src) {
			this->push(reinterpret_cast<const unsigned char *>(src.data()), src.size() * sizeof(typename Container::value_type));
			return *this;
		}
    };

	struct MD5
	{
        union {
            unsigned int w[4];
            unsigned char x[4 * sizeof(unsigned int)];
        } result;
    private:
		unsigned int blocks;

    public:
		MD5();
	
		void operator()(hash_block&);
		void finish(hash_block&, const unsigned int used);
		inline void operator++() { ++blocks; }
	};

	struct SHA1
	{
        union {
            unsigned int w[5];
            unsigned char x[5 * sizeof(unsigned int)];
        } result;
    private:
		unsigned int blocks;

    public:
		SHA1();
	
		void operator()(hash_block&);
		void finish(hash_block&, const unsigned int used);
		inline void operator++() { ++blocks; }
	};

	struct SHA256
	{
        union {
            unsigned int w[8];
            unsigned char x[8 * sizeof(unsigned int)];
        } result;
    private:
		unsigned int blocks1, blocks2;

    public:
		SHA256();
	
		void operator()(hash_block&);
		void finish(hash_block&, const unsigned int used);
		void operator++();
	};
}