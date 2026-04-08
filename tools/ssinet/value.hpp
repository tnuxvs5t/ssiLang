#pragma once

#include <cmath>
#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ssilang::net {

class Error : public std::runtime_error {
public:
    explicit Error(const std::string& message) : std::runtime_error(message) {}
};

struct Value;
struct List;
struct Map;
struct MapEntry;

struct Value {
    enum class Type : std::uint8_t {
        Null,
        Number,
        String,
        Bool,
        List,
        Map
    };

    Type type = Type::Null;
    double number_value = 0.0;
    bool bool_value = false;
    std::string string_value;
    std::shared_ptr<List> list_value;
    std::shared_ptr<Map> map_value;

    Value() = default;

    static Value null() {
        return {};
    }

    static Value number(double v) {
        Value out;
        out.type = Type::Number;
        out.number_value = v;
        return out;
    }

    static Value boolean(bool v) {
        Value out;
        out.type = Type::Bool;
        out.bool_value = v;
        return out;
    }

    static Value string(std::string v) {
        Value out;
        out.type = Type::String;
        out.string_value = std::move(v);
        return out;
    }

    static Value list();
    static Value list(std::initializer_list<Value> items);
    static Value map();
    static Value map(std::initializer_list<MapEntry> entries);

    bool is_null() const { return type == Type::Null; }
    bool is_number() const { return type == Type::Number; }
    bool is_string() const { return type == Type::String; }
    bool is_bool() const { return type == Type::Bool; }
    bool is_list() const { return type == Type::List; }
    bool is_map() const { return type == Type::Map; }

    double as_number() const {
        if (!is_number()) {
            throw Error("Value is not a number");
        }
        return number_value;
    }

    bool as_bool() const {
        if (!is_bool()) {
            throw Error("Value is not a bool");
        }
        return bool_value;
    }

    const std::string& as_string() const {
        if (!is_string()) {
            throw Error("Value is not a string");
        }
        return string_value;
    }

    std::string& as_string() {
        if (!is_string()) {
            throw Error("Value is not a string");
        }
        return string_value;
    }

    const List& as_list() const;
    List& as_list();
    const Map& as_map() const;
    Map& as_map();

    const Value* find(std::string_view key) const;
    Value* find(std::string_view key);

    std::string type_name() const {
        switch (type) {
        case Type::Null:
            return "null";
        case Type::Number:
            return "number";
        case Type::String:
            return "string";
        case Type::Bool:
            return "bool";
        case Type::List:
            return "list";
        case Type::Map:
            return "map";
        }
        return "?";
    }

    std::string debug_string() const;
};

struct MapEntry {
    std::string key;
    Value value;

    MapEntry() = default;
    MapEntry(std::string k, Value v) : key(std::move(k)), value(std::move(v)) {}
};

struct List {
    std::vector<Value> items;

    List() = default;
    explicit List(std::vector<Value> values) : items(std::move(values)) {}
};

struct Map {
    std::vector<MapEntry> items;

    Map() = default;
    explicit Map(std::vector<MapEntry> values) : items(std::move(values)) {}

    const Value* find(std::string_view key) const {
        for (const auto& entry : items) {
            if (entry.key == key) {
                return &entry.value;
            }
        }
        return nullptr;
    }

    Value* find(std::string_view key) {
        for (auto& entry : items) {
            if (entry.key == key) {
                return &entry.value;
            }
        }
        return nullptr;
    }
};

inline Value Value::list() {
    Value out;
    out.type = Type::List;
    out.list_value = std::make_shared<List>();
    return out;
}

inline Value Value::list(std::initializer_list<Value> items) {
    Value out;
    out.type = Type::List;
    out.list_value = std::make_shared<List>(std::vector<Value>(items.begin(), items.end()));
    return out;
}

inline Value Value::map() {
    Value out;
    out.type = Type::Map;
    out.map_value = std::make_shared<Map>();
    return out;
}

inline Value Value::map(std::initializer_list<MapEntry> entries) {
    Value out;
    out.type = Type::Map;
    out.map_value = std::make_shared<Map>(std::vector<MapEntry>(entries.begin(), entries.end()));
    return out;
}

inline const List& Value::as_list() const {
    if (!is_list()) {
        throw Error("Value is not a list");
    }
    return *list_value;
}

inline List& Value::as_list() {
    if (!is_list()) {
        throw Error("Value is not a list");
    }
    return *list_value;
}

inline const Map& Value::as_map() const {
    if (!is_map()) {
        throw Error("Value is not a map");
    }
    return *map_value;
}

inline Map& Value::as_map() {
    if (!is_map()) {
        throw Error("Value is not a map");
    }
    return *map_value;
}

inline const Value* Value::find(std::string_view key) const {
    if (!is_map()) {
        return nullptr;
    }
    return map_value->find(key);
}

inline Value* Value::find(std::string_view key) {
    if (!is_map()) {
        return nullptr;
    }
    return map_value->find(key);
}

inline std::string number_to_string(double value) {
    if (value == static_cast<std::int64_t>(value) && std::abs(value) < 1e15) {
        return std::to_string(static_cast<std::int64_t>(value));
    }
    std::ostringstream out;
    out << value;
    return out.str();
}

inline std::string Value::debug_string() const {
    switch (type) {
    case Type::Null:
        return "null";
    case Type::Number:
        return number_to_string(number_value);
    case Type::String:
        return string_value;
    case Type::Bool:
        return bool_value ? "true" : "false";
    case Type::List: {
        std::string out = "[";
        const auto& items = as_list().items;
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (i != 0) {
                out += ", ";
            }
            if (items[i].is_string()) {
                out += "\"";
                out += items[i].as_string();
                out += "\"";
            } else {
                out += items[i].debug_string();
            }
        }
        out += "]";
        return out;
    }
    case Type::Map: {
        std::string out = "{";
        const auto& items = as_map().items;
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (i != 0) {
                out += ", ";
            }
            out += items[i].key;
            out += ": ";
            if (items[i].value.is_string()) {
                out += "\"";
                out += items[i].value.as_string();
                out += "\"";
            } else {
                out += items[i].value.debug_string();
            }
        }
        out += "}";
        return out;
    }
    }
    return "?";
}

inline bool operator==(const Value& lhs, const Value& rhs) {
    if (lhs.type != rhs.type) {
        return false;
    }
    switch (lhs.type) {
    case Value::Type::Null:
        return true;
    case Value::Type::Number:
        return lhs.number_value == rhs.number_value;
    case Value::Type::String:
        return lhs.string_value == rhs.string_value;
    case Value::Type::Bool:
        return lhs.bool_value == rhs.bool_value;
    case Value::Type::List: {
        const auto& a = lhs.as_list().items;
        const auto& b = rhs.as_list().items;
        if (a.size() != b.size()) {
            return false;
        }
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (!(a[i] == b[i])) {
                return false;
            }
        }
        return true;
    }
    case Value::Type::Map: {
        const auto& a = lhs.as_map().items;
        const auto& b = rhs.as_map().items;
        if (a.size() != b.size()) {
            return false;
        }
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (a[i].key != b[i].key || !(a[i].value == b[i].value)) {
                return false;
            }
        }
        return true;
    }
    }
    return false;
}

inline bool operator!=(const Value& lhs, const Value& rhs) {
    return !(lhs == rhs);
}

} // namespace ssilang::net
