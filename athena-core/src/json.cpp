// ATHENA Core - JSON Parser Implementation
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/json.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include <cctype>

namespace athena {
namespace json {

// =============================================================================
// Static null value
// =============================================================================

static const Value NULL_VALUE = Null{};

const Value& null_value() {
    return NULL_VALUE;
}

// =============================================================================
// Value Implementation
// =============================================================================

const Value& Value::operator[](const std::string& key) const {
    if (!is_object()) return null_value();
    const auto& obj = as_object();
    auto it = obj.find(key);
    if (it == obj.end()) return null_value();
    return it->second;
}

const Value& Value::operator[](usize index) const {
    if (!is_array()) return null_value();
    const auto& arr = as_array();
    if (index >= arr.size()) return null_value();
    return arr[index];
}

bool Value::get_bool(const std::string& key, bool default_val) const {
    const Value& v = (*this)[key];
    if (v.is_bool()) return v.as_bool();
    return default_val;
}

f64 Value::get_number(const std::string& key, f64 default_val) const {
    const Value& v = (*this)[key];
    if (v.is_number()) return v.as_number();
    return default_val;
}

std::string Value::get_string(const std::string& key, const std::string& default_val) const {
    const Value& v = (*this)[key];
    if (v.is_string()) return v.as_string();
    return default_val;
}

i64 Value::get_int(const std::string& key, i64 default_val) const {
    const Value& v = (*this)[key];
    if (v.is_number()) return static_cast<i64>(v.as_number());
    return default_val;
}

u64 Value::get_uint(const std::string& key, u64 default_val) const {
    const Value& v = (*this)[key];
    if (v.is_number()) {
        f64 n = v.as_number();
        if (n >= 0) return static_cast<u64>(n);
    }
    return default_val;
}

bool Value::has(const std::string& key) const {
    if (!is_object()) return false;
    const auto& obj = as_object();
    return obj.find(key) != obj.end();
}

usize Value::size() const {
    if (is_array()) return as_array().size();
    if (is_object()) return as_object().size();
    if (is_string()) return as_string().size();
    return 0;
}

// =============================================================================
// ParseError
// =============================================================================

std::string ParseError::format() const {
    std::ostringstream oss;
    oss << "JSON parse error at line " << line << ", column " << column 
        << ": " << message;
    return oss.str();
}

// =============================================================================
// Parser Implementation
// =============================================================================

class Parser {
public:
    explicit Parser(const std::string& input)
        : input_(input)
        , pos_(0)
        , line_(1)
        , column_(1)
    {}
    
    Result<Value> parse() {
        skip_whitespace();
        auto result = parse_value();
        if (!result.ok()) return result;
        
        skip_whitespace();
        if (pos_ < input_.size()) {
            return make_error("Unexpected characters after JSON value");
        }
        
        return result;
    }

private:
    const std::string& input_;
    usize pos_;
    usize line_;
    usize column_;
    
    char peek() const {
        if (pos_ >= input_.size()) return '\0';
        return input_[pos_];
    }
    
    char advance() {
        if (pos_ >= input_.size()) return '\0';
        char c = input_[pos_++];
        if (c == '\n') {
            line_++;
            column_ = 1;
        } else {
            column_++;
        }
        return c;
    }
    
    bool match(char expected) {
        if (peek() == expected) {
            advance();
            return true;
        }
        return false;
    }
    
    void skip_whitespace() {
        while (pos_ < input_.size()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                advance();
            } else {
                break;
            }
        }
    }
    
    Result<Value> make_error(const std::string& msg) {
        return Error(ErrorCode::INVALID_ARGUMENT,
            "JSON parse error at line " + std::to_string(line_) +
            ", column " + std::to_string(column_) + ": " + msg);
    }
    
    Result<Value> parse_value() {
        skip_whitespace();
        
        char c = peek();
        
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') return parse_string();
        if (c == 't' || c == 'f') return parse_bool();
        if (c == 'n') return parse_null();
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number();
        
        return make_error("Unexpected character: '" + std::string(1, c) + "'");
    }
    
    Result<Value> parse_object() {
        if (!match('{')) {
            return make_error("Expected '{'");
        }
        
        Object obj;
        skip_whitespace();
        
        if (match('}')) {
            return Value(std::move(obj));
        }
        
        while (true) {
            skip_whitespace();
            
            // Parse key
            if (peek() != '"') {
                return make_error("Expected string key");
            }
            
            auto key_result = parse_string();
            if (!key_result.ok()) return key_result;
            
            std::string key = key_result.get().as_string();
            
            skip_whitespace();
            
            if (!match(':')) {
                return make_error("Expected ':' after object key");
            }
            
            // Parse value
            auto value_result = parse_value();
            if (!value_result.ok()) return value_result;
            
            obj[key] = std::move(value_result.get());
            
            skip_whitespace();
            
            if (match('}')) {
                break;
            }
            
            if (!match(',')) {
                return make_error("Expected ',' or '}' in object");
            }
        }
        
        return Value(std::move(obj));
    }
    
