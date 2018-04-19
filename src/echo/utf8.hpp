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
#include "type.hpp"

namespace Violet::utf8x
{
    template<typename A>
    constexpr void remove_potential_BOM(A &text)
    {
        const unsigned char * check = nullptr;
        if constexpr (is_view<A>::value) {
            static_assert(sizeof(typename A::value_type) == 1);
            if (text.size() < 3)
                return;
            check = reinterpret_cast<const unsigned char *>(text.data());
        }
        else {
            static_assert(sizeof(std::decay_t<decltype(*text)>) == 1);
            check = (const unsigned char *)&(*text);    // should take care of pointers and iterators
        }
        for (unsigned char bom : { 0xef, 0xbb, 0xbf })
            if (bom != *check++)
                return;
        if constexpr (is_view<A>::value) {
            text.remove_prefix(3);
        }
        else {
            text += 3;
        }
    }

    template <typename Char>
    constexpr inline auto scoop(Char c) {
        return static_cast<uint8_t>(c) & 0x3f; // Scoop dat 10xxxxxx, yo
    }

    template <typename Char>
    constexpr inline bool check(Char c) {
        return static_cast<uint8_t>(c) >> 6 == 0x2;
    }

    template <typename CharIt>
    constexpr inline unsigned short sequence_length(CharIt it)
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
                return utf8x::get_2(it);
            case 3:
                return utf8x::get_3(it);
            case 4:
                return utf8x::get_4(it);
            case 5:
                return utf8x::get_5(it);
            case 6:
                return utf8x::get_6(it);
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
    constexpr inline unsigned short code_length(const uint32_t code) {
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





    template<class Iter>
    constexpr inline Iter __find_unicode(Iter p, const size_t n, const unsigned cp)
    {
        const auto e = p + n;
        while (p < e) {
            const auto len = sequence_length(p);
            if (len < 1 || len > 4 || p + len >= e)
                return nullptr;
            if (get_switch(p, len) == cp)
                return p;
            p += len;
        }
        return nullptr;
    }

    template<class Str>
    constexpr size_t find_unicode(const Str &__s, const unsigned __c, const size_t __pos = 0)
    {
        const size_t __size = __s.size();
        if (__pos < __size)
        {
            const auto __data = __s.data();
            const size_t __n = __size - __pos;
            const auto __p = __find_unicode(__data + __pos, __n, __c);
            if (__p)
                return __p - __data;
        }
        return Str::npos;
    }

    template<class Iter, class A, typename = std::enable_if_t<!std::is_arithmetic_v<A>>>
    constexpr inline Iter __find_unicode_array(Iter p, const size_t n, const A &cp)
    {
        using value_type = std::decay_t<decltype(cp[0])>;
        static_assert(std::is_arithmetic_v<value_type> || std::is_enum_v<value_type>);
        const auto e = p + n;
        unsigned min_len = 4;
        for (value_type a : cp)
            min_len = std::min<unsigned>(min_len, utf8x::code_length(a));
        while (p < e) {
            const auto len = sequence_length(p);
            if (len < 1 || len > 4 || p + len >= e)
                return nullptr;
            if (len >= min_len) {
                const auto val = get_switch(p, len);
                for (value_type a : cp)
                    if (val == a)
                        return p;
            }
            p += len;
        }
        return nullptr;
    }

    template<class Str, class N, typename = std::enable_if_t<!std::is_arithmetic_v<N>>>
    constexpr size_t find_unicode_array(const Str &__s, N &&__c, const size_t __pos = 0)
    {
        const size_t __size = __s.size();
        if (__pos < __size)
        {
            const auto __data = __s.data();
            const auto __p = __find_unicode_array(__data + __pos, __size - __pos, std::forward<N>(__c));
            if (__p)
                return __p - __data;
        }
        return Str::npos;
    }




    template<typename A = char>
    class translator {
        using view_t = std::basic_string_view<A>;
        using int_t = unsigned int;

        view_t _data;
        size_t _pos = 0;
        unsigned _len = 1;

        void __get_length(void) {
            _len = is_at_end() ? 0
                : sequence_length(&_data[_pos]);
            if (_len > 4 || _pos + _len > _data.size()) // Invalid sequence
                _len = 0;
        }

        inline void __iterate(void) { _pos += _len; __get_length(); }

    public:

        translator() = default;
        translator(const translator &) = default;
        translator(translator &&) = default;
        translator &operator=(const translator &) = default;
        translator &operator=(translator &&) = default;

        translator(const view_t &_sv) : _data(_sv), _pos(0), _len(1) { remove_potential_BOM(_data); __get_length(); }
        translator &operator=(const view_t &_sv) { _data = _sv; remove_potential_BOM(_data); _pos = 0; _len = 1; __get_length(); return *this; }

        inline bool is_at_end() const { return _len == 0 || _pos >= _data.size(); }
        inline int_t get() const { return is_at_end() ? 0x0 : get_switch(&_data[_pos], _len); }
        int_t get_and_iterate() { int_t i = get(); __iterate(); return i; }
        inline operator int_t() const { return get(); }
        inline auto get_pos() const { return _pos; }
        inline auto get_len() const { return _len; }
        inline void set_pos(size_t _p) { _pos = _p; __get_length(); }
        inline auto substr() { return _data.substr(_pos); }
        inline auto substr(size_t p, size_t s = view_t::npos) { return _data.substr(p, s); }
        inline auto size() const { return _data.size(); }

        inline auto operator*() const { return get(); }

        inline translator &operator++() {
            __iterate();
            return *this;
        }

        inline translator operator++(int) {
            translator copy{*this};
            this->__iterate();
            return copy;
        }
        
        inline auto find_and_iterate(unsigned a) {
            _pos = std::min(find_unicode(_data, a, _pos), _data.size());
            __get_length();
            return _pos;
        }

        template<class N>
        inline auto find_and_iterate_array(N&&a) {
            _pos = std::min(find_unicode_array(_data, std::forward<N>(a), _pos), _data.size());
            __get_length();
            return _pos;
        }

        template<class Predicate>
        void skip_all(Predicate&&p) {
            while(!is_at_end() && p(get()))
                __iterate();
        }

        template<class Predicate>
        auto pop_substr_until(Predicate&&p) {
            const auto i = _pos;
            while(!(is_at_end() || p(get())))
                __iterate();
            return _data.substr(i, _pos - i);
        }

        void skip_whitespace(void) {
            while(!is_at_end() && _len == 1 && !!std::isspace(static_cast<unsigned char>(_data[_pos])))
                __iterate();
        }
    };
}