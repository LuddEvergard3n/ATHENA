// ATHENA Core - Scheduler
// Contract: Deterministic tick-based execution.
//
// RULES:
// - Fixed execution order within tick
// - No parallelism within single iteration
// - All systems called in explicit sequence
// - Order documented and auditable
//
// Execution order per tick:
//   1. Pre-tick hooks
//   2. Process events (by event_id order)
//   3. Update movement (by entity index)
//   4. Update combat (by entity index)
//   5. Update logistics (by entity index)
//   6. Compact dead entities
//   7. Post-tick hooks
//   8. Record outputs
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_SCHEDULER_HPP
#define ATHENA_SCHEDULER_HPP

#include "types.hpp"
#include "context.hpp"
#include "entities.hpp"
#include "rng.hpp"
#include <optional>

namespace athena {

// =============================================================================
// System Function Signatures
// =============================================================================

// System function type
// Takes: storage (mutable), rng stream (mutable), tick
// Returns: status
using SystemFn = Status(*)(EntityStorage&, Rng&, Tick);

// =============================================================================
// Scheduler Configuration
// =============================================================================

struct SchedulerConfig {
    // Maximum ticks to run (0 = unlimited)
    Tick max_ticks = 0;
    
    // Tick at which to stop (if reached)
    Tick end_tick = 0;
    
    // Enable entity compaction after each tick
    bool compact_per_tick = true;
    
    // Record state every N ticks (0 = never)
    u32 record_interval = 0;
};

// =============================================================================
// Scheduler
// =============================================================================

class Scheduler {
public:
    // Phase identifiers (explicit, ordered)
    enum class Phase : u8 {
        PRE_TICK = 0,
        MOVEMENT = 1,
        DETECTION = 2,
        COMBAT = 3,
        LOGISTICS = 4,
        CASUALTIES = 5,
        POST_TICK = 6,
        
        PHASE_COUNT = 7
    };
    
    // Constructor
    Scheduler();
    
    // Initialize with context and entities
    Status init(Context& ctx, EntityManager& entities, const SchedulerConfig& config);
    
    // Register a system for a specific phase
    // Systems within same phase execute in registration order
    Status register_system(Phase phase, SystemFn system, const char* name);
    
    // Run one tick
    // Executes all phases in order
    Status run_tick();
    
    // Run until end condition
    // Returns when: max_ticks reached, end_tick reached, or error
    Status run_until_complete();
    
    // Get execution count for phase (for debugging/audit)
    u64 phase_execution_count(Phase phase) const;
    
    // Get total ticks executed
    Tick ticks_executed() const { return ticks_executed_; }
    
    // Check if complete
    bool is_complete() const;

private:
    // Registered systems per phase
    static constexpr usize MAX_SYSTEMS_PER_PHASE = 16;
    
    struct RegisteredSystem {
        SystemFn fn;
        const char* name;  // For audit/debug
    };
    
    struct PhaseData {
        RegisteredSystem systems[MAX_SYSTEMS_PER_PHASE];
        usize system_count;
        u64 execution_count;
        std::optional<Rng> rng;  // Per-phase RNG stream (initialized in init())
        
        PhaseData() : system_count(0), execution_count(0), rng(std::nullopt) {
            for (usize i = 0; i < MAX_SYSTEMS_PER_PHASE; ++i) {
                systems[i].fn = nullptr;
                systems[i].name = nullptr;
            }
        }
    };
    
    PhaseData phases_[static_cast<usize>(Phase::PHASE_COUNT)];
    
    // References (not owned)
    Context* ctx_;
    EntityManager* entities_;
    
    // Configuration
    SchedulerConfig config_;
    
    // State
    bool initialized_;
    Tick ticks_executed_;
    
    // Execute single phase
    Status execute_phase(Phase phase);
};

// =============================================================================
// Built-in Systems
// =============================================================================

namespace systems {

// Movement system: updates positions based on velocities
// Deterministic: iterates by index order
Status movement(EntityStorage& storage, Rng& rng, Tick tick);

// Logistics system: consumes supplies
// Deterministic: iterates by index order
Status logistics(EntityStorage& storage, Rng& rng, Tick tick);

// Casualty system: processes deaths
// Deterministic: iterates by index order
Status casualties(EntityStorage& storage, Rng& rng, Tick tick);

}  // namespace systems

}  // namespace athena

#endif  // ATHENA_SCHEDULER_HPP
