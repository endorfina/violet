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
#include <utility> // pair
#include <memory>
#include <vector>
#include <exception>

namespace Violet
{
    template<typename A, unsigned int ChunkSize = 64,
            typename = std::enable_if_t<std::is_trivially_destructible_v<A> && ChunkSize >= 8>>
    class colony {
    public:
        using value_type = A;

    private:
        using pair_t = std::pair<unsigned int, value_type>;
        using list_t = std::vector<std::unique_ptr<pair_t[]>>;

        list_t _data;
        size_t _inner_size = 0;

    public:
        class iterator {
            unsigned int _chunk_pos = 0;
            typename list_t::iterator _l_it;
            size_t _limit;

            void _iterate() {
                ++_chunk_pos;
                while (true) {
                    if (_chunk_pos >= ChunkSize) {
                        _chunk_pos = 0;
                        continue;
                    }
                    if (const auto skip = (*_l_it)[_chunk_pos].first; skip > 0)
                        _chunk_pos += skip;
                    else
                        break;
                }
            }

            iterator(typename list_t::iterator _b, size_t _l)
                : _l_it(_b), _limit(_l) {}

        public:
            inline const value_type* operator->() const { return &(_l_it->operator[](_chunk_pos).second); }

            inline value_type* operator->() { return &(_l_it->operator[](_chunk_pos).second); }

            inline const value_type& operator*() const& { return _l_it->operator[](_chunk_pos).second; }

            inline value_type& operator*() & { return _l_it->operator[](_chunk_pos).second; }

            inline const value_type&& operator*() const&& { return _l_it->operator[](_chunk_pos).second; }

            inline value_type&& operator*() && { return _l_it->operator[](_chunk_pos).second; }

            iterator &operator++() {
                if (--_limit) {
                    _iterate();
                }
                return *this;
            }

            iterator operator++(int) {
                iterator copy{*this};
                this->operator++();
                return copy;
            }

            inline bool is_at_end() const { return _limit == 0; }

            void append_next_skip() {
                (*_l_it)[_chunk_pos].first = _chunk_pos + 1 < ChunkSize ? (*_l_it)[_chunk_pos + 1].first + 1 : 1;
            }

            struct __end_sentinel {};

            friend bool operator!=(const iterator &_lhs, const iterator &_rhs) {
                return _lhs._l_it != _rhs._l_it || _lhs._chunk_pos != _rhs._chunk_pos || _lhs._limit != _rhs._limit;
            }
            friend bool operator!=(__end_sentinel _es, const iterator &_it) {
                return _it._limit > 0;
            }
            friend bool operator!=(const iterator &_it, __end_sentinel _es) {
                return _it._limit > 0;
            }

            friend class colony;
        };

        inline bool empty() const { return _inner_size == 0; }
        inline auto size() const { return _inner_size; }
        inline void clear() { _data.clear(); _inner_size = 0; }

        inline typename iterator::__end_sentinel end(void) { return {}; }

        inline iterator begin(void) { return { _data.begin(), _inner_size }; }

        template<class... Args>
        value_type &emplace_at_first_empty(Args&&... args) {
            for (auto&&it : _data) {
                for (unsigned int i = 0; i < ChunkSize; ++i)
                    if (it[i].first > 0) {
                        if (it[i].first > 1 && i + 1 < ChunkSize) {
                            it[i + 1].first = it[i].first - 1;
                        }
                        it[i].first = 0;
                        ++_inner_size;
                        return *::new(static_cast<void*>(&(it[i].second))) value_type(std::forward<Args>(args)...);
                    }
            }
            auto ptr = _data.emplace_back(new pair_t[ChunkSize]).get();
            //memset(ptr, 0, ChunkSize * sizeof(pair_t));
            ptr[1].first = ChunkSize;
            ++_inner_size;
            return *::new(static_cast<void*>(&(ptr->second))) value_type(std::forward<Args>(args)...);
        }

        void erase(iterator & it) {
            if (_inner_size > 0) {
                it.append_next_skip();
                ++it;
                --_inner_size;
            }
        }
    };
}