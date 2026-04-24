// ATHENA Core - Movement System
// Contract: Deterministic movement with terrain effects.
//
// RULES:
// - Movement order by entity index (deterministic)
// - Terrain effects applied uniformly
// - No floating-point order dependence
// - All calculations in double precision
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_SYSTEMS_MOVEMENT_HPP
#define ATHENA_SYSTEMS_MOVEMENT_HPP

#include "athena/types.hpp"
#include "athena/entities.hpp"
#include "athena/rng.hpp"
#include "athena/terrain_semantics.hpp"
#include <cmath>

namespace athena {
namespace systems {

// =============================================================================
// Movement Configuration
// =============================================================================

struct MovementConfig {
    // Time step in seconds (matches tick_duration_seconds from scenario)
    f64 dt_seconds = 3600.0;  // Default 1 hour
    
    // Terrain modifiers
    f64 terrain_speed_modifier = 1.0;  // Global terrain effect
    bool terrain_effects_enabled = true;
    
    // Use TerrainSemantics for movement costs (NEW)
    bool use_terrain_semantics = false;
    
    // Fuel consumption
    f64 fuel_consumption_per_km = 0.001;  // Fuel units per km traveled
    bool fuel_required_for_movement = true;
    
    // Movement thresholds
    f64 min_speed_threshold = 0.1;  // m/s below which entity stops
    f64 min_fuel_threshold = 0.01;  // Fuel below which entity cannot move
};

// =============================================================================
// Waypoint System
// =============================================================================

struct Waypoint {
    f64 x;
    f64 y;
    f64 z;
    f64 arrival_speed;  // Desired speed at waypoint (0 = stop)
};

// Waypoint queue per entity (stored separately, not in SoA for simplicity)
// In production, this would be in a separate SoA structure
struct WaypointQueue {
    static constexpr usize MAX_WAYPOINTS = 16;
    
    Waypoint waypoints[MAX_WAYPOINTS];
    usize count;
    usize current;
    
    WaypointQueue() : count(0), current(0) {}
    
    bool empty() const { return current >= count; }
    
    const Waypoint* current_waypoint() const {
        if (empty()) return nullptr;
        return &waypoints[current];
    }
    
    void advance() {
        if (current < count) current++;
    }
    
    void clear() {
        count = 0;
        current = 0;
    }
    
    bool add(const Waypoint& wp) {
        if (count >= MAX_WAYPOINTS) return false;
        waypoints[count++] = wp;
        return true;
    }
};

// =============================================================================
// Movement State (per-entity, stored in auxiliary arrays)
// =============================================================================

struct MovementState {
    // Current movement target
    f64 target_x;
    f64 target_y;
    f64 target_z;
    
    // Movement parameters
    f64 max_speed;       // m/s
    f64 current_speed;   // m/s
    f64 acceleration;    // m/s^2
    
    // Terrain effect at current position
    f64 terrain_modifier;

    // v1.2.3: Fuel consumption modifier from terrain/weather (1.0 = normal, >1 = more)
    f64 fuel_modifier;
    
    // Movement type
    enum class Type : u8 {
        STATIONARY = 0,
        MOVING = 1,
        APPROACHING = 2  // Decelerating to waypoint
    } type;
    
    // Entity movement type for terrain semantics
    MovementType movement_type;
    
    MovementState() 
        : target_x(0), target_y(0), target_z(0)
        , max_speed(0), current_speed(0), acceleration(0)
        , terrain_modifier(1.0)
        , fuel_modifier(1.0)
        , type(Type::STATIONARY)
        , movement_type(MovementType::FOOT) {}
};

// =============================================================================
// Movement System
// =============================================================================

class MovementSystem {
public:
    MovementSystem();
    
    // Initialize for given entity capacity
    void init(usize capacity, const MovementConfig& config);
    
    // Reset all movement state
    void reset();
    
    // Set terrain semantics for movement cost calculations (NEW)
    void set_terrain(const TerrainSemantics* terrain);
    
    // Set movement type for entity (NEW)
    void set_movement_type(usize entity_idx, MovementType type);
    
    // Set movement target for entity
    void set_target(usize entity_idx, f64 x, f64 y, f64 z);
    
    // Set maximum speed for entity (m/s)
    void set_max_speed(usize entity_idx, f64 speed);
    
    // Set terrain modifier for entity position
    void set_terrain_modifier(usize entity_idx, f64 modifier);
    
    // Stop entity
    void stop(usize entity_idx);
    
    // Add waypoint to entity's queue
    bool add_waypoint(usize entity_idx, const Waypoint& wp);
    
    // Clear entity's waypoint queue
    void clear_waypoints(usize entity_idx);
    
    // Get movement state (read-only)
    const MovementState& get_state(usize entity_idx) const;
    
    // Main update function - called by scheduler
    // Updates positions based on velocities and terrain
    // Deterministic: iterates by index order
    Status update(EntityStorage& storage, Rng& rng, Tick tick);
    
    // Get configuration
    const MovementConfig& config() const { return config_; }

private:
    MovementConfig config_;
    const TerrainSemantics* terrain_;
    
    // Per-entity movement state
    std::vector<MovementState> states_;
    std::vector<WaypointQueue> waypoints_;
    
    usize capacity_;
    bool initialized_;
    
    // Internal: compute velocity towards target
    void compute_velocity(usize idx, EntityStorage& storage, Tick tick);
    
    // Internal: apply movement for one entity
    void apply_movement(usize idx, EntityStorage& storage, Tick tick);
    
    // Internal: check waypoint arrival
    void check_waypoint_arrival(usize idx, EntityStorage& storage);
    
    // Internal: consume fuel for movement
    void consume_fuel(usize idx, EntityStorage& storage, f64 distance_km);
    
    // Internal: get terrain cost at position (NEW)
    f64 get_terrain_cost(f64 x, f64 y, Tick tick, MovementType type) const;
};

// =============================================================================
// Standalone update function for scheduler registration
// =============================================================================

// Global movement system instance (set before simulation)
extern MovementSystem* g_movement_system;

// Scheduler-compatible update function
Status movement_system_update(EntityStorage& storage, Rng& rng, Tick tick);

}  // namespace systems
}  // namespace athena

#endif  // ATHENA_SYSTEMS_MOVEMENT_HPP
