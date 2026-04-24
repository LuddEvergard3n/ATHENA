// ATHENA Core - Logistics System
// Contract: Deterministic supply consumption and resupply.
//
// RULES:
// - Consumption/resupply in entity index order
// - Supply chain effects calculated per-tick
// - No floating-point order dependence
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_SYSTEMS_LOGISTICS_HPP
#define ATHENA_SYSTEMS_LOGISTICS_HPP

#include "athena/types.hpp"
#include "athena/entities.hpp"
#include "athena/rng.hpp"
#include <vector>

namespace athena {

// Forward declaration
class Environment;
namespace systems {

// =============================================================================
// Logistics Configuration
// =============================================================================

struct LogisticsConfig {
    // Base consumption rates (per tick)
    f64 base_fuel_consumption = 0.01;
    f64 base_ammo_consumption = 0.005;
    
    // Activity multipliers
    f64 moving_fuel_multiplier = 3.0;     // Moving consumes more fuel
    f64 combat_ammo_multiplier = 5.0;     // Combat consumes more ammo
    f64 combat_fuel_multiplier = 1.5;     // Combat uses more fuel
    
    // Resupply
    f64 depot_resupply_rate = 0.1;        // Supply gain per tick at depot
    f64 resupply_range = 10000.0;         // Meters from depot to resupply
    
    // Supply effects
    f64 low_supply_threshold = 0.2;       // Below this, penalties apply
    f64 critical_supply_threshold = 0.05; // Below this, severe penalties
    f64 low_supply_effectiveness = 0.7;   // Effectiveness at low supply
    f64 critical_supply_effectiveness = 0.3;
    
    // Morale effects
    f64 supply_morale_factor = 0.1;       // How much supply affects morale
    f64 morale_recovery_rate = 0.02;      // Morale recovery per tick
    f64 morale_decay_rate = 0.05;         // Morale decay when unsupplied
};

// =============================================================================
// Supply Point (depot, base, etc.)
// =============================================================================

struct SupplyPoint {
    f64 x;
    f64 y;
    f64 z;
    f64 capacity;         // Maximum supply available
    f64 current_supply;   // Current supply level
    f64 resupply_rate;    // Rate at which depot restocks
    Side side;            // Which side owns this depot
    bool active;
};

// =============================================================================
// Logistics System
// =============================================================================

class LogisticsSystem {
public:
    LogisticsSystem();
    
    // Initialize for given entity capacity
    void init(usize entity_capacity, const LogisticsConfig& config);
    
    // Reset logistics state
    void reset();
    
    // Add supply point
    usize add_supply_point(const SupplyPoint& point);
    
    // Remove supply point
    void remove_supply_point(usize index);
    
    // Get supply points
    const std::vector<SupplyPoint>& supply_points() const { return supply_points_; }
    
    // Main update function - called by scheduler
    Status update(EntityStorage& storage, Rng& rng, Tick tick);
    
    // Get configuration
    const LogisticsConfig& config() const { return config_; }
    
    // Modify config
    void set_fuel_consumption(f64 rate) { config_.base_fuel_consumption = rate; }
    void set_ammo_consumption(f64 rate) { config_.base_ammo_consumption = rate; }

    // v1.2.1: Connect environment for weather-based consumption modifiers
    void set_environment(const Environment* env) { environment_ = env; }

private:
    LogisticsConfig config_;
    const Environment* environment_ = nullptr;
    
    std::vector<SupplyPoint> supply_points_;
    
    usize capacity_;
    bool initialized_;
    
    // Internal: compute consumption for entity
    void compute_consumption(usize idx, EntityStorage& storage);
    
    // Internal: check and apply resupply
    void check_resupply(usize idx, EntityStorage& storage);
    
    // Internal: update morale based on supply
    void update_morale(usize idx, EntityStorage& storage);
    
    // Internal: compute distance to nearest friendly depot
    f64 distance_to_nearest_depot(usize idx, const EntityStorage& storage) const;
    
    // Internal: find nearest depot (returns nullptr if none)
    const SupplyPoint* find_nearest_depot(usize idx, 
                                          const EntityStorage& storage) const;
};

// =============================================================================
// Standalone update function for scheduler registration
// =============================================================================

extern LogisticsSystem* g_logistics_system;

Status logistics_system_update(EntityStorage& storage, Rng& rng, Tick tick);

}  // namespace systems
}  // namespace athena

#endif  // ATHENA_SYSTEMS_LOGISTICS_HPP
