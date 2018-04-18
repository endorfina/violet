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
}