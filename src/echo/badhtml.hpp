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
#include <vector>
#include <array>
#include <stack>
#include <map>
#include <list>
#include <optional>

// depends on:
#include "buffers.hpp" // Unicode

namespace Violet::badhtml
{

    enum parse_option : unsigned
    {
        default_opt = 0x0,
        no_data_nodes = 0x1,
        no_string_terminators = 0x4,
        no_entity_translation = 0x8,
        no_utf8 = 0x10, // should perhaps be deleted, or extended for performance (ex. switching between uskip and skip)
        comment_nodes = 0x40,
        doctype_node = 0x80,
        trim_whitespace = 0x100,
        normalize_whitespace = 0x200,
        non_destructive = no_string_terminators | no_entity_translation,
        fastest = non_destructive | no_data_nodes,
        full = comment_nodes | doctype_node
    };

    // Specification mandated
    constexpr bool is_html_forbidden(unsigned long c) {
        return ((c >= 0x1 && c <= 0x8) || (c >= 0xE && c <= 0x1F) || (c >= 0x7F && c <= 0x9F) || (c >= 0xFDD0 && c <= 0xFDEF) || c == 0xB || c == 0xFFFE || c == 0xFFFF || c == 0x1FFFE || c == 0x1FFFF || c == 0x2FFFE || c == 0x2FFFF || c == 0x3FFFE || c == 0x3FFFF || c == 0x4FFFE || c == 0x4FFFF || c == 0x5FFFE || c == 0x5FFFF || c == 0x6FFFE || c == 0x6FFFF || c == 0x7FFFE || c == 0x7FFFF || c == 0x8FFFE || c == 0x8FFFF || c == 0x9FFFE || c == 0x9FFFF || c == 0xAFFFE || c == 0xAFFFF || c == 0xBFFFE || c == 0xBFFFF || c == 0xCFFFE || c == 0xCFFFF || c == 0xDFFFE || c == 0xDFFFF || c == 0xEFFFE || c == 0xEFFFF || c == 0xFFFFE || c == 0xFFFFF || c == 0x10FFFE || c == 0x10FFFF);
    }

    constexpr bool is_whitespace(unsigned char c) {
        //return !!std::isspace(c); // Not constexpr + locale dependent
        return c == 0x20 || (c >= 0x9 && c <= 0xd);
    }

    template<typename A>
    constexpr inline std::size_t bytelength [[maybe_unused]] (const A *p)
    {
        const A *tmp = p;
        while (*tmp)
            ++tmp;
        return tmp - p;
    }

    template <typename A = char>
    class Node
    {
    public:
        using char_t = A;
        using view_t = std::basic_string_view<char_t>;

        enum class Type
        {
            document,
            element,
            data,
            cdata,
            comment,
            doctype
        };

        const Type type;
        view_t data;
        std::map<view_t, view_t> attributes;
        std::list<Node> children;

        Node(Type _Nt) : type(_Nt) {};
        Node(const Node &) = default;
        Node(Node&&) = default;
        Node& operator=(const Node &) = default;
        Node& operator=(Node&&) = default;

        void clear(void) { attributes.clear(); children.clear(); }

        const Node *GetElementById(const view_t &key) const {
            if (type == Type::element) {
                if (auto id = this->operator[]("id"); id && *id == key)
                    return this;
            }
            for (auto&&it : children)
                if (auto f = it.GetElementById(key); f != nullptr)
                        return f;
            return nullptr;
        }

        Node *GetElementById(const view_t &key) {
            return const_cast<Node*>(const_cast<const Node*>(this)->GetElementById(key));
        }

        auto GetElementsByName(const view_t &key) const {
            std::vector<const Node*> out;
            find_by_name_and_append(std::back_inserter(out), key);
            return out;
        }

        auto GetElementsByName(const view_t &key) {
            std::vector<Node*> out;
            find_by_name_and_append(std::back_inserter(out), key);
            return out;
        }

        template<class T>
        void print_data(T & out) const {
            if (type == Type::data) {
                out << data;
            }
            else for (auto&&it : children)
                it.print_data(out);
        }

