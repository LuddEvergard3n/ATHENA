// ATHENA Core - Random Number Generator Implementation
// 
// PCG64 Implementation Details:
// - State: 128-bit (two u64)
// - Output: 64-bit
// - Variant: PCG-XSL-RR-128/64
// - Multiplier: 0x2360ED051FC65DA44385DF649FCCF645
// - Increment: derived from stream (must be odd)
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/rng.hpp"
#include <cmath>

namespace athena {

// =============================================================================
// PCG64 Constants
// =============================================================================

// Multiplier for PCG64 (128-bit, split into high/low)
static constexpr u64 PCG_MULT_HIGH = 0x2360ED051FC65DA4ULL;
static constexpr u64 PCG_MULT_LOW  = 0x4385DF649FCCF645ULL;

// Default increment base (will be ORed with 1 to ensure odd)
static constexpr u64 PCG_INC_HIGH = 0x5851F42D4C957F2DULL;
static constexpr u64 PCG_INC_LOW  = 0x14057B7EF767814FULL;

// =============================================================================
// 128-bit arithmetic helpers
// =============================================================================

namespace {

// Multiply two 64-bit numbers, return 128-bit result as (high, low)
inline void mul64(u64 a, u64 b, u64& hi, u64& lo) noexcept {
    // Split into 32-bit parts
    u64 a_lo = a & 0xFFFFFFFFULL;
    u64 a_hi = a >> 32;
    u64 b_lo = b & 0xFFFFFFFFULL;
    u64 b_hi = b >> 32;
    
    // Partial products
    u64 p0 = a_lo * b_lo;
    u64 p1 = a_lo * b_hi;
    u64 p2 = a_hi * b_lo;
    u64 p3 = a_hi * b_hi;
    
    // Sum with carries
    u64 mid = p1 + (p0 >> 32);
    mid += p2;
    
    // Handle carry from mid
    if (mid < p2) {
        p3 += 0x100000000ULL;
    }
    
    lo = (p0 & 0xFFFFFFFFULL) | (mid << 32);
    hi = p3 + (mid >> 32);
}

// Multiply 128-bit by 128-bit, return low 128 bits
inline void mul128(u64 a_hi, u64 a_lo, u64 b_hi, u64 b_lo,
                   u64& r_hi, u64& r_lo) noexcept {
    u64 lo_hi, lo_lo;
    mul64(a_lo, b_lo, lo_hi, lo_lo);
    
    // Cross terms (only need low 64 bits of each)
    u64 cross1 = a_lo * b_hi;
    u64 cross2 = a_hi * b_lo;
    
    r_lo = lo_lo;
    r_hi = lo_hi + cross1 + cross2;
}

// Add 128-bit numbers
inline void add128(u64 a_hi, u64 a_lo, u64 b_hi, u64 b_lo,
                   u64& r_hi, u64& r_lo) noexcept {
    r_lo = a_lo + b_lo;
    r_hi = a_hi + b_hi + (r_lo < a_lo ? 1 : 0);
}

// Rotate right 64-bit
inline u64 rotr64(u64 x, unsigned k) noexcept {
    return (x >> k) | (x << (64 - k));
}

}  // anonymous namespace

// =============================================================================
// Rng Implementation
// =============================================================================

Rng::Rng(Seed seed, u64 stream_id) noexcept
    : state_high_(0)
    , state_low_(0)
    , inc_high_(PCG_INC_HIGH ^ (stream_id >> 1))
    , inc_low_((PCG_INC_LOW ^ (stream_id << 63)) | 1)  // Ensure odd
    , seed_(seed)
    , stream_(stream_id)
    , cached_normal_(0.0)
    , has_cached_normal_(false)
{
    // Initialize state from seed
    // state = 0, then advance, then add seed, then advance
    state_high_ = 0;
    state_low_ = 0;
    advance();
    
    // Add seed to low part
    u64 new_low = state_low_ + seed;
    if (new_low < state_low_) {
        state_high_++;
    }
    state_low_ = new_low;
    
    advance();
}

void Rng::advance() noexcept {
    // state = state * multiplier + increment (128-bit arithmetic)
    u64 new_hi, new_lo;
    mul128(state_high_, state_low_, PCG_MULT_HIGH, PCG_MULT_LOW, new_hi, new_lo);
    add128(new_hi, new_lo, inc_high_, inc_low_, state_high_, state_low_);
}

u64 Rng::output() const noexcept {
    // XSL-RR: XOR high into low, then rotate by high bits
    u64 xored = state_high_ ^ state_low_;
    unsigned rot = static_cast<unsigned>(state_high_ >> 58);
    return rotr64(xored, rot);
}

u64 Rng::next_u64() noexcept {
    u64 result = output();
    advance();
    return result;
}

u32 Rng::next_u32() noexcept {
    return static_cast<u32>(next_u64() >> 32);
}

f64 Rng::next_f64() noexcept {
    // Use 53 bits for full mantissa precision
    // [0, 1) with uniform distribution
    u64 bits = next_u64() >> 11;  // 53 bits
    return static_cast<f64>(bits) * (1.0 / 9007199254740992.0);  // 1 / 2^53
}

f64 Rng::next_f64_range(f64 min, f64 max) noexcept {
    return min + next_f64() * (max - min);
}

u64 Rng::next_u64_bounded(u64 bound) noexcept {
    if (bound == 0) return 0;
    
    // Lemire's method (unbiased, fast)
    u64 threshold = (-bound) % bound;
    
    while (true) {
        u64 r = next_u64();
        if (r >= threshold) {
            return r % bound;
        }
    }
}

u32 Rng::next_u32_bounded(u32 bound) noexcept {
    if (bound == 0) return 0;
    return static_cast<u32>(next_u64_bounded(bound));
}

f64 Rng::next_normal() noexcept {
    if (has_cached_normal_) {
        has_cached_normal_ = false;
        return cached_normal_;
    }
    
    // Box-Muller transform
    f64 u1, u2;
    do {
        u1 = next_f64();
    } while (u1 == 0.0);  // Avoid log(0)
    u2 = next_f64();
    
    f64 radius = std::sqrt(-2.0 * std::log(u1));
    f64 theta = 2.0 * 3.14159265358979323846 * u2;
    
    cached_normal_ = radius * std::sin(theta);
    has_cached_normal_ = true;
    
    return radius * std::cos(theta);
}

f64 Rng::next_normal(f64 mean, f64 stddev) noexcept {
    return mean + stddev * next_normal();
}

f64 Rng::next_exponential(f64 lambda) noexcept {
    f64 u;
    do {
        u = next_f64();
    } while (u == 0.0);
    return -std::log(u) / lambda;
}

bool Rng::next_bool(f64 p) noexcept {
    return next_f64() < p;
}

Rng Rng::split(u64 split_id) const noexcept {
    // Derive new stream deterministically
    // Combine original stream with split_id
    u64 new_stream = stream_ ^ split_id;
    
    // Use current state to derive new seed
    // This ensures splits from same RNG at different times differ
    u64 new_seed = seed_ ^ state_low_ ^ (split_id * 0x9E3779B97F4A7C15ULL);
    
    return Rng(new_seed, new_stream);
}

void Rng::set_state(u64 high, u64 low) noexcept {
    state_high_ = high;
    state_low_ = low;
    has_cached_normal_ = false;  // Invalidate cache on state restore
}

}  // namespace athena
