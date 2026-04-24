// ATHENA Core - Combat System Implementation
//
// Combat model based on Lanchester attrition equations.
// Simplified for agent-based resolution.
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/systems/combat.hpp"
#include <cmath>
#include <algorithm>

namespace athena {
namespace systems {

// =============================================================================
// Global instance pointer
// =============================================================================

CombatSystem* g_combat_system = nullptr;

// =============================================================================
// CombatSystem Implementation
// =============================================================================

CombatSystem::CombatSystem()
    : config_()
    , terrain_(nullptr)
    , engagements_()
    , engagement_count_()
    , capacity_(0)
    , initialized_(false)
{
}

void CombatSystem::init(usize capacity, const CombatConfig& config) {
    config_ = config;
    capacity_ = capacity;
    
    engagement_count_.resize(capacity, 0);
    engagements_.reserve(capacity * config.max_engagements_per_tick);
    
    initialized_ = true;
}

void CombatSystem::reset() {
    engagements_.clear();
    std::fill(engagement_count_.begin(), engagement_count_.end(), 0);
}

void CombatSystem::set_terrain(const TerrainSemantics* terrain) {
    terrain_ = terrain;
}

bool CombatSystem::can_engage(usize attacker, usize defender,
                              const EntityStorage& storage, Tick tick) const {
    // Must be active
    if (!storage.is_active(attacker) || !storage.is_active(defender)) {
        return false;
    }

    // v1.1.8: Broken units cannot engage — operationally dead
    if ((storage.flags[attacker] & entity_flags::BROKEN) != 0) {
        return false;
    }
    
    // Check friendly fire
    if (!config_.friendly_fire_enabled) {
        if (storage.side[attacker] == storage.side[defender]) {
            return false;
        }
    }
    
    // Neutral cannot attack
    if (storage.side[attacker] == Side::NEUTRAL) {
        return false;
    }
    
    // Must have meaningful firepower (skips support units: medical=0.0, logistics=0.1)
    if (storage.firepower[attacker] < 0.15) {
        return false;
    }
    
    // Must have ammo
    if (storage.ammo[attacker] <= 0.0) {
        return false;
    }
    
    // Check engagement count limit
    if (engagement_count_[attacker] >= config_.max_engagements_per_tick) {
        return false;
    }
    
    // Check range
    f64 distance = compute_distance(attacker, defender, storage);
    f64 range = storage.engagement_range[attacker] * config_.engagement_range_multiplier;

    // v1.2.1: Weather/terrain reduces effective engagement range (direct fire only)
    const auto& atk_combat = storage.combat[attacker];
    if (config_.use_terrain_semantics && terrain_ && !atk_combat.is_indirect) {
        f64 range_mod = terrain_->engagement_range_modifier(
            storage.pos_x[attacker], storage.pos_y[attacker], tick);
        range *= range_mod;
    }
    
    if (distance > range) {
        return false;
    }

    // v1.2.0: indirect fire has minimum range (dead zone)
    if (atk_combat.is_indirect && atk_combat.min_range_m > 0.0) {
        if (distance < atk_combat.min_range_m) {
            return false;
        }
    }
    
    // Check line of sight — v1.2.0: indirect fire skips LOS requirement
    if (config_.use_terrain_semantics && terrain_ && !atk_combat.is_indirect) {
        f64 los = terrain_->line_of_sight(
            storage.pos_x[attacker], storage.pos_y[attacker],
            storage.pos_x[defender], storage.pos_y[defender],
            tick);
        if (los < 0.1) {
            return false;  // No line of sight (direct fire only)
        }
    }
    
    return true;
}

f64 CombatSystem::compute_distance(usize a, usize b, 
                                    const EntityStorage& storage) const {
    f64 dx = storage.pos_x[a] - storage.pos_x[b];
    f64 dy = storage.pos_y[a] - storage.pos_y[b];
    f64 dz = storage.pos_z[a] - storage.pos_z[b];
    f64 d = std::sqrt(dx*dx + dy*dy + dz*dz);
    return safe::finite(d, 0.0);
}

f64 CombatSystem::get_terrain_defense(usize entity, const EntityStorage& storage, Tick tick) const {
    if (!terrain_ || !config_.use_terrain_semantics) {
        return config_.terrain_defense_bonus;  // Use legacy constant
    }
    
    f64 x = storage.pos_x[entity];
    f64 y = storage.pos_y[entity];
    
    auto def_value = terrain_->defense_value(x, y, tick);
    
    // Convert cover + concealment to defense multiplier
    // Higher values = more protection = lower damage taken
    f64 protection = (def_value.cover + def_value.concealment) / 2.0;
    
    // Convert to damage reduction: 0.5 protection = 1.5x defense (50% damage reduction)
    return 1.0 + protection;
}

f64 CombatSystem::compute_effectiveness(usize entity, 
                                         const EntityStorage& storage, Tick tick) const {
    f64 effectiveness = 1.0;
    
    // Supply factor
    f64 supply_factor = storage.supply[entity];
    effectiveness *= (1.0 - config_.supply_effectiveness_factor) + 
                     (config_.supply_effectiveness_factor * supply_factor);
    
    // Morale factor
    f64 morale_factor = storage.morale[entity];
    effectiveness *= (1.0 - config_.morale_effectiveness_factor) +
                     (config_.morale_effectiveness_factor * morale_factor);
    
    // Readiness factor (linear)
    effectiveness *= storage.readiness[entity];
    
    // Health factor (wounded units less effective)
    effectiveness *= storage.health[entity];

    // v1.1.8: Fatigue factor — fatigued units are significantly less effective
    f64 fatigue = storage.fatigue[entity];
    effectiveness *= (1.0 - fatigue * 0.5);

    // v1.1.8: Cohesion factor — disorganized units can't coordinate fire
    f64 cohesion = storage.cohesion[entity];
    effectiveness *= (0.3 + 0.7 * cohesion);

    // v1.2.0: Suppression — units under fire are less accurate and slower to react
    // At suppression 0.0: no penalty. At 0.5: 30% penalty. At 1.0: 60% penalty.
    f64 suppression = storage.suppression[entity];
    effectiveness *= (1.0 - suppression * 0.6);
    
    // Terrain concealment affects attacker's ability to be effective
    if (terrain_ && config_.use_terrain_semantics) {
        f64 concealment = terrain_->concealment(
            storage.pos_x[entity], storage.pos_y[entity], tick);
        effectiveness *= (1.0 - concealment * 0.2);
    }
    
    return safe::clamp(effectiveness, 0.0, 10.0);
}

void CombatSystem::apply_damage(usize entity, f64 damage, EntityStorage& storage) {
    damage = safe::finite(damage, 0.0);
    if (damage <= 0.0) return;
    
    storage.health[entity] -= damage;
    
    // Clamp health
    if (storage.health[entity] < 0.0) {
        storage.health[entity] = 0.0;
    }
    
    // Check death
    if (storage.health[entity] <= config_.kill_threshold) {
        storage.flags[entity] &= ~entity_flags::ACTIVE;
        storage.flags[entity] |= entity_flags::DEAD;
    }
    
    // Morale impact from damage
    f64 morale_loss = damage * 0.5;  // Taking damage hurts morale
    storage.morale[entity] -= morale_loss;
    if (storage.morale[entity] < 0.0) {
        storage.morale[entity] = 0.0;
    }
}

void CombatSystem::resolve_engagement(usize attacker, usize defender,
                                      EntityStorage& storage, Rng& rng, 
                                      Tick tick) {
    const auto& atk = storage.combat[attacker];
    const auto& def = storage.combat[defender];

    f64 distance = compute_distance(attacker, defender, storage);
    f64 attacker_eff = compute_effectiveness(attacker, storage, tick);
    f64 defender_eff = compute_effectiveness(defender, storage, tick);

    f64 attacker_damage = 0.0;
    f64 defender_damage = 0.0;

    // v1.2.0: Elevation advantage
    // Higher position improves hit probability and reduces incoming accuracy.
    f64 elev_advantage = 0.0;
    if (terrain_ && config_.use_terrain_semantics) {
        f64 atk_elev = terrain_->elevation(storage.pos_x[attacker], storage.pos_y[attacker]);
        f64 def_elev = terrain_->elevation(storage.pos_x[defender], storage.pos_y[defender]);
        f64 elev_diff_m = atk_elev - def_elev;
        // +0.15 per 100m advantage, capped at +-0.30
        elev_advantage = safe::clamp(elev_diff_m * 0.0015, -0.30, 0.30);
    }

    // v1.2.0: Aspect angle — select defender armor face based on attack angle.
    // bearing_to_target - target_heading = relative angle of incidence.
    // |angle| < 60deg = frontal, 60-120deg = side, >120deg = rear.
    auto compute_armor_face = [&](usize target, usize shooter) -> f64 {
        const auto& tgt = storage.combat[target];
        f64 dx = storage.pos_x[target] - storage.pos_x[shooter];
        f64 dy = storage.pos_y[target] - storage.pos_y[shooter];
        f64 bearing = std::atan2(dy, dx);
        f64 tgt_heading = storage.heading[target];
        f64 aspect = std::abs(bearing - tgt_heading);
        while (aspect > 3.14159265358979323846) aspect -= 2.0 * 3.14159265358979323846;
        aspect = std::abs(aspect);

        constexpr f64 FRONT_ARC = 1.0472;   // 60deg
        constexpr f64 SIDE_ARC  = 2.0944;   // 120deg
        if (aspect <= FRONT_ARC) return tgt.armor_front_mm;
        if (aspect <= SIDE_ARC)  return tgt.armor_side_mm;
        return tgt.armor_rear_mm;
    };

    // v1.2.0: Suppression — incoming fire suppresses even on miss.
    auto generate_suppression = [&](usize target, f64 shots, f64 caliber) {
        f64 caliber_factor = std::min(1.0, caliber / 150.0);
        f64 volume_factor = std::min(1.0, shots / 50.0);
        f64 sup_delta = (0.05 + 0.15 * caliber_factor) * (0.3 + 0.7 * volume_factor);
        storage.suppression[target] = safe::clamp(
            storage.suppression[target] + sup_delta, 0.0, 1.0);
        if (storage.suppression[target] > 0.4)
            storage.flags[target] |= entity_flags::SUPPRESSED;
    };

    // =========================================================================
    // PLATFORM-DRIVEN COMBAT (penetration vs armor)
    // =========================================================================
    if (atk.has_platform_data && def.has_platform_data) {

        // --- Attacker fires at Defender ---
        if (atk.effective_range_m > 0.0 && atk.penetration_mm > 0.0) {
            f64 tick_seconds = 3600.0;
            f64 shots = atk.rate_of_fire_rpm * (tick_seconds / 60.0) * 0.10;
            if (shots < 1.0) shots = 1.0;

            f64 p_hit;
            if (atk.is_indirect) {
                // v1.2.0: Indirect fire — CEP-based, no LOS needed
                f64 range_ratio = distance / std::max(atk.effective_range_m, 1.0);
                f64 base = 0.35 - 0.15 * range_ratio;
                f64 sensor_bonus = atk.has_lrf ? 0.05 : 0.0;
                p_hit = std::min(0.60, (base + sensor_bonus) * attacker_eff);
                p_hit += elev_advantage * 0.5;
            } else {
                // Direct fire
                f64 range_ratio = std::min(1.0, distance / std::max(atk.effective_range_m, 1.0));
                f64 base = 0.6 - 0.4 * range_ratio;
                f64 sensor_bonus = 0.0;
                if (atk.thermal_gen > 0) sensor_bonus += 0.05 * atk.thermal_gen;
                if (atk.has_lrf) sensor_bonus += 0.10;
                p_hit = std::min(0.95, (base + sensor_bonus) * attacker_eff);
                p_hit += elev_advantage;
            }
            p_hit = safe::clamp(p_hit, 0.01, 0.95);
            p_hit /= get_terrain_defense(defender, storage, tick);

            f64 expected_hits = shots * p_hit;
            f64 hits = std::max(0.0, expected_hits + rng.next_normal(0.0, expected_hits * 0.2));

            // v1.2.0: Armor face from aspect angle
            f64 armor_mm = compute_armor_face(defender, attacker);

            // APS (less effective vs indirect/arcing rounds)
            if (def.has_aps) hits *= atk.is_indirect ? 0.70 : 0.50;

            if (armor_mm > 0.0) {
                f64 overmatch = atk.penetration_mm / armor_mm;
                if (overmatch >= 1.0) {
                    defender_damage = hits * std::min(0.50, 0.15 * overmatch);
                } else {
                    defender_damage = hits * 0.02 * (overmatch * overmatch);
                }
            } else {
                f64 dmg = 0.10 + atk.caliber_mm * 0.001;
                if (atk.splash_radius_m > 0.0)
                    dmg *= 1.0 + std::min(0.5, atk.splash_radius_m / 100.0);
                defender_damage = hits * dmg;
            }

            generate_suppression(defender, shots, atk.caliber_mm);
        }

        // --- Defender return fire (direct fire units only) ---
        bool def_can_return = def.effective_range_m > 0.0 &&
                              distance <= def.effective_range_m &&
                              !def.is_indirect;
        if (def_can_return) {
            f64 tick_seconds = 3600.0;
            f64 shots = def.rate_of_fire_rpm * (tick_seconds / 60.0) * 0.10;
            if (shots < 1.0) shots = 1.0;

            f64 range_ratio = std::min(1.0, distance / std::max(def.effective_range_m, 1.0));
            f64 p_hit = std::min(0.95, (0.6 - 0.4 * range_ratio) * defender_eff);
            if (def.thermal_gen > 0) p_hit = std::min(0.95, p_hit + 0.05 * def.thermal_gen);
            if (def.has_lrf) p_hit = std::min(0.95, p_hit + 0.10);
            p_hit = safe::clamp(p_hit - elev_advantage, 0.01, 0.95);
            p_hit /= get_terrain_defense(attacker, storage, tick);

            f64 hits = std::max(0.0, shots * p_hit +
                       rng.next_normal(0.0, shots * p_hit * 0.2));

            f64 armor_mm = compute_armor_face(attacker, defender);
            if (atk.has_aps) hits *= 0.50;

            if (armor_mm > 0.0 && def.penetration_mm > 0.0) {
                f64 overmatch = def.penetration_mm / armor_mm;
                if (overmatch >= 1.0) {
                    attacker_damage = hits * std::min(0.50, 0.15 * overmatch);
                } else {
                    attacker_damage = hits * 0.02 * (overmatch * overmatch);
                }
            } else if (def.penetration_mm > 0.0) {
                attacker_damage = hits * (0.10 + def.caliber_mm * 0.001);
            }

            generate_suppression(attacker, shots, def.caliber_mm);
        }

    } else {
        // =====================================================================
        // LEGACY COMBAT (Lanchester attrition)
        // =====================================================================
        f64 attacker_fp = storage.firepower[attacker] * attacker_eff;
        f64 defender_fp = storage.firepower[defender] * defender_eff;
        f64 attacker_range = storage.engagement_range[attacker];
        // v1.2.1: Weather reduces effective range in legacy combat
        if (config_.use_terrain_semantics && terrain_ && !storage.combat[attacker].is_indirect) {
            attacker_range *= terrain_->engagement_range_modifier(
                storage.pos_x[attacker], storage.pos_y[attacker], tick);
        }
        f64 range_factor = 1.0;
        if (attacker_range > 0.0) {
            range_factor = 1.0 - (distance / attacker_range) * 0.5;
            if (range_factor < 0.3) range_factor = 0.3;
        }
        f64 terrain_modifier = get_terrain_defense(defender, storage, tick);
        f64 elev_mod = 1.0 + elev_advantage;

        f64 base_attacker_damage = config_.base_attrition_rate *
                                   (defender_fp / (attacker_fp + 1.0)) * terrain_modifier;
        f64 base_defender_damage = config_.base_attrition_rate *
                                   (attacker_fp / (defender_fp + 1.0)) * range_factor * elev_mod;

        attacker_damage = std::max(config_.min_damage,
            std::min(config_.max_damage,
                base_attacker_damage + rng.next_normal(0.0, config_.attrition_variance)));
        defender_damage = std::max(config_.min_damage,
            std::min(config_.max_damage,
                base_defender_damage + rng.next_normal(0.0, config_.attrition_variance)));
    }

    attacker_damage = safe::clamp(attacker_damage, 0.0, 1.0);
    defender_damage = safe::clamp(defender_damage, 0.0, 1.0);

    apply_damage(attacker, attacker_damage, storage);
    apply_damage(defender, defender_damage, storage);
    
    f64 ammo_consumed = 0.05;
    storage.ammo[attacker] -= ammo_consumed;
    if (storage.ammo[attacker] < 0.0) storage.ammo[attacker] = 0.0;
    storage.supply[attacker] = (storage.fuel[attacker] + storage.ammo[attacker]) * 0.5;
    
    CombatEngagement eng;
    eng.attacker = storage.id[attacker];
    eng.defender = storage.id[defender];
    eng.attacker_damage = attacker_damage;
    eng.defender_damage = defender_damage;
    eng.distance = distance;
    eng.tick = tick;
    engagements_.push_back(eng);
    
    engagement_count_[attacker]++;
    storage.flags[attacker] |= entity_flags::ENGAGED;
    storage.flags[defender] |= entity_flags::ENGAGED;
}

Status CombatSystem::update(EntityStorage& storage, Rng& rng, Tick tick) {
    if (!initialized_) {
        return Error(ErrorCode::CONTEXT_NOT_INITIALIZED,
            "Combat system not initialized");
    }
    
    // Clear previous tick data
    engagements_.clear();
    std::fill(engagement_count_.begin(), engagement_count_.end(), 0);
    
    // Clear engaged flags
    for (usize i = 0; i < storage.count; ++i) {
        storage.flags[i] &= ~entity_flags::ENGAGED;
        storage.flags[i] &= ~entity_flags::SUPPRESSED;  // v1.2.0: reset, will be set if fired upon
    }
    
    // Process all potential engagements
    // Order: attacker by index, then defender by index (deterministic)
    for (usize attacker = 0; attacker < storage.count; ++attacker) {
        if (!storage.is_active(attacker)) continue;
        
        for (usize defender = 0; defender < storage.count; ++defender) {
            // Skip self
            if (attacker == defender) continue;
            
            if (!can_engage(attacker, defender, storage, tick)) continue;
            
            // Resolve engagement
            resolve_engagement(attacker, defender, storage, rng, tick);
            
            // Check if attacker reached engagement limit
            if (engagement_count_[attacker] >= config_.max_engagements_per_tick) {
                break;
            }
            
            // If multiple engagements not allowed, stop
            if (!config_.allow_multiple_engagements) {
                break;
            }
        }
    }
    
    return Status();
}

// =============================================================================
// Standalone function
// =============================================================================

Status combat_system_update(EntityStorage& storage, Rng& rng, Tick tick) {
    if (!g_combat_system) {
        return Error(ErrorCode::CONTEXT_NOT_INITIALIZED,
            "Global combat system not set");
    }
    return g_combat_system->update(storage, rng, tick);
}

}  // namespace systems
}  // namespace athena
