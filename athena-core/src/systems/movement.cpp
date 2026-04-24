// ATHENA Core - Movement System Implementation
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/systems/movement.hpp"
#include <cmath>
#include <algorithm>

namespace athena {
namespace systems {

// =============================================================================
// Global instance pointer
// =============================================================================

MovementSystem* g_movement_system = nullptr;

// =============================================================================
// MovementSystem Implementation
// =============================================================================

MovementSystem::MovementSystem()
    : config_()
    , terrain_(nullptr)
    , states_()
    , waypoints_()
    , capacity_(0)
    , initialized_(false)
{
}

void MovementSystem::init(usize capacity, const MovementConfig& config) {
    config_ = config;
    capacity_ = capacity;
    
    states_.resize(capacity);
    waypoints_.resize(capacity);
    
    // Initialize all states
    for (usize i = 0; i < capacity; ++i) {
        states_[i] = MovementState();
        waypoints_[i] = WaypointQueue();
    }
    
    initialized_ = true;
}

void MovementSystem::reset() {
    for (usize i = 0; i < capacity_; ++i) {
        states_[i] = MovementState();
        waypoints_[i].clear();
    }
}

void MovementSystem::set_terrain(const TerrainSemantics* terrain) {
    terrain_ = terrain;
}

void MovementSystem::set_movement_type(usize entity_idx, MovementType type) {
    if (entity_idx >= capacity_) return;
    states_[entity_idx].movement_type = type;
}

void MovementSystem::set_target(usize entity_idx, f64 x, f64 y, f64 z) {
    if (entity_idx >= capacity_) return;
    
    auto& state = states_[entity_idx];
    state.target_x = x;
    state.target_y = y;
    state.target_z = z;
    state.type = MovementState::Type::MOVING;
}

void MovementSystem::set_max_speed(usize entity_idx, f64 speed) {
    if (entity_idx >= capacity_) return;
    states_[entity_idx].max_speed = speed;
}

void MovementSystem::set_terrain_modifier(usize entity_idx, f64 modifier) {
    if (entity_idx >= capacity_) return;
    states_[entity_idx].terrain_modifier = modifier;
}

void MovementSystem::stop(usize entity_idx) {
    if (entity_idx >= capacity_) return;
    
    auto& state = states_[entity_idx];
    state.type = MovementState::Type::STATIONARY;
    state.current_speed = 0.0;
}

bool MovementSystem::add_waypoint(usize entity_idx, const Waypoint& wp) {
    if (entity_idx >= capacity_) return false;
    return waypoints_[entity_idx].add(wp);
}

void MovementSystem::clear_waypoints(usize entity_idx) {
    if (entity_idx >= capacity_) return;
    waypoints_[entity_idx].clear();
}

const MovementState& MovementSystem::get_state(usize entity_idx) const {
    static const MovementState DEFAULT_STATE;
    if (entity_idx >= capacity_) return DEFAULT_STATE;
    return states_[entity_idx];
}

f64 MovementSystem::get_terrain_cost(f64 x, f64 y, Tick tick, MovementType type) const {
    if (!terrain_ || !config_.use_terrain_semantics) {
        return 1.0;  // No cost penalty
    }
    
    auto cost = terrain_->movement_cost(x, y, tick, type);
    if (!cost.passable) {
        return 999.0;  // Impassable
    }
    return cost.cost;
}

void MovementSystem::compute_velocity(usize idx, EntityStorage& storage, Tick tick) {
    auto& state = states_[idx];
    
    // Get current position
    f64 px = storage.pos_x[idx];
    f64 py = storage.pos_y[idx];
    f64 pz = storage.pos_z[idx];
    
    // Compute direction to target
    f64 dx = state.target_x - px;
    f64 dy = state.target_y - py;
    f64 dz = state.target_z - pz;
    
    f64 distance = std::sqrt(dx*dx + dy*dy + dz*dz);
    
    // If very close to target, stop
    if (distance < 1.0) {  // Within 1 meter
        storage.vel_x[idx] = 0.0;
        storage.vel_y[idx] = 0.0;
        storage.vel_z[idx] = 0.0;
        state.current_speed = 0.0;
        return;
    }
    
    // Normalize direction
    f64 inv_dist = 1.0 / distance;
    dx *= inv_dist;
    dy *= inv_dist;
    dz *= inv_dist;
    
    // Compute effective speed
    f64 effective_max_speed = state.max_speed;
    
    // Use real platform speed if available
    const auto& cp = storage.combat[idx];
    if (cp.has_platform_data && cp.max_speed_offroad_mps > 0.0) {
        // Off-road speed as base (conservative; road speed requires roads)
        effective_max_speed = cp.max_speed_offroad_mps;
    } else {
        // Legacy: apply UnitType mobility modifier
        effective_max_speed *= get_unit_modifiers(storage.unit_type[idx]).mobility_factor;
    }
    
    // Apply terrain modifier (legacy method)
    if (config_.terrain_effects_enabled && !config_.use_terrain_semantics) {
        effective_max_speed *= state.terrain_modifier;
        effective_max_speed *= config_.terrain_speed_modifier;
    }
    
    // Apply terrain semantics cost
    state.fuel_modifier = 1.0;  // Reset each tick
    if (config_.use_terrain_semantics && terrain_) {
        auto mc = terrain_->movement_cost(px, py, tick, state.movement_type);
        if (!mc.passable || mc.cost >= 100.0) {
            // Impassable terrain - stop
            storage.vel_x[idx] = 0.0;
            storage.vel_y[idx] = 0.0;
            storage.vel_z[idx] = 0.0;
            state.current_speed = 0.0;
            return;
        }
        // Cost is a multiplier on movement time, so we divide speed by it
        // v1.1.7: safe division — terrain_cost should always be >= 1 but protect anyway
        effective_max_speed = safe::div(effective_max_speed, mc.cost, 0.0);
        // v1.2.3: Capture fuel modifier from terrain/weather for consume_fuel
        state.fuel_modifier = mc.fuel_modifier;
    }
    
    // Simple speed model: instantly reach max speed
    // (More complex acceleration model could be added)
    state.current_speed = effective_max_speed;
    
    // Set velocity
    storage.vel_x[idx] = dx * state.current_speed;
    storage.vel_y[idx] = dy * state.current_speed;
    storage.vel_z[idx] = dz * state.current_speed;
}

void MovementSystem::apply_movement(usize idx, EntityStorage& storage, Tick /*tick*/) {
    // Euler integration: p = p + v * dt
    f64 dt = config_.dt_seconds;
    
    f64 old_x = storage.pos_x[idx];
    f64 old_y = storage.pos_y[idx];
    f64 old_z = storage.pos_z[idx];
    
    storage.pos_x[idx] += storage.vel_x[idx] * dt;
    storage.pos_y[idx] += storage.vel_y[idx] * dt;
    storage.pos_z[idx] += storage.vel_z[idx] * dt;
    
    // v1.1.7: NaN/Inf guard — revert to previous position if integration blew up
    if (!std::isfinite(storage.pos_x[idx])) storage.pos_x[idx] = old_x;
    if (!std::isfinite(storage.pos_y[idx])) storage.pos_y[idx] = old_y;
    if (!std::isfinite(storage.pos_z[idx])) storage.pos_z[idx] = old_z;
    
    // Compute distance traveled
    f64 dx = storage.pos_x[idx] - old_x;
    f64 dy = storage.pos_y[idx] - old_y;
    f64 dz = storage.pos_z[idx] - old_z;
    f64 distance_m = std::sqrt(dx*dx + dy*dy + dz*dz);
    f64 distance_km = distance_m / 1000.0;
    
    // Consume fuel
    if (config_.fuel_required_for_movement && distance_km > 0.0) {
        consume_fuel(idx, storage, distance_km);
    }
}

void MovementSystem::check_waypoint_arrival(usize idx, EntityStorage& storage) {
    auto& queue = waypoints_[idx];
    if (queue.empty()) return;
    
    const Waypoint* wp = queue.current_waypoint();
    if (!wp) return;
    
    // Check distance to waypoint
    f64 dx = wp->x - storage.pos_x[idx];
    f64 dy = wp->y - storage.pos_y[idx];
    f64 dz = wp->z - storage.pos_z[idx];
    f64 distance = std::sqrt(dx*dx + dy*dy + dz*dz);
    
    // Arrival threshold: 50 meters
    constexpr f64 ARRIVAL_THRESHOLD = 50.0;
    
    if (distance < ARRIVAL_THRESHOLD) {
        // Arrived at waypoint
        queue.advance();
        
        // Set next waypoint as target, or stop
        const Waypoint* next = queue.current_waypoint();
        if (next) {
            set_target(idx, next->x, next->y, next->z);
        } else {
            stop(idx);
        }
    }
}

void MovementSystem::consume_fuel(usize idx, EntityStorage& storage, f64 distance_km) {
    f64 consumption = distance_km * config_.fuel_consumption_per_km;
    // v1.2.3: Terrain/weather fuel modifier — muddy terrain, rain, etc. increase consumption
    consumption *= states_[idx].fuel_modifier;
    storage.fuel[idx] -= consumption;
    
    // Clamp to zero
    if (storage.fuel[idx] < 0.0) {
        storage.fuel[idx] = 0.0;
    }
    
    // Update supply level
    storage.supply[idx] = (storage.fuel[idx] + storage.ammo[idx]) * 0.5;
}

Status MovementSystem::update(EntityStorage& storage, Rng& /*rng*/, Tick tick) {
    if (!initialized_) {
        return Error(ErrorCode::CONTEXT_NOT_INITIALIZED,
            "Movement system not initialized");
    }
    
    // Process entities in index order (deterministic)
    for (usize i = 0; i < storage.count; ++i) {
        // Skip inactive entities
        if (!storage.is_active(i)) continue;
        
        auto& state = states_[i];
        
        // Skip stationary entities
        if (state.type == MovementState::Type::STATIONARY) continue;
        
        // Check fuel
        if (config_.fuel_required_for_movement) {
            if (storage.fuel[i] < config_.min_fuel_threshold) {
                stop(i);
                continue;
            }
        }
        
        // Check if following waypoints
        if (!waypoints_[i].empty()) {
            const Waypoint* wp = waypoints_[i].current_waypoint();
            if (wp) {
                state.target_x = wp->x;
                state.target_y = wp->y;
                state.target_z = wp->z;
            }
        }
        
        // Compute velocity towards target
        compute_velocity(i, storage, tick);
        
        // Apply movement
        apply_movement(i, storage, tick);
        
        // Check waypoint arrival
        check_waypoint_arrival(i, storage);
        
        // Update movement flag
        if (state.current_speed > config_.min_speed_threshold) {
            storage.flags[i] |= entity_flags::MOVING;
            // v1.2.0: update heading from velocity direction
            storage.heading[i] = std::atan2(storage.vel_y[i], storage.vel_x[i]);
        } else {
            storage.flags[i] &= ~entity_flags::MOVING;
            // Heading preserved when stationary — unit keeps last facing
        }
    }
    
    return Status();
}

// =============================================================================
// Standalone function
// =============================================================================

Status movement_system_update(EntityStorage& storage, Rng& rng, Tick tick) {
    if (!g_movement_system) {
        return Error(ErrorCode::CONTEXT_NOT_INITIALIZED,
            "Global movement system not set");
    }
    return g_movement_system->update(storage, rng, tick);
}

}  // namespace systems
}  // namespace athena
