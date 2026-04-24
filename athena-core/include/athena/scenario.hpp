// ATHENA Core - Scenario Schema Types
// Contract: Matches DATA_SCHEMA.md specification exactly.
//
// RULES:
// - All fields explicitly typed
// - No optional fields without explicit Optional<T>
// - Validation on load, not on use
// - Deterministic iteration order
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_SCENARIO_HPP
#define ATHENA_SCENARIO_HPP

#include "types.hpp"
#include "json.hpp"
#include <string>
#include <vector>
#include <map>
#include <optional>

namespace athena {

// =============================================================================
// Geographic Types
// =============================================================================

struct GeoBounds {
    f64 min_lat;
    f64 max_lat;
    f64 min_lon;
    f64 max_lon;
    
    bool contains(f64 lat, f64 lon) const {
        return lat >= min_lat && lat <= max_lat &&
               lon >= min_lon && lon <= max_lon;
    }
    
    bool valid() const {
        return min_lat <= max_lat && min_lon <= max_lon &&
               min_lat >= -90.0 && max_lat <= 90.0 &&
               min_lon >= -180.0 && max_lon <= 180.0;
    }
};

struct GeoPoint {
    f64 lat;
    f64 lon;
    f64 alt;  // meters above sea level
};

// =============================================================================
// Temporal Types
// =============================================================================

struct TemporalConfig {
    std::string start_date;      // ISO 8601
    std::string end_date;        // ISO 8601
    f64 tick_duration_seconds;   // Duration of one simulation tick
    u64 max_ticks;               // Maximum ticks (0 = unlimited)
};

// =============================================================================
// Scale
// =============================================================================

enum class ScenarioScale : u8 {
    MACRO = 0,      // Strategic (1km resolution)
    MESO = 1,       // Operational (90m resolution)
    MICRO = 2       // Tactical (30m resolution)
};

// =============================================================================
// Distribution Types (for uncertainty)
// =============================================================================

enum class DistributionType : u8 {
    POINT = 0,
    UNIFORM = 1,
    NORMAL = 2,
    LOGNORMAL = 3,
    TRIANGULAR = 4,
    BETA = 5
};

struct Distribution {
    DistributionType type;
    f64 param1;  // min/mean/shape depending on type
    f64 param2;  // max/stddev/scale depending on type
    f64 param3;  // mode (for triangular) or 0
    
    static Distribution point(f64 value) {
        return Distribution{DistributionType::POINT, value, 0.0, 0.0};
    }
    
    static Distribution uniform(f64 min, f64 max) {
        return Distribution{DistributionType::UNIFORM, min, max, 0.0};
    }
    
    static Distribution normal(f64 mean, f64 stddev) {
        return Distribution{DistributionType::NORMAL, mean, stddev, 0.0};
    }
};

// =============================================================================
// Parameter with Uncertainty
// =============================================================================

struct UncertainParameter {
    std::string name;
    f64 base_value;
    std::optional<Distribution> uncertainty;
    std::optional<std::string> source;
    std::optional<f64> confidence;  // 0-1
    bool sensitivity_flag;          // Include in sensitivity analysis
};

// =============================================================================
// Actor Capabilities
// =============================================================================

struct MobilityCapability {
    f64 max_speed_kmh;
    f64 cruise_speed_kmh;
    f64 terrain_factor;      // Multiplier for terrain difficulty
    std::string movement_type;  // "ground", "air", "naval", "amphibious"
};

struct FirepowerCapability {
    f64 base_firepower;
    f64 range_km;
    f64 accuracy;            // 0-1
    f64 rate_of_fire;        // Engagements per tick
};

struct SensorCapability {
    f64 detection_range_km;
    f64 identification_range_km;
    std::string sensor_type;  // "radar", "optical", "sigint", etc.
};

struct LogisticsCapability {
    f64 fuel_capacity;
    f64 ammo_capacity;
    f64 consumption_rate;    // Per tick
    f64 resupply_rate;       // Per tick when at depot
};

struct C2Capability {
    f64 command_range_km;
    f64 communication_reliability;  // 0-1
    u32 max_subordinates;
};

// =============================================================================
// Actor Definition
// =============================================================================

struct ActorDefinition {
    // Identity
    std::string id;
    std::string name;
    std::string type;            // "unit", "formation", "asset"
    std::string side;            // "blue", "red", "neutral"
    
    // Platform reference (if set, capabilities derived from platform database)
    std::string platform_id;     // e.g. "de-leopard2a7", "us-m4a1"
    
    // Hierarchy
    std::optional<std::string> parent_id;
    std::vector<std::string> subordinate_ids;
    
    // Capabilities
    std::optional<MobilityCapability> mobility;
    std::optional<FirepowerCapability> firepower;
    std::optional<SensorCapability> sensors;
    std::optional<LogisticsCapability> logistics;
    std::optional<C2Capability> command;
    
