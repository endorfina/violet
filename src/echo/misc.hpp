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
                    decltype(std::declval<T>().remove_prefix(0))
                    >,
                void
                >
            > : public std::true_type {};

    template<typename A>
    static void skip_potential_bom(A &text)
    {
        if constexpr (is_view<A>::value) {
            if (text.size() < 3)
                return;
        }
        if (static_cast<unsigned char>(text[0]) == 0xef && 
            static_cast<unsigned char>(text[1]) == 0xbb && 
            static_cast<unsigned char>(text[2]) == 0xbf)
        {
            if constexpr (is_view<A>::value)
                text.remove_prefix(3);
            else
                text += 3;
        }
    }

    template<typename A>
    inline const A * __find(const A * p, const size_t n, const A c) {
        for (const auto e = p + n; p < e; ++p)
            if (*p == c)
                return p;
        return nullptr;
    }

    template<typename A>
    inline const A * __find7(const A * p, const size_t n, const A c)
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
    const A * __find7q(const A * p, const size_t n, const A c)
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
    size_t find_skip_utf8(const std::basic_string_view<A>&__s, const A __c, const size_t __pos = 0)
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
    size_t find_skip_utf8q(const std::basic_string_view<A>&__s, const A __c, const size_t __pos = 0)
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
}