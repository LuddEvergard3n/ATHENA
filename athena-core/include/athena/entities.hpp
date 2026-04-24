// ATHENA Core - Entity Storage (SoA)
// Contract: All entity data in Structure of Arrays format.
//
// RULES:
// - SoA always (no AoS in core)
// - Contiguous memory
// - Explicit alignment
// - Zero allocation during simulation
// - Deterministic iteration order (by ID)
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_ENTITIES_HPP
#define ATHENA_ENTITIES_HPP

#include "types.hpp"
#include <vector>
#include <cstring>
#include <string>

namespace athena {

// =============================================================================
// Entity Flags
// =============================================================================

namespace entity_flags {
    constexpr u8 NONE     = 0x00;
    constexpr u8 ACTIVE   = 0x01;  // Entity is active in simulation
    constexpr u8 DEAD     = 0x02;  // Entity is dead/destroyed
    constexpr u8 MOVING   = 0x04;  // Entity is currently moving
    constexpr u8 ENGAGED  = 0x08;  // Entity is in combat
    constexpr u8 FATIGUED = 0x10;  // v1.1.8: fatigue > 0.7 — degraded performance
    constexpr u8 BROKEN   = 0x20;  // v1.1.8: cohesion < threshold — operationally dead
    constexpr u8 SUPPRESSED = 0x40; // v1.2.0: under effective fire — accuracy/movement degraded
}

// =============================================================================
// Unit Type Classification
// =============================================================================

enum class UnitType : u8 {
    GENERIC         = 0,
    LIGHT_INFANTRY  = 1,
    HEAVY_INFANTRY  = 2,
    MECHANIZED      = 3,
    ARMOR           = 4,
    ARTILLERY       = 5,
    AIR_DEFENSE     = 6,
    LOGISTICS       = 7,
    MEDICAL         = 8,
    RECON           = 9,
    SPECIAL_FORCES  = 10,
    ENGINEER        = 11,
    FIGHTER         = 12,
    ATTACK_HELO     = 13,
    NAVAL           = 14,
    MAX_TYPES       = 15
};

// Static modifier table: indexed by UnitType.
// These are multiplicative factors applied to base stats.
struct UnitTypeModifiers {
    f64 mobility_factor;     // Multiplied with base speed
    f64 firepower_factor;    // Multiplied with base firepower
    f64 defense_factor;      // Reduces incoming damage (higher = tougher)
    f64 morale_impact;       // Morale effect on nearby friendlies
    f64 supply_weight;       // Logistics consumption rate
    f64 detection_factor;    // Detection range modifier
};

// Design rationale:
//   - Armor: slow, devastating firepower, very tough, high logistics cost
//   - Light infantry: fast, low firepower, fragile, low cost
//   - Artillery: immobile, high firepower, fragile, long range
//   - Medical/Logistics: non-combat, support roles
//   - Recon: very fast, low firepower, fragile, excellent detection
//   - Special Forces: fast, good firepower, moderate defense, high morale
//   - Engineer: slow, low firepower, moderate defense, obstacle clearance
//   - Fighter: very fast, high firepower, fragile, extreme range sensors
//   - Attack Helo: fast, devastating firepower, fragile, good sensors
//   - Naval: slow, very high firepower, very tough, long range sensors
inline constexpr UnitTypeModifiers UNIT_TYPE_TABLE[] = {
    // mobility  firepower  defense  morale  supply  detection
    {  1.0,      1.0,       1.0,     0.0,    1.0,    1.0  },  // GENERIC
    {  1.5,      0.6,       0.5,     0.1,    0.5,    1.2  },  // LIGHT_INFANTRY
    {  1.0,      1.0,       0.8,     0.15,   0.8,    1.0  },  // HEAVY_INFANTRY
    {  1.3,      1.4,       1.2,     0.2,    1.5,    1.1  },  // MECHANIZED
    {  0.7,      2.5,       2.0,     0.3,    2.5,    0.8  },  // ARMOR
    {  0.3,      3.0,       0.4,     0.1,    1.8,    0.5  },  // ARTILLERY
    {  0.5,      1.5,       0.6,     0.1,    1.2,    2.0  },  // AIR_DEFENSE
    {  0.8,      0.1,       0.3,     0.05,   0.3,    0.6  },  // LOGISTICS
    {  0.8,      0.0,       0.3,     0.2,    0.4,    0.6  },  // MEDICAL
    {  2.0,      0.4,       0.3,     0.1,    0.4,    2.5  },  // RECON
    {  1.8,      1.2,       0.7,     0.35,   0.6,    1.8  },  // SPECIAL_FORCES
    {  0.6,      0.3,       0.8,     0.05,   1.0,    0.8  },  // ENGINEER
    {  5.0,      2.0,       0.3,     0.2,    3.0,    3.0  },  // FIGHTER
    {  3.0,      2.8,       0.4,     0.25,   2.0,    2.2  },  // ATTACK_HELO
    {  0.4,      3.5,       2.5,     0.3,    3.0,    2.5  },  // NAVAL
};

// Lookup helper (bounds-checked)
inline const UnitTypeModifiers& get_unit_modifiers(UnitType t) {
    auto idx = static_cast<u8>(t);
    if (idx >= static_cast<u8>(UnitType::MAX_TYPES)) idx = 0;
    return UNIT_TYPE_TABLE[idx];
}

// Parse unit type from string (case-insensitive match)
inline UnitType unit_type_from_string(const std::string& s) {
    if (s == "light_infantry"  || s == "LightInfantry")  return UnitType::LIGHT_INFANTRY;
    if (s == "heavy_infantry"  || s == "HeavyInfantry")  return UnitType::HEAVY_INFANTRY;
    if (s == "mechanized"      || s == "Mechanized")     return UnitType::MECHANIZED;
    if (s == "armor"           || s == "Armor")           return UnitType::ARMOR;
    if (s == "artillery"       || s == "Artillery")       return UnitType::ARTILLERY;
    if (s == "air_defense"     || s == "AirDefense")      return UnitType::AIR_DEFENSE;
    if (s == "logistics"       || s == "Logistics")       return UnitType::LOGISTICS;
    if (s == "medical"         || s == "Medical")         return UnitType::MEDICAL;
    if (s == "recon"           || s == "Recon")           return UnitType::RECON;
    if (s == "special_forces"  || s == "SpecialForces")   return UnitType::SPECIAL_FORCES;
    if (s == "engineer"        || s == "Engineer")        return UnitType::ENGINEER;
    if (s == "fighter"         || s == "Fighter")         return UnitType::FIGHTER;
    if (s == "attack_helo"     || s == "AttackHelo")      return UnitType::ATTACK_HELO;
    if (s == "naval"           || s == "Naval")           return UnitType::NAVAL;
    return UnitType::GENERIC;
}

inline const char* unit_type_to_string(UnitType t) {
    switch (t) {
        case UnitType::LIGHT_INFANTRY: return "Light Infantry";
        case UnitType::HEAVY_INFANTRY: return "Heavy Infantry";
        case UnitType::MECHANIZED:     return "Mechanized";
        case UnitType::ARMOR:          return "Armor";
        case UnitType::ARTILLERY:      return "Artillery";
        case UnitType::AIR_DEFENSE:    return "Air Defense";
        case UnitType::LOGISTICS:      return "Logistics";
        case UnitType::MEDICAL:        return "Medical";
        case UnitType::RECON:          return "Recon";
        case UnitType::SPECIAL_FORCES: return "Special Forces";
        case UnitType::ENGINEER:       return "Engineer";
        case UnitType::FIGHTER:        return "Fighter";
        case UnitType::ATTACK_HELO:    return "Attack Helo";
        case UnitType::NAVAL:          return "Naval";
        default:                       return "Generic";
    }
}

// =============================================================================
// Side (deterministic enum)
// =============================================================================

enum class Side : u8 {
    NEUTRAL = 0,
    BLUE = 1,
    RED = 2,
    GREEN = 3,
    ORANGE = 4,
    YELLOW = 5,
    // Reserve 6-15 for future use
    MAX_SIDES = 16
};

// =============================================================================
// Combat Profile — derived from real platform specs
// =============================================================================
// When an entity is backed by a PlatformSpec (via platform_id),
// this struct holds the spec-derived values used in combat resolution.
// The combat system uses penetration-vs-armor instead of abstract ratings.
// If has_platform_data is false, the legacy Lanchester model is used.

struct CombatProfile {
    bool has_platform_data = false;    // True if derived from PlatformSpec