    // Initial state
    GeoPoint initial_position;
    f64 initial_health;          // 0-1
    f64 initial_supply;          // 0-1
    f64 initial_morale;          // 0-1
    f64 initial_readiness;       // 0-1
    
    // Behavior
    std::string behavior_script; // Reference to behavior definition
    std::map<std::string, f64> behavior_params;
};

// =============================================================================
// Environment Configuration
// =============================================================================

struct TerrainConfig {
    std::string theater_id;
    ScenarioScale resolution;
    std::optional<GeoBounds> area_of_interest;  // Subset of theater

    // Procedural terrain generation (v1.1)
    std::string base_type;          // "flat", "rolling", "mountains", "coastal"
    f64 roughness = 0.3;            // 0-1, elevation variation intensity
    f64 forest_density = 0.0;       // 0-1, fraction covered by forest
    f64 urban_density = 0.0;        // 0-1, fraction covered by urban
    bool rivers_enabled = false;    // Generate river obstacles
};

struct ClimateConfig {
    std::string season;          // "winter", "spring", "summer", "fall"
    std::string weather_model;   // Reference to weather model
    bool weather_effects_enabled;
};

struct InfrastructureConfig {
    bool roads_enabled;
    bool ports_enabled;
    bool airfields_enabled;
    bool bridges_enabled;
    std::vector<std::string> critical_nodes;  // IDs of key infrastructure
};

struct EnvironmentConfig {
    TerrainConfig terrain;
    ClimateConfig climate;
    InfrastructureConfig infrastructure;
};

// =============================================================================
// Scenario Root
// =============================================================================

struct ScenarioMetadata {
    std::string schema_version;
    std::string created_at;
    std::string modified_at;
    std::string created_by;
    std::optional<std::string> description;
    std::vector<std::string> tags;
};

struct Scenario {
    // Identity
    std::string id;
    std::string name;
    std::string version;
    
    // Metadata
    ScenarioMetadata metadata;
    
    // Configuration
    ScenarioScale scale;
    TemporalConfig temporal;
    GeoBounds spatial_bounds;
    
    // Environment
    EnvironmentConfig environment;
    
    // Actors
    std::vector<ActorDefinition> actors;
    
    // Parameters
    std::map<std::string, UncertainParameter> parameters;
    
    // Validation
    bool is_valid;
    std::vector<std::string> validation_errors;
};

// =============================================================================
// Scenario Loader
// =============================================================================

class ScenarioLoader {
public:
    ScenarioLoader() = default;
    
    // Load scenario from JSON file
    Result<Scenario> load(const std::string& path);
    
    // Load scenario from JSON value
    Result<Scenario> load(const json::Value& json);
    
    // Validate scenario (called automatically by load)
    bool validate(Scenario& scenario);
    
    // Get last error details
    const std::string& last_error() const { return last_error_; }

private:
    std::string last_error_;
    
    // Parsing helpers
    Result<GeoBounds> parse_geo_bounds(const json::Value& v);
    Result<GeoPoint> parse_geo_point(const json::Value& v);
    Result<TemporalConfig> parse_temporal(const json::Value& v);
    Result<Distribution> parse_distribution(const json::Value& v);
    Result<UncertainParameter> parse_parameter(const json::Value& v);
    Result<ActorDefinition> parse_actor(const json::Value& v);
    Result<EnvironmentConfig> parse_environment(const json::Value& v);
    Result<ScenarioMetadata> parse_metadata(const json::Value& v);
    
    // Validation helpers
    bool validate_actor(const ActorDefinition& actor, Scenario& scenario);
    bool validate_bounds(const Scenario& scenario);
    bool validate_references(Scenario& scenario);
};

// =============================================================================
// Scenario to EntityStorage Converter
// =============================================================================

// Convert loaded scenario into EntityStorage for simulation
class ScenarioConverter {
public:
    // Set platform database for spec-driven entity creation.
    // If set, actors with platform_id will derive all combat attributes
    // from the platform specs instead of manual capabilities.
    void set_platform_database(const class PlatformDatabase* db) { platform_db_ = db; }

    // Convert scenario actors to entity storage
    // Returns number of entities created
    Result<usize> convert(const Scenario& scenario, 
                          class EntityManager& entities,
                          class Rng& rng);
    
    // Get mapping of actor IDs to entity IDs (after convert)
    const std::map<std::string, EntityId>& actor_mapping() const {
        return actor_to_entity_;
    }

private:
    std::map<std::string, EntityId> actor_to_entity_;
    const class PlatformDatabase* platform_db_ = nullptr;
};

}  // namespace athena

#endif  // ATHENA_SCENARIO_HPP
