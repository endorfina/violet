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
#include <utility>  // pair
#include <memory>
#include <vector>
#include <stack>    // perhaps change later to <priority_queue>, as of now
                    // the lagging skip points aren't being invalidated
                    // hm -- let me mark that as 'irrelevant'

namespace Violet
{
    template<typename A, unsigned int ChunkSize = 256,
            typename = std::enable_if_t<std::is_trivially_destructible_v<A> && ChunkSize >= 8>>
    class colony {
    public:
        using value_type = A;

    private:
        using pos_t =           unsigned int;
        using pair_t =          std::pair<pos_t, value_type>;
        using list_t =          std::vector<std::unique_ptr<pair_t[]>>;
        using stack_pair_t =    std::pair<pair_t*, pos_t>;
        using stack_t =         std::stack<stack_pair_t>;

        list_t _data;
        stack_t _stack;
        size_t _inner_size = 0;

    public:
        class iterator {
            pos_t _chunk_pos = 0;
            typename list_t::iterator _l_it;
            size_t _limit;

        public:
            iterator(typename list_t::iterator _b, size_t _l)
                : _l_it(_b), _limit(_l) {}

            inline const value_type* operator->() const { return &(_l_it->operator[](_chunk_pos).second); }

            inline value_type* operator->() { return &(_l_it->operator[](_chunk_pos).second); }

            inline const value_type& operator*() const& { return _l_it->operator[](_chunk_pos).second; }

            inline value_type& operator*() & { return _l_it->operator[](_chunk_pos).second; }

            inline const value_type&& operator*() const&& { return _l_it->operator[](_chunk_pos).second; }

            inline value_type&& operator*() && { return _l_it->operator[](_chunk_pos).second; }

            inline stack_pair_t get_pos() const { return { _l_it->get(), _chunk_pos }; }

            iterator &operator++() {
                if (--_limit) {
                    ++_chunk_pos;
                    while (true) {
                        if (_chunk_pos < ChunkSize) {
                            if (const auto skip = (*_l_it)[_chunk_pos].first; skip > 0)
                                _chunk_pos += skip;
                            else
                                break;
                        }
                        else
                            _chunk_pos = 0;
                    }
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
        };

        inline bool empty() const { return _inner_size == 0; }
        inline auto size() const { return _inner_size; }

        inline void clear() {
            _data.clear();
            _inner_size = 0;
            while (!!_stack.size())
                _stack.pop();
        }

        inline typename iterator::__end_sentinel end(void) { return {}; }

        inline iterator begin(void) { return { _data.begin(), _inner_size }; }

        template<class... Args>
        value_type &emplace_at_first_empty(Args&&... args) {
            if (_stack.empty()) {
                for (auto&&it : _data) {
                    for (pos_t i = 0; i < ChunkSize; ++i)
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
                for (pos_t i = 1; i < ChunkSize; ++i)
                    ptr[i].first = ChunkSize - i;
                ++_inner_size;
                return *::new(static_cast<void*>(&(ptr->second))) value_type(std::forward<Args>(args)...);
            }
            else {
                const auto top = _stack.top();
                auto ptr = top.first + top.second;
                if (ptr->first > 1 && top.second + 1 < ChunkSize) {
                    ptr[1].first = ptr->first - 1;
                }
                for (pos_t i = 0; ptr->first > 0; --ptr, ++i) {
                    ptr->first = i;
                    if (ptr == top.first)
                        break;
                }
                _stack.pop();
                ++_inner_size;
                return *::new(static_cast<void*>(&((top.first + top.second)->second))) value_type(std::forward<Args>(args)...);
            }
        }

        void erase(iterator & it) {
            it.append_next_skip();
            _stack.emplace(it.get_pos());
            ++it;
            if (_inner_size)
                --_inner_size;
        }
    };
}