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
#include "echo/buffers.hpp"
#include <variant>

namespace Lovely
{
	enum Codepoints : unsigned
	{
		SuitHeart = 0x2665,
		RedHeart = 0x2764,
		Tangerine = 0x1F34A,
		Ghost = 0x1F47B,
		Grape = 0x1F347,
		Watermelon = 0x1F349,
		Strawberry = 0x1F353,
		Lemon = 0x1F34B,
		CrossMark = 0x274C,
		IceCream = 0x1F366,
		Custard = 0x1F36E,
		WrappedGift = 0x1F381,
		Scroll = 0x1F4DC,
		GrinningFace = 0x1F600
	};
	enum class Operator { none, equality, lessthan, greaterthan };

	enum class Function { Unknown, Constant, Variable, Wrapper, Title, Content, BlogLoad, Posts, StatusErrorCode, StatusErrorText, Copyright, Bitcoin, Battery, Include, StartSession, KillSession, Register, SessionInfo, Echo, GenerateCaptcha, Catch_recv_error };

	struct TangerineBlock {
		std::string_view name, sub;
		Operator op = Operator::none;
		std::variant<std::string_view, long> comp_val;
	};

	struct HeartFunction {
		const Function key;
		std::vector<std::string_view> args;
		std::unique_ptr<HeartFunction> chain;

		HeartFunction(Function _k) : key(_k) {}
	};

	struct StrategicEscape {
		unsigned int val;

		StrategicEscape(unsigned int _v) : val(_v) {}
	};

	using Candy = std::variant<bool, HeartFunction, TangerineBlock, StrategicEscape>;

	template<typename A>
	static inline bool is_operator(const A c) {
		return (c == '!' || c == '=' || c == ',' || c == ':' || c == '(' || c == ')' || c == '<' || c == '>');
	}
	
	template<typename A>
	static inline bool is_acceptable(const A c)
	{
		return (c >= '!' && c <= '~' && c != '=' && c != ',' && c != ':' && c != '(' && c != ')' && c != '{' && c != '}');
	}

	Candy parse(Violet::UnicodeKeeper<char> &_uc);
}

//bool cik_strcmp(const char * s, const char * k);