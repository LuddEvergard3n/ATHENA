// ATHENA Core - Minimal JSON Parser
// Contract: Deterministic parsing, no external dependencies.
//
// This is a minimal JSON parser for configuration loading.
// NOT a general-purpose parser. Supports:
// - Objects, arrays, strings, numbers, booleans, null
// - UTF-8 strings (no escapes except basic ones)
// - IEEE-754 doubles
//
// RULES:
// - No exceptions
// - Deterministic parsing order
// - Clear error reporting
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_JSON_HPP
#define ATHENA_JSON_HPP

#include "types.hpp"
#include <string>
#include <vector>
#include <map>
#include <variant>

namespace athena {
namespace json {

// =============================================================================
// JSON Value Types
// =============================================================================

struct Value;

using Null = std::monostate;
using Bool = bool;
using Number = f64;
using String = std::string;
using Array = std::vector<Value>;
using Object = std::map<std::string, Value>;  // std::map for deterministic order

using ValueVariant = std::variant<Null, Bool, Number, String, Array, Object>;

struct Value : ValueVariant {
    using ValueVariant::ValueVariant;
    
    // Type checks
    bool is_null() const { return std::holds_alternative<Null>(*this); }
    bool is_bool() const { return std::holds_alternative<Bool>(*this); }
    bool is_number() const { return std::holds_alternative<Number>(*this); }
    bool is_string() const { return std::holds_alternative<String>(*this); }
    bool is_array() const { return std::holds_alternative<Array>(*this); }
    bool is_object() const { return std::holds_alternative<Object>(*this); }
    
    // Accessors (undefined behavior if wrong type - check first)
    Bool as_bool() const { return std::get<Bool>(*this); }
    Number as_number() const { return std::get<Number>(*this); }
    const String& as_string() const { return std::get<String>(*this); }
    const Array& as_array() const { return std::get<Array>(*this); }
    const Object& as_object() const { return std::get<Object>(*this); }
    
    // Mutable accessors
    String& as_string() { return std::get<String>(*this); }
    Array& as_array() { return std::get<Array>(*this); }
    Object& as_object() { return std::get<Object>(*this); }
    
    // Object key access (returns null Value if not found or not object)
    const Value& operator[](const std::string& key) const;
    const Value& operator[](usize index) const;
    
    // Safe getters with defaults
    bool get_bool(const std::string& key, bool default_val) const;
    f64 get_number(const std::string& key, f64 default_val) const;
    std::string get_string(const std::string& key, const std::string& default_val) const;
    i64 get_int(const std::string& key, i64 default_val) const;
    u64 get_uint(const std::string& key, u64 default_val) const;
    
    // Check if object has key
    bool has(const std::string& key) const;
    
    // Get size (array/object/string)
    usize size() const;
};

// Static null value for returning references
const Value& null_value();

// =============================================================================
// Parse Error
// =============================================================================

struct ParseError {
    usize line;
    usize column;
    std::string message;
    
    std::string format() const;
};

// =============================================================================
// Parser
// =============================================================================

// Parse JSON string
// Returns Value on success, Error on failure
Result<Value> parse(const std::string& json);

// Parse JSON from file
Result<Value> parse_file(const std::string& path);

// =============================================================================
// Serialization
// =============================================================================

// Serialize to compact JSON string
std::string to_string(const Value& value);

// Serialize to pretty-printed JSON string
std::string to_string_pretty(const Value& value, int indent = 2);

}  // namespace json
}  // namespace athena

#endif  // ATHENA_JSON_HPP
