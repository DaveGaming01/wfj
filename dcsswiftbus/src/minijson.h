/*
 * dcsswiftbus - minimal JSON parser (just enough for the DCS-SRS export datagrams)
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace dcsswiftbus::json {

class Value;
using ValuePtr = std::shared_ptr<Value>;

class Value
{
public:
    enum Type { Null, Boolean, Number, String, Array, Object };

    Type type = Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<ValuePtr> array;
    std::map<std::string, ValuePtr> object;

    bool isNumber() const { return type == Number; }
    bool isString() const { return type == String; }

    //! Object member access, nullptr if absent or not an object
    ValuePtr get(const std::string &key) const
    {
        if (type != Object) { return nullptr; }
        auto it = object.find(key);
        return it == object.end() ? nullptr : it->second;
    }

    //! Array element access, nullptr if out of range or not an array
    ValuePtr at(std::size_t i) const
    {
        if (type != Array || i >= array.size()) { return nullptr; }
        return array[i];
    }

    double toNumber(double fallback = 0.0) const { return type == Number ? number : fallback; }
};

//! Parse a JSON document. Returns nullptr on malformed input.
ValuePtr parse(const std::string &text);

} // namespace dcsswiftbus::json
