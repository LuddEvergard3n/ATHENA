// ATHENA Core - Random Number Generator
// Contract: Single RNG source, deterministic splits, reproducible.
//
// Implementation: PCG64 (Permuted Congruential Generator)
// Reference: https://www.pcg-random.org/
//
// RULES:
// - ONE master RNG per execution
// - Streams derived ONLY via split()
// - NO std::random_device
// - NO local RNG in functions
// - NO implicit RNG in external libs
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_RNG_HPP
#define ATHENA_RNG_HPP

#include "types.hpp"

namespace athena {

// =============================================================================
// PCG64 Implementation
// =============================================================================

// PCG64 state (128-bit state, 64-bit output)
// This is PCG-XSL-RR variant
class Rng {
public:
    // Construct with seed and stream
    // stream_id differentiates parallel streams from same seed
    explicit Rng(Seed seed, u64 stream_id = 0) noexcept;
    
    // No default construction (must have explicit seed)
    Rng() = delete;
    
    // Copy allowed (for checkpointing)
    Rng(const Rng&) = default;
    Rng& operator=(const Rng&) = default;
    
    // Move allowed
    Rng(Rng&&) = default;
    Rng& operator=(Rng&&) = default;
    
    // =========================================================================
    // Core generation
    // =========================================================================
    
    // Generate uniform u64 in [0, 2^64)
    u64 next_u64() noexcept;
    
    // Generate uniform u32 in [0, 2^32)
    u32 next_u32() noexcept;
    
    // Generate uniform f64 in [0, 1)
    // Uses full 53-bit mantissa precision
    f64 next_f64() noexcept;
    
    // Generate uniform f64 in [min, max)
    f64 next_f64_range(f64 min, f64 max) noexcept;
    
    // Generate uniform u64 in [0, bound)
    // Unbiased (rejection sampling)
    u64 next_u64_bounded(u64 bound) noexcept;
    
    // Generate uniform u32 in [0, bound)
    u32 next_u32_bounded(u32 bound) noexcept;
    
    // =========================================================================
    // Distributions
    // =========================================================================
    
    // Standard normal (Box-Muller, pairs cached internally)
    f64 next_normal() noexcept;
    
    // Normal with mean and stddev
    f64 next_normal(f64 mean, f64 stddev) noexcept;
    
    // Exponential with rate lambda
    f64 next_exponential(f64 lambda) noexcept;
    
    // Boolean with probability p
    bool next_bool(f64 p) noexcept;
    
    // =========================================================================
    // Stream derivation
    // =========================================================================
    
    // Create a new RNG with deterministically derived stream
    // The split_id should be unique per split from this RNG
    // This allows parallel execution with reproducible results
    Rng split(u64 split_id) const noexcept;
    
    // =========================================================================
    // State access (for checkpointing/manifest)
    // =========================================================================
    
    Seed get_seed() const noexcept { return seed_; }
    u64 get_stream() const noexcept { return stream_; }
    u64 get_state_high() const noexcept { return state_high_; }
    u64 get_state_low() const noexcept { return state_low_; }
    
    // Restore from checkpoint
    void set_state(u64 high, u64 low) noexcept;

private:
    // PCG state (128-bit)
    u64 state_high_;
    u64 state_low_;
    
    // Increment (derived from stream, odd)
    u64 inc_high_;
    u64 inc_low_;
    
    // Original seed and stream (for manifest)
    Seed seed_;
    u64 stream_;
    
    // Cached normal (Box-Muller generates pairs)
    f64 cached_normal_;
    bool has_cached_normal_;
    
    // Internal: advance state
    void advance() noexcept;
    
    // Internal: output permutation
    u64 output() const noexcept;
};

// =============================================================================
// RNG Stream IDs (deterministic, documented)
// =============================================================================

namespace rng_streams {
    // Well-known stream IDs for different subsystems
    // These ensure deterministic stream derivation
    
    constexpr u64 MASTER        = 0x0000000000000000ULL;
    constexpr u64 COMBAT        = 0x434F4D4241540000ULL;  // "COMBAT"
    constexpr u64 LOGISTICS     = 0x4C4F474953544943ULL;  // "LOGISTIC"
    constexpr u64 MOVEMENT      = 0x4D4F56454D454E54ULL;  // "MOVEMENT"
    constexpr u64 SENSORS       = 0x53454E534F525300ULL;  // "SENSORS"
    constexpr u64 DECISIONS     = 0x4445434953494F4EULL;  // "DECISION"
    constexpr u64 ENVIRONMENT   = 0x454E5649524F4E4DULL;  // "ENVIRON"
    constexpr u64 CASUALTIES    = 0x4341535541545459ULL;  // "CASUALTY"
    constexpr u64 SUPPLY        = 0x535550504C590000ULL;  // "SUPPLY"
}

}  // namespace athena

#endif  // ATHENA_RNG_HPP