    // Armament (main weapon, best available round)
    f64 penetration_mm   = 0.0;       // Best AP round penetration (mm RHA)
    f64 caliber_mm       = 0.0;       // Main weapon caliber
    f64 muzzle_vel_mps   = 0.0;       // Muzzle velocity (m/s)
    f64 rate_of_fire_rpm = 0.0;       // Rounds per minute
    f64 effective_range_m = 0.0;      // Weapon effective range (m)
    i32 ammo_carried     = 0;         // Total rounds

    // Protection (mm RHA equivalent)
    f64 armor_front_mm   = 0.0;       // Turret/hull front (whichever higher)
    f64 armor_side_mm    = 0.0;
    f64 armor_rear_mm    = 0.0;
    f64 armor_top_mm     = 0.0;
    bool has_aps         = false;     // Active Protection System
    bool has_era         = false;     // Explosive Reactive Armor

    // Sensors
    f64 sensor_range_m   = 2000.0;    // Effective detection range
    i32 thermal_gen      = 0;         // 0=none, 1-4
    bool has_lrf         = false;     // Laser rangefinder
    bool has_radar       = false;     // Active radar

    // Mobility (m/s, converted from km/h at load time)
    f64 max_speed_road_mps    = 0.0;
    f64 max_speed_offroad_mps = 0.0;
    f64 range_km              = 0.0;

