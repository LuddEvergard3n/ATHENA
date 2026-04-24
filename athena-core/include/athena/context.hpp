// ATHENA Core - Simulation Context
// Contract: Single point of control for simulation state.
//
// RULES:
// - ONE context per simulation
// - Context owns the master RNG
// - All state changes through context
// - No global state anywhere
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_CONTEXT_HPP
#define ATHENA_CONTEXT_HPP

#include "types.hpp"
#include "rng.hpp"
#include <string>
#include <memory>

namespace athena {

// =============================================================================
// Context Configuration
// =============================================================================

struct ContextConfig {
    // RNG seed (REQUIRED)
    Seed seed = 0;
    
    // Maximum entities allowed
    usize max_entities = constants::MAX_ENTITIES;
    
    // Thread count (0 = single-threaded, which is default for determinism)
    u32 thread_count = 0;
    
    // Memory limit in bytes (0 = no limit)
    usize memory_limit = 0;
    
    // Log path (empty = no file logging)
    std::string log_path;
    
    // Enable verbose logging
    bool verbose = false;
};

// =============================================================================
// Simulation State
// =============================================================================

enum class SimState : u8 {
    UNINITIALIZED = 0,
    INITIALIZED = 1,
    RUNNING = 2,
    PAUSED = 3,
    COMPLETED = 4,
    ERROR = 5
};

// =============================================================================
// Context
// =============================================================================

class Context {
public:
    // Create context with configuration
    // Returns error if seed is 0 (must be explicit)
    static Result<std::unique_ptr<Context>> create(const ContextConfig& config);
    
    // Destructor
    ~Context();
    
    // No copy (unique ownership)
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    
    // Move allowed
    Context(Context&&) noexcept;
    Context& operator=(Context&&) noexcept;
    
    // =========================================================================
    // State access
    // =========================================================================
    
    SimState state() const noexcept { return state_; }
    bool is_initialized() const noexcept { return state_ != SimState::UNINITIALIZED; }
    bool is_running() const noexcept { return state_ == SimState::RUNNING; }
    
    // Current simulation tick
    Tick current_tick() const noexcept { return current_tick_; }
    
    // Configuration (read-only after creation)
    const ContextConfig& config() const noexcept { return config_; }
    
    // =========================================================================
    // RNG access
    // =========================================================================
    
    // Get master RNG (for derived streams)
    // WARNING: Direct use should be rare; prefer split() for subsystems
    Rng& master_rng() noexcept { return *rng_; }
    const Rng& master_rng() const noexcept { return *rng_; }
    
    // Create a derived RNG stream for a subsystem
    Rng create_stream(u64 stream_id) const;
    
    // =========================================================================
    // Simulation control
    // =========================================================================
    
    // Start simulation (transition to RUNNING)
    Status start();
    
    // Advance one tick
    // Returns error if not running or tick overflow
    Status advance_tick();
    
    // Pause simulation
    Status pause();
    
    // Resume from pause
    Status resume();
    
    // Stop simulation (transition to COMPLETED)
    Status stop();
    
    // Reset to initial state (re-seeds RNG)
    Status reset();
    
    // =========================================================================
    // Statistics
    // =========================================================================
    
    struct Stats {
        u64 ticks_executed = 0;
        u64 entities_created = 0;
        u64 entities_destroyed = 0;
        u64 rng_calls = 0;  // Approximate
    };
    
    const Stats& stats() const noexcept { return stats_; }

private:
    // Private constructor (use create())
    explicit Context(const ContextConfig& config);
    
    // Configuration
    ContextConfig config_;
    
    // State
    SimState state_;
    Tick current_tick_;
    
    // Master RNG (owned)
    std::unique_ptr<Rng> rng_;
    
    // Statistics
    Stats stats_;
};

}  // namespace athena

#endif  // ATHENA_CONTEXT_HPP
