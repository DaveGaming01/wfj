/*
 * dcsswiftbus - minimal JSON parser (just enough for the DCS-SRS export datagrams)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "minijson.h"

#include <cctype>
#include <cstdlib>

namespace dcsswiftbus::json {

namespace {

class Parser
{
public:
    explicit Parser(const std::string &text) : m_text(text) {}

    ValuePtr parseDocument()
    {
        ValuePtr v = parseValue();
        if (!v) { return nullptr; }
        skipWhitespace();
        // trailing garbage is tolerated (SRS appends " \n")
        return v;
    }

private:
    void skipWhitespace()
    {
        while (m_pos < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_pos]))) { ++m_pos; }
    }

    char peek()
    {
        skipWhitespace();
        return m_pos < m_text.size() ? m_text[m_pos] : '\0';
    }

    bool consume(char c)
    {
        if (peek() != c) { return false; }
        ++m_pos;
        return true;
    }

    bool consumeLiteral(const char *lit)
    {
        const std::size_t len = std::char_traits<char>::length(lit);
        if (m_text.compare(m_pos, len, lit) != 0) { return false; }
        m_pos += len;
        return true;
    }

    ValuePtr parseValue()
    {
        switch (peek()) {
        case '{': return parseObject();
        case '[': return parseArray();
        case '"': return parseString();
        case 't':
            if (!consumeLiteral("true")) { return nullptr; }
            return makeBool(true);
        case 'f':
            if (!consumeLiteral("false")) { return nullptr; }
            return makeBool(false);
        case 'n':
            if (!consumeLiteral("null")) { return nullptr; }
            return std::make_shared<Value>();
        default: return parseNumber();
        }
    }

    static ValuePtr makeBool(bool b)
    {
        auto v = std::make_shared<Value>();
        v->type = Value::Boolean;
        v->boolean = b;
        return v;
    }

    ValuePtr parseNumber()
    {
        const char *start = m_text.c_str() + m_pos;
        char *end = nullptr;
        const double d = std::strtod(start, &end);
        if (end == start) { return nullptr; }
        m_pos += static_cast<std::size_t>(end - start);
        auto v = std::make_shared<Value>();
        v->type = Value::Number;
        v->number = d;
        return v;
    }

    ValuePtr parseString()
    {
        if (!consume('"')) { return nullptr; }
        auto v = std::make_shared<Value>();
        v->type = Value::String;
        while (m_pos < m_text.size()) {
            const char c = m_text[m_pos++];
            if (c == '"') { return v; }
            if (c == '\\') {
                if (m_pos >= m_text.size()) { return nullptr; }
                const char esc = m_text[m_pos++];
                switch (esc) {
                case '"': v->string += '"'; break;
                case '\\': v->string += '\\'; break;
                case '/': v->string += '/'; break;
                case 'b': v->string += '\b'; break;
                case 'f': v->string += '\f'; break;
                case 'n': v->string += '\n'; break;
                case 'r': v->string += '\r'; break;
                case 't': v->string += '\t'; break;
                case 'u':
                    // not needed for SRS data; keep a placeholder and skip the 4 hex digits
                    if (m_pos + 4 > m_text.size()) { return nullptr; }
                    m_pos += 4;
                    v->string += '?';
                    break;
                default: return nullptr;
                }
            } else {
                v->string += c;
            }
        }
        return nullptr; // unterminated
    }

    ValuePtr parseArray()
    {
        if (!consume('[')) { return nullptr; }
        auto v = std::make_shared<Value>();
        v->type = Value::Array;
        if (consume(']')) { return v; }
        while (true) {
            ValuePtr element = parseValue();
            if (!element) { return nullptr; }
            v->array.push_back(element);
            if (consume(']')) { return v; }
            if (!consume(',')) { return nullptr; }
        }
    }

    ValuePtr parseObject()
    {
        if (!consume('{')) { return nullptr; }
        auto v = std::make_shared<Value>();
        v->type = Value::Object;
        if (consume('}')) { return v; }
        while (true) {
            ValuePtr key = parseString();
            if (!key) { return nullptr; }
            if (!consume(':')) { return nullptr; }
            ValuePtr element = parseValue();
            if (!element) { return nullptr; }
            v->object[key->string] = element;
            if (consume('}')) { return v; }
            if (!consume(',')) { return nullptr; }
        }
    }

    const std::string &m_text;
    std::size_t m_pos = 0;
};

} // namespace

ValuePtr parse(const std::string &text)
{
    return Parser(text).parseDocument();
}

} // namespace dcsswiftbus::json
