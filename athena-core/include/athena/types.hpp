// ATHENA Core - Basic Types
// Contract: These types are stable and auditable.
// 
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_TYPES_HPP
#define ATHENA_TYPES_HPP

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <string>
#include <optional>
#include <array>

namespace athena {

// =============================================================================
// Fixed-width types (explicit, no ambiguity)
// =============================================================================

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;  // Default floating point type (IEEE-754 strict)

using usize = std::size_t;

// =============================================================================
// Simulation types
// =============================================================================

// Tick counter (simulation time unit)
using Tick = u64;

// Entity identifier (deterministic, sequential assignment)
using EntityId = u32;

// Invalid entity sentinel
constexpr EntityId INVALID_ENTITY = 0xFFFFFFFF;

// Seed type for RNG
using Seed = u64;

// =============================================================================
// Hash type (SHA-256, 32 bytes)
// =============================================================================

struct Hash256 {
    std::array<u8, 32> bytes;
    
    bool operator==(const Hash256& other) const noexcept {
        for (usize i = 0; i < 32; ++i) {
            if (bytes[i] != other.bytes[i]) return false;
        }
        return true;
    }
    
    bool operator!=(const Hash256& other) const noexcept {
        return !(*this == other);
    }
    
    // Convert to hex string (lowercase, 64 chars)
    std::string to_hex() const;
    
    // Parse from hex string
    static std::optional<Hash256> from_hex(const std::string& hex);
};

// =============================================================================
// Error handling (explicit, no exceptions in core)
// =============================================================================

enum class ErrorCode : u32 {
    OK = 0,
    
    // General errors (1-99)
    INTERNAL_ERROR = 1,
    INVALID_ARGUMENT = 2,
    OUT_OF_MEMORY = 3,
    NOT_IMPLEMENTED = 4,
    
    // Context errors (100-199)
    CONTEXT_NOT_INITIALIZED = 100,
    CONTEXT_ALREADY_INITIALIZED = 101,
    CONTEXT_ALREADY_RUNNING = 102,
    
    // RNG errors (200-299)
    RNG_INVALID_SEED = 200,
    RNG_STREAM_EXHAUSTED = 201,
    
    // Simulation errors (300-399)
    SIM_ALREADY_RUNNING = 300,
    SIM_NOT_RUNNING = 301,
    SIM_TICK_OVERFLOW = 302,
    
    // Entity errors (400-499)
    ENTITY_NOT_FOUND = 400,
    ENTITY_LIMIT_REACHED = 401,
    
    // Manifest errors (500-599)
    MANIFEST_GENERATION_FAILED = 500,
    MANIFEST_VERIFICATION_FAILED = 501,
    
    // Determinism errors (600-699)
    DETERMINISM_VIOLATION = 600,
    
    // I/O errors (700-799)
    IO_ERROR = 700,
    FILE_NOT_FOUND = 701,
    INVALID_FORMAT = 702,
    CHECKSUM_MISMATCH = 703,
};

// Error with message
struct Error {
    ErrorCode code;
    std::string message;
    
    Error() : code(ErrorCode::OK), message() {}
    Error(ErrorCode c) : code(c), message() {}
    Error(ErrorCode c, std::string msg) : code(c), message(std::move(msg)) {}
    
    bool ok() const noexcept { return code == ErrorCode::OK; }
    explicit operator bool() const noexcept { return ok(); }
};

// Result type: value or error
template<typename T>
struct Result {
    std::optional<T> value;
    Error error;
    
    Result() : value(std::nullopt), error(ErrorCode::OK) {}
    
    // Success constructor
    Result(T val) : value(std::move(val)), error(ErrorCode::OK) {}
    
    // Error constructor
    Result(Error err) : value(std::nullopt), error(std::move(err)) {}
    Result(ErrorCode code) : value(std::nullopt), error(code) {}
    Result(ErrorCode code, std::string msg) 
        : value(std::nullopt), error(code, std::move(msg)) {}
    
    bool ok() const noexcept { return value.has_value(); }
    explicit operator bool() const noexcept { return ok(); }
    
    const T& get() const { return *value; }
    T& get() { return *value; }
    
    const T& operator*() const { return *value; }
    T& operator*() { return *value; }
};

// Specialization for void (status only)
template<>
struct Result<void> {
    Error error;
    
    Result() : error(ErrorCode::OK) {}
    Result(Error err) : error(std::move(err)) {}
    Result(ErrorCode code) : error(code) {}
    Result(ErrorCode code, std::string msg) : error(code, std::move(msg)) {}
    
    bool ok() const noexcept { return error.ok(); }
    explicit operator bool() const noexcept { return ok(); }
};

using Status = Result<void>;

// =============================================================================
// Constants
// =============================================================================

namespace constants {
    // Maximum entities per simulation
    constexpr usize MAX_ENTITIES = 1'000'000;
    
    // Maximum ticks before overflow protection
    constexpr Tick MAX_TICKS = 0xFFFFFFFFFFFFFFFFULL - 1;
    
    // Version string
    constexpr const char* VERSION = "1.1.7";
    
    // Schema version
    constexpr const char* SCHEMA_VERSION = "1.0";
}

// =============================================================================
// Safe arithmetic utilities (NaN/Inf protection)
// =============================================================================

namespace safe {

/// Clamp value to finite range, replacing NaN/Inf with fallback
inline f64 finite(f64 v, f64 fallback = 0.0) {
    // std::isfinite returns false for NaN, +Inf, -Inf
    return std::isfinite(v) ? v : fallback;
}

/// Safe division: returns fallback if divisor is zero or result is non-finite
inline f64 div(f64 num, f64 den, f64 fallback = 0.0) {
    if (den == 0.0) return fallback;
    f64 r = num / den;
    return std::isfinite(r) ? r : fallback;
}

/// Clamp to [lo, hi] with NaN protection (NaN maps to lo)
inline f64 clamp(f64 v, f64 lo, f64 hi) {
    if (!std::isfinite(v)) return lo;
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

}  // namespace safe

}  // namespace athena

#endif  // ATHENA_TYPES_HPP