        std::optional<view_t> operator[](const view_t &attribute) const {
            if(auto f = attributes.find(attribute); f != attributes.end())
                return { f->second };
            return {};
        }

    private:
        template<class T>
        void find_by_name_and_append(std::back_insert_iterator<T> out, const view_t &key) const {
            for (auto&&it : children)
                if (it.type == Type::element) {
                    if (it.data == key)
                        out = const_cast<typename T::value_type>(&it);
                    else
                        if (children.size() > 0)
                            it.find_by_name_and_append(out, key);
                }
        }
    };

    template<class Iterator, class StopCondition>
    auto skip(Iterator &a, const Iterator e, StopCondition p)
    {
        for (; a != e && p(*a); ++a);
        return *a;
    }

    template<class Iterator, class SingleByteStopCondition> // For simplicity, multiple bytes cannot function as delimiters
    uint32_t uskip(Iterator &a, const Iterator e, SingleByteStopCondition p)
    {
        using namespace Violet::internal::utf8x;
        while (a != e) {
            auto len = sequence_length(a);
            if (len < 1 || len > 4)
                throw std::make_pair<const char *, const char *>("Forbidden unicode codepoint", a);
            if (const auto v = get_switch(a, len); is_html_forbidden(v))
                throw std::make_pair<const char *, const char *>("Forbidden unicode codepoint", a);
            else
                if (len == 1 && !p(*a)) break;
            for (; a != e && len > 0; --len, ++a);
        }
        return *a;
    }

    template<typename A = char,            // This will only really work with utf8, hm... Let's ensure the size is just 1
                typename = std::enable_if_t<sizeof(A) == 1>>
    class Document : public Node<A>
    {
        static inline void parsing_error(const char * what, const char * where) {
            throw std::make_pair(what, where);
        }

    public:
        using char_t = A;
        using char_it_t = typename std::add_pointer<char_t>::type;
        using node_t = Node<char_t>;
        using view_t = typename node_t::view_t;

        template<unsigned Options>
        void parse(char_it_t text, const char_it_t end)
        {
            std::stack<node_t*> depth;

            node_t::clear();
            skip_bom(text, end);
            //preprocess(text); // functionality merged with skip into uskip
                        
            while (true)
            {
                uskip(text, end, is_whitespace);
                if (text == end || !*text)
                    break;

                if (depth.empty()) {
                    if (*text == '<')
                    {
                        if constexpr (!(Options & parse_option::no_string_terminators)) {
                            *text = 0x0;
                        }
                        ++text;
                        if (auto n = parse_node<Options>(text, end); n.has_value()) {
                            node_t::children.push_back(std::move(n->first));
                            if (n->second)
                                depth.push(&node_t::children.back());
                        }
                    }
                    else {
                        parsing_error("expected <", text);
                    }
                }
                else if (!*text)
                    parsing_error("unexpected end of data", text);
                else
                    if (*text == '<') {
                    if constexpr (!(Options & parse_option::no_string_terminators)) {
                        *text = 0x0;
                    }
                    if (*++text == '/')
                    {
                        const auto name = ++text;
                        uskip(text, end, [](unsigned char c){ return c != '>' && c != '/' && !is_whitespace(c); });
                        if (text == name)
                            parsing_error("expected tag name", text);
                        
                        std::transform(name, text, name, ::tolower);
                        const view_t endtag{ name, static_cast<size_t>(text - name) };

                        uskip(text, end, is_whitespace);
                        if (text == end || *text != '>')
                            parsing_error("expected >", text);
                        ++text;
                        for(;depth.size(); depth.pop())
                            if (depth.top()->data == endtag) {
                                depth.pop();
                                break;
                            }
                    }
                    else
                    {
                        if (auto n = parse_node<Options>(text, end); n.has_value()) {
                            depth.top()->children.push_back(std::move(n->first));
                            if (n->second) {
                                constexpr static const std::array<view_t, 3> __raw_elems = {
                                    "script", "style", "title"
                                };
                                auto &m = depth.top()->children.back();
                                bool is_raw = false;
                                for (auto&&it : __raw_elems)
                                    if (it == m.data) {
                                        uskip(text, end, is_whitespace);
                                        m.children.emplace_back(node_t::Type::data).data = just_find_innerhtml<Options>(text, end, it);
                                        is_raw = true;
                                        break;
                                    }
                                if (!is_raw)
                                    depth.push(&m);
                            }
                        }
                    }
                }
                else {
                    view_t dd = parse_and_append_data<Options>(text, end);
                    if constexpr (!(Options & parse_option::no_data_nodes))
                        depth.top()->children.emplace_back(node_t::Type::data).data = dd;
                }
            }
        }
        
