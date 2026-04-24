// ATHENA Core - Combat System
// Contract: Deterministic combat resolution with Lanchester-based attrition.
//
// RULES:
// - Combat pairs resolved in deterministic order
// - Lanchester equations for attrition
// - No floating-point order dependence
// - All random draws from provided RNG
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_SYSTEMS_COMBAT_HPP
#define ATHENA_SYSTEMS_COMBAT_HPP

#include "athena/types.hpp"
#include "athena/entities.hpp"
#include "athena/rng.hpp"
#include "athena/terrain_semantics.hpp"
#include <vector>

namespace athena {
namespace systems {

// =============================================================================
// Combat Configuration
// =============================================================================

struct CombatConfig {
    // Attrition model
    f64 base_attrition_rate = 0.1;     // Base attrition per engagement
    f64 attrition_variance = 0.02;     // Stddev of attrition
    
    // Combat effectiveness
    f64 terrain_defense_bonus = 1.5;   // Defender terrain bonus (legacy)
    f64 supply_effectiveness_factor = 0.5;  // How much supply affects combat
    f64 morale_effectiveness_factor = 0.3;  // How much morale affects combat
    
    // Terrain integration (NEW)
    bool use_terrain_semantics = false;
    
    // Engagement rules
    f64 engagement_range_multiplier = 1.0;  // Scale engagement ranges
    bool friendly_fire_enabled = false;     // Same-side engagements
    bool allow_multiple_engagements = true; // Entity can engage multiple targets
    u32 max_engagements_per_tick = 3;       // Max targets per entity per tick
    
    // Damage model
    f64 min_damage = 0.01;             // Minimum damage per hit
    f64 max_damage = 0.5;              // Maximum damage per hit
    f64 kill_threshold = 0.0;          // Health below which entity dies
};

// =============================================================================
// Combat Result (for logging/analysis)
// =============================================================================

struct CombatEngagement {
    EntityId attacker;
    EntityId defender;
    f64 attacker_damage;
    f64 defender_damage;
    f64 distance;
    Tick tick;
};

// =============================================================================
// Combat System
// =============================================================================

class CombatSystem {
public:
    CombatSystem();
    
    // Initialize for given entity capacity
    void init(usize capacity, const CombatConfig& config);
    
    // Reset combat state
    void reset();
    
    // Set terrain semantics for defense calculations (NEW)
    void set_terrain(const TerrainSemantics* terrain);
    
    // Main update function - called by scheduler
    // Resolves all engagements for this tick
    // Deterministic: pairs resolved in entity index order
    Status update(EntityStorage& storage, Rng& rng, Tick tick);
    
    // Get engagements from last tick (for analysis)
    const std::vector<CombatEngagement>& last_engagements() const {
        return engagements_;
    }
    
    // Get configuration
    const CombatConfig& config() const { return config_; }
    
    // Modify config (for parameter sweeps)
    void set_attrition_rate(f64 rate) { config_.base_attrition_rate = rate; }
    void set_terrain_bonus(f64 bonus) { config_.terrain_defense_bonus = bonus; }

private:
    CombatConfig config_;
    const TerrainSemantics* terrain_;
    
    // Tracking
    std::vector<CombatEngagement> engagements_;
    std::vector<u32> engagement_count_;  // Per-entity engagement count this tick
    
    usize capacity_;
    bool initialized_;
    
    // Internal: check if two entities can engage
    bool can_engage(usize attacker, usize defender, 
                    const EntityStorage& storage, Tick tick) const;
    
    // Internal: compute distance between entities
    f64 compute_distance(usize a, usize b, const EntityStorage& storage) const;
    
    // Internal: resolve single engagement
    void resolve_engagement(usize attacker, usize defender,
                           EntityStorage& storage, Rng& rng, Tick tick);
    
    // Internal: compute combat effectiveness
    f64 compute_effectiveness(usize entity, const EntityStorage& storage, Tick tick) const;
    
    // Internal: get defense bonus from terrain (NEW)
    f64 get_terrain_defense(usize entity, const EntityStorage& storage, Tick tick) const;
    
    // Internal: apply damage to entity
    void apply_damage(usize entity, f64 damage, EntityStorage& storage);
};

// =============================================================================
// Standalone update function for scheduler registration
// =============================================================================

extern CombatSystem* g_combat_system;

Status combat_system_update(EntityStorage& storage, Rng& rng, Tick tick);

}  // namespace systems
}  // namespace athena

#endif  // ATHENA_SYSTEMS_COMBAT_HPP
