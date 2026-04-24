// ATHENA Core - Scheduler Implementation
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/scheduler.hpp"
#include <cstring>

namespace athena {

// =============================================================================
// Phase RNG stream IDs
// =============================================================================

namespace {

constexpr u64 PHASE_STREAM_IDS[] = {
    0x5052455449434B00ULL,  // "PRETICK" - PRE_TICK
    0x4D4F56454D454E54ULL,  // "MOVEMENT" - MOVEMENT
    0x4445544543544900ULL,  // "DETECTION" - DETECTION  
    0x434F4D4241540000ULL,  // "COMBAT" - COMBAT
    0x4C4F47495354494CULL,  // "LOGISTIC" - LOGISTICS
    0x434153554C545945ULL,  // "CASUALTY" - CASUALTIES
    0x504F53545449434BULL,  // "POSTTICK" - POST_TICK
};

static_assert(sizeof(PHASE_STREAM_IDS) / sizeof(PHASE_STREAM_IDS[0]) == 
              static_cast<usize>(Scheduler::Phase::PHASE_COUNT),
              "Phase stream IDs count mismatch");

}  // anonymous namespace

// =============================================================================
// Scheduler Implementation
// =============================================================================

Scheduler::Scheduler()
    : ctx_(nullptr)
    , entities_(nullptr)
    , config_()
    , initialized_(false)
    , ticks_executed_(0)
{
    // PhaseData default constructor handles initialization
}

Status Scheduler::init(Context& ctx, EntityManager& entities, 
                       const SchedulerConfig& config) {
    if (initialized_) {
        return Error(ErrorCode::CONTEXT_ALREADY_INITIALIZED,
            "Scheduler already initialized");
    }
    
    ctx_ = &ctx;
    entities_ = &entities;
    config_ = config;
    ticks_executed_ = 0;
    
    // Initialize per-phase RNG streams
    for (usize i = 0; i < static_cast<usize>(Phase::PHASE_COUNT); ++i) {
        phases_[i].rng.emplace(ctx.create_stream(PHASE_STREAM_IDS[i]));
        phases_[i].system_count = 0;
        phases_[i].execution_count = 0;
    }
    
    initialized_ = true;
    return Status();
}

Status Scheduler::register_system(Phase phase, SystemFn system, const char* name) {
    if (!initialized_) {
        return Error(ErrorCode::CONTEXT_NOT_INITIALIZED,
            "Scheduler not initialized");
    }
    
    if (system == nullptr) {
        return Error(ErrorCode::INVALID_ARGUMENT,
            "System function cannot be null");
    }
    
    usize phase_idx = static_cast<usize>(phase);
    if (phase_idx >= static_cast<usize>(Phase::PHASE_COUNT)) {
        return Error(ErrorCode::INVALID_ARGUMENT,
            "Invalid phase");
    }
    
    PhaseData& pd = phases_[phase_idx];
    
    if (pd.system_count >= MAX_SYSTEMS_PER_PHASE) {
        return Error(ErrorCode::ENTITY_LIMIT_REACHED,
            "Maximum systems per phase reached");
    }
    
    pd.systems[pd.system_count].fn = system;
    pd.systems[pd.system_count].name = name;
    pd.system_count++;
    
    return Status();
}

Status Scheduler::execute_phase(Phase phase) {
    usize phase_idx = static_cast<usize>(phase);
    PhaseData& pd = phases_[phase_idx];
    
    // Execute each system in registration order (deterministic)
    for (usize i = 0; i < pd.system_count; ++i) {
        Status status = pd.systems[i].fn(
            entities_->storage(),
            *pd.rng,  // Dereference optional
            ctx_->current_tick()
        );
        
        if (!status.ok()) {
            return Error(status.error.code,
                std::string("System '") + 
                (pd.systems[i].name ? pd.systems[i].name : "unnamed") +
                "' failed: " + status.error.message);
        }
    }
    
    pd.execution_count++;
    return Status();
}

Status Scheduler::run_tick() {
    if (!initialized_) {
        return Error(ErrorCode::CONTEXT_NOT_INITIALIZED,
            "Scheduler not initialized");
    }
    
    if (!ctx_->is_running()) {
        return Error(ErrorCode::SIM_NOT_RUNNING,
            "Context must be running");
    }
    
    // Check end conditions BEFORE tick
    if (config_.max_ticks > 0 && ticks_executed_ >= config_.max_ticks) {
        return Error(ErrorCode::SIM_TICK_OVERFLOW,
            "Maximum ticks reached");
    }
    
    if (config_.end_tick > 0 && ctx_->current_tick() >= config_.end_tick) {
        return Error(ErrorCode::SIM_TICK_OVERFLOW,
            "End tick reached");
    }
    
    // Execute all phases in order
    // Order is CRITICAL for determinism
    
    // Phase 0: PRE_TICK
    Status status = execute_phase(Phase::PRE_TICK);
    if (!status.ok()) return status;
    
    // Phase 1: MOVEMENT
    status = execute_phase(Phase::MOVEMENT);
    if (!status.ok()) return status;
    
    // Phase 2: DETECTION
    status = execute_phase(Phase::DETECTION);
    if (!status.ok()) return status;
    
    // Phase 3: COMBAT
    status = execute_phase(Phase::COMBAT);
    if (!status.ok()) return status;
    
    // Phase 4: LOGISTICS
    status = execute_phase(Phase::LOGISTICS);
    if (!status.ok()) return status;
    
    // Phase 5: CASUALTIES
    status = execute_phase(Phase::CASUALTIES);
    if (!status.ok()) return status;
    
    // Compact dead entities (if enabled)
    if (config_.compact_per_tick) {
        entities_->compact();
    }
    
    // Phase 6: POST_TICK
    status = execute_phase(Phase::POST_TICK);
    if (!status.ok()) return status;
    
    // Advance context tick
    status = ctx_->advance_tick();
    if (!status.ok()) return status;
    
    ticks_executed_++;
    
    return Status();
}

Status Scheduler::run_until_complete() {
    if (!initialized_) {
        return Error(ErrorCode::CONTEXT_NOT_INITIALIZED,
            "Scheduler not initialized");
    }
    
    // Start context if not running
    if (!ctx_->is_running()) {
        Status status = ctx_->start();
        if (!status.ok()) return status;
    }
    
    while (!is_complete()) {
        Status status = run_tick();
        if (!status.ok()) {
            // Check if it's an expected end condition
            if (status.error.code == ErrorCode::SIM_TICK_OVERFLOW) {
                break;  // Normal completion
            }
            return status;
        }
    }
    
    return ctx_->stop();
}

u64 Scheduler::phase_execution_count(Phase phase) const {
    usize phase_idx = static_cast<usize>(phase);
    if (phase_idx >= static_cast<usize>(Phase::PHASE_COUNT)) {
        return 0;
    }
    return phases_[phase_idx].execution_count;
}

bool Scheduler::is_complete() const {
    if (!initialized_) return false;
    
    if (config_.max_ticks > 0 && ticks_executed_ >= config_.max_ticks) {
        return true;
    }
    
    if (config_.end_tick > 0 && ctx_->current_tick() >= config_.end_tick) {
        return true;
    }
    
    // Early termination: check if any combatant side has been eliminated.
    // Count active entities per side; if at least 2 sides had units initially
    // but only 0 or 1 side remains, the battle is decided.
    if (entities_ && ticks_executed_ > 0) {
        const auto& st = entities_->storage();
        bool side_alive[static_cast<int>(Side::MAX_SIDES)] = {};
        for (usize i = 0; i < st.count; ++i) {
            if (st.is_active(i) && st.health[i] > 0.0) {
                side_alive[static_cast<int>(st.side[i])] = true;
            }
        }
        // Count non-neutral sides still alive
        int alive_count = 0;
        for (int s = 1; s < static_cast<int>(Side::MAX_SIDES); ++s) {
            if (side_alive[s]) alive_count++;
        }
        // If 0 or 1 combatant side remains, simulation is decided
        if (alive_count <= 1) return true;
    }
    
    return false;
}

// =============================================================================
// Built-in Systems
// =============================================================================

namespace systems {

Status movement(EntityStorage& storage, Rng& /*rng*/, Tick /*tick*/) {
    // Simple Euler integration
    // Iterates in index order (deterministic)
    
    for (usize i = 0; i < storage.count; ++i) {
        if (!storage.is_active(i)) continue;
        
        // Update position: p = p + v * dt
        // dt = 1 tick (unit time)
        storage.pos_x[i] += storage.vel_x[i];
        storage.pos_y[i] += storage.vel_y[i];
        storage.pos_z[i] += storage.vel_z[i];
    }
    
    return Status();
}

Status logistics(EntityStorage& storage, Rng& /*rng*/, Tick /*tick*/) {
    // Consume supplies
    // Iterates in index order (deterministic)
    
    for (usize i = 0; i < storage.count; ++i) {
        if (!storage.is_active(i)) continue;
        
        f64 consumption = storage.consumption_rate[i];
        
        // Consume fuel
        storage.fuel[i] -= consumption;
        if (storage.fuel[i] < 0.0) storage.fuel[i] = 0.0;
        
        // Consume ammo (slower)
        storage.ammo[i] -= consumption * 0.1;
        if (storage.ammo[i] < 0.0) storage.ammo[i] = 0.0;
        
        // Update supply level (average of fuel and ammo)
        storage.supply[i] = (storage.fuel[i] + storage.ammo[i]) * 0.5;
    }
    
    return Status();
}

Status casualties(EntityStorage& storage, Rng& /*rng*/, Tick /*tick*/) {
    // Process deaths
    // Iterates in index order (deterministic)
    
    for (usize i = 0; i < storage.count; ++i) {
        if (!storage.is_active(i)) continue;
        
        // Entity dies if health <= 0
        if (storage.health[i] <= 0.0) {
            storage.flags[i] &= ~entity_flags::ACTIVE;
            storage.flags[i] |= entity_flags::DEAD;
        }
    }
    
    return Status();
}

}  // namespace systems

}  // namespace athena
