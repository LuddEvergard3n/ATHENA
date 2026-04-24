// ATHENA Core - Environment System (Environmental Layer)
// Contract: Temporal modifiers that affect cost functions, not terrain.
//
// ARCHITECTURE:
// Layer 2 (Environment) - This file
//   - Weather, temperature, visibility
//   - Modifies cost functions, not terrain geometry
//   - Can vary with time (tick)
//   - Events as temporal functions
//
// RULES:
// - Environment NEVER modifies Layer 1 (Physical)
// - All effects are multiplicative modifiers
// - Events are deterministic functions of (x, y, t)
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_ENVIRONMENT_HPP
#define ATHENA_ENVIRONMENT_HPP

#include "athena/types.hpp"
#include <vector>
#include <string>

namespace athena {

// =============================================================================
// Environment Types
// =============================================================================

enum class Climate : u8 {
    TEMPERATE = 0,  // Moderate conditions
    ARCTIC,         // Cold, snow possible
    TROPICAL,       // Hot, humid, rain possible
    ARID,           // Hot, dry, dust possible
    CONTINENTAL     // Extreme seasonal variation
};

enum class Weather : u8 {
    CLEAR = 0,      // No weather effects
    OVERCAST,       // Reduced air support effectiveness
    RAIN,           // Reduced visibility, movement penalty
    HEAVY_RAIN,     // Severe visibility/movement penalty
    SNOW,           // Cold + movement penalty
    BLIZZARD,       // Severe cold + visibility + movement
    FOG,            // Severe visibility reduction
    DUST,           // Visibility reduction (arid)
    SANDSTORM       // Severe visibility + equipment damage
};

enum class EventType : u8 {
    NONE = 0,
    FLOOD,          // Water level rise, area becomes impassable
    FIRE,           // Spreading damage, area denial
    LANDSLIDE,      // Terrain becomes impassable
    EARTHQUAKE,     // Temporary disruption
    CHEMICAL,       // Area denial, requires protection
    NUCLEAR         // Severe area denial + EMP effects
};

// =============================================================================
// Configuration Structures (DSL)
// =============================================================================

struct TerrainEnvironmentConfig {
    Climate climate = Climate::TEMPERATE;
    Weather weather = Weather::CLEAR;
    
    f64 temperature_c = 15.0;       // Celsius
    f64 visibility_km = 10.0;       // Base visibility
    f64 wind_speed_kmh = 10.0;      // Wind speed
    f64 wind_direction_deg = 0.0;   // Wind from (0 = North)
    f64 humidity_pct = 50.0;        // Relative humidity
    
    // Day/night cycle
    bool day_night_enabled = false;
    f64 sunrise_hour = 6.0;         // Hour of sunrise
    f64 sunset_hour = 18.0;         // Hour of sunset
    f64 current_hour = 12.0;        // Starting hour
};

struct EventConfig {
    EventType type = EventType::NONE;
    
    // Spatial extent
    f64 center_x = 0.0;             // Meters
    f64 center_y = 0.0;             // Meters
    f64 radius_m = 1000.0;          // Affected radius
    
    // Temporal extent
    Tick start_tick = 0;
    Tick duration_ticks = 24;
    
    // Intensity (0-1)
    f64 intensity = 1.0;
    
    // For spreading events (fire, flood)
    f64 spread_rate_m_per_tick = 0.0;  // 0 = no spread
    f64 spread_direction_deg = 0.0;    // Dominant spread direction
};

// =============================================================================
// Environment Query Results
// =============================================================================

struct EnvironmentSample {
    Weather weather;
    f64 temperature_c;
    f64 visibility_km;
    f64 wind_speed_kmh;
    f64 wind_direction_deg;
    bool is_night;
    
    // Active events at this point
    std::vector<EventType> active_events;
    f64 max_event_intensity;
};

// =============================================================================
// Environment Modifiers (output of Layer 2)
// =============================================================================

struct EnvironmentModifiers {
    f64 movement_modifier;      // 1.0 = normal, >1 = slower
    f64 visibility_modifier;    // 1.0 = normal, <1 = reduced
    f64 sensor_modifier;        // 1.0 = normal, <1 = degraded
    f64 logistics_modifier;     // 1.0 = normal, >1 = higher consumption
    f64 equipment_degradation;  // 0.0 = none, >0 = damage per tick
    f64 personnel_attrition;    // 0.0 = none, >0 = casualties per tick
    bool area_denied;           // true = impassable
};

// =============================================================================
// Environment System (Layer 2)
// =============================================================================

class Environment {
public:
    Environment();
    