    Result<Value> parse_array() {
        if (!match('[')) {
            return make_error("Expected '['");
        }
        
        Array arr;
        skip_whitespace();
        
        if (match(']')) {
            return Value(std::move(arr));
        }
        
        while (true) {
            auto value_result = parse_value();
            if (!value_result.ok()) return value_result;
            
            arr.push_back(std::move(value_result.get()));
            
            skip_whitespace();
            
            if (match(']')) {
                break;
            }
            
            if (!match(',')) {
                return make_error("Expected ',' or ']' in array");
            }
        }
        
        return Value(std::move(arr));
    }
    
    Result<Value> parse_string() {
        if (!match('"')) {
            return make_error("Expected '\"'");
        }
        
        std::string result;
        
        while (true) {
            if (pos_ >= input_.size()) {
                return make_error("Unterminated string");
            }
            
            char c = advance();
            
            if (c == '"') {
                break;
            }
            
            if (c == '\\') {
                if (pos_ >= input_.size()) {
                    return make_error("Unterminated escape sequence");
                }
                
                char escaped = advance();
                switch (escaped) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        // Parse 4 hex digits
                        if (pos_ + 4 > input_.size()) {
                            return make_error("Invalid unicode escape");
                        }
                        std::string hex = input_.substr(pos_, 4);
                        pos_ += 4;
                        column_ += 4;
                        
                        char* end;
                        unsigned long code = std::strtoul(hex.c_str(), &end, 16);
                        if (end != hex.c_str() + 4) {
                            return make_error("Invalid unicode escape");
                        }
                        
                        // Simple UTF-8 encoding for BMP
                        if (code < 0x80) {
                            result += static_cast<char>(code);
                        } else if (code < 0x800) {
                            result += static_cast<char>(0xC0 | (code >> 6));
                            result += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            result += static_cast<char>(0xE0 | (code >> 12));
                            result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            result += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        break;
                    }
                    default:
                        return make_error("Invalid escape sequence");
                }
            } else if (static_cast<unsigned char>(c) < 0x20) {
                return make_error("Invalid control character in string");
            } else {
                result += c;
            }
        }
        
        return Value(String(std::move(result)));
    }
    
    Result<Value> parse_number() {
        usize start = pos_;
        
        // Optional minus
        if (peek() == '-') advance();
        
        // Integer part
        if (peek() == '0') {
            advance();
        } else if (std::isdigit(static_cast<unsigned char>(peek()))) {
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
        } else {
            return make_error("Invalid number");
        }
        
        // Fractional part
        if (peek() == '.') {
            advance();
            if (!std::isdigit(static_cast<unsigned char>(peek()))) {
                return make_error("Invalid number: expected digit after '.'");
            }
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
        }
        
        // Exponent
        if (peek() == 'e' || peek() == 'E') {
            advance();
            if (peek() == '+' || peek() == '-') advance();
            if (!std::isdigit(static_cast<unsigned char>(peek()))) {
                return make_error("Invalid number: expected digit in exponent");
            }
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                advance();
            }
        }
        
        std::string num_str = input_.substr(start, pos_ - start);
        
        // Use strtod for deterministic parsing
        char* end;
        f64 value = std::strtod(num_str.c_str(), &end);
        
        if (end != num_str.c_str() + num_str.size()) {
            return make_error("Invalid number format");
        }
        
        return Value(Number(value));
    }
    
    Result<Value> parse_bool() {
        if (input_.compare(pos_, 4, "true") == 0) {
            pos_ += 4;
            column_ += 4;
            return Value(Bool(true));
        }
        
        if (input_.compare(pos_, 5, "false") == 0) {
            pos_ += 5;
            column_ += 5;
            return Value(Bool(false));
        }
        
        return make_error("Expected 'true' or 'false'");
    }
    
    Result<Value> parse_null() {
        if (input_.compare(pos_, 4, "null") == 0) {
            pos_ += 4;
            column_ += 4;
            return Value(Null{});
        }
        
        return make_error("Expected 'null'");
    }
};

// =============================================================================
// Public API
// =============================================================================

Result<Value> parse(const std::string& json) {
    Parser parser(json);
    return parser.parse();
}