    private:

        template<unsigned Options>
        static std::optional<std::pair<node_t, bool>> parse_node(char_it_t &text, const char_it_t end)
        {
            if (*text == '!')
            {
                if (text + 2 < end && text[1] == '-' && text[2] == '-') {
                    text += 3;
                    return parse_comment<Options>(text, end);
                }
                if (text + 7 < end && Violet::internal::compare<const char *>(text + 1, 7, "[CDATA[", 7)) {
                    text += 8;
                    return parse_cdata<Options>(text, end);
                }
                if (text + 8 < end && Violet::internal::ciscompare<const char *>(text + 1, 7, "doctype", 7) && is_whitespace(static_cast<unsigned char>(text[8]))) {
                    text += 9;
                    return parse_doctype<Options>(text, end);
                }
                uskip(text, end, [](char_t c) { return c != '>'; });
                if (text == end || !*text)
                    parsing_error("unexpected end of data", text);
                
                ++text;
            }
            else {
                bool self_closed = false;
                const auto name = text;
                node_t node{ node_t::Type::element };
                
                uskip(text, end, [](char_t c){ return c != '>' && c != '/' && !is_whitespace(c); });
                if (text == name)
                    parsing_error("expected element name", text);

                std::transform(name, text, name, ::tolower);
                node.data = view_t{ name, static_cast<size_t>(text - name) };

                uskip(text, end, is_whitespace);

                parse_node_attributes<Options>(text, end, node);

                if (text == end)
                    parsing_error("unexpected end of data", text);
                
                if (*text == '/') {
                    if (text + 1 == end || text[1] != '>')
                        parsing_error("expected >", text);
                    if constexpr (!(Options & parse_option::no_string_terminators))
                        *text = 0x0;
                    text += 2;
                    self_closed = true;
                }
                else if (*text == '>') {
                    constexpr static const std::array<view_t, 14> void_elems = {
                        "area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param", "source", "track", "wbr"
                    };
                    self_closed = false;
                    for (auto&&it : void_elems)
                        if (node.data == it) {
                            self_closed = true;     // Skip '>'
                            break;
                        }
                    if constexpr (!(Options & parse_option::no_string_terminators))
                        *text = 0x0;
                    ++text;
                }
                else parsing_error("expected >", text);

                if constexpr (!(Options & parse_option::no_string_terminators))
                    name[node.data.size()] = 0x0;
                return { { node, !self_closed } };
            }
            return {};
        }
        
