#pragma once

#include "value.hpp"

#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>

namespace ssilang::net {

namespace detail {

[[noreturn]] inline void fail(const std::string& message) {
    throw Error(message);
}

inline void append_number(std::string& out, double value) {
    out.push_back('N');
    out += number_to_string(value);
    out.push_back('\n');
}

inline void append_string(std::string& out, std::string_view value) {
    out.push_back('S');
    out += std::to_string(value.size());
    out.push_back(':');
    out.append(value.data(), value.size());
}

inline void append_bool(std::string& out, bool value) {
    out.push_back('B');
    out.push_back(value ? '1' : '0');
    out.push_back('\n');
}

inline void append_inner(std::string& out, const Value& value) {
    switch (value.type) {
    case Value::Type::Null:
        out += "Z\n";
        return;
    case Value::Type::Number:
        append_number(out, value.number_value);
        return;
    case Value::Type::String:
        append_string(out, value.string_value);
        return;
    case Value::Type::Bool:
        append_bool(out, value.bool_value);
        return;
    case Value::Type::List:
        out.push_back('L');
        out += std::to_string(value.as_list().items.size());
        out.push_back('\n');
        for (const auto& item : value.as_list().items) {
            append_inner(out, item);
        }
        return;
    case Value::Type::Map:
        out.push_back('M');
        out += std::to_string(value.as_map().items.size());
        out.push_back('\n');
        for (const auto& entry : value.as_map().items) {
            append_string(out, entry.key);
            append_inner(out, entry.value);
        }
        return;
    }
}

inline std::size_t find_char(std::string_view input, std::size_t pos, char needle) {
    const auto found = input.find(needle, pos);
    if (found == std::string_view::npos) {
        fail("Malformed %net payload");
    }
    return found;
}

inline std::string_view parse_line(std::string_view input, std::size_t& pos) {
    const auto end = find_char(input, pos, '\n');
    const auto line = input.substr(pos, end - pos);
    pos = end + 1;
    return line;
}

inline double parse_number_token(std::string_view input, std::size_t& pos) {
    const auto line = parse_line(input, pos);
    try {
        std::size_t consumed = 0;
        const double value = std::stod(std::string(line), &consumed);
        if (consumed != line.size()) {
            fail("Malformed number in %net payload");
        }
        return value;
    } catch (const std::exception&) {
        fail("Malformed number in %net payload");
    }
}

inline std::size_t parse_size_token(std::string_view token, const char* what) {
    try {
        std::size_t consumed = 0;
        const unsigned long long value = std::stoull(std::string(token), &consumed);
        if (consumed != token.size()) {
            fail(std::string("Malformed ") + what + " in %net payload");
        }
        if (value > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
            fail(std::string("Overflow while parsing ") + what + " in %net payload");
        }
        return static_cast<std::size_t>(value);
    } catch (const std::exception&) {
        fail(std::string("Malformed ") + what + " in %net payload");
    }
}

inline std::string parse_string_token(std::string_view input, std::size_t& pos) {
    const auto colon = find_char(input, pos, ':');
    const auto len_view = input.substr(pos, colon - pos);
    const std::size_t len = parse_size_token(len_view, "string length");
    pos = colon + 1;
    if (pos + len > input.size()) {
        fail("Truncated string in %net payload");
    }
    std::string out(input.substr(pos, len));
    pos += len;
    return out;
}

inline Value decode_inner(std::string_view input, std::size_t& pos) {
    if (pos >= input.size()) {
        fail("Unexpected end of %net payload");
    }

    const char tag = input[pos++];
    switch (tag) {
    case 'N':
        return Value::number(parse_number_token(input, pos));
    case 'S':
        return Value::string(parse_string_token(input, pos));
    case 'B':
        if (pos + 1 >= input.size() || input[pos + 1] != '\n') {
            fail("Malformed bool in %net payload");
        }
        if (input[pos] != '0' && input[pos] != '1') {
            fail("Malformed bool in %net payload");
        }
        {
            const bool value = input[pos] == '1';
            pos += 2;
            return Value::boolean(value);
        }
    case 'Z':
        if (pos >= input.size() || input[pos] != '\n') {
            fail("Malformed null in %net payload");
        }
        ++pos;
        return Value::null();
    case 'L': {
        const auto count = parse_size_token(parse_line(input, pos), "list length");
        Value out = Value::list();
        auto& items = out.as_list().items;
        items.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            items.push_back(decode_inner(input, pos));
        }
        return out;
    }
    case 'M': {
        const auto count = parse_size_token(parse_line(input, pos), "map length");
        Value out = Value::map();
        auto& items = out.as_map().items;
        items.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            Value key = decode_inner(input, pos);
            if (!key.is_string()) {
                fail("Corrupted %net payload: map key is not a string");
            }
            items.emplace_back(key.as_string(), decode_inner(input, pos));
        }
        return out;
    }
    default:
        fail(std::string("Unknown %net tag: ") + tag);
    }
}

} // namespace detail

inline std::string encode_payload(const Value& value) {
    std::string out = "V1\n";
    detail::append_inner(out, value);
    return out;
}

inline Value decode_payload(std::string_view payload) {
    std::size_t pos = 0;
    if (payload.rfind("V", 0) == 0) {
        const auto header_end = payload.find('\n');
        if (header_end == std::string_view::npos) {
            throw Error("Malformed %net version header");
        }
        pos = header_end + 1;
    }

    Value out = detail::decode_inner(payload, pos);
    if (pos != payload.size()) {
        throw Error("Trailing bytes after %net value");
    }
    return out;
}

inline Value decode_payload(const std::string& payload) {
    return decode_payload(std::string_view(payload));
}

} // namespace ssilang::net
