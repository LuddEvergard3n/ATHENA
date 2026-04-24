// ATHENA Core - Monte Carlo Executor Implementation
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/analysis/montecarlo.hpp"
#include "athena/systems.hpp"
#include "athena/systems/tactical_ai.hpp"
#include "athena/terrain.hpp"
#include "athena/terrain_semantics.hpp"
#include "athena/environment.hpp"
#include "athena/platform_loader.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <map>
#include <chrono>

namespace athena {
namespace analysis {

// =============================================================================
// DistributionSampler Implementation
// =============================================================================

f64 DistributionSampler::sample(const Distribution& dist, Rng& rng) {
    switch (dist.type) {
        case DistributionType::POINT:
            return dist.param1;
            
        case DistributionType::UNIFORM:
            return rng.next_f64_range(dist.param1, dist.param2);
            
        case DistributionType::NORMAL:
            return rng.next_normal(dist.param1, dist.param2);
            
        case DistributionType::LOGNORMAL: {
            f64 normal = rng.next_normal(dist.param1, dist.param2);
            return std::exp(normal);
        }
        
        case DistributionType::TRIANGULAR: {
            f64 min = dist.param1;
            f64 max = dist.param2;
            f64 mode = dist.param3;
            f64 u = rng.next_f64();
            f64 fc = (mode - min) / (max - min);
            
            if (u < fc) {
                return min + std::sqrt(u * (max - min) * (mode - min));
            } else {
                return max - std::sqrt((1.0 - u) * (max - min) * (max - mode));
            }
        }
        
        case DistributionType::BETA: {
            // Simple beta sampling using gamma ratio
            // For production, use more accurate method
            f64 alpha = dist.param1;
            f64 beta = dist.param2;
            
            // Generate gamma variates (simplified)
            f64 x = 0.0, y = 0.0;
            for (int i = 0; i < static_cast<int>(alpha); ++i) {
                x -= std::log(1.0 - rng.next_f64());
            }
            for (int i = 0; i < static_cast<int>(beta); ++i) {
                y -= std::log(1.0 - rng.next_f64());
            }
            
            if (x + y > 0.0) {
                return x / (x + y);
            }
            return 0.5;
        }
        
        default:
            return dist.param1;
    }
}

f64 DistributionSampler::sample_parameter(const UncertainParameter& param, Rng& rng) {
    if (param.uncertainty.has_value()) {
        return sample(*param.uncertainty, rng);
    }
    return param.base_value;
}

std::vector<std::map<std::string, f64>> 
DistributionSampler::latin_hypercube(
    const std::map<std::string, UncertainParameter>& params,
    u32 num_samples, Rng& rng) {
    
    std::vector<std::map<std::string, f64>> samples(num_samples);
    
    // For each parameter
    for (const auto& [name, param] : params) {
        // Create stratified samples
        std::vector<f64> values(num_samples);
        
        for (u32 i = 0; i < num_samples; ++i) {
            // Stratified position within interval
            f64 low = static_cast<f64>(i) / num_samples;
            f64 high = static_cast<f64>(i + 1) / num_samples;
            f64 u = rng.next_f64_range(low, high);
            
            // Transform to parameter distribution
            if (param.uncertainty.has_value()) {
                const auto& dist = *param.uncertainty;
                switch (dist.type) {
                    case DistributionType::UNIFORM:
                        values[i] = dist.param1 + u * (dist.param2 - dist.param1);
                        break;
                    case DistributionType::NORMAL: {
                        // Inverse CDF (simplified)
                        f64 z = std::sqrt(2.0) * std::erfc(2.0 * u);
                        values[i] = dist.param1 + z * dist.param2;
                        break;
                    }
                    default:
                        values[i] = param.base_value;
                }
            } else {
                values[i] = param.base_value;
            }
        }
        
        // Shuffle values
        for (u32 i = num_samples - 1; i > 0; --i) {
            u32 j = rng.next_u32_bounded(i + 1);
            std::swap(values[i], values[j]);
        }
        
        // Assign to samples
        for (u32 i = 0; i < num_samples; ++i) {
            samples[i][name] = values[i];
        }
    }
    
    return samples;
}

// =============================================================================
// MonteCarloExecutor Implementation
// =============================================================================

MonteCarloExecutor::MonteCarloExecutor()
    : config_()
    , scenario_(nullptr)
    , parameter_overrides_()
    , results_()
    , current_iteration_(0)
    , cancelled_(false)
    , running_(false)
{
}

void MonteCarloExecutor::configure(const BatchConfig& config) {
    config_ = config;
}

void MonteCarloExecutor::set_scenario(const Scenario& scenario) {
    scenario_ = &scenario;
}

void MonteCarloExecutor::set_parameter(const std::string& name, f64 value) {
    parameter_overrides_[name] = value;
}

void MonteCarloExecutor::clear_parameters() {
    parameter_overrides_.clear();
}

Seed MonteCarloExecutor::derive_seed(u32 iteration_id) const {
    // Deterministic seed derivation
    // Uses master seed and iteration ID
    u64 combined = config_.master_seed ^ 
                   (static_cast<u64>(iteration_id) * 0x9E3779B97F4A7C15ULL);
    
    // Mix bits
    combined ^= combined >> 33;
    combined *= 0xFF51AFD7ED558CCDULL;
    combined ^= combined >> 33;
    combined *= 0xC4CEB9FE1A85EC53ULL;
    combined ^= combined >> 33;
    
    return combined;
}

bool MonteCarloExecutor::check_decisive(const EntityStorage& storage) const {
    u32 blue_alive = 0, red_alive = 0, green_alive = 0;
    u32 blue_initial = 0, red_initial = 0, green_initial = 0;
    
    for (usize i = 0; i < storage.count; ++i) {
        if (storage.side[i] == Side::BLUE) {
            blue_initial++;
            if (storage.is_active(i)) blue_alive++;
        } else if (storage.side[i] == Side::RED) {
            red_initial++;
            if (storage.is_active(i)) red_alive++;
        } else if (storage.side[i] == Side::GREEN) {
            green_initial++;
            if (storage.is_active(i)) green_alive++;
        }
    }
    
    // Multi-side: decisive when only 0 or 1 sides have surviving units
    u32 sides_started = 0, sides_alive = 0;
    if (blue_initial > 0)  { sides_started++; if (blue_alive > 0)  sides_alive++; }
    if (red_initial > 0)   { sides_started++; if (red_alive > 0)   sides_alive++; }
    if (green_initial > 0) { sides_started++; if (green_alive > 0) sides_alive++; }
    
    if (sides_started >= 2 && sides_alive <= 1) return true;
    
    // Also check threshold-based (any side below threshold)
    f64 blue_ratio = blue_initial > 0 ?
        static_cast<f64>(blue_alive) / blue_initial : 1.0;
    f64 red_ratio = red_initial > 0 ?
        static_cast<f64>(red_alive) / red_initial : 1.0;
    
    return blue_ratio < config_.decisive_threshold ||
           red_ratio < config_.decisive_threshold;
}

// v1.1.7: Check if no remaining active unit has offensive capability (firepower >= 0.15)
bool MonteCarloExecutor::check_no_offensive(const EntityStorage& storage) const {
    // Count sides that still have offensive-capable units
    bool blue_has_offense = false, red_has_offense = false, green_has_offense = false;
    for (usize i = 0; i < storage.count; ++i) {
        if (!storage.is_active(i)) continue;
        if (storage.firepower[i] < 0.15) continue;  // Same threshold as combat
        if (storage.ammo[i] <= 0.0) continue;         // No ammo = no offense
        switch (storage.side[i]) {
            case Side::BLUE:  blue_has_offense = true; break;
            case Side::RED:   red_has_offense  = true; break;
            case Side::GREEN: green_has_offense = true; break;
            default: break;
        }
    }
    // Stalemate: at least 2 sides have surviving units but neither has offense
    bool b_alive = false, r_alive = false, g_alive = false;
    for (usize i = 0; i < storage.count; ++i) {
        if (!storage.is_active(i)) continue;
        if (storage.side[i] == Side::BLUE)  b_alive = true;
        if (storage.side[i] == Side::RED)   r_alive = true;
        if (storage.side[i] == Side::GREEN) g_alive = true;
    }
    u32 sides_alive = (u32)b_alive + (u32)r_alive + (u32)g_alive;
    u32 sides_armed = (u32)blue_has_offense + (u32)red_has_offense + (u32)green_has_offense;

    // Stalemate: multiple sides alive but zero (or only one) has offense
    return (sides_alive >= 2 && sides_armed == 0);
}

// v1.1.7: Check if no side can detect any enemy unit
bool MonteCarloExecutor::check_mutual_blindness(const EntityStorage& storage) const {
    // For each active unit, check if it has detection range that could reach any enemy
    // Simplified: if max detection range of all units on one side is less than
    // the minimum distance to any enemy unit, they're blind.
    
    // Collect per-side: max detection range, positions
    struct SideInfo { f64 max_det_range = 0; std::vector<usize> units; };
    SideInfo blue_info, red_info;
    
    for (usize i = 0; i < storage.count; ++i) {
        if (!storage.is_active(i)) continue;
        if (storage.side[i] == Side::BLUE) {
            blue_info.units.push_back(i);
            blue_info.max_det_range = std::max(blue_info.max_det_range, storage.detection_range[i]);
        } else if (storage.side[i] == Side::RED) {
            red_info.units.push_back(i);
            red_info.max_det_range = std::max(red_info.max_det_range, storage.detection_range[i]);
        }
    }
    
    if (blue_info.units.empty() || red_info.units.empty()) return false;
    
    // Find minimum distance between any blue and any red unit
    f64 min_dist = 1e18;
    for (usize bi : blue_info.units) {
        for (usize ri : red_info.units) {
            f64 dx = storage.pos_x[bi] - storage.pos_x[ri];
            f64 dy = storage.pos_y[bi] - storage.pos_y[ri];
            f64 d = std::sqrt(dx*dx + dy*dy);
            if (d < min_dist) min_dist = d;
        }
    }
    
    // Neither side can detect: both max detection ranges less than min distance
    return (blue_info.max_det_range < min_dist && red_info.max_det_range < min_dist);
}

// v1.1.7: Check if remaining units cannot close distance to enemies
// within remaining simulation time at their max speed
bool MonteCarloExecutor::check_unreachable(const EntityStorage& storage,
                                           Tick current_tick, Tick max_ticks,
                                           f64 tick_duration_s) const {
    Tick remaining = (current_tick < max_ticks) ? (max_ticks - current_tick) : 0;
    if (remaining == 0) return false;  // Already at end — let MAX_TICKS handle it

    f64 remaining_seconds = remaining * tick_duration_s;

    // For each pair of opposing sides, check if any unit can reach any enemy
    // within remaining time. Conservative: use max speed of fastest unit on each side.
    struct SideData {
        f64 max_speed_mps = 0.0;  // Fastest unit's speed
        bool has_units = false;
        f64 min_x = 1e18, min_y = 1e18, max_x = -1e18, max_y = -1e18;
    };
    SideData blue_d, red_d;

    for (usize i = 0; i < storage.count; ++i) {
        if (!storage.is_active(i)) continue;
        SideData* sd = nullptr;
        if (storage.side[i] == Side::BLUE) sd = &blue_d;
        else if (storage.side[i] == Side::RED) sd = &red_d;
        else continue;

        sd->has_units = true;
        // Use platform off-road speed if available, else nominal 10 m/s
        f64 spd = storage.combat[i].max_speed_offroad_mps;
        if (spd <= 0.0) spd = storage.combat[i].max_speed_road_mps;
        if (spd <= 0.0) spd = 10.0;
        if (spd > sd->max_speed_mps) sd->max_speed_mps = spd;

        if (storage.pos_x[i] < sd->min_x) sd->min_x = storage.pos_x[i];
        if (storage.pos_x[i] > sd->max_x) sd->max_x = storage.pos_x[i];
        if (storage.pos_y[i] < sd->min_y) sd->min_y = storage.pos_y[i];
        if (storage.pos_y[i] > sd->max_y) sd->max_y = storage.pos_y[i];
    }

    if (!blue_d.has_units || !red_d.has_units) return false;

    // Minimum distance between the two side bounding boxes
    f64 gap_x = 0.0, gap_y = 0.0;
    if (blue_d.max_x < red_d.min_x) gap_x = red_d.min_x - blue_d.max_x;
    else if (red_d.max_x < blue_d.min_x) gap_x = blue_d.min_x - red_d.max_x;
    if (blue_d.max_y < red_d.min_y) gap_y = red_d.min_y - blue_d.max_y;
    else if (red_d.max_y < blue_d.min_y) gap_y = blue_d.min_y - red_d.max_y;

    f64 min_gap = std::sqrt(gap_x * gap_x + gap_y * gap_y);
    if (min_gap < 100.0) return false;  // Already close enough

    // Combined closing speed: both sides can move toward each other
    f64 closing_speed = blue_d.max_speed_mps + red_d.max_speed_mps;
    if (closing_speed <= 0.0) return true;  // Nobody can move

    f64 time_to_close = min_gap / closing_speed;
    return (time_to_close > remaining_seconds);
}

void MonteCarloExecutor::collect_metrics(IterationResult& result,
                                         const EntityStorage& storage,
                                         const systems::CombatSystem* combat) {
    result.blue_surviving = 0;
    result.red_surviving = 0;
    result.neutral_surviving = 0;
    result.blue_total_health = 0.0;
    result.red_total_health = 0.0;
    result.blue_avg_supply = 0.0;
    result.red_avg_supply = 0.0;
    result.blue_avg_morale = 0.0;
    result.red_avg_morale = 0.0;
    
    for (usize i = 0; i < storage.count; ++i) {
        if (!storage.is_active(i)) continue;
        
        switch (storage.side[i]) {
            case Side::BLUE:
                result.blue_surviving++;
                result.blue_total_health += storage.health[i];
                result.blue_avg_supply += storage.supply[i];
                result.blue_avg_morale += storage.morale[i];
                break;
            case Side::RED:
                result.red_surviving++;
                result.red_total_health += storage.health[i];
                result.red_avg_supply += storage.supply[i];
                result.red_avg_morale += storage.morale[i];
                break;
            default:
                result.neutral_surviving++;
                break;
        }
    }
    
    // Compute averages
    if (result.blue_surviving > 0) {
        result.blue_avg_supply /= result.blue_surviving;
        result.blue_avg_morale /= result.blue_surviving;
    }
    if (result.red_surviving > 0) {
        result.red_avg_supply /= result.red_surviving;
        result.red_avg_morale /= result.red_surviving;
    }
    
    // Combat damage statistics from accumulated engagement details
    // Note: total_engagements and engagement_details already populated in run_iteration
    result.total_blue_damage = 0.0;
    result.total_red_damage = 0.0;
    
    for (const auto& eng : result.engagement_details) {
        // Damage dealt by attacker to defender
        // Need to check sides to categorize
        // For now, assume damage dealt goes to the appropriate side counter
        result.total_blue_damage += eng.total_damage_received;  // Damage received by blue
        result.total_red_damage += eng.total_damage_dealt;      // Damage dealt to red
    }
}

Result<IterationResult> MonteCarloExecutor::run_iteration(u32 iteration_id, Seed seed) {
    IterationResult result;
    result.iteration_id = iteration_id;
    result.seed = seed;
    result.time_to_first_casualty = 0;
    result.time_to_decisive = 0;
    
    // Create context
    ContextConfig ctx_config;
    ctx_config.seed = seed;
    ctx_config.max_entities = config_.entity_capacity;
    
    auto ctx_result = Context::create(ctx_config);
    if (!ctx_result.ok()) {
        return ctx_result.error;
    }
    auto& ctx = *ctx_result.get();
    
    // Initialize entities
    EntityManager entities;
    entities.init(config_.entity_capacity);
    
    // Convert scenario to entities
    Rng conv_rng(seed, 0x5C3A1210ULL);
    ScenarioConverter converter;
    converter.set_platform_database(platform_db_);
    auto conv_result = converter.convert(*scenario_, entities, conv_rng);
    if (!conv_result.ok()) {
        return conv_result.error;
    }
    
    // Build entity mappings for UI correlation
    const auto& actor_map = converter.actor_mapping();
    result.entity_mappings.reserve(actor_map.size());
    
    for (const auto& [actor_id, entity_id] : actor_map) {
        EntityMapping mapping;
        mapping.entity_id = entity_id;
        mapping.actor_id = actor_id;
        
        // Get initial position and side from storage
        auto& storage = entities.storage();
        if (entity_id < storage.count) {
            mapping.side = storage.side[entity_id];
            mapping.initial_x = storage.pos_x[entity_id];
            mapping.initial_y = storage.pos_y[entity_id];
        }
        
        result.entity_mappings.push_back(mapping);
    }
    
    // Count initial entities
    u32 initial_blue = 0, initial_red = 0;
    for (usize i = 0; i < entities.storage().count; ++i) {
        if (entities.storage().side[i] == Side::BLUE) initial_blue++;
        else if (entities.storage().side[i] == Side::RED) initial_red++;
    }
    
    // Initialize systems
    systems::SystemsBundle systems;
    systems::MovementConfig move_cfg;
    move_cfg.dt_seconds = scenario_->temporal.tick_duration_seconds;
    
    systems::CombatConfig combat_cfg;
    // Apply parameter overrides
    auto it = parameter_overrides_.find("attrition_coefficient");
    if (it != parameter_overrides_.end()) {
        combat_cfg.base_attrition_rate = it->second;
    }
    it = parameter_overrides_.find("terrain_defense_bonus");
    if (it != parameter_overrides_.end()) {
        combat_cfg.terrain_defense_bonus = it->second;
    }
    
    systems::LogisticsConfig log_cfg;
    
    // Create terrain from scenario config (same as interactive simulation)
    PhysicalTerrain mc_terrain;
    Environment mc_env;
    TerrainSemantics mc_sem;
    bool terrain_ok = false;
    {
        const auto& tc = scenario_->environment.terrain;
        const auto& sb = scenario_->spatial_bounds;
        constexpr f64 EARTH_R = 6371000.0;
        constexpr f64 D2R = 3.14159265358979323846 / 180.0;
        f64 clat = ((sb.min_lat + sb.max_lat) / 2.0) * D2R;
        f64 w = EARTH_R * (sb.max_lon - sb.min_lon) * D2R * std::cos(clat);
        f64 h = EARTH_R * (sb.max_lat - sb.min_lat) * D2R;
        if (w < 1000.0) w = 100000.0;
        if (h < 1000.0) h = 100000.0;

        TerrainBase base = TerrainBase::FLAT;
        if (tc.base_type == "rolling")    base = TerrainBase::HILLS;
        else if (tc.base_type == "mountains") base = TerrainBase::MOUNTAINS;
        else if (tc.base_type == "coastal")   base = TerrainBase::VALLEY;

        TerrainPhysicalConfig tp;
        tp.base = base; tp.roughness = tc.roughness; tp.seed = seed;
        tp.width_m = w; tp.height_m = h;
        tp.elevation_range_m = (base == TerrainBase::MOUNTAINS) ? 800.0 : 200.0;
        if (tc.forest_density > 0.01) {
            tp.overlays.push_back({OverlayType::FOREST, tc.forest_density, 0.0, 1});
        }
        if (tc.urban_density > 0.01) {
            tp.overlays.push_back({OverlayType::URBAN, tc.urban_density, 0.0, 2});
        }

        auto ts = mc_terrain.init(tp);
        if (ts.ok()) {
            // v1.2.1: Use environment config from BatchConfig (carries GUI weather)
            mc_env.init(config_.env_config);
            mc_sem.init(&mc_terrain, &mc_env);
            terrain_ok = true;
        }
    }

    // Enable terrain semantics if terrain initialized
    if (terrain_ok) {
        move_cfg.use_terrain_semantics = true;
        combat_cfg.use_terrain_semantics = true;
    }

    systems.init(config_.entity_capacity, move_cfg, combat_cfg, log_cfg);

    // Connect terrain to systems
    if (terrain_ok) {
        systems.movement.set_terrain(&mc_sem);
        systems.combat.set_terrain(&mc_sem);
    }

    // v1.2.1: dt-scaling for operational wear
    systems::g_operational_config.dt_seconds = move_cfg.dt_seconds;

    // v1.2.1: Connect environment to logistics for weather consumption
    if (terrain_ok) {
        systems.logistics.set_environment(&mc_env);
    }
    
    // Initialize scheduler
    SchedulerConfig sched_cfg;
    sched_cfg.max_ticks = config_.max_ticks_per_iteration > 0 ? 
        config_.max_ticks_per_iteration : scenario_->temporal.max_ticks;
    sched_cfg.compact_per_tick = true;
    
    Scheduler scheduler;
    auto init_status = scheduler.init(ctx, entities, sched_cfg);
    if (!init_status.ok()) {
        return init_status.error;
    }
    
    auto reg_status = systems::register_all_systems(scheduler);
    if (!reg_status.ok()) {
        return reg_status.error;
    }
    
    // Register tactical AI (seek-enemy behavior) — same as interactive sim
    systems::g_tactical_ai_dt_seconds.store(move_cfg.dt_seconds);
    systems::g_tactical_ai_terrain = terrain_ok ? &mc_sem : nullptr;

    // v1.2.5: A* pathfinder for MC iterations
    Pathfinder mc_pathfinder;
    systems::g_tactical_ai_pathfinder = nullptr;
    if (terrain_ok) {
        PathfindingConfig pf_cfg;
        pf_cfg.cell_size = 200.0;
        pf_cfg.max_iterations = 50000;
        pf_cfg.allow_diagonal = true;
        auto pf_status = mc_pathfinder.init(&mc_sem, pf_cfg);
        if (pf_status.ok()) {
            systems::g_tactical_ai_pathfinder = &mc_pathfinder;
        }
    }

    // v1.2.5: Initialize path cache for this MC iteration
    systems::tactical_ai_init_path_cache(entities.storage().count);

    // v1.1.8: Operational snapshot (captures health before combat)
    scheduler.register_system(
        Scheduler::Phase::PRE_TICK, systems::operational_snapshot_update, "op_snapshot");

    reg_status = scheduler.register_system(
        Scheduler::Phase::PRE_TICK, systems::seek_enemy_ai_update, "seek_enemy_ai");
    if (!reg_status.ok()) {
        return reg_status.error;
    }

    // Register morale propagation
    scheduler.register_system(
        Scheduler::Phase::POST_TICK, systems::morale_propagation_update, "morale");

    // v1.1.8: Operational wear (fatigue + cohesion)
    scheduler.register_system(
        Scheduler::Phase::POST_TICK, systems::operational_wear_update, "op_wear");
    
    // Run simulation
    ctx.start();
    bool first_casualty_recorded = false;
    bool decisive_recorded = false;
    
    // Engagement accumulator: key = (attacker_id, defender_id)
    using EngagementKey = std::pair<EntityId, EntityId>;
    std::map<EngagementKey, EngagementSummary> engagement_map;
    u64 total_engagement_count = 0;
    
    while (!scheduler.is_complete() && !cancelled_) {
        auto status = scheduler.run_tick();
        if (!status.ok()) {
            result.completed_normally = false;
            break;
        }
        
        // Accumulate engagements from this tick
        const auto& tick_engagements = systems.combat.last_engagements();
        for (const auto& eng : tick_engagements) {
            EngagementKey key{eng.attacker, eng.defender};
            auto& summary = engagement_map[key];
            
            if (summary.engagement_count == 0) {
                // First engagement for this pair
                summary.attacker = eng.attacker;
                summary.defender = eng.defender;
                summary.first_engagement = eng.tick;
            }
            
            summary.engagement_count++;
            summary.total_damage_dealt += eng.defender_damage;
            summary.total_damage_received += eng.attacker_damage;
            summary.last_engagement = eng.tick;
            total_engagement_count++;
        }
        
        // Capture timeline snapshot if enabled
        Tick current_tick = ctx.current_tick();
        if (config_.record_timeline && 
            (config_.timeline_sample_interval == 0 || 
             current_tick % config_.timeline_sample_interval == 0)) {
            
            TickSnapshot snapshot;
            snapshot.tick = current_tick;
            snapshot.engagements_this_tick = static_cast<u32>(tick_engagements.size());
            
            const auto& storage = entities.storage();
            snapshot.entities.reserve(storage.count);
            
            for (usize i = 0; i < storage.count; ++i) {
                EntitySnapshot es;
                es.entity_id = static_cast<EntityId>(i);
                es.pos_x = storage.pos_x[i];
                es.pos_y = storage.pos_y[i];
                es.health = storage.health[i];
                es.supply = storage.supply[i];
                es.morale = storage.morale[i];
                es.is_active = storage.is_active(i);
                snapshot.entities.push_back(es);
                
                if (es.is_active) {
                    if (storage.side[i] == Side::BLUE) snapshot.blue_count++;
                    else if (storage.side[i] == Side::RED) snapshot.red_count++;
                }
            }
            
            result.tick_snapshots.push_back(std::move(snapshot));
        }
        
        // Check for first casualty
        if (!first_casualty_recorded) {
            u32 current_blue = 0, current_red = 0;
            for (usize i = 0; i < entities.storage().count; ++i) {
                if (!entities.storage().is_active(i)) continue;
                if (entities.storage().side[i] == Side::BLUE) current_blue++;
                else if (entities.storage().side[i] == Side::RED) current_red++;
            }
            if (current_blue < initial_blue || current_red < initial_red) {
                result.time_to_first_casualty = ctx.current_tick();
                first_casualty_recorded = true;
            }
        }
        
        // Check for decisive outcome
        if (!decisive_recorded && check_decisive(entities.storage())) {
            result.time_to_decisive = ctx.current_tick();
            decisive_recorded = true;
            result.termination_reason = TerminationReason::DECISIVE_VICTORY;
            
            if (config_.stop_on_decisive) {
                break;
            }
        }
        
        // v1.1.7: Stalemate detection (check every 10 ticks to save CPU)
        if (!decisive_recorded && ctx.current_tick() > 5 && ctx.current_tick() % 10 == 0) {
            if (check_no_offensive(entities.storage())) {
                result.termination_reason = TerminationReason::NO_OFFENSIVE_CAPACITY;
                break;
            }
            if (check_mutual_blindness(entities.storage())) {
                result.termination_reason = TerminationReason::MUTUAL_BLINDNESS;
                break;
            }
            if (check_unreachable(entities.storage(), ctx.current_tick(),
                                  sched_cfg.max_ticks, move_cfg.dt_seconds)) {
                result.termination_reason = TerminationReason::UNREACHABLE;
                break;
            }
        }
    }
    
    // Convert engagement map to vector
    result.engagement_details.reserve(engagement_map.size());
    for (const auto& [key, summary] : engagement_map) {
        result.engagement_details.push_back(summary);
    }
    result.total_engagements = total_engagement_count;
    result.blue_initial = initial_blue;
    result.red_initial = initial_red;
    
    result.ticks_executed = ctx.current_tick();
    result.completed_normally = true;
    
    // Collect final metrics
    collect_metrics(result, entities.storage(), &systems.combat);
    
    return result;
}

Status MonteCarloExecutor::execute() {
    if (!scenario_) {
        return Error(ErrorCode::INVALID_ARGUMENT, "No scenario set");
    }
    
    if (config_.master_seed == 0) {
        return Error(ErrorCode::RNG_INVALID_SEED, "Master seed must be non-zero");
    }
    
    if (config_.num_iterations == 0) {
        return Error(ErrorCode::INVALID_ARGUMENT, "num_iterations must be > 0");
    }
    
    running_ = true;
    cancelled_ = false;
    results_.clear();
    results_.reserve(config_.num_iterations);
    
    auto batch_t0 = std::chrono::steady_clock::now();
    
    // Execute iterations sequentially (deterministic)
    for (u32 i = 0; i < config_.num_iterations && !cancelled_; ++i) {
        current_iteration_ = i;
        
        Seed iter_seed = derive_seed(i);
        auto t0 = std::chrono::steady_clock::now();
        auto result = run_iteration(i, iter_seed);
        auto t1 = std::chrono::steady_clock::now();
        f64 elapsed_ms = std::chrono::duration<f64, std::milli>(t1 - t0).count();
        
        if (result.ok()) {
            result.get().wall_time_ms = elapsed_ms;
            results_.push_back(std::move(result.get()));
        } else {
            // Record failure but continue
            IterationResult failed;
            failed.iteration_id = i;
            failed.seed = iter_seed;
            failed.completed_normally = false;
            results_.push_back(failed);
        }
        
        // Progress callback
        if (config_.progress_callback) {
            config_.progress_callback(i + 1, config_.num_iterations);
        }

        // Convergence check: stop early if win probability CI is narrow enough
        if (config_.convergence_enabled &&
            (i + 1) >= config_.convergence_min_iterations &&
            (i + 1) % config_.convergence_check_interval == 0)
        {
            u32 n = static_cast<u32>(results_.size());
            if (n >= 2) {
                u32 blue_w = 0;
                for (const auto& r : results_) {
                    if (r.completed_normally && r.blue_surviving > r.red_surviving)
                        blue_w++;
                }
                // Wilson score CI half-width (z=1.96 for 95% CI)
                f64 p_hat = static_cast<f64>(blue_w) / n;
                f64 z = 1.96;
                f64 z2 = z * z;
                f64 denom = 1.0 + z2 / n;
                f64 center = (p_hat + z2 / (2.0 * n)) / denom;
                f64 spread = z * std::sqrt((p_hat * (1.0 - p_hat) + z2 / (4.0 * n)) / n) / denom;
                (void)center;  // We only care about spread width

                if (spread < config_.convergence_threshold) {
                    // Converged — CI is narrow enough
                    break;
                }
            }
        }
    }
    
    auto batch_t1 = std::chrono::steady_clock::now();
    batch_total_time_ms_ = std::chrono::duration<f64, std::milli>(batch_t1 - batch_t0).count();

    running_ = false;
    return Status();
}

f64 MonteCarloExecutor::progress() const {
    if (config_.num_iterations == 0) return 0.0;
    return static_cast<f64>(current_iteration_) / config_.num_iterations;
}

void MonteCarloExecutor::cancel() {
    cancelled_ = true;
}

BatchStatistics MonteCarloExecutor::compute_statistics() const {
    BatchStatistics stats;
    stats.total_iterations = static_cast<u32>(results_.size());
    stats.completed_iterations = 0;
    stats.failed_iterations = 0;
    stats.blue_wins = 0;
    stats.red_wins = 0;
    stats.draws = 0;
    stats.converged_early = (results_.size() < config_.num_iterations && !cancelled_);
    
    std::vector<f64> ticks_data;
    std::vector<f64> blue_survival_data;
    std::vector<f64> red_survival_data;
    std::vector<f64> blue_health_data;
    std::vector<f64> red_health_data;
    
    for (const auto& result : results_) {
        if (result.completed_normally) {
            stats.completed_iterations++;
            
            // Win/loss
            if (result.blue_surviving > result.red_surviving) {
                stats.blue_wins++;
            } else if (result.red_surviving > result.blue_surviving) {
                stats.red_wins++;
            } else {
                stats.draws++;
            }
            
            ticks_data.push_back(static_cast<f64>(result.ticks_executed));
            blue_health_data.push_back(result.blue_total_health);
            red_health_data.push_back(result.red_total_health);
            
            // Survival rates (need initial counts - approximate)
            blue_survival_data.push_back(result.blue_surviving);
            red_survival_data.push_back(result.red_surviving);
        } else {
            stats.failed_iterations++;
        }
    }
    
    // Helper to compute metric stats
    auto compute_metric_stats = [](const std::vector<f64>& data) -> BatchStatistics::MetricStats {
        BatchStatistics::MetricStats ms = {};
        if (data.empty()) return ms;
        
        // Sort for percentiles
        std::vector<f64> sorted = data;
        std::sort(sorted.begin(), sorted.end());
        
        // Mean
        f64 sum = 0.0;
        for (f64 v : data) sum += v;
        ms.mean = sum / data.size();
        
        // Stddev
        f64 var_sum = 0.0;
        for (f64 v : data) {
            f64 diff = v - ms.mean;
            var_sum += diff * diff;
        }
        ms.stddev = std::sqrt(var_sum / data.size());
        
        // Min/max
        ms.min = sorted.front();
        ms.max = sorted.back();
        
        // Median
        usize mid = sorted.size() / 2;
        if (sorted.size() % 2 == 0) {
            ms.median = (sorted[mid - 1] + sorted[mid]) / 2.0;
        } else {
            ms.median = sorted[mid];
        }
        
        // Percentiles
        ms.p5 = sorted[static_cast<usize>(sorted.size() * 0.05)];
        ms.p95 = sorted[static_cast<usize>(sorted.size() * 0.95)];
        
        return ms;
    };
    
    stats.ticks_to_completion = compute_metric_stats(ticks_data);
    stats.blue_survival_rate = compute_metric_stats(blue_survival_data);
    stats.red_survival_rate = compute_metric_stats(red_survival_data);
    stats.blue_final_health = compute_metric_stats(blue_health_data);
    stats.red_final_health = compute_metric_stats(red_health_data);

    // Iteration timing stats (v1.1.1)
    std::vector<f64> timing_data;
    timing_data.reserve(results_.size());
    for (const auto& r : results_) {
        if (r.wall_time_ms > 0.0) timing_data.push_back(r.wall_time_ms);
    }
    stats.iteration_time_ms = compute_metric_stats(timing_data);
    stats.total_batch_time_ms = batch_total_time_ms_;

    // v1.1.7: Termination reason distribution
    for (const auto& r : results_) {
        switch (r.termination_reason) {
            case TerminationReason::DECISIVE_VICTORY:      stats.term_decisive++; break;
            case TerminationReason::MAX_TICKS:             stats.term_max_ticks++; break;
            case TerminationReason::NO_OFFENSIVE_CAPACITY: stats.term_no_offensive++; break;
            case TerminationReason::MUTUAL_BLINDNESS:      stats.term_mutual_blind++; break;
            case TerminationReason::UNREACHABLE:           stats.term_unreachable++; break;
            case TerminationReason::CANCELLED:             stats.term_cancelled++; break;
            default: break;
        }
    }

    // v1.1.7: Sensitivity analysis via outcome correlation
    // For each metric that varies across iterations, compute how much it
    // correlates with the outcome (blue_wins). Uses absolute Pearson r.
    if (stats.completed_iterations >= 30) {
        // Build outcome vector: 1.0 = blue win, 0.0 = not
        std::vector<f64> outcome;
        std::vector<f64> ticks_vec, blue_surv_vec, red_surv_vec;
        std::vector<f64> engagements_vec, blue_health_vec, red_health_vec;
        outcome.reserve(results_.size());
        
        for (const auto& r : results_) {
            if (!r.completed_normally) continue;
            outcome.push_back(r.blue_surviving > r.red_surviving ? 1.0 : 0.0);
            ticks_vec.push_back(static_cast<f64>(r.ticks_executed));
            blue_surv_vec.push_back(static_cast<f64>(r.blue_surviving));
            red_surv_vec.push_back(static_cast<f64>(r.red_surviving));
            engagements_vec.push_back(static_cast<f64>(r.total_engagements));
            blue_health_vec.push_back(r.blue_total_health);
            red_health_vec.push_back(r.red_total_health);
        }
        
        // Pearson correlation helper
        auto pearson_r = [](const std::vector<f64>& x, const std::vector<f64>& y) -> f64 {
            if (x.size() != y.size() || x.size() < 3) return 0.0;
            f64 n = static_cast<f64>(x.size());
            f64 sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
            for (usize i = 0; i < x.size(); ++i) {
                sx += x[i]; sy += y[i];
                sxx += x[i]*x[i]; syy += y[i]*y[i];
                sxy += x[i]*y[i];
            }
            f64 denom = std::sqrt((n*sxx - sx*sx) * (n*syy - sy*sy));
            if (denom < 1e-12) return 0.0;
            return (n*sxy - sx*sy) / denom;
        };
        
        // Compute correlations for available metrics
        struct Factor { std::string name; f64 abs_r; };
        std::vector<Factor> factors;
        
        auto add = [&](const char* name, const std::vector<f64>& vec) {
            f64 r = std::abs(pearson_r(outcome, vec));
            if (std::isfinite(r) && r > 0.01) {
                factors.push_back({name, r});
            }
        };
        
        add("Resolution time", ticks_vec);
        add("Blue force survival", blue_surv_vec);
        add("Red force survival", red_surv_vec);
        add("Total engagements", engagements_vec);
        add("Blue force health", blue_health_vec);
        add("Red force health", red_health_vec);
        
        // Sort by impact (descending)
        std::sort(factors.begin(), factors.end(),
                  [](const Factor& a, const Factor& b) { return a.abs_r > b.abs_r; });
        
        // Top 5
        for (usize i = 0; i < std::min<usize>(5, factors.size()); ++i) {
            stats.sensitivity_factors.emplace_back(factors[i].name, factors[i].abs_r);
        }
    }
    
    return stats;
}

std::string MonteCarloExecutor::export_json() const {
    std::ostringstream oss;
    
    oss << "{\n";
    oss << "  \"config\": {\n";
    oss << "    \"num_iterations\": " << config_.num_iterations << ",\n";
    oss << "    \"master_seed\": " << config_.master_seed << "\n";
    oss << "  },\n";
    
    oss << "  \"results\": [\n";
    for (usize i = 0; i < results_.size(); ++i) {
        const auto& r = results_[i];
        oss << "    {\n";
        oss << "      \"iteration_id\": " << r.iteration_id << ",\n";
        oss << "      \"seed\": " << r.seed << ",\n";
        oss << "      \"ticks_executed\": " << r.ticks_executed << ",\n";
        oss << "      \"completed\": " << (r.completed_normally ? "true" : "false") << ",\n";
        oss << "      \"blue_surviving\": " << r.blue_surviving << ",\n";
        oss << "      \"red_surviving\": " << r.red_surviving << ",\n";
        oss << "      \"blue_total_health\": " << std::setprecision(4) << r.blue_total_health << ",\n";
        oss << "      \"red_total_health\": " << r.red_total_health << "\n";
        oss << "    }";
        if (i < results_.size() - 1) oss << ",";
        oss << "\n";
    }
    oss << "  ],\n";
    
    auto stats = compute_statistics();
    oss << "  \"statistics\": {\n";
    oss << "    \"completed\": " << stats.completed_iterations << ",\n";
    oss << "    \"failed\": " << stats.failed_iterations << ",\n";
    oss << "    \"blue_wins\": " << stats.blue_wins << ",\n";
    oss << "    \"red_wins\": " << stats.red_wins << ",\n";
    oss << "    \"draws\": " << stats.draws << "\n";
    oss << "  }\n";
    
    oss << "}\n";
    
    return oss.str();
}

}  // namespace analysis
}  // namespace athena