        template<unsigned Options>
        static void parse_node_attributes(char_it_t &text, const char_it_t end, node_t &node)
        {
            auto attrib_pred = [](unsigned char c) { return !is_whitespace(c) && c != '/' && c != '>' && c != '=' && c != '\'' && c != '\"' && c != '<'; };
            uskip(text, end, is_whitespace);
            
            while (*text != '/' && *text != '>' && text != end)
            {
                auto name = text++;
                uskip(text, end, attrib_pred);
                if (text == name)
                    parsing_error("expected attribute name", name);

                std::transform(name, text, name, ::tolower);
                view_t attribute_name{ name, static_cast<size_t>(text - name) };

                uskip(text, end, is_whitespace);
                if (text == end)
                    break;

                if (*text != '=') {
                    if constexpr (!(Options & parse_option::no_string_terminators))
                        name[attribute_name.size()] = 0x0;
                    node.attributes[attribute_name];
                    continue;
                }
                ++text;

                if constexpr (!(Options & parse_option::no_string_terminators))
                        name[attribute_name.size()] = 0x0;

                uskip(text, end, is_whitespace);
                if (text == end)
                    break;

                if (*text == '\'' || *text == '\"') {
                    const auto quote = text++;
                    auto b = skip_unicode_body<Options>(text, end, [q = (*quote)](unsigned char c) { return c != q; });
                    if (*text != *quote)
                        parsing_error("expected \' or \"", text);
                    node.attributes[attribute_name] = view_t{ quote + 1, static_cast<size_t>(b - quote - 1) };

                    if constexpr (!(Options & parse_option::no_string_terminators))
                        *b = 0x0;
                    ++text;
                }
                else {
                    name = text;
                    auto b = skip_unicode_body<Options>(text, end, attrib_pred);
                    if (text == name)
                        parsing_error("expected attribute value", name);
                    node.attributes[attribute_name] = view_t{ name, static_cast<size_t>(b - name) };
                    if constexpr (!(Options & parse_option::no_string_terminators))
                        if (*b != '/' && *b != '>')
                            *b = 0x0;
                }
                uskip(text, end, is_whitespace);
            }
        }

        template<unsigned Options>
        static std::optional<std::pair<node_t, bool>> parse_cdata(char_it_t &text, const char_it_t end)
        {
            if (text + 2 >= end)
                parsing_error("unexpected end of data", text);
            if constexpr (!!(Options & parse_option::no_data_nodes))
            {
                while (text[0] != ']' || text[1] != ']' || text[2] != '>')
                    if (!*text || ++text + 2 == end)
                        parsing_error("unexpected end of data", text);
                text += 3;
                return {};
            }
            else {
                const auto value = text;
                while (text[0] != ']' || text[1] != ']' || text[2] != '>')
                    if (!*text || ++text + 2 == end)
                        parsing_error("unexpected end of data", text);
                
                node_t cdata{ node_t::Type::cdata };
                cdata.data = view_t{ value, static_cast<size_t>(text - value) };

                if constexpr (!(Options & parse_option::no_string_terminators))
                    *text = 0x0;

                text += 3;
                return { { cdata, false } };
            }
        }

        template<unsigned Options>
        static std::optional<std::pair<node_t, bool>> parse_comment(char_it_t &text, const char_it_t end)
        {
            //for (int n = 0; n < 2; ++n)
                //if (text == end || text + 1 == end || text + 2 == end) // No need for a loop here
                if (text + 2 >= end)
                    parsing_error("unexpected end of data", text);
            if constexpr (!!(Options & parse_option::comment_nodes))
            {
                const auto value = text;

                while (text[0] != '-' || text[1] != '-' || text[2] != '>')
                    if (!*text || ++text + 2 == end)
                        parsing_error("unexpected end of data", text);

                node_t comment{ node_t::Type::comment };
                comment.data = view_t{ value, text - value };
                
                if constexpr (!(Options & parse_option::no_string_terminators))
                    *text = 0x0;
                
                text += 3;
                return { { comment, false } };
            }
            else {
                while (text[0] != '-' || text[1] != '-' || text[2] != '>')
                    if (!*text || ++text + 2 == end)
                        parsing_error("unexpected end of data", text);
                text += 3;
                return {};
            }
        }

        template<unsigned Options>
        static std::optional<std::pair<node_t, bool>> parse_doctype(char_it_t &text, const char_it_t end)
        {
            const auto value = text;

            while (text != end && *text != '>')
            {
                if (!*text)
                    parsing_error("unexpected end of data", text);
                else if (*text++ == '[')
                    for (int depth = 1; depth > 0 && text != end; ++text)
                        switch (*text)
                        {
                            case '[': ++depth; break;
                            case ']': --depth; break;
                            case 0x0: parsing_error("unexpected end of data", text);
                        }
            }
            
            if constexpr (!!(Options & parse_option::doctype_node))
            {
                node_t doctype(node_t::Type::doctype);
                doctype.data = view_t{ value, text - value };
                
                if (!(Options & parse_option::no_string_terminators))
                    *text = 0x0;

                ++text;
                return { { doctype, false } };
            }
            else {
                ++text;
                return {};
            }

        }