    // v1.2.0: Indirect fire capability
    bool is_indirect          = false;   // True for artillery, mortars, MLRS
    f64 splash_radius_m       = 0.0;     // HE blast radius (0 = point target only)
    f64 min_range_m           = 0.0;     // Minimum firing range (dead zone for arty)

    // v1.2.3: Drive type from PlatformSpec for MovementType inference
    // 0=foot, 1=wheeled, 2=tracked, 3=fixed_wing, 4=helicopter, 5=naval
    u8 drive_type_code        = 0;
};

// =============================================================================
// Entity Storage (SoA)
// =============================================================================

// Structure of Arrays for entity data
// All arrays are parallel - same index = same entity
// Allocated once at init, never during simulation
struct EntityStorage {
    // Maximum capacity (fixed at creation)
    usize capacity;
    
    // Current count of active entities
    usize count;
    
    // === Identity ===
    std::vector<EntityId> id;           // Unique ID (index in sparse set)
    std::vector<u8> flags;              // Bitfield (entity_flags::)
    std::vector<Side> side;             // Which side
    std::vector<UnitType> unit_type;    // Classification (for display/NATO symbol)
    std::vector<std::string> platform_id; // PlatformSpec ID (empty = no platform)
    
    // === Position (3D) ===
    std::vector<f64> pos_x;             // X coordinate (meters)
    std::vector<f64> pos_y;             // Y coordinate (meters)  
    std::vector<f64> pos_z;             // Z coordinate (meters, altitude)
    
    // === Velocity ===
    std::vector<f64> vel_x;             // X velocity (m/s)
    std::vector<f64> vel_y;             // Y velocity (m/s)
    std::vector<f64> vel_z;             // Z velocity (m/s)
    
    // === Orientation (v1.2.0) ===
    std::vector<f64> heading;           // Facing direction (radians, 0=East, CCW)

    // === State ===
    std::vector<f64> health;            // Health [0, 1]
    std::vector<f64> supply;            // Supply level [0, 1]
    std::vector<f64> morale;            // Morale [0, 1]
    std::vector<f64> readiness;         // Combat readiness [0, 1]
    std::vector<f64> suppression;       // v1.2.0: suppression level [0, 1]
    
    // === Combat (legacy scalars — used when CombatProfile not available) ===
    std::vector<f64> firepower;         // Firepower rating
    std::vector<f64> detection_range;   // Detection range (meters)
    std::vector<f64> engagement_range;  // Max engagement range (meters)
    
    // === Platform-derived combat data ===
    std::vector<CombatProfile> combat;  // Real specs per entity
    
    // === Logistics ===
    std::vector<f64> fuel;              // Fuel remaining [0, 1]
    std::vector<f64> ammo;              // Ammo remaining [0, 1]
    std::vector<f64> consumption_rate;  // Supply consumption per tick

    // === Attrition Memory (v1.1.8) ===
    // Persistent per-entity state that accumulates across ticks.
    // These fields represent the cumulative operational wear on a unit.
    std::vector<f64> fatigue;           // [0, 1] — 0 = fresh, 1 = exhausted
    std::vector<f64> cohesion;          // [0, 1] — organizational effectiveness
    std::vector<u32> stress_ticks;      // Consecutive ticks under fire or moving
    std::vector<u32> rest_ticks;        // Consecutive ticks stationary & not engaged
    std::vector<f64> casualty_rate;     // Rolling casualty accumulator (health lost per tick, decayed)
    
    // Default constructor
    EntityStorage() : capacity(0), count(0) {}
    