Result<Value> parse_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return Error(ErrorCode::INVALID_ARGUMENT,
            "Failed to open file: " + path);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    
    return parse(buffer.str());
}

// =============================================================================
// Serialization
// =============================================================================

namespace {

void serialize_string(std::ostream& os, const std::string& s) {
    os << '"';
    for (char c : s) {
        switch (c) {
            case '"': os << "\\\""; break;
            case '\\': os << "\\\\"; break;
            case '\b': os << "\\b"; break;
            case '\f': os << "\\f"; break;
            case '\n': os << "\\n"; break;
            case '\r': os << "\\r"; break;
            case '\t': os << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    os << "\\u" << std::hex << std::setfill('0') 
                       << std::setw(4) << static_cast<int>(c)
                       << std::dec;
                } else {
                    os << c;
                }
        }
    }
    os << '"';
}

void serialize_value(std::ostream& os, const Value& value);

void serialize_array(std::ostream& os, const Array& arr) {
    os << '[';
    bool first = true;
    for (const auto& v : arr) {
        if (!first) os << ',';
        first = false;
        serialize_value(os, v);
    }
    os << ']';
}

void serialize_object(std::ostream& os, const Object& obj) {
    os << '{';
    bool first = true;
    for (const auto& [key, value] : obj) {
        if (!first) os << ',';
        first = false;
        serialize_string(os, key);
        os << ':';
        serialize_value(os, value);
    }
    os << '}';
}

void serialize_value(std::ostream& os, const Value& value) {
    if (value.is_null()) {
        os << "null";
    } else if (value.is_bool()) {
        os << (value.as_bool() ? "true" : "false");
    } else if (value.is_number()) {
        f64 n = value.as_number();
        if (std::isfinite(n)) {
            // Check if it's an integer
            if (n == std::floor(n) && std::abs(n) < 1e15) {
                os << static_cast<i64>(n);
            } else {
                os << std::setprecision(17) << n;
            }
        } else {
            os << "null";  // JSON doesn't support inf/nan
        }
    } else if (value.is_string()) {
        serialize_string(os, value.as_string());
    } else if (value.is_array()) {
        serialize_array(os, value.as_array());
    } else if (value.is_object()) {
        serialize_object(os, value.as_object());
    }
}

void serialize_value_pretty(std::ostream& os, const Value& value, 
                            int indent_size, int current_indent);

void serialize_array_pretty(std::ostream& os, const Array& arr,
                           int indent_size, int current_indent) {
    if (arr.empty()) {
        os << "[]";
        return;
    }
    
    os << "[\n";
    bool first = true;
    for (const auto& v : arr) {
        if (!first) os << ",\n";
        first = false;
        os << std::string(current_indent + indent_size, ' ');
        serialize_value_pretty(os, v, indent_size, current_indent + indent_size);
    }
    os << "\n" << std::string(current_indent, ' ') << "]";
}

void serialize_object_pretty(std::ostream& os, const Object& obj,
                            int indent_size, int current_indent) {
    if (obj.empty()) {
        os << "{}";
        return;
    }
    
    os << "{\n";
    bool first = true;
    for (const auto& [key, value] : obj) {
        if (!first) os << ",\n";
        first = false;
        os << std::string(current_indent + indent_size, ' ');
        serialize_string(os, key);
        os << ": ";
        serialize_value_pretty(os, value, indent_size, current_indent + indent_size);
    }
    os << "\n" << std::string(current_indent, ' ') << "}";
}

void serialize_value_pretty(std::ostream& os, const Value& value,
                           int indent_size, int current_indent) {
    if (value.is_null()) {
        os << "null";
    } else if (value.is_bool()) {
        os << (value.as_bool() ? "true" : "false");
    } else if (value.is_number()) {
        f64 n = value.as_number();
        if (std::isfinite(n)) {
            if (n == std::floor(n) && std::abs(n) < 1e15) {
                os << static_cast<i64>(n);
            } else {
                os << std::setprecision(17) << n;
            }
        } else {
            os << "null";
        }
    } else if (value.is_string()) {
        serialize_string(os, value.as_string());
    } else if (value.is_array()) {
        serialize_array_pretty(os, value.as_array(), indent_size, current_indent);
    } else if (value.is_object()) {
        serialize_object_pretty(os, value.as_object(), indent_size, current_indent);
    }
}

}  // anonymous namespace

std::string to_string(const Value& value) {
    std::ostringstream oss;
    serialize_value(oss, value);
    return oss.str();
}

std::string to_string_pretty(const Value& value, int indent) {
    std::ostringstream oss;
    serialize_value_pretty(oss, value, indent, 0);
    return oss.str();
}

}  // namespace json
}  // namespace athena
