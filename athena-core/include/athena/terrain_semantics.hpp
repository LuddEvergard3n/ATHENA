// ATHENA Core - Terrain Semantics (Semantic Layer)
// Contract: Derived operational meaning from Physical + Environmental layers.
//
// ARCHITECTURE:
// Layer 3 (Semantic) - This file
//   - Movement cost, defense advantage, concealment
//   - NEVER stored, always computed on demand
//   - Pure functions of (Physical, Environment, x, y, t)
//
// RULES:
// - No state storage (derive everything)
// - No opinions, only calculations
// - All parameters documented with source
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_TERRAIN_SEMANTICS_HPP
#define ATHENA_TERRAIN_SEMANTICS_HPP

#include "athena/types.hpp"
#include "athena/terrain.hpp"
#include "athena/environment.hpp"

namespace athena {

// =============================================================================
// Movement Types (for cost calculation)
// =============================================================================

enum class MovementType : u8 {
    FOOT = 0,           // Infantry on foot
    WHEELED,            // Wheeled vehicles
    TRACKED,            // Tracked vehicles (tanks, IFVs)
    AMPHIBIOUS,         // Can cross water
    HELICOPTER,         // Low altitude rotary
    FIXED_WING,         // Aircraft (minimal terrain effect)
    NAVAL               // Water only
};

// =============================================================================
// Sensor Types (for effectiveness calculation)
// =============================================================================

enum class SensorType : u8 {
    VISUAL = 0,         // Mk 1 eyeball, optics
    THERMAL,            // IR, thermal imaging
    RADAR,              // Ground/air radar
    ACOUSTIC,           // Sound detection
    SIGNALS,             // Signals intelligence
    SEISMIC             // Ground vibration
};

// =============================================================================
// Semantic Query Results
// =============================================================================

struct MovementCost {
    f64 cost;           // Multiplier (1.0 = normal, 2.0 = half speed)
    bool passable;      // Can unit traverse at all?
    f64 fuel_modifier;  // Extra fuel consumption (1.0 = normal)
    f64 fatigue_rate;   // Personnel fatigue accumulation
};

struct DefenseValue {
    f64 cover;          // Protection from fire (0-1)
    f64 concealment;    // Protection from detection (0-1)
    f64 advantage;      // Combined defensive advantage multiplier
};

struct SensorEffectiveness {
    f64 detection_modifier;      // Range multiplier
    f64 identification_modifier; // Accuracy multiplier
    f64 false_positive_rate;     // Increased by clutter
};

// =============================================================================
// Terrain Semantics (Layer 3 - Derived)
// =============================================================================

class TerrainSemantics {
public:
    TerrainSemantics();
    
    /// Initialize with physical terrain and environment references
    /// NOTE: Does not own these - caller must keep them alive
    Status init(const PhysicalTerrain* physical, const Environment* environment);
    
    /// Reset
    void reset();
    
    // =========================================================================
    // Movement Queries
    // =========================================================================
    
    /// Get movement cost at point for given unit type
    /// Returns cost multiplier (1.0 = normal speed)
    MovementCost movement_cost(f64 x, f64 y, Tick tick, MovementType type) const;
    
    /// Check if point is passable for unit type
    bool is_passable(f64 x, f64 y, Tick tick, MovementType type) const;
    
    /// Get optimal movement direction from point (steepest descent for ground)
    f64 optimal_direction(f64 x, f64 y, MovementType type) const;
    
    // =========================================================================
    // Combat Queries
    // =========================================================================
    
    /// Get defensive value at point
    DefenseValue defense_value(f64 x, f64 y, Tick tick) const;
    
    /// Get cover value (protection from fire)
    f64 cover(f64 x, f64 y, Tick tick) const;
    
    /// Get concealment value (protection from detection)
    f64 concealment(f64 x, f64 y, Tick tick) const;
    
    /// Get engagement range modifier (terrain effect on weapon range)
    f64 engagement_range_modifier(f64 x, f64 y, Tick tick) const;
    
    // =========================================================================
    // Sensor Queries
    // =========================================================================
    
    /// Get sensor effectiveness at point
    SensorEffectiveness sensor_effectiveness(
        f64 x, f64 y, Tick tick, SensorType sensor) const;
    
    /// Check line of sight between two points
    /// Returns fraction visible (0 = blocked, 1 = clear)
    f64 line_of_sight(f64 x1, f64 y1, f64 x2, f64 y2, Tick tick) const;
    
