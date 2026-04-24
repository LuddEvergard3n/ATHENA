// ATHENA Core - Logistics System Implementation
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/systems/logistics.hpp"
#include "athena/environment.hpp"
#include "athena/types.hpp"
#include <cmath>
#include <algorithm>

namespace athena {
namespace systems {

// =============================================================================
// Global instance pointer
// =============================================================================

LogisticsSystem* g_logistics_system = nullptr;

// =============================================================================
// LogisticsSystem Implementation
// =============================================================================

LogisticsSystem::LogisticsSystem()
    : config_()
    , supply_points_()
    , capacity_(0)
    , initialized_(false)
{
}

void LogisticsSystem::init(usize entity_capacity, const LogisticsConfig& config) {
    config_ = config;
    capacity_ = entity_capacity;
    supply_points_.clear();
    initialized_ = true;
}

void LogisticsSystem::reset() {
    supply_points_.clear();
}

usize LogisticsSystem::add_supply_point(const SupplyPoint& point) {
    supply_points_.push_back(point);
    return supply_points_.size() - 1;
}

void LogisticsSystem::remove_supply_point(usize index) {
    if (index < supply_points_.size()) {
        supply_points_[index].active = false;
    }
}

f64 LogisticsSystem::distance_to_nearest_depot(usize idx, 
                                                const EntityStorage& storage) const {
    const SupplyPoint* depot = find_nearest_depot(idx, storage);
    if (!depot) return -1.0;
    
    f64 dx = storage.pos_x[idx] - depot->x;
    f64 dy = storage.pos_y[idx] - depot->y;
    f64 dz = storage.pos_z[idx] - depot->z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

const SupplyPoint* LogisticsSystem::find_nearest_depot(usize idx,
                                                       const EntityStorage& storage) const {
    Side entity_side = storage.side[idx];
    const SupplyPoint* nearest = nullptr;
    f64 min_dist = std::numeric_limits<f64>::max();
    
    for (const auto& depot : supply_points_) {
        // Must be active and same side
        if (!depot.active) continue;
        if (depot.side != entity_side) continue;
        
        // Must have supply
        if (depot.current_supply <= 0.0) continue;
        
        // Compute distance
        f64 dx = storage.pos_x[idx] - depot.x;
        f64 dy = storage.pos_y[idx] - depot.y;
        f64 dz = storage.pos_z[idx] - depot.z;
        f64 dist = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        if (dist < min_dist) {
            min_dist = dist;
            nearest = &depot;
        }
    }
    
    return nearest;
}

void LogisticsSystem::compute_consumption(usize idx, EntityStorage& storage) {
    // Base consumption
    f64 fuel_consumption = config_.base_fuel_consumption;
    f64 ammo_consumption = config_.base_ammo_consumption;
    
    // Activity modifiers
    if ((storage.flags[idx] & entity_flags::MOVING) != 0) {
        fuel_consumption *= config_.moving_fuel_multiplier;
    }
    
    if ((storage.flags[idx] & entity_flags::ENGAGED) != 0) {
        fuel_consumption *= config_.combat_fuel_multiplier;
        ammo_consumption *= config_.combat_ammo_multiplier;
    }
    
    // Apply consumption rate from entity
    fuel_consumption *= storage.consumption_rate[idx];
    ammo_consumption *= storage.consumption_rate[idx];

    // v1.2.1: Weather logistics modifier — bad weather increases consumption
    if (environment_ && environment_->is_initialized()) {
        auto mods = environment_->get_modifiers(
            storage.pos_x[idx], storage.pos_y[idx], 0);
        fuel_consumption *= mods.logistics_modifier;
        ammo_consumption *= mods.logistics_modifier;

        // Equipment degradation: directly reduces health over time
        if (mods.equipment_degradation > 0.0) {
            storage.health[idx] -= mods.equipment_degradation;
            storage.health[idx] = safe::clamp(storage.health[idx], 0.0, 1.0);
        }
    }
    
    // Consume
    storage.fuel[idx] -= fuel_consumption;
    storage.ammo[idx] -= ammo_consumption;
    
    // Clamp (NaN-safe)
    storage.fuel[idx] = safe::clamp(storage.fuel[idx], 0.0, 1.0);
    storage.ammo[idx] = safe::clamp(storage.ammo[idx], 0.0, 1.0);
    
    // Update combined supply
    storage.supply[idx] = (storage.fuel[idx] + storage.ammo[idx]) * 0.5;
}

void LogisticsSystem::check_resupply(usize idx, EntityStorage& storage) {
    const SupplyPoint* depot = find_nearest_depot(idx, storage);
    if (!depot) return;
    
    // Compute distance
    f64 dx = storage.pos_x[idx] - depot->x;
    f64 dy = storage.pos_y[idx] - depot->y;
    f64 dz = storage.pos_z[idx] - depot->z;
    f64 dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    
    // Check if in range
    if (dist > config_.resupply_range) return;
    
    // Compute resupply amount (decreases with distance)
    // safe::div guards against resupply_range == 0
    f64 range_factor = 1.0 - safe::div(dist, config_.resupply_range, 1.0);
    f64 resupply_amount = config_.depot_resupply_rate * range_factor;
    
    // Limit by depot supply
    // Note: In production, we'd track depot consumption
    // For now, assume depots have unlimited supply
    
    // Apply resupply
    storage.fuel[idx] += resupply_amount;
    storage.ammo[idx] += resupply_amount;
    
    // Clamp to max (1.0)
    if (storage.fuel[idx] > 1.0) storage.fuel[idx] = 1.0;
    if (storage.ammo[idx] > 1.0) storage.ammo[idx] = 1.0;
    
    // Update combined supply
    storage.supply[idx] = (storage.fuel[idx] + storage.ammo[idx]) * 0.5;
}

void LogisticsSystem::update_morale(usize idx, EntityStorage& storage) {
    f64 supply_level = storage.supply[idx];
    
    // Morale effects based on supply
    if (supply_level < config_.critical_supply_threshold) {
        // Critical supply - morale decays
        storage.morale[idx] -= config_.morale_decay_rate;
    } else if (supply_level < config_.low_supply_threshold) {
        // Low supply - slow decay
        storage.morale[idx] -= config_.morale_decay_rate * 0.5;
    } else {
        // Good supply - morale recovers
        storage.morale[idx] += config_.morale_recovery_rate;
    }
    
    // Clamp morale (NaN-safe)
    storage.morale[idx] = safe::clamp(storage.morale[idx], 0.0, 1.0);
    
    // Update readiness based on supply and morale
    f64 supply_factor = 1.0;
    if (supply_level < config_.critical_supply_threshold) {
        supply_factor = config_.critical_supply_effectiveness;
    } else if (supply_level < config_.low_supply_threshold) {
        supply_factor = config_.low_supply_effectiveness;
    }
    
    storage.readiness[idx] = safe::clamp(
        supply_factor * storage.morale[idx] * storage.health[idx], 0.0, 1.0);
}

Status LogisticsSystem::update(EntityStorage& storage, Rng& /*rng*/, Tick /*tick*/) {
    if (!initialized_) {
        return Error(ErrorCode::CONTEXT_NOT_INITIALIZED,
            "Logistics system not initialized");
    }
    
    // Process entities in index order (deterministic)
    for (usize i = 0; i < storage.count; ++i) {
        if (!storage.is_active(i)) continue;
        
        // Compute and apply consumption
        compute_consumption(i, storage);
        
        // Check for resupply opportunities
        check_resupply(i, storage);
        
        // Update morale based on supply
        update_morale(i, storage);
    }
    
    // Update depot supplies (if tracking)
    for (auto& depot : supply_points_) {
        if (!depot.active) continue;
        
        // Depots slowly resupply themselves
        depot.current_supply += depot.resupply_rate;
        if (depot.current_supply > depot.capacity) {
            depot.current_supply = depot.capacity;
        }
    }
    
    return Status();
}

// =============================================================================
// Standalone function
// =============================================================================

Status logistics_system_update(EntityStorage& storage, Rng& rng, Tick tick) {
    if (!g_logistics_system) {
        return Error(ErrorCode::CONTEXT_NOT_INITIALIZED,
            "Global logistics system not set");
    }
    return g_logistics_system->update(storage, rng, tick);
}

}  // namespace systems
}  // namespace athena