        // returns true if found
        static bool search_common_codes(char_it_t& src, char_it_t& dest, const char_it_t end) {
            const std::map<view_t, char_t> comm { {"amp", '&'}, {"apos", '\''}, {"quot", '\"'}, {"gt", '>'}, {"lt", '<'} };
            const char_it_t s = src + 1; // skip &
            char_it_t sc = s;
            for (unsigned lim = 5; lim > 0 && *sc != ';'; --lim, ++sc)
                if (sc == end)
                    return false;
            
            if (auto f = comm.find(view_t{ s, static_cast<size_t>(sc++ - s) }); f != comm.end())
            {
                *dest++ = f->second;
                src = sc;
                return true;
            }
            return false;
        }
        
        template<unsigned Options, class Predicate>
        static char_it_t skip_unicode_body(char_it_t &text, const char_it_t end, Predicate stop_if_not)
        {
            if constexpr (!!(Options & parse_option::no_entity_translation) && 
                !(Options & parse_option::normalize_whitespace) &&
                !(Options & parse_option::trim_whitespace))
            {
                uskip(text, end, stop_if_not);
                return text;
            }
            else {
                if constexpr (!!(Options & parse_option::normalize_whitespace)) {
                    uskip(text, end, [&](unsigned char c){ return stop_if_not(c) && c != '&' && !is_whitespace(c); });
                }
                else {
                    uskip(text, end, [&](unsigned char c){ return stop_if_not(c) && c != '&'; });
                }

                char_it_t src = text, translate = text;
                while (src != end && stop_if_not(*src))
                {
                    if constexpr (!(Options & parse_option::no_entity_translation))
                    {
                        if (*src == '&' && src + 1 != end)
                        {
                            if (src + 1 != end)
                                if (src[1] == '#')
                                {
                                    if (src + 2 != end && src[2] == 'x')
                                    {
                                        unsigned long code = 0;
                                        src += 3;   // Skip &#x
                                        while (src != end)
                                        {
                                            unsigned char digit = static_cast<unsigned char>(*src);
                                            if (digit >= '0' && digit <= '9')
                                                digit -= '0';
                                            else if (digit >= 'a' && digit <= 'f')
                                                digit = digit - 'a' + 10;
                                            else if (digit >= 'A' && digit <= 'F')
                                                digit = digit - 'A' + 10;
                                            else break;
                                            code = code * 16 + digit;
                                            ++src;
                                        }
                                        // Invalid, only codes up to 0x10FFFF are allowed in Unicode
                                        if (code > 0x10ffff)
                                            parsing_error("Invalid unicode", src);
                                        else
                                            Violet::internal::utf8x::put(translate, code);
                                    }
                                    else
                                    {
                                        unsigned long code = 0;
                                        src += 2;   // Skip &#
                                        while (src != end)
                                        {
                                            unsigned char digit = static_cast<unsigned char>(*src);
                                            if (digit >= '0' && digit <= '9')
                                                digit -= '0';
                                            else if (digit >= 'a' && digit <= 'f')
                                                digit = digit - 'a' + 10;
                                            else if (digit >= 'A' && digit <= 'F')
                                                digit = digit - 'A' + 10;
                                            else break;
                                            code = code * 10 + digit;
                                            ++src;
                                        }
                                        if (code > 0x10ffff)
                                            parsing_error("Invalid unicode", src);
                                        else
                                            Violet::internal::utf8x::put(translate, code);
                                    }
                                    if (*src == ';')
                                        ++src;
                                    else
                                        parsing_error("expected ;", src);
                                    continue;
                                }
                                else search_common_codes(src, translate, end);
                        }
                    }
                    
                    if constexpr (!!(Options & parse_option::normalize_whitespace))
                    {              
                        if (is_whitespace(static_cast<unsigned char>(*src)))
                        {
                            *translate++ = 0x20;
                            ++src;
                            
                            skip(src, end, is_whitespace);
                            continue;
                        }
                    }
                    *translate++ = *src++;
                }
                
                if constexpr (!(Options & parse_option::no_string_terminators)) {
                    if (translate < src)
                        *translate = 0x0;
                }
                text = src;
                return translate;
            }
        }