    // Initialize with capacity (allocate once)
    void init(usize cap) {
        capacity = cap;
        count = 0;
        
        // Allocate all arrays
        id.resize(cap, INVALID_ENTITY);
        flags.resize(cap, entity_flags::NONE);
        side.resize(cap, Side::NEUTRAL);
        unit_type.resize(cap, UnitType::GENERIC);
        platform_id.resize(cap);
        
        pos_x.resize(cap, 0.0);
        pos_y.resize(cap, 0.0);
        pos_z.resize(cap, 0.0);
        
        vel_x.resize(cap, 0.0);
        vel_y.resize(cap, 0.0);
        vel_z.resize(cap, 0.0);
        
        heading.resize(cap, 0.0);   // v1.2.0: default facing East
        
        health.resize(cap, 1.0);
        supply.resize(cap, 1.0);
        morale.resize(cap, 1.0);
        readiness.resize(cap, 1.0);
        suppression.resize(cap, 0.0);  // v1.2.0
        
        firepower.resize(cap, 0.0);
        detection_range.resize(cap, 0.0);
        engagement_range.resize(cap, 0.0);
        
        combat.resize(cap);
        
        fuel.resize(cap, 1.0);
        ammo.resize(cap, 1.0);
        consumption_rate.resize(cap, 0.0);

        // Attrition memory (v1.1.8)
        fatigue.resize(cap, 0.0);
        cohesion.resize(cap, 1.0);       // Start fully cohesive
        stress_ticks.resize(cap, 0);
        rest_ticks.resize(cap, 0);
        casualty_rate.resize(cap, 0.0);
    }
    
    // Clear all data (reset to initial state)
    // Does NOT deallocate - just zeros
    void clear() {
        count = 0;
        
        // Zero all arrays (keep capacity)
        std::fill(id.begin(), id.end(), INVALID_ENTITY);
        std::fill(flags.begin(), flags.end(), entity_flags::NONE);
        std::fill(side.begin(), side.end(), Side::NEUTRAL);
        std::fill(unit_type.begin(), unit_type.end(), UnitType::GENERIC);
        for (auto& s : platform_id) s.clear();
        
        std::fill(pos_x.begin(), pos_x.end(), 0.0);
        std::fill(pos_y.begin(), pos_y.end(), 0.0);
        std::fill(pos_z.begin(), pos_z.end(), 0.0);
        
        std::fill(vel_x.begin(), vel_x.end(), 0.0);
        std::fill(vel_y.begin(), vel_y.end(), 0.0);
        std::fill(vel_z.begin(), vel_z.end(), 0.0);
        
        std::fill(heading.begin(), heading.end(), 0.0);  // v1.2.0
        
        std::fill(health.begin(), health.end(), 1.0);
        std::fill(supply.begin(), supply.end(), 1.0);
        std::fill(morale.begin(), morale.end(), 1.0);
        std::fill(readiness.begin(), readiness.end(), 1.0);
        std::fill(suppression.begin(), suppression.end(), 0.0);  // v1.2.0
        
        std::fill(firepower.begin(), firepower.end(), 0.0);
        std::fill(detection_range.begin(), detection_range.end(), 0.0);
        std::fill(engagement_range.begin(), engagement_range.end(), 0.0);
        
        std::fill(combat.begin(), combat.end(), CombatProfile{});
        
        std::fill(fuel.begin(), fuel.end(), 1.0);
        std::fill(ammo.begin(), ammo.end(), 1.0);
        std::fill(consumption_rate.begin(), consumption_rate.end(), 0.0);

        // Attrition memory (v1.1.8)
        std::fill(fatigue.begin(), fatigue.end(), 0.0);
        std::fill(cohesion.begin(), cohesion.end(), 1.0);
        std::fill(stress_ticks.begin(), stress_ticks.end(), 0u);
        std::fill(rest_ticks.begin(), rest_ticks.end(), 0u);
        std::fill(casualty_rate.begin(), casualty_rate.end(), 0.0);
    }
    
    // Check if index is valid and active
    bool is_active(usize idx) const {
        if (idx >= capacity) return false;
        return (flags[idx] & entity_flags::ACTIVE) != 0;
    }
    
    // Check if index is dead
    bool is_dead(usize idx) const {
        if (idx >= capacity) return false;
        return (flags[idx] & entity_flags::DEAD) != 0;
    }

