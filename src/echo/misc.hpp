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
#include <random>
#include <optional>

namespace Violet
{
    template<typename T, typename = void>
    struct is_view : std::false_type {};

    template<typename... Ts>
    struct is_view_helper {};

    template<typename T>
    struct is_view<
            T,
            std::conditional_t<
                false,
                is_view_helper<
                    typename T::size_type,
                    decltype(std::declval<T>().size()),
                    decltype(std::declval<T>().begin()),
                    decltype(std::declval<T>().end()),
                    decltype(std::declval<T>().remove_prefix(0)),
                    decltype(std::declval<T>().remove_suffix(0))
                    >,
                void
                >
            > : public std::true_type {};

    template<typename A>
    constexpr void skip_potential_bom(A &text)
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

    template<typename A>
    constexpr inline const A * __find(const A * p, const size_t n, const A c) {
        for (const auto e = p + n; p < e; ++p)
            if (*p == c)
                return p;
        return nullptr;
    }

    template<typename A>
    constexpr inline const A * __find7(const A * p, const size_t n, const A c)
    {
        const auto e = p + n;
        while (p < e) {
            const auto len = Violet::internal::utf8x::sequence_length(p);
            if (len > 1 && len <= 4) {
                p += len;
                continue;
            }
            else if (*p == c)
                return p;
            ++p;
        }
        return nullptr;
    }

    template<typename A>
    constexpr const A * __find7q(const A * p, const size_t n, const A c)
    {	
        const auto e = p + n;
        while (p < e) {
            const auto len = Violet::internal::utf8x::sequence_length(p);
            const auto q = *p;
            if (len > 1 && len <= 4) {
                p += len;
                continue;
            }
            else if (q == c)
                return p;
            else if (q == '\"' || q == '\'')
                do { ++p; }
                while (*p != q && p < e);
            ++p;
        }
        return nullptr;
    }

    template<typename A>
    constexpr size_t find_skip_utf8(const std::basic_string_view<A>&__s, const A __c, const size_t __pos = 0)
    {
        size_t __ret = std::basic_string_view<A>::npos;
        const size_t __size = __s.size();
        if (__pos < __size)
        {
            const A * __data = __s.data();
            const size_t __n = __size - __pos;
            const A * __p = __find7<A>(__data + __pos, __n, __c);
            if (__p)
            __ret = __p - __data;
        }
        return __ret;
    }

    template<typename A>
    constexpr size_t find_skip_utf8q(const std::basic_string_view<A>&__s, const A __c, const size_t __pos = 0)
    {
        size_t __ret = std::basic_string_view<A>::npos;
        const size_t __size = __s.size();
        if (__pos < __size)
        {
            const A * __data = __s.data();
            const size_t __n = __size - __pos;
            const A * __p = __find7q<A>(__data + __pos, __n, __c);
            if (__p)
            __ret = __p - __data;
        }
        return __ret;
    }

    template<class Iter>
    constexpr inline Iter __find_unicode(Iter p, const size_t n, const unsigned cp)
    {
        const auto e = p + n;
        while (p < e) {
            const auto len = Violet::internal::utf8x::sequence_length(p);
            if (len < 1 || len > 4 || p + len >= e)
                return nullptr;
            if (Violet::internal::utf8x::get_switch(p, len) == cp)
                return p;
            p += len;
        }
        return nullptr;
    }