    /// v1.2.0: Get elevation at point (forwarded from physical layer)
    f64 elevation(f64 x, f64 y) const;
    
    /// Get detection probability modifier
    f64 detection_modifier(f64 x, f64 y, Tick tick, SensorType sensor) const;
    
    // =========================================================================
    // Logistics Queries
    // =========================================================================
    
    /// Get logistics penalty at point (supply consumption modifier)
    f64 logistics_penalty(f64 x, f64 y, Tick tick) const;
    
    /// Get equipment degradation rate
    f64 equipment_degradation(f64 x, f64 y, Tick tick) const;
    
    /// Get personnel attrition rate (environmental casualties)
    f64 personnel_attrition(f64 x, f64 y, Tick tick) const;
    
    // =========================================================================
    // Operational Queries
    // =========================================================================
    
    /// Is this a natural chokepoint?
    f64 chokepoint_value(f64 x, f64 y) const;
    
    /// Suitability for armor operations (0 = bad, 1 = ideal)
    f64 armor_suitability(f64 x, f64 y, Tick tick) const;
    
    /// Suitability for infantry operations (0 = bad, 1 = ideal)
    f64 infantry_suitability(f64 x, f64 y, Tick tick) const;
    
    /// Suitability for air operations (0 = bad, 1 = ideal)
    f64 air_suitability(f64 x, f64 y, Tick tick) const;
    
    // =========================================================================
    // Bulk Queries
    // =========================================================================
    
    /// Compute movement cost along path
    f64 path_cost(const std::vector<std::pair<f64, f64>>& path,
                  Tick tick, MovementType type) const;
    
    /// Find best defensive positions in area
    std::vector<std::pair<f64, f64>> find_defensive_positions(
        f64 center_x, f64 center_y, f64 radius,
        Tick tick, int max_results) const;

private:
    const PhysicalTerrain* physical_;
    const Environment* environment_;
    bool initialized_;
    
    // Internal computation helpers
    f64 slope_movement_cost(f64 slope_deg, MovementType type) const;
    f64 overlay_movement_cost(OverlayType overlay, MovementType type) const;
    f64 overlay_cover(OverlayType overlay) const;
    f64 overlay_concealment(OverlayType overlay) const;
    f64 overlay_sensor_effect(OverlayType overlay, SensorType sensor) const;
};

// =============================================================================
// Cost Tables (documented sources)
// =============================================================================

namespace terrain_tables {

// Movement cost multipliers by slope (degrees)
// Source: FM 5-33 Terrain Analysis, US Army
// 0-5°: normal, 5-15°: +50%, 15-30°: +100%, 30-45°: +200%, >45°: impassable
constexpr f64 SLOPE_COST_0_5 = 1.0;
constexpr f64 SLOPE_COST_5_15 = 1.5;
constexpr f64 SLOPE_COST_15_30 = 2.0;
constexpr f64 SLOPE_COST_30_45 = 3.0;
constexpr f64 SLOPE_IMPASSABLE = 45.0;  // degrees

// Movement cost by overlay type
// Source: FM 5-33, adapted for simulation
struct OverlayMovementCosts {
    f64 foot;
    f64 wheeled;
    f64 tracked;
};

constexpr OverlayMovementCosts FOREST_COST = {1.5, 3.0, 2.0};
constexpr OverlayMovementCosts URBAN_COST = {1.2, 1.5, 2.0};
constexpr OverlayMovementCosts SWAMP_COST = {2.5, 999.0, 3.0};  // 999 = impassable
constexpr OverlayMovementCosts RIVER_COST = {999.0, 999.0, 999.0};  // Need bridge
constexpr OverlayMovementCosts ROAD_COST = {0.8, 0.5, 0.7};  // Faster than open

// Cover values by overlay (0-1)
// Source: FM 7-8 Infantry Rifle Platoon and Squad
constexpr f64 FOREST_COVER = 0.6;
constexpr f64 URBAN_COVER = 0.8;
constexpr f64 SWAMP_COVER = 0.3;
constexpr f64 OPEN_COVER = 0.1;

// Concealment values by overlay (0-1)
constexpr f64 FOREST_CONCEALMENT = 0.8;
constexpr f64 URBAN_CONCEALMENT = 0.7;
constexpr f64 SWAMP_CONCEALMENT = 0.5;
constexpr f64 OPEN_CONCEALMENT = 0.1;

}  // namespace terrain_tables

}  // namespace athena

#endif  // ATHENA_TERRAIN_SEMANTICS_HPP