    /// Initialize environment
    Status init(const TerrainEnvironmentConfig& config);
    
    /// Reset to uninitialized state
    void reset();
    
    /// Check if initialized
    bool is_initialized() const { return initialized_; }
    
    // =========================================================================
    // Configuration
    // =========================================================================
    
    /// Update base environment (e.g., weather change)
    void set_config(const TerrainEnvironmentConfig& config);
    
    /// Add event
    void add_event(const EventConfig& event);
    
    /// Clear all events
    void clear_events();
    
    /// Remove expired events (call each tick)
    void update(Tick current_tick);
    
    // =========================================================================
    // Queries (Layer 2)
    // =========================================================================
    
    /// Get environment sample at point and time
    EnvironmentSample sample(f64 x, f64 y, Tick tick) const;
    
    /// Get visibility at point (km)
    f64 visibility(f64 x, f64 y, Tick tick) const;
    
    /// Get temperature at point (Celsius)
    f64 temperature(f64 x, f64 y, Tick tick) const;
    
    /// Check if point is in area denial
    bool is_denied(f64 x, f64 y, Tick tick) const;
    
    /// Get all modifiers at point (for Layer 3 derivation)
    EnvironmentModifiers get_modifiers(f64 x, f64 y, Tick tick) const;
    
    // =========================================================================
    // Weather Effects (pure functions)
    // =========================================================================
    
    /// Get base modifiers for weather type
    static EnvironmentModifiers weather_modifiers(Weather weather);
    
    /// Get base modifiers for climate type
    static EnvironmentModifiers climate_modifiers(Climate climate);
    
    /// Get event modifiers
    static EnvironmentModifiers event_modifiers(EventType event, f64 intensity);
    
    // =========================================================================
    // Configuration Access
    // =========================================================================
    
    const TerrainEnvironmentConfig& config() const { return config_; }
    const std::vector<EventConfig>& events() const { return events_; }

private:
    TerrainEnvironmentConfig config_;
    std::vector<EventConfig> events_;
    Tick current_tick_;
    bool initialized_;
    
    // Internal helpers
    bool is_event_active(const EventConfig& event, Tick tick) const;
    f64 event_intensity_at(const EventConfig& event, f64 x, f64 y, Tick tick) const;
    f64 compute_event_radius(const EventConfig& event, Tick tick) const;
    bool is_night_at(Tick tick) const;
};

// =============================================================================
// String conversions
// =============================================================================

inline const char* climate_name(Climate c) {
    switch (c) {
        case Climate::TEMPERATE:   return "temperate";
        case Climate::ARCTIC:      return "arctic";
        case Climate::TROPICAL:    return "tropical";
        case Climate::ARID:        return "arid";
        case Climate::CONTINENTAL: return "continental";
        default:                   return "unknown";
    }
}

inline const char* weather_name(Weather w) {
    switch (w) {
        case Weather::CLEAR:      return "clear";
        case Weather::OVERCAST:   return "overcast";
        case Weather::RAIN:       return "rain";
        case Weather::HEAVY_RAIN: return "heavy_rain";
        case Weather::SNOW:       return "snow";
        case Weather::BLIZZARD:   return "blizzard";
        case Weather::FOG:        return "fog";
        case Weather::DUST:       return "dust";
        case Weather::SANDSTORM:  return "sandstorm";
        default:                  return "unknown";
    }
}

inline const char* event_type_name(EventType e) {
    switch (e) {
        case EventType::NONE:      return "none";
        case EventType::FLOOD:     return "flood";
        case EventType::FIRE:      return "fire";
        case EventType::LANDSLIDE: return "landslide";
        case EventType::EARTHQUAKE:return "earthquake";
        case EventType::CHEMICAL:  return "chemical";
        case EventType::NUCLEAR:   return "nuclear";
        default:                   return "unknown";
    }
}

// Parse from string
Climate parse_climate(const std::string& name);
Weather parse_weather(const std::string& name);
EventType parse_event_type(const std::string& name);

}  // namespace athena

#endif  // ATHENA_ENVIRONMENT_HPP