    template<class Str>
    constexpr std::optional<size_t> find_unicode(const Str &__s, const unsigned __c, const size_t __pos = 0)
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
        return {};
    }

    template<class Iter, class A, typename = std::enable_if_t<!std::is_arithmetic_v<A>>>
    constexpr inline Iter __find_unicode_array(Iter p, const size_t n, const A &cp)
    {
        using value_type = std::decay_t<decltype(cp[0])>;
        static_assert(std::is_arithmetic_v<value_type> || std::is_enum_v<value_type>);
        const auto e = p + n;
        unsigned min_len = 4;
        for (value_type a : cp)
            min_len = std::min<unsigned>(min_len, internal::utf8x::code_length(a));
        while (p < e) {
            const auto len = Violet::internal::utf8x::sequence_length(p);
            if (len < 1 || len > 4 || p + len >= e)
                return nullptr;
            if (len >= min_len) {
                const auto val = Violet::internal::utf8x::get_switch(p, len);
                for (value_type a : cp)
                    if (val == a)
                        return p;
            }
            p += len;
        }
        return nullptr;
    }

    template<class Str, class N, typename = std::enable_if_t<!std::is_arithmetic_v<N>>>
    constexpr std::optional<size_t> find_unicode_array(const Str &__s, N &&__c, const size_t __pos = 0)
    {
        const size_t __size = __s.size();
        if (__pos < __size)
        {
            const auto __data = __s.data();
            const auto __p = __find_unicode_array(__data + __pos, __size - __pos, std::forward<N>(__c));
            if (__p)
                return __p - __data;
        }
        return {};
    }

    template<typename A>
    constexpr void remove_prefix_whitespace(std::basic_string_view<A> &_sv) {
        while (!!_sv.length() && !!std::isspace(static_cast<unsigned char>(_sv.front())))
				_sv.remove_prefix(1);
    }

    template<typename A>
    constexpr void remove_suffix_whitespace(std::basic_string_view<A> &_sv) {
        while (!!_sv.length() && !!std::isspace(static_cast<unsigned char>(_sv.back())))
				_sv.remove_suffix(1);
    }

    template<typename Int, typename Char>
    inline Char __random_char(Int i) { // Int from 0 to 61 = 10 + 2 * 26 + 1
        return static_cast<Char>(i + (i < 10 ? '0' : (i < 36 ? 55 : 61)));
    }

    template<class Generator, typename CharIter>
    void generate_random_string(Generator&& gen, CharIter str, const size_t len) {
        std::uniform_int_distribution<short int> d(0, 10 + ('z' - 'a') + ('Z' - 'A'));
        for (const auto end = str + len; str != end; ++str)
            *str = __random_char<short int, std::decay_t<decltype(*str)>>(d(gen));
    }

    template<typename CharIter>
    inline void generate_random_string(CharIter str, const size_t len) {
        std::random_device dev;
        std::minstd_rand gen{ dev() };
        generate_random_string(std::move(gen), str, len);
    }

    template<typename A>
    class UnicodeKeeper {
        using view_t = std::basic_string_view<A>;
        using int_t = unsigned int;

        view_t _data;
        size_t _pos = 0;
        unsigned _len = 1;

        void __get_length(void) {
            _len = is_at_end() ? 0
                : internal::utf8x::sequence_length(&_data[_pos]);
            if (_len > 4 || _pos + _len > _data.size()) // Invalid sequence
                _len = 0;
        }

        inline void __iterate(void) { _pos += _len; __get_length(); }

    public:

        UnicodeKeeper() = default;
        UnicodeKeeper(const UnicodeKeeper &) = default;
        UnicodeKeeper(UnicodeKeeper &&) = default;
        UnicodeKeeper &operator=(const UnicodeKeeper &) = default;
        UnicodeKeeper &operator=(UnicodeKeeper &&) = default;

        UnicodeKeeper(const view_t &_sv) : _data(_sv), _pos(0), _len(1) { skip_potential_bom(_data); __get_length(); }
        UnicodeKeeper &operator=(const view_t &_sv) { _data = _sv; skip_potential_bom(_data); _pos = 0; _len = 1; __get_length(); return *this; }

        inline bool is_at_end() const { return _len == 0 || _pos >= _data.size(); }
        inline int_t get() const { return is_at_end() ? 0x0 : internal::utf8x::get_switch(&_data[_pos], _len); }
        int_t get_and_iterate() { int_t i = get(); __iterate(); return i; }
        inline operator int_t() const { return get(); }
        inline auto get_pos() const { return _pos; }
        inline auto get_len() const { return _len; }
        inline void set_pos(size_t _p) { _pos = _p; __get_length(); }
        inline auto substr() { return _data.substr(_pos); }
        inline auto substr(size_t p, size_t s = view_t::npos) { return _data.substr(p, s); }
        inline auto size() const { return _data.size(); }

        inline auto operator*() const { return get(); }

        inline UnicodeKeeper &operator++() {
            __iterate();
            return *this;
        }

        inline UnicodeKeeper operator++(int) {
            UnicodeKeeper copy{*this};
            this->__iterate();
            return copy;
        }
        
        inline auto find_and_iterate(unsigned a) {
            const auto f = find_unicode(_data, a, _pos);
            _pos = f.has_value() ? *f : _data.size();
            __get_length();
            return _pos;
        }

        template<class N>
        inline auto find_and_iterate_array(N&&a) {
            const auto f = find_unicode_array(_data, std::forward<N>(a), _pos);
            _pos = f.has_value() ? *f : _data.size();
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
            while(!is_at_end() && !p(get()))
                __iterate();
            return _data.substr(i, _pos - i);
        }

        void skip_whitespace(void) {
            while(!is_at_end() && _len == 1 && !!std::isspace(static_cast<unsigned char>(_data[_pos])))
                __iterate();
        }
    };
}