    // v1.1.8: Check if entity is operationally effective
    // Active AND not broken (cohesion collapse). A broken unit exists physically
    // but cannot act as a coordinated fighting force.
    bool is_operational(usize idx) const {
        if (idx >= capacity) return false;
        return (flags[idx] & entity_flags::ACTIVE) != 0 &&
               (flags[idx] & entity_flags::BROKEN) == 0;
    }
};

// =============================================================================
// Entity Manager
// =============================================================================

class EntityManager {
public:
    EntityManager() : next_id_(0) {}
    
    // Initialize with capacity
    void init(usize capacity) {
        storage_.init(capacity);
        next_id_ = 0;
    }
    
    // Create a new entity, returns ID
    // Returns INVALID_ENTITY if at capacity
    EntityId create() {
        if (storage_.count >= storage_.capacity) {
            return INVALID_ENTITY;
        }
        
        usize idx = storage_.count;
        EntityId eid = next_id_++;
        
        storage_.id[idx] = eid;
        storage_.flags[idx] = entity_flags::ACTIVE;
        storage_.count++;
        
        return eid;
    }
    
    // Mark entity as dead (does not remove from storage)
    // Removal happens during compaction
    void destroy(usize idx) {
        if (idx < storage_.capacity) {
            storage_.flags[idx] &= ~entity_flags::ACTIVE;
            storage_.flags[idx] |= entity_flags::DEAD;
        }
    }
    
    // Get storage (for direct access in systems)
    EntityStorage& storage() { return storage_; }
    const EntityStorage& storage() const { return storage_; }
    
    // Count active entities
    usize active_count() const {
        usize count = 0;
        for (usize i = 0; i < storage_.count; ++i) {
            if (storage_.is_active(i)) count++;
        }
        return count;
    }
    
    // Compact storage (remove dead entities)
    // MUST be called at consistent points (e.g., end of tick)
    // Maintains deterministic order by original ID
    void compact() {
        usize write_idx = 0;
        
        for (usize read_idx = 0; read_idx < storage_.count; ++read_idx) {
            if (storage_.is_active(read_idx)) {
                if (write_idx != read_idx) {
                    // Move data
                    storage_.id[write_idx] = storage_.id[read_idx];
                    storage_.flags[write_idx] = storage_.flags[read_idx];
                    storage_.side[write_idx] = storage_.side[read_idx];
                    storage_.unit_type[write_idx] = storage_.unit_type[read_idx];
                    storage_.platform_id[write_idx] = storage_.platform_id[read_idx];
                    
                    storage_.pos_x[write_idx] = storage_.pos_x[read_idx];
                    storage_.pos_y[write_idx] = storage_.pos_y[read_idx];
                    storage_.pos_z[write_idx] = storage_.pos_z[read_idx];
                    
                    storage_.vel_x[write_idx] = storage_.vel_x[read_idx];
                    storage_.vel_y[write_idx] = storage_.vel_y[read_idx];
                    storage_.vel_z[write_idx] = storage_.vel_z[read_idx];
                    
                    storage_.health[write_idx] = storage_.health[read_idx];
                    storage_.supply[write_idx] = storage_.supply[read_idx];
                    storage_.morale[write_idx] = storage_.morale[read_idx];
                    storage_.readiness[write_idx] = storage_.readiness[read_idx];
                    
                    storage_.firepower[write_idx] = storage_.firepower[read_idx];
                    storage_.detection_range[write_idx] = storage_.detection_range[read_idx];
                    storage_.engagement_range[write_idx] = storage_.engagement_range[read_idx];
                    
                    storage_.combat[write_idx] = storage_.combat[read_idx];
                    
                    storage_.fuel[write_idx] = storage_.fuel[read_idx];
                    storage_.ammo[write_idx] = storage_.ammo[read_idx];
                    storage_.consumption_rate[write_idx] = storage_.consumption_rate[read_idx];

                    // Attrition memory (v1.1.8)
                    storage_.fatigue[write_idx] = storage_.fatigue[read_idx];
                    storage_.cohesion[write_idx] = storage_.cohesion[read_idx];
                    storage_.stress_ticks[write_idx] = storage_.stress_ticks[read_idx];
                    storage_.rest_ticks[write_idx] = storage_.rest_ticks[read_idx];
                    storage_.casualty_rate[write_idx] = storage_.casualty_rate[read_idx];
                }
                write_idx++;
            }
        }
        
        storage_.count = write_idx;
    }
    
    // Reset to initial state
    void reset() {
        storage_.clear();
        next_id_ = 0;
    }

private:
    EntityStorage storage_;
    EntityId next_id_;
};

}  // namespace athena

#endif  // ATHENA_ENTITIES_HPP
