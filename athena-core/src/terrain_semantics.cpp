// ATHENA Core - Terrain Semantics Implementation
//
// Layer 3: Derived operational meaning.
// All values computed on demand, never stored.
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/terrain_semantics.hpp"
#include <cmath>
#include <algorithm>

namespace athena {

// =============================================================================
// TerrainSemantics Implementation
// =============================================================================

TerrainSemantics::TerrainSemantics()
    : physical_(nullptr)
    , environment_(nullptr)
    , initialized_(false)
{
}

Status TerrainSemantics::init(const PhysicalTerrain* physical, 
                               const Environment* environment) {
    if (!physical || !physical->is_initialized()) {
        return Error(ErrorCode::INVALID_ARGUMENT, 
            "Physical terrain must be initialized");
    }
    
    physical_ = physical;
    environment_ = environment;  // Can be null (no environmental effects)
    initialized_ = true;
    return Status();
}

void TerrainSemantics::reset() {
    physical_ = nullptr;
    environment_ = nullptr;
    initialized_ = false;
}

// =============================================================================
// Internal Helpers
// =============================================================================

f64 TerrainSemantics::slope_movement_cost(f64 slope_deg, MovementType type) const {
    using namespace terrain_tables;
    
    // Aircraft ignore terrain
    if (type == MovementType::HELICOPTER || type == MovementType::FIXED_WING) {
        return 1.0;
    }
    
    // Naval only in water
    if (type == MovementType::NAVAL) {
        return 1.0;  // Handled separately
    }
    
    // Ground units affected by slope
    if (slope_deg >= SLOPE_IMPASSABLE) {
        return 999.0;  // Impassable
    }
    
    if (slope_deg >= 30.0) {
        // Tracked can handle steep slopes better
        if (type == MovementType::TRACKED) {
            return SLOPE_COST_30_45 * 0.8;
        }
        return SLOPE_COST_30_45;
    }
    
    if (slope_deg >= 15.0) {
        return SLOPE_COST_15_30;
    }
    
    if (slope_deg >= 5.0) {
        return SLOPE_COST_5_15;
    }
    
    return SLOPE_COST_0_5;
}

f64 TerrainSemantics::overlay_movement_cost(OverlayType overlay, 
                                            MovementType type) const {
    using namespace terrain_tables;
    
    // Aircraft ignore most overlays
    if (type == MovementType::HELICOPTER || type == MovementType::FIXED_WING) {
        return 1.0;
    }
    
    // Amphibious can cross water
    if (type == MovementType::AMPHIBIOUS && 
        (overlay == OverlayType::RIVER || overlay == OverlayType::LAKE)) {
        return 2.0;  // Slow but possible
    }
    
    switch (overlay) {
        case OverlayType::NONE:
            return 1.0;
            
        case OverlayType::FOREST:
            if (type == MovementType::FOOT) return FOREST_COST.foot;
            if (type == MovementType::WHEELED) return FOREST_COST.wheeled;
            if (type == MovementType::TRACKED) return FOREST_COST.tracked;
            break;
            
        case OverlayType::URBAN:
            if (type == MovementType::FOOT) return URBAN_COST.foot;
            if (type == MovementType::WHEELED) return URBAN_COST.wheeled;
            if (type == MovementType::TRACKED) return URBAN_COST.tracked;
            break;
            
        case OverlayType::SWAMP:
            if (type == MovementType::FOOT) return SWAMP_COST.foot;
            if (type == MovementType::WHEELED) return SWAMP_COST.wheeled;
            if (type == MovementType::TRACKED) return SWAMP_COST.tracked;
            break;
            
        case OverlayType::RIVER:
        case OverlayType::LAKE:
            if (type == MovementType::NAVAL) return 1.0;
            return RIVER_COST.foot;  // Impassable for ground
            
        case OverlayType::ROAD:
            if (type == MovementType::FOOT) return ROAD_COST.foot;
            if (type == MovementType::WHEELED) return ROAD_COST.wheeled;
            if (type == MovementType::TRACKED) return ROAD_COST.tracked;
            break;
            
        case OverlayType::BRIDGE:
            return 1.0;  // Normal crossing
    }
    
    return 1.0;
}

f64 TerrainSemantics::overlay_cover(OverlayType overlay) const {
    using namespace terrain_tables;
    
    switch (overlay) {
        case OverlayType::FOREST: return FOREST_COVER;
        case OverlayType::URBAN:  return URBAN_COVER;
        case OverlayType::SWAMP:  return SWAMP_COVER;
        default:                  return OPEN_COVER;
    }
}

f64 TerrainSemantics::overlay_concealment(OverlayType overlay) const {
    using namespace terrain_tables;
    
    switch (overlay) {
        case OverlayType::FOREST: return FOREST_CONCEALMENT;
        case OverlayType::URBAN:  return URBAN_CONCEALMENT;
        case OverlayType::SWAMP:  return SWAMP_CONCEALMENT;
        default:                  return OPEN_CONCEALMENT;
    }
}

f64 TerrainSemantics::overlay_sensor_effect(OverlayType overlay, 
                                            SensorType sensor) const {
    switch (overlay) {
        case OverlayType::FOREST:
            switch (sensor) {
                case SensorType::VISUAL:   return 0.3;
                case SensorType::THERMAL:  return 0.5;
                case SensorType::RADAR:    return 0.4;  // Clutter
                case SensorType::ACOUSTIC: return 0.8;
                case SensorType::SIGNALS:   return 0.9;
                case SensorType::SEISMIC:  return 1.0;
            }
            break;
            
        case OverlayType::URBAN:
            switch (sensor) {
                case SensorType::VISUAL:   return 0.4;
                case SensorType::THERMAL:  return 0.6;
                case SensorType::RADAR:    return 0.3;  // Heavy clutter
                case SensorType::ACOUSTIC: return 0.5;  // Noise
                case SensorType::SIGNALS:   return 0.7;  // Interference
                case SensorType::SEISMIC:  return 0.6;
            }
            break;
            
        case OverlayType::SWAMP:
            switch (sensor) {
                case SensorType::VISUAL:   return 0.5;
                case SensorType::THERMAL:  return 0.7;
                case SensorType::RADAR:    return 0.6;
                case SensorType::ACOUSTIC: return 0.6;
                case SensorType::SIGNALS:   return 1.0;
                case SensorType::SEISMIC:  return 0.4;  // Water absorption
            }
            break;
            
        default:
            return 1.0;
    }
    
    return 1.0;
}

// =============================================================================
// Movement Queries
// =============================================================================

MovementCost TerrainSemantics::movement_cost(f64 x, f64 y, Tick tick, 
                                             MovementType type) const {
    MovementCost result = {1.0, true, 1.0, 0.0};
    
    if (!initialized_) return result;
    
    // Get physical terrain data
    TerrainSample terrain = physical_->sample(x, y);
    
    // Check basic passability
    if (terrain.is_water && type != MovementType::NAVAL && 
        type != MovementType::AMPHIBIOUS &&
        type != MovementType::HELICOPTER &&
        type != MovementType::FIXED_WING) {
        result.passable = false;
        result.cost = 999.0;
        return result;
    }
    
    // Naval only in water
    if (type == MovementType::NAVAL && !terrain.is_water) {
        result.passable = false;
        result.cost = 999.0;
        return result;
    }
    
    // Slope cost
    f64 slope_cost = slope_movement_cost(terrain.slope, type);
    if (slope_cost >= 999.0) {
        result.passable = false;
        result.cost = 999.0;
        return result;
    }
    
    // Overlay cost
    f64 overlay_cost = overlay_movement_cost(terrain.primary_overlay, type);
    if (overlay_cost >= 999.0) {
        result.passable = false;
        result.cost = 999.0;
        return result;
    }
    
    // Base cost from terrain
    result.cost = slope_cost * overlay_cost;
    
    // Environmental modifiers
    if (environment_ && environment_->is_initialized()) {
        // Check area denial
        if (environment_->is_denied(x, y, tick)) {
            result.passable = false;
            result.cost = 999.0;
            return result;
        }
        
        EnvironmentModifiers env_mods = environment_->get_modifiers(x, y, tick);
        result.cost *= env_mods.movement_modifier;
        result.fuel_modifier = env_mods.logistics_modifier;
        result.fatigue_rate = env_mods.personnel_attrition;
    }
    
    // Obstacle effect
    if (terrain.obstacle > 0.5) {
        result.cost *= (1.0 + terrain.obstacle * 2.0);
        if (terrain.obstacle > 0.9) {
            result.passable = false;
        }
    }
    
    return result;
}

bool TerrainSemantics::is_passable(f64 x, f64 y, Tick tick, 
                                   MovementType type) const {
    return movement_cost(x, y, tick, type).passable;
}

f64 TerrainSemantics::optimal_direction(f64 x, f64 y, MovementType type) const {
    if (!initialized_) return 0.0;
    
    // For ground units, downhill is easier
    if (type != MovementType::HELICOPTER && type != MovementType::FIXED_WING) {
        return physical_->slope_direction(x, y);
    }
    
    return 0.0;  // No preference for air
}

// =============================================================================
// Combat Queries
// =============================================================================

DefenseValue TerrainSemantics::defense_value(f64 x, f64 y, Tick tick) const {
    DefenseValue result = {0.0, 0.0, 1.0};
    
    if (!initialized_) return result;
    
    TerrainSample terrain = physical_->sample(x, y);
    
    // Base cover/concealment from overlay
    result.cover = overlay_cover(terrain.primary_overlay);
    result.concealment = overlay_concealment(terrain.primary_overlay);
    
    // Slope provides cover (higher ground advantage)
    if (terrain.slope > 10.0) {
        result.cover += 0.1;
    }
    
    // Environmental effects on concealment
    if (environment_ && environment_->is_initialized()) {
        EnvironmentModifiers mods = environment_->get_modifiers(x, y, tick);
        
        // Low visibility helps concealment
        if (mods.visibility_modifier < 0.5) {
            result.concealment += 0.2;
        }
    }
    
    // Clamp
    result.cover = std::min(1.0, result.cover);
    result.concealment = std::min(1.0, result.concealment);
    
    // Combined advantage
    result.advantage = 1.0 + result.cover * 0.5 + result.concealment * 0.3;
    
    return result;
}

f64 TerrainSemantics::cover(f64 x, f64 y, Tick tick) const {
    return defense_value(x, y, tick).cover;
}

f64 TerrainSemantics::concealment(f64 x, f64 y, Tick tick) const {
    return defense_value(x, y, tick).concealment;
}

f64 TerrainSemantics::engagement_range_modifier(f64 x, f64 y, Tick tick) const {
    if (!initialized_) return 1.0;
    
    TerrainSample terrain = physical_->sample(x, y);
    
    f64 modifier = 1.0;
    
    // Dense overlays reduce effective range
    switch (terrain.primary_overlay) {
        case OverlayType::FOREST: modifier *= 0.5; break;
        case OverlayType::URBAN:  modifier *= 0.4; break;
        case OverlayType::SWAMP:  modifier *= 0.7; break;
        default: break;
    }
    
    // Visibility effects
    if (environment_ && environment_->is_initialized()) {
        f64 vis = environment_->visibility(x, y, tick);
        // Normalize visibility effect (10km = full range)
        modifier *= std::min(1.0, vis / 10.0);
    }
    
    return modifier;
}

// =============================================================================
// Sensor Queries
// =============================================================================

SensorEffectiveness TerrainSemantics::sensor_effectiveness(
    f64 x, f64 y, Tick tick, SensorType sensor) const {
    
    SensorEffectiveness result = {1.0, 1.0, 0.0};
    
    if (!initialized_) return result;
    
    TerrainSample terrain = physical_->sample(x, y);
    
    // Overlay effects
    f64 overlay_effect = overlay_sensor_effect(terrain.primary_overlay, sensor);
    result.detection_modifier = overlay_effect;
    result.identification_modifier = overlay_effect * 0.8;  // ID harder than detection
    
    // False positives in cluttered terrain
    if (terrain.primary_overlay == OverlayType::URBAN) {
        result.false_positive_rate = 0.2;
    } else if (terrain.primary_overlay == OverlayType::FOREST) {
        result.false_positive_rate = 0.1;
    }
    
    // Environmental effects
    if (environment_ && environment_->is_initialized()) {
        EnvironmentModifiers mods = environment_->get_modifiers(x, y, tick);
        result.detection_modifier *= mods.sensor_modifier;
        result.identification_modifier *= mods.sensor_modifier * 0.8;
        
        // Visibility affects visual sensors heavily
        if (sensor == SensorType::VISUAL) {
            result.detection_modifier *= mods.visibility_modifier;
            result.identification_modifier *= mods.visibility_modifier;
        }
        
        // Thermal less affected by weather (except rain)
        if (sensor == SensorType::THERMAL) {
            f64 thermal_weather = std::max(0.5, mods.visibility_modifier);
            result.detection_modifier *= thermal_weather;
        }
    }
    
    return result;
}

f64 TerrainSemantics::line_of_sight(f64 x1, f64 y1, f64 x2, f64 y2, 
                                    Tick tick) const {
    if (!initialized_) return 1.0;
    
    // Sample terrain along line
    auto samples = physical_->sample_line(x1, y1, x2, y2, 50.0);  // 50m intervals
    
    if (samples.size() < 2) return 1.0;
    
    // Compute LOS using elevation profile
    f64 start_elev = samples.front().elevation + 2.0;  // Observer height
    f64 end_elev = samples.back().elevation + 2.0;      // Target height
    
    f64 blocked_fraction = 0.0;
    
    for (usize i = 1; i < samples.size() - 1; ++i) {
        f64 t = static_cast<f64>(i) / (samples.size() - 1);
        f64 expected_elev = start_elev + t * (end_elev - start_elev);
        
        // Check if terrain blocks LOS
        if (samples[i].elevation > expected_elev) {
            blocked_fraction += 1.0 / samples.size();
        }
        
        // Dense overlays partially block LOS
        f64 concealment = overlay_concealment(samples[i].primary_overlay);
        blocked_fraction += concealment * 0.1 / samples.size();
    }
    
    return std::max(0.0, 1.0 - blocked_fraction);
}

f64 TerrainSemantics::detection_modifier(f64 x, f64 y, Tick tick, 
                                         SensorType sensor) const {
    return sensor_effectiveness(x, y, tick, sensor).detection_modifier;
}

f64 TerrainSemantics::elevation(f64 x, f64 y) const {
    if (!initialized_ || !physical_) return 0.0;
    return physical_->elevation(x, y);
}

// =============================================================================
// Logistics Queries
// =============================================================================

f64 TerrainSemantics::logistics_penalty(f64 x, f64 y, Tick tick) const {
    if (!initialized_) return 1.0;
    
    f64 penalty = 1.0;
    
    TerrainSample terrain = physical_->sample(x, y);
    
    // Difficult terrain increases consumption
    if (terrain.slope > 15.0) penalty *= 1.2;
    if (terrain.slope > 30.0) penalty *= 1.5;
    
    // Overlays
    switch (terrain.primary_overlay) {
        case OverlayType::SWAMP: penalty *= 1.5; break;
        case OverlayType::FOREST: penalty *= 1.2; break;
        default: break;
    }
    
    // Environment
    if (environment_ && environment_->is_initialized()) {
        EnvironmentModifiers mods = environment_->get_modifiers(x, y, tick);
        penalty *= mods.logistics_modifier;
    }
    
    return penalty;
}

f64 TerrainSemantics::equipment_degradation(f64 x, f64 y, Tick tick) const {
    if (!initialized_ || !environment_ || !environment_->is_initialized()) {
        return 0.0;
    }
    
    return environment_->get_modifiers(x, y, tick).equipment_degradation;
}

f64 TerrainSemantics::personnel_attrition(f64 x, f64 y, Tick tick) const {
    if (!initialized_ || !environment_ || !environment_->is_initialized()) {
        return 0.0;
    }
    
    return environment_->get_modifiers(x, y, tick).personnel_attrition;
}

// =============================================================================
// Operational Queries
// =============================================================================

f64 TerrainSemantics::chokepoint_value(f64 x, f64 y) const {
    if (!initialized_) return 0.0;
    
    // Sample surrounding area to detect constrictions
    constexpr f64 SAMPLE_RADIUS = 500.0;  // meters
    constexpr int SAMPLES = 8;
    
    int passable_count = 0;
    
    for (int i = 0; i < SAMPLES; ++i) {
        f64 angle = (2.0 * 3.14159265358979323846 * i) / SAMPLES;
        f64 sx = x + SAMPLE_RADIUS * std::cos(angle);
        f64 sy = y + SAMPLE_RADIUS * std::sin(angle);
        
        TerrainSample s = physical_->sample(sx, sy);
        if (!s.is_water && s.obstacle < 0.5 && s.slope < 45.0) {
            passable_count++;
        }
    }
    
    // Fewer passable directions = more of a chokepoint
    if (passable_count <= 2) return 1.0;
    if (passable_count <= 4) return 0.5;
    return 0.0;
}

f64 TerrainSemantics::armor_suitability(f64 x, f64 y, Tick tick) const {
    if (!initialized_) return 0.5;
    
    TerrainSample terrain = physical_->sample(x, y);
    
    f64 suitability = 1.0;
    
    // Steep slopes bad for armor
    if (terrain.slope > 30.0) suitability *= 0.2;
    else if (terrain.slope > 15.0) suitability *= 0.5;
    
    // Overlays
    switch (terrain.primary_overlay) {
        case OverlayType::FOREST: suitability *= 0.3; break;  // Bad
        case OverlayType::URBAN:  suitability *= 0.4; break;  // Risky
        case OverlayType::SWAMP:  suitability *= 0.1; break;  // Very bad
        case OverlayType::ROAD:   suitability *= 1.2; break;  // Good
        default: break;
    }
    
    // Water impassable
    if (terrain.is_water) suitability = 0.0;
    
    return std::min(1.0, suitability);
}

f64 TerrainSemantics::infantry_suitability(f64 x, f64 y, Tick tick) const {
    if (!initialized_) return 0.5;
    
    TerrainSample terrain = physical_->sample(x, y);
    
    f64 suitability = 1.0;
    
    // Infantry handles slopes better
    if (terrain.slope > 45.0) suitability *= 0.3;
    else if (terrain.slope > 30.0) suitability *= 0.6;
    
    // Overlays
    switch (terrain.primary_overlay) {
        case OverlayType::FOREST: suitability *= 1.2; break;  // Good cover
        case OverlayType::URBAN:  suitability *= 1.3; break;  // Excellent
        case OverlayType::SWAMP:  suitability *= 0.4; break;  // Tough
        default: break;
    }
    
    if (terrain.is_water) suitability = 0.0;
    
    return std::min(1.0, suitability);
}

f64 TerrainSemantics::air_suitability(f64 x, f64 y, Tick tick) const {
    if (!initialized_) return 0.8;
    
    f64 suitability = 1.0;
    
    // Terrain barely matters for fixed-wing
    // Helicopters care about landing zones
    
    TerrainSample terrain = physical_->sample(x, y);
    
    // Can't land on steep terrain
    if (terrain.slope > 15.0) suitability *= 0.5;
    if (terrain.slope > 30.0) suitability *= 0.2;
    
    // Can't land in water or dense forest
    if (terrain.is_water) suitability *= 0.3;
    if (terrain.primary_overlay == OverlayType::FOREST) suitability *= 0.4;
    
    // Weather matters most
    if (environment_ && environment_->is_initialized()) {
        EnvironmentModifiers mods = environment_->get_modifiers(x, y, tick);
        suitability *= mods.visibility_modifier;  // Can't fly blind
    }
    
    return std::min(1.0, suitability);
}

// =============================================================================
// Bulk Queries
// =============================================================================

f64 TerrainSemantics::path_cost(const std::vector<std::pair<f64, f64>>& path,
                                Tick tick, MovementType type) const {
    if (!initialized_ || path.size() < 2) return 0.0;
    
    f64 total_cost = 0.0;
    
    for (usize i = 1; i < path.size(); ++i) {
        f64 x1 = path[i-1].first;
        f64 y1 = path[i-1].second;
        f64 x2 = path[i].first;
        f64 y2 = path[i].second;
        
        // Distance
        f64 dx = x2 - x1;
        f64 dy = y2 - y1;
        f64 dist = std::sqrt(dx * dx + dy * dy);
        
        // Cost at midpoint
        f64 mx = (x1 + x2) / 2.0;
        f64 my = (y1 + y2) / 2.0;
        MovementCost mc = movement_cost(mx, my, tick, type);
        
        if (!mc.passable) {
            return 999999.0;  // Path blocked
        }
        
        total_cost += dist * mc.cost;
    }
    
    return total_cost;
}

std::vector<std::pair<f64, f64>> TerrainSemantics::find_defensive_positions(
    f64 center_x, f64 center_y, f64 radius,
    Tick tick, int max_results) const {
    
    std::vector<std::pair<f64, f64>> positions;
    
    if (!initialized_) return positions;
    
    // Grid search
    constexpr f64 STEP = 100.0;  // 100m grid
    
    std::vector<std::pair<f64, std::pair<f64, f64>>> scored;
    
    for (f64 dx = -radius; dx <= radius; dx += STEP) {
        for (f64 dy = -radius; dy <= radius; dy += STEP) {
            if (dx * dx + dy * dy > radius * radius) continue;
            
            f64 x = center_x + dx;
            f64 y = center_y + dy;
            
            DefenseValue dv = defense_value(x, y, tick);
            scored.push_back({dv.advantage, {x, y}});
        }
    }
    
    // Sort by defense value
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });
    
    // Return top results
    for (int i = 0; i < max_results && i < static_cast<int>(scored.size()); ++i) {
        positions.push_back(scored[i].second);
    }
    
    return positions;
}

}  // namespace athena
