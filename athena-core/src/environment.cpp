// ATHENA Core - Environment System Implementation
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/environment.hpp"
#include <cmath>
#include <algorithm>

namespace athena {

// =============================================================================
// Environment Implementation
// =============================================================================

Environment::Environment()
    : config_()
    , events_()
    , current_tick_(0)
    , initialized_(false)
{
}

Status Environment::init(const TerrainEnvironmentConfig& config) {
    config_ = config;
    events_.clear();
    current_tick_ = 0;
    initialized_ = true;
    return Status();
}

void Environment::reset() {
    config_ = TerrainEnvironmentConfig();
    events_.clear();
    current_tick_ = 0;
    initialized_ = false;
}

void Environment::set_config(const TerrainEnvironmentConfig& config) {
    config_ = config;
}

void Environment::add_event(const EventConfig& event) {
    events_.push_back(event);
}

void Environment::clear_events() {
    events_.clear();
}

void Environment::update(Tick current_tick) {
    current_tick_ = current_tick;
    
    // Remove expired events
    events_.erase(
        std::remove_if(events_.begin(), events_.end(),
            [current_tick](const EventConfig& e) {
                return current_tick > e.start_tick + e.duration_ticks;
            }),
        events_.end());
}

bool Environment::is_event_active(const EventConfig& event, Tick tick) const {
    return tick >= event.start_tick && 
           tick < event.start_tick + event.duration_ticks;
}

f64 Environment::compute_event_radius(const EventConfig& event, Tick tick) const {
    if (!is_event_active(event, tick)) return 0.0;
    
    Tick elapsed = tick - event.start_tick;
    f64 spread = event.spread_rate_m_per_tick * elapsed;
    return event.radius_m + spread;
}

f64 Environment::event_intensity_at(const EventConfig& event, 
                                     f64 x, f64 y, Tick tick) const {
    if (!is_event_active(event, tick)) return 0.0;
    
    f64 dx = x - event.center_x;
    f64 dy = y - event.center_y;
    f64 dist = std::sqrt(dx * dx + dy * dy);
    
    f64 radius = compute_event_radius(event, tick);
    if (dist > radius) return 0.0;
    
    // Linear falloff from center
    f64 falloff = 1.0 - (dist / radius);
    return event.intensity * falloff;
}

bool Environment::is_night_at(Tick tick) const {
    if (!config_.day_night_enabled) return false;
    
    // Assume 1 tick = 1 hour for day/night calculation
    // Adjust based on actual tick duration in real implementation
    f64 hour = std::fmod(config_.current_hour + tick, 24.0);
    
    return hour < config_.sunrise_hour || hour >= config_.sunset_hour;
}

f64 Environment::visibility(f64 x, f64 y, Tick tick) const {
    if (!initialized_) return 10.0;
    
    f64 vis = config_.visibility_km;
    
    // Weather effects
    switch (config_.weather) {
        case Weather::CLEAR:      break;
        case Weather::OVERCAST:   vis *= 0.9; break;
        case Weather::RAIN:       vis *= 0.5; break;
        case Weather::HEAVY_RAIN: vis *= 0.2; break;
        case Weather::SNOW:       vis *= 0.4; break;
        case Weather::BLIZZARD:   vis *= 0.1; break;
        case Weather::FOG:        vis *= 0.1; break;
        case Weather::DUST:       vis *= 0.3; break;
        case Weather::SANDSTORM:  vis *= 0.05; break;
    }
    
    // Night effect
    if (is_night_at(tick)) {
        vis *= 0.3;  // Significant reduction at night
    }
    
    // Event effects (smoke, fire, etc.)
    for (const auto& event : events_) {
        f64 intensity = event_intensity_at(event, x, y, tick);
        if (intensity > 0.0) {
            switch (event.type) {
                case EventType::FIRE:
                    vis *= (1.0 - 0.5 * intensity);  // Smoke
                    break;
                case EventType::CHEMICAL:
                case EventType::NUCLEAR:
                    vis *= (1.0 - 0.8 * intensity);
                    break;
                default:
                    break;
            }
        }
    }
    
    return std::max(0.01, vis);  // Minimum 10m visibility
}

f64 Environment::temperature(f64 x, f64 y, Tick tick) const {
    if (!initialized_) return 15.0;
    
    f64 temp = config_.temperature_c;
    
    // Climate base temperature adjustment
    switch (config_.climate) {
        case Climate::ARCTIC:      temp -= 10.0; break;
        case Climate::TROPICAL:    temp += 5.0; break;
        case Climate::ARID:        temp += 10.0; break;
        default: break;
    }
    
    // Day/night variation
    if (is_night_at(tick)) {
        temp -= 8.0;  // Cooler at night
    }
    
    // Event effects
    for (const auto& event : events_) {
        f64 intensity = event_intensity_at(event, x, y, tick);
        if (intensity > 0.0) {
            switch (event.type) {
                case EventType::FIRE:
                    temp += 50.0 * intensity;  // Fire is hot
                    break;
                case EventType::NUCLEAR:
                    temp += 200.0 * intensity;  // Very hot
                    break;
                default:
                    break;
            }
        }
    }
    
    return temp;
}

bool Environment::is_denied(f64 x, f64 y, Tick tick) const {
    if (!initialized_) return false;
    
    for (const auto& event : events_) {
        f64 intensity = event_intensity_at(event, x, y, tick);
        if (intensity > 0.5) {  // High intensity = denial
            switch (event.type) {
                case EventType::FLOOD:
                case EventType::FIRE:
                case EventType::LANDSLIDE:
                case EventType::CHEMICAL:
                case EventType::NUCLEAR:
                    return true;
                default:
                    break;
            }
        }
    }
    
    return false;
}

EnvironmentSample Environment::sample(f64 x, f64 y, Tick tick) const {
    EnvironmentSample s;
    s.weather = config_.weather;
    s.temperature_c = temperature(x, y, tick);
    s.visibility_km = visibility(x, y, tick);
    s.wind_speed_kmh = config_.wind_speed_kmh;
    s.wind_direction_deg = config_.wind_direction_deg;
    s.is_night = is_night_at(tick);
    s.max_event_intensity = 0.0;
    
    for (const auto& event : events_) {
        f64 intensity = event_intensity_at(event, x, y, tick);
        if (intensity > 0.0) {
            s.active_events.push_back(event.type);
            s.max_event_intensity = std::max(s.max_event_intensity, intensity);
        }
    }
    
    return s;
}

EnvironmentModifiers Environment::get_modifiers(f64 x, f64 y, Tick tick) const {
    EnvironmentModifiers mods = weather_modifiers(config_.weather);
    EnvironmentModifiers climate_mods = climate_modifiers(config_.climate);
    
    // Combine base modifiers
    mods.movement_modifier *= climate_mods.movement_modifier;
    mods.visibility_modifier *= climate_mods.visibility_modifier;
    mods.sensor_modifier *= climate_mods.sensor_modifier;
    mods.logistics_modifier *= climate_mods.logistics_modifier;
    mods.equipment_degradation += climate_mods.equipment_degradation;
    mods.personnel_attrition += climate_mods.personnel_attrition;
    
    // Night effects
    if (is_night_at(tick)) {
        mods.visibility_modifier *= 0.3;
        mods.sensor_modifier *= 0.7;  // Thermal still works
    }
    
    // Event effects
    for (const auto& event : events_) {
        f64 intensity = event_intensity_at(event, x, y, tick);
        if (intensity > 0.0) {
            EnvironmentModifiers event_mods = event_modifiers(event.type, intensity);
            mods.movement_modifier *= event_mods.movement_modifier;
            mods.visibility_modifier *= event_mods.visibility_modifier;
            mods.sensor_modifier *= event_mods.sensor_modifier;
            mods.logistics_modifier *= event_mods.logistics_modifier;
            mods.equipment_degradation += event_mods.equipment_degradation;
            mods.personnel_attrition += event_mods.personnel_attrition;
            if (event_mods.area_denied) mods.area_denied = true;
        }
    }
    
    return mods;
}

// =============================================================================
// Static Modifier Tables
// =============================================================================

EnvironmentModifiers Environment::weather_modifiers(Weather weather) {
    EnvironmentModifiers m = {1.0, 1.0, 1.0, 1.0, 0.0, 0.0, false};
    
    switch (weather) {
        case Weather::CLEAR:
            break;
            
        case Weather::OVERCAST:
            m.sensor_modifier = 0.9;  // Slight sensor degradation
            break;
            
        case Weather::RAIN:
            m.movement_modifier = 1.3;
            m.visibility_modifier = 0.5;
            m.sensor_modifier = 0.7;
            m.equipment_degradation = 0.001;
            break;
            
        case Weather::HEAVY_RAIN:
            m.movement_modifier = 1.8;
            m.visibility_modifier = 0.2;
            m.sensor_modifier = 0.4;
            m.logistics_modifier = 1.5;
            m.equipment_degradation = 0.005;
            break;
            
        case Weather::SNOW:
            m.movement_modifier = 1.5;
            m.visibility_modifier = 0.4;
            m.sensor_modifier = 0.6;
            m.logistics_modifier = 1.3;
            m.equipment_degradation = 0.002;
            break;
            
        case Weather::BLIZZARD:
            m.movement_modifier = 3.0;
            m.visibility_modifier = 0.1;
            m.sensor_modifier = 0.2;
            m.logistics_modifier = 2.0;
            m.equipment_degradation = 0.01;
            m.personnel_attrition = 0.001;
            break;
            
        case Weather::FOG:
            m.movement_modifier = 1.2;
            m.visibility_modifier = 0.1;
            m.sensor_modifier = 0.3;
            break;
            
        case Weather::DUST:
            m.visibility_modifier = 0.3;
            m.sensor_modifier = 0.5;
            m.equipment_degradation = 0.003;
            break;
            
        case Weather::SANDSTORM:
            m.movement_modifier = 2.0;
            m.visibility_modifier = 0.05;
            m.sensor_modifier = 0.1;
            m.logistics_modifier = 2.0;
            m.equipment_degradation = 0.02;
            m.personnel_attrition = 0.001;
            break;
    }
    
    return m;
}

EnvironmentModifiers Environment::climate_modifiers(Climate climate) {
    EnvironmentModifiers m = {1.0, 1.0, 1.0, 1.0, 0.0, 0.0, false};
    
    switch (climate) {
        case Climate::TEMPERATE:
            break;
            
        case Climate::ARCTIC:
            m.movement_modifier = 1.2;
            m.logistics_modifier = 1.5;
            m.equipment_degradation = 0.002;
            m.personnel_attrition = 0.0005;
            break;
            
        case Climate::TROPICAL:
            m.movement_modifier = 1.1;
            m.logistics_modifier = 1.2;
            m.equipment_degradation = 0.003;
            m.personnel_attrition = 0.0003;
            break;
            
        case Climate::ARID:
            m.logistics_modifier = 1.3;  // Water consumption
            m.equipment_degradation = 0.002;
            break;
            
        case Climate::CONTINENTAL:
            m.logistics_modifier = 1.1;
            break;
    }
    
    return m;
}

EnvironmentModifiers Environment::event_modifiers(EventType event, f64 intensity) {
    EnvironmentModifiers m = {1.0, 1.0, 1.0, 1.0, 0.0, 0.0, false};
    
    switch (event) {
        case EventType::NONE:
            break;
            
        case EventType::FLOOD:
            m.movement_modifier = 1.0 + 5.0 * intensity;
            m.visibility_modifier = 0.8;
            if (intensity > 0.5) m.area_denied = true;
            break;
            
        case EventType::FIRE:
            m.movement_modifier = 1.0 + 3.0 * intensity;
            m.visibility_modifier = 1.0 - 0.7 * intensity;
            m.personnel_attrition = 0.01 * intensity;
            if (intensity > 0.7) m.area_denied = true;
            break;
            
        case EventType::LANDSLIDE:
            m.movement_modifier = 1.0 + 10.0 * intensity;
            if (intensity > 0.3) m.area_denied = true;
            break;
            
        case EventType::EARTHQUAKE:
            m.movement_modifier = 2.0;
            m.equipment_degradation = 0.05 * intensity;
            m.personnel_attrition = 0.005 * intensity;
            break;
            
        case EventType::CHEMICAL:
            m.personnel_attrition = 0.1 * intensity;  // Severe
            m.area_denied = true;
            break;
            
        case EventType::NUCLEAR:
            m.sensor_modifier = 0.1;  // EMP
            m.equipment_degradation = 0.2 * intensity;
            m.personnel_attrition = 0.5 * intensity;
            m.area_denied = true;
            break;
    }
    
    return m;
}

// =============================================================================
// String parsing
// =============================================================================

Climate parse_climate(const std::string& name) {
    if (name == "temperate") return Climate::TEMPERATE;
    if (name == "arctic") return Climate::ARCTIC;
    if (name == "tropical") return Climate::TROPICAL;
    if (name == "arid") return Climate::ARID;
    if (name == "continental") return Climate::CONTINENTAL;
    return Climate::TEMPERATE;
}

Weather parse_weather(const std::string& name) {
    if (name == "clear") return Weather::CLEAR;
    if (name == "overcast") return Weather::OVERCAST;
    if (name == "rain") return Weather::RAIN;
    if (name == "heavy_rain") return Weather::HEAVY_RAIN;
    if (name == "snow") return Weather::SNOW;
    if (name == "blizzard") return Weather::BLIZZARD;
    if (name == "fog") return Weather::FOG;
    if (name == "dust") return Weather::DUST;
    if (name == "sandstorm") return Weather::SANDSTORM;
    return Weather::CLEAR;
}

EventType parse_event_type(const std::string& name) {
    if (name == "flood") return EventType::FLOOD;
    if (name == "fire") return EventType::FIRE;
    if (name == "landslide") return EventType::LANDSLIDE;
    if (name == "earthquake") return EventType::EARTHQUAKE;
    if (name == "chemical") return EventType::CHEMICAL;
    if (name == "nuclear") return EventType::NUCLEAR;
    return EventType::NONE;
}

}  // namespace athena
