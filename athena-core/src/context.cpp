// ATHENA Core - Context Implementation
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/context.hpp"

namespace athena {

// =============================================================================
// Context Implementation
// =============================================================================

Context::Context(const ContextConfig& config)
    : config_(config)
    , state_(SimState::INITIALIZED)
    , current_tick_(0)
    , rng_(std::make_unique<Rng>(config.seed, rng_streams::MASTER))
    , stats_()
{
}

Context::~Context() = default;

Context::Context(Context&&) noexcept = default;
Context& Context::operator=(Context&&) noexcept = default;

Result<std::unique_ptr<Context>> Context::create(const ContextConfig& config) {
    // Validate seed (must be explicit, non-zero)
    if (config.seed == 0) {
        return Error(ErrorCode::RNG_INVALID_SEED, 
            "Seed must be non-zero and explicitly set");
    }
    
    // Validate max_entities
    if (config.max_entities == 0) {
        return Error(ErrorCode::INVALID_ARGUMENT,
            "max_entities must be greater than 0");
    }
    
    if (config.max_entities > constants::MAX_ENTITIES) {
        return Error(ErrorCode::INVALID_ARGUMENT,
            "max_entities exceeds maximum allowed");
    }
    
    // Create context
    // Using new instead of make_unique because constructor is private
    auto ctx = std::unique_ptr<Context>(new Context(config));
    
    return ctx;
}

Rng Context::create_stream(u64 stream_id) const {
    return rng_->split(stream_id);
}

Status Context::start() {
    if (state_ == SimState::RUNNING) {
        return Error(ErrorCode::SIM_ALREADY_RUNNING,
            "Simulation is already running");
    }
    
    if (state_ != SimState::INITIALIZED && state_ != SimState::PAUSED) {
        return Error(ErrorCode::CONTEXT_NOT_INITIALIZED,
            "Context must be initialized or paused to start");
    }
    
    state_ = SimState::RUNNING;
    return Status();
}

Status Context::advance_tick() {
    if (state_ != SimState::RUNNING) {
        return Error(ErrorCode::SIM_NOT_RUNNING,
            "Simulation must be running to advance tick");
    }
    
    // Check for tick overflow
    if (current_tick_ >= constants::MAX_TICKS) {
        state_ = SimState::ERROR;
        return Error(ErrorCode::SIM_TICK_OVERFLOW,
            "Maximum tick count reached");
    }
    
    current_tick_++;
    stats_.ticks_executed++;
    
    return Status();
}

Status Context::pause() {
    if (state_ != SimState::RUNNING) {
        return Error(ErrorCode::SIM_NOT_RUNNING,
            "Can only pause a running simulation");
    }
    
    state_ = SimState::PAUSED;
    return Status();
}

Status Context::resume() {
    if (state_ != SimState::PAUSED) {
        return Error(ErrorCode::INVALID_ARGUMENT,
            "Can only resume a paused simulation");
    }
    
    state_ = SimState::RUNNING;
    return Status();
}

Status Context::stop() {
    if (state_ != SimState::RUNNING && state_ != SimState::PAUSED) {
        return Error(ErrorCode::SIM_NOT_RUNNING,
            "Can only stop a running or paused simulation");
    }
    
    state_ = SimState::COMPLETED;
    return Status();
}

Status Context::reset() {
    // Re-create RNG with same seed
    rng_ = std::make_unique<Rng>(config_.seed, rng_streams::MASTER);
    
    // Reset state
    state_ = SimState::INITIALIZED;
    current_tick_ = 0;
    stats_ = Stats();
    
    return Status();
}

}  // namespace athena