        template<unsigned Options>
        static view_t parse_and_append_data(char_it_t &text, const char_it_t end)
        {
            const char_it_t start = text;
            char_it_t translate = skip_unicode_body<Options>(text, end, [](unsigned char c){ return c != '<'; });

            if constexpr (!!(Options & parse_option::trim_whitespace))
            {
                if constexpr (!!(Options & parse_option::normalize_whitespace))
                {
                    if (translate != start && is_whitespace(*(translate - 1))) --translate;
                }
                else while (translate != start && is_whitespace(static_cast<unsigned char>(*(translate - 1)))) --translate;
            }
            return view_t{ start, static_cast<size_t>(translate - start) };
        }

        template<unsigned Options>
        static view_t just_find_innerhtml(char_it_t &text, const char_it_t end, const view_t &key)
        {
            const char_it_t start = text;
            if constexpr (Options & parse_option::normalize_whitespace > 0) {
                uskip(text, end, [](unsigned char c){ return c != '<' && !is_whitespace(c); });
            }
            else {
                uskip(text, end, [](char_t c){ return c != '<'; });
            }

            char_it_t translate = text;
            while (text != end)
            {
                if (bool found_end_tag = true; *text == '<' && text + 1 != end && text[1] == '/') {
                    for (std::size_t i = 0; i < key.size(); ++i)
                        if (text + (i + 2) == end || ::tolower(text[i + 2]) != key[i]) {
                            found_end_tag = false;
                            break;
                        }
                    if (found_end_tag) {
                        translate = text;
                        if constexpr (!(Options & parse_option::no_string_terminators))
                            *text++ = 0x0;
                        else
                            ++text;
                        uskip(text, end, [](char_t c){ return c != '>'; });
                        ++text;
                        break;
                    }
                }
                
                if constexpr (!!(Options & parse_option::normalize_whitespace > 0))
                {            
                    if (is_whitespace(static_cast<unsigned char>(*text)))
                    {
                        *translate++ = 0x20;
                        ++text;
                        skip(text, end, is_whitespace);
                    }
                    else
                        *translate++ = *text++;
                }
                else ++text;
            }

            if constexpr (!!(Options & parse_option::normalize_whitespace) && !(Options & parse_option::no_string_terminators)) {
                if (translate < text)
                    *translate = 0x0;
            }

            if constexpr (!!(Options & parse_option::trim_whitespace))
            {
                if constexpr (!!(Options & parse_option::normalize_whitespace))
                {
                    if (translate != start && is_whitespace(*(translate - 1))) --translate;
                }
                else while (translate != start && is_whitespace(static_cast<unsigned char>(*(translate - 1)))) --translate;
            }
            return view_t{ start, static_cast<size_t>(translate - start) };
        }
        
        static void skip_bom(char_it_t &text, const char_it_t end)
        {
            char_it_t t = text;
            for (unsigned char bom : { 0xef, 0xbb, 0xbf })
                if (bom != *reinterpret_cast<unsigned char *>(t++))
                    return;
            text += 3;
        }

        // static void preprocess(char_it_t src)
        // {
        //     while (*src)
        //     {
        //         const auto len = Violet::internal::utf8x::sequence_length(src);
        //         // if (c == 0xd) {
        //         //     *dest++ = 0xa;
        //         //     if(*++src == 0xa)
        //         //         ++src;
        //         // }
        //         if (len < 1 || len > 4)
        //             parsing_error("Forbidden unicode codepoint found", src);
        //         if (is_html_forbidden(Violet::internal::utf8x::get_switch(src, len)))
        //             parsing_error("Forbidden unicode codepoint found", src);
        //         src += len;
        //     }
        // }

    public:
        Document() : node_t{ node_t::Type::document } {}
    };
}