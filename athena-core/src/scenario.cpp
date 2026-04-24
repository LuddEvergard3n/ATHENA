// ATHENA Core - Scenario Loader Implementation
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/scenario.hpp"
#include "athena/entities.hpp"
#include "athena/platform_loader.hpp"
#include "athena/rng.hpp"
#include <set>
#include <cmath>

namespace athena {

// =============================================================================
// ScenarioLoader Implementation
// =============================================================================

Result<Scenario> ScenarioLoader::load(const std::string& path) {
    auto json_result = json::parse_file(path);
    if (!json_result.ok()) {
        last_error_ = "Failed to parse JSON: " + json_result.error.message;
        return json_result.error;
    }
    
    return load(json_result.get());
}

Result<Scenario> ScenarioLoader::load(const json::Value& json) {
    if (!json.is_object()) {
        last_error_ = "Scenario root must be an object";
        return Error(ErrorCode::INVALID_ARGUMENT, last_error_);
    }
    
    Scenario scenario;
    scenario.is_valid = false;
    
    // Required fields
    if (!json.has("id") || !json["id"].is_string()) {
        last_error_ = "Missing or invalid 'id' field";
        return Error(ErrorCode::INVALID_ARGUMENT, last_error_);
    }
    scenario.id = json["id"].as_string();
    
    if (!json.has("name") || !json["name"].is_string()) {
        last_error_ = "Missing or invalid 'name' field";
        return Error(ErrorCode::INVALID_ARGUMENT, last_error_);
    }
    scenario.name = json["name"].as_string();
    
    scenario.version = json.get_string("version", "1.0.0");
    
    // Parse metadata
    if (json.has("metadata")) {
        auto meta_result = parse_metadata(json["metadata"]);
        if (!meta_result.ok()) return meta_result.error;
        scenario.metadata = std::move(meta_result.get());
    } else {
        scenario.metadata.schema_version = constants::SCHEMA_VERSION;
    }
    
    // Parse scale
    std::string scale_str = json.get_string("scale", "meso");
    if (scale_str == "macro") {
        scenario.scale = ScenarioScale::MACRO;
    } else if (scale_str == "meso") {
        scenario.scale = ScenarioScale::MESO;
    } else if (scale_str == "micro") {
        scenario.scale = ScenarioScale::MICRO;
    } else {
        last_error_ = "Invalid scale: " + scale_str;
        return Error(ErrorCode::INVALID_ARGUMENT, last_error_);
    }
    
    // Parse temporal config
    if (json.has("temporal")) {
        auto temp_result = parse_temporal(json["temporal"]);
        if (!temp_result.ok()) return temp_result.error;
        scenario.temporal = std::move(temp_result.get());
    } else {
        // Defaults
        scenario.temporal.start_date = "2025-01-01T00:00:00Z";
        scenario.temporal.end_date = "2025-12-31T23:59:59Z";
        scenario.temporal.tick_duration_seconds = 3600.0;  // 1 hour
        scenario.temporal.max_ticks = 0;
    }
    
    // Parse spatial bounds
    if (json.has("spatial_bounds")) {
        auto bounds_result = parse_geo_bounds(json["spatial_bounds"]);
        if (!bounds_result.ok()) return bounds_result.error;
        scenario.spatial_bounds = bounds_result.get();
    } else {
        // Default to global
        scenario.spatial_bounds = GeoBounds{-90.0, 90.0, -180.0, 180.0};
    }
    
    // Parse environment
    if (json.has("environment")) {
        auto env_result = parse_environment(json["environment"]);
        if (!env_result.ok()) return env_result.error;
        scenario.environment = std::move(env_result.get());
    }
    
    // Parse actors
    if (json.has("actors") && json["actors"].is_array()) {
        const auto& actors_json = json["actors"].as_array();
        scenario.actors.reserve(actors_json.size());
        
        for (usize i = 0; i < actors_json.size(); ++i) {
            auto actor_result = parse_actor(actors_json[i]);
            if (!actor_result.ok()) {
                last_error_ = "Error parsing actor " + std::to_string(i) + 
                             ": " + actor_result.error.message;
                return actor_result.error;
            }
            scenario.actors.push_back(std::move(actor_result.get()));
        }
    }
    
    // Parse parameters
    if (json.has("parameters") && json["parameters"].is_object()) {
        const auto& params_json = json["parameters"].as_object();
        for (const auto& [key, value] : params_json) {
            auto param_result = parse_parameter(value);
            if (!param_result.ok()) {
                last_error_ = "Error parsing parameter '" + key + 
                             "': " + param_result.error.message;
                return param_result.error;
            }
            auto param = std::move(param_result.get());
            param.name = key;
            scenario.parameters[key] = std::move(param);
        }
    }
    
    // Validate
    if (!validate(scenario)) {
        return Error(ErrorCode::INVALID_ARGUMENT, 
            "Scenario validation failed: " + last_error_);
    }
    
    scenario.is_valid = true;
    return scenario;
}

// =============================================================================
// Parsing Helpers
// =============================================================================

Result<GeoBounds> ScenarioLoader::parse_geo_bounds(const json::Value& v) {
    if (!v.is_object()) {
        return Error(ErrorCode::INVALID_ARGUMENT, "GeoBounds must be an object");
    }
    
    GeoBounds bounds;
    bounds.min_lat = v.get_number("min_lat", -90.0);
    bounds.max_lat = v.get_number("max_lat", 90.0);
    bounds.min_lon = v.get_number("min_lon", -180.0);
    bounds.max_lon = v.get_number("max_lon", 180.0);
    
    if (!bounds.valid()) {
        return Error(ErrorCode::INVALID_ARGUMENT, "Invalid geographic bounds");
    }
    
    return bounds;
}

Result<GeoPoint> ScenarioLoader::parse_geo_point(const json::Value& v) {
    if (!v.is_object()) {
        return Error(ErrorCode::INVALID_ARGUMENT, "GeoPoint must be an object");
    }
    
    GeoPoint point;
    point.lat = v.get_number("lat", 0.0);
    point.lon = v.get_number("lon", 0.0);
    point.alt = v.get_number("alt", 0.0);
    
    if (point.lat < -90.0 || point.lat > 90.0) {
        return Error(ErrorCode::INVALID_ARGUMENT, "Invalid latitude");
    }
    if (point.lon < -180.0 || point.lon > 180.0) {
        return Error(ErrorCode::INVALID_ARGUMENT, "Invalid longitude");
    }
    
    return point;
}

Result<TemporalConfig> ScenarioLoader::parse_temporal(const json::Value& v) {
    if (!v.is_object()) {
        return Error(ErrorCode::INVALID_ARGUMENT, "TemporalConfig must be an object");
    }
    
    TemporalConfig config;
    config.start_date = v.get_string("start_date", "2025-01-01T00:00:00Z");
    config.end_date = v.get_string("end_date", "2025-12-31T23:59:59Z");
    config.tick_duration_seconds = v.get_number("tick_duration_seconds", 3600.0);
    config.max_ticks = v.get_uint("max_ticks", 0);
    
    if (config.tick_duration_seconds <= 0.0) {
        return Error(ErrorCode::INVALID_ARGUMENT, "tick_duration_seconds must be positive");
    }
    
    return config;
}

Result<Distribution> ScenarioLoader::parse_distribution(const json::Value& v) {
    if (!v.is_object()) {
        return Error(ErrorCode::INVALID_ARGUMENT, "Distribution must be an object");
    }
    
    Distribution dist;
    std::string type_str = v.get_string("type", "point");
    
    if (type_str == "point") {
        dist.type = DistributionType::POINT;
        dist.param1 = v.get_number("value", 0.0);
    } else if (type_str == "uniform") {
        dist.type = DistributionType::UNIFORM;
        dist.param1 = v.get_number("min", 0.0);
        dist.param2 = v.get_number("max", 1.0);
    } else if (type_str == "normal") {
        dist.type = DistributionType::NORMAL;
        dist.param1 = v.get_number("mean", 0.0);
        dist.param2 = v.get_number("stddev", 1.0);
    } else if (type_str == "lognormal") {
        dist.type = DistributionType::LOGNORMAL;
        dist.param1 = v.get_number("mu", 0.0);
        dist.param2 = v.get_number("sigma", 1.0);
    } else if (type_str == "triangular") {
        dist.type = DistributionType::TRIANGULAR;
        dist.param1 = v.get_number("min", 0.0);
        dist.param2 = v.get_number("max", 1.0);
        dist.param3 = v.get_number("mode", 0.5);
    } else if (type_str == "beta") {
        dist.type = DistributionType::BETA;
        dist.param1 = v.get_number("alpha", 1.0);
        dist.param2 = v.get_number("beta", 1.0);
    } else {
        return Error(ErrorCode::INVALID_ARGUMENT, "Unknown distribution type: " + type_str);
    }
    
    return dist;
}

Result<UncertainParameter> ScenarioLoader::parse_parameter(const json::Value& v) {
    if (!v.is_object()) {
        return Error(ErrorCode::INVALID_ARGUMENT, "Parameter must be an object");
    }
    
    UncertainParameter param;
    param.name = v.get_string("name", "");
    param.base_value = v.get_number("value", 0.0);
    param.sensitivity_flag = v.get_bool("sensitivity", false);
    
    if (v.has("source")) {
        param.source = v.get_string("source", "");
    }
    
    if (v.has("confidence")) {
        param.confidence = v.get_number("confidence", 1.0);
    }
    
    if (v.has("uncertainty")) {
        auto dist_result = parse_distribution(v["uncertainty"]);
        if (!dist_result.ok()) return dist_result.error;
        param.uncertainty = dist_result.get();
    }
    
    return param;
}

Result<ActorDefinition> ScenarioLoader::parse_actor(const json::Value& v) {
    if (!v.is_object()) {
        return Error(ErrorCode::INVALID_ARGUMENT, "Actor must be an object");
    }
    
    ActorDefinition actor;
    
    // Required fields
    if (!v.has("id") || !v["id"].is_string()) {
        return Error(ErrorCode::INVALID_ARGUMENT, "Actor missing 'id'");
    }
    actor.id = v["id"].as_string();
    actor.name = v.get_string("name", actor.id);
    actor.type = v.get_string("type", "unit");
    actor.side = v.get_string("side", "neutral");
    actor.platform_id = v.get_string("platform_id", "");
    
    // Hierarchy
    if (v.has("parent_id") && v["parent_id"].is_string()) {
        actor.parent_id = v["parent_id"].as_string();
    }
    
    if (v.has("subordinates") && v["subordinates"].is_array()) {
        const auto& subs = v["subordinates"].as_array();
        for (const auto& sub : subs) {
            if (sub.is_string()) {
                actor.subordinate_ids.push_back(sub.as_string());
            }
        }
    }
    
    // Capabilities
    if (v.has("mobility") && v["mobility"].is_object()) {
        const auto& m = v["mobility"];
        MobilityCapability mob;
        mob.max_speed_kmh = m.get_number("max_speed_kmh", 50.0);
        mob.cruise_speed_kmh = m.get_number("cruise_speed_kmh", 30.0);
        mob.terrain_factor = m.get_number("terrain_factor", 1.0);
        mob.movement_type = m.get_string("movement_type", "ground");
        actor.mobility = mob;
    }
    
    if (v.has("firepower") && v["firepower"].is_object()) {
        const auto& f = v["firepower"];
        FirepowerCapability fire;
        fire.base_firepower = f.get_number("base_firepower", 1.0);
        fire.range_km = f.get_number("range_km", 1.0);
        fire.accuracy = f.get_number("accuracy", 0.5);
        fire.rate_of_fire = f.get_number("rate_of_fire", 1.0);
        actor.firepower = fire;
    }
    
    if (v.has("sensors") && v["sensors"].is_object()) {
        const auto& s = v["sensors"];
        SensorCapability sens;
        sens.detection_range_km = s.get_number("detection_range_km", 10.0);
        sens.identification_range_km = s.get_number("identification_range_km", 5.0);
        sens.sensor_type = s.get_string("sensor_type", "optical");
        actor.sensors = sens;
    }
    
    if (v.has("logistics") && v["logistics"].is_object()) {
        const auto& l = v["logistics"];
        LogisticsCapability log;
        log.fuel_capacity = l.get_number("fuel_capacity", 100.0);
        log.ammo_capacity = l.get_number("ammo_capacity", 100.0);
        log.consumption_rate = l.get_number("consumption_rate", 0.01);
        log.resupply_rate = l.get_number("resupply_rate", 0.1);
        actor.logistics = log;
    }
    
    if (v.has("command") && v["command"].is_object()) {
        const auto& c = v["command"];
        C2Capability cmd;
        cmd.command_range_km = c.get_number("command_range_km", 50.0);
        cmd.communication_reliability = c.get_number("communication_reliability", 0.95);
        cmd.max_subordinates = static_cast<u32>(c.get_uint("max_subordinates", 10));
        actor.command = cmd;
    }
    
    // Initial state
    if (v.has("position")) {
        auto pos_result = parse_geo_point(v["position"]);
        if (!pos_result.ok()) return pos_result.error;
        actor.initial_position = pos_result.get();
    } else {
        actor.initial_position = GeoPoint{0.0, 0.0, 0.0};
    }
    
    actor.initial_health = v.get_number("health", 1.0);
    actor.initial_supply = v.get_number("supply", 1.0);
    actor.initial_morale = v.get_number("morale", 1.0);
    actor.initial_readiness = v.get_number("readiness", 1.0);
    
    // Behavior
    actor.behavior_script = v.get_string("behavior", "default");
    
    if (v.has("behavior_params") && v["behavior_params"].is_object()) {
        const auto& bp = v["behavior_params"].as_object();
        for (const auto& [key, val] : bp) {
            if (val.is_number()) {
                actor.behavior_params[key] = val.as_number();
            }
        }
    }
    
    return actor;
}

Result<EnvironmentConfig> ScenarioLoader::parse_environment(const json::Value& v) {
    if (!v.is_object()) {
        return Error(ErrorCode::INVALID_ARGUMENT, "Environment must be an object");
    }
    
    EnvironmentConfig env;
    
    // Terrain
    if (v.has("terrain") && v["terrain"].is_object()) {
        const auto& t = v["terrain"];
        env.terrain.theater_id = t.get_string("theater_id", "default");
        
        std::string res = t.get_string("resolution", "meso");
        if (res == "macro") {
            env.terrain.resolution = ScenarioScale::MACRO;
        } else if (res == "meso") {
            env.terrain.resolution = ScenarioScale::MESO;
        } else {
            env.terrain.resolution = ScenarioScale::MICRO;
        }
        
        if (t.has("area_of_interest")) {
            auto aoi_result = parse_geo_bounds(t["area_of_interest"]);
            if (aoi_result.ok()) {
                env.terrain.area_of_interest = aoi_result.get();
            }
        }
        // v1.1 terrain generation params
        env.terrain.base_type = t.get_string("base_type", "flat");
        env.terrain.roughness = t.get_number("roughness", 0.3);
        env.terrain.forest_density = t.get_number("forest_density", 0.0);
        env.terrain.urban_density = t.get_number("urban_density", 0.0);
        env.terrain.rivers_enabled = t.get_bool("rivers", false);
    } else {
        env.terrain.theater_id = "default";
        env.terrain.resolution = ScenarioScale::MESO;
    }
    
    // Climate
    if (v.has("climate") && v["climate"].is_object()) {
        const auto& c = v["climate"];
        env.climate.season = c.get_string("season", "summer");
        env.climate.weather_model = c.get_string("weather_model", "clear");
        env.climate.weather_effects_enabled = c.get_bool("weather_effects", true);
    } else {
        env.climate.season = "summer";
        env.climate.weather_model = "clear";
        env.climate.weather_effects_enabled = true;
    }
    
    // Infrastructure
    if (v.has("infrastructure") && v["infrastructure"].is_object()) {
        const auto& i = v["infrastructure"];
        env.infrastructure.roads_enabled = i.get_bool("roads", true);
        env.infrastructure.ports_enabled = i.get_bool("ports", true);
        env.infrastructure.airfields_enabled = i.get_bool("airfields", true);
        env.infrastructure.bridges_enabled = i.get_bool("bridges", true);
        
        if (i.has("critical_nodes") && i["critical_nodes"].is_array()) {
            const auto& nodes = i["critical_nodes"].as_array();
            for (const auto& node : nodes) {
                if (node.is_string()) {
                    env.infrastructure.critical_nodes.push_back(node.as_string());
                }
            }
        }
    } else {
        env.infrastructure.roads_enabled = true;
        env.infrastructure.ports_enabled = true;
        env.infrastructure.airfields_enabled = true;
        env.infrastructure.bridges_enabled = true;
    }
    
    return env;
}

Result<ScenarioMetadata> ScenarioLoader::parse_metadata(const json::Value& v) {
    if (!v.is_object()) {
        return Error(ErrorCode::INVALID_ARGUMENT, "Metadata must be an object");
    }
    
    ScenarioMetadata meta;
    meta.schema_version = v.get_string("schema_version", constants::SCHEMA_VERSION);
    meta.created_at = v.get_string("created_at", "");
    meta.modified_at = v.get_string("modified_at", "");
    meta.created_by = v.get_string("created_by", "");
    
    if (v.has("description") && v["description"].is_string()) {
        meta.description = v["description"].as_string();
    }
    
    if (v.has("tags") && v["tags"].is_array()) {
        const auto& tags = v["tags"].as_array();
        for (const auto& tag : tags) {
            if (tag.is_string()) {
                meta.tags.push_back(tag.as_string());
            }
        }
    }
    
    return meta;
}

// =============================================================================
// Validation
// =============================================================================

bool ScenarioLoader::validate(Scenario& scenario) {
    scenario.validation_errors.clear();
    
    // Validate bounds
    if (!validate_bounds(scenario)) {
        return false;
    }
    
    // Validate each actor
    for (auto& actor : scenario.actors) {
        if (!validate_actor(actor, scenario)) {
            return false;
        }
    }
    
    // Validate references
    if (!validate_references(scenario)) {
        return false;
    }
    
    return scenario.validation_errors.empty();
}

bool ScenarioLoader::validate_actor(const ActorDefinition& actor, Scenario& scenario) {
    // Check actor is within spatial bounds
    if (!scenario.spatial_bounds.contains(
            actor.initial_position.lat, actor.initial_position.lon)) {
        scenario.validation_errors.push_back(
            "Actor '" + actor.id + "' position outside scenario bounds");
        last_error_ = scenario.validation_errors.back();
        return false;
    }
    
    // Check state values in valid range
    if (actor.initial_health < 0.0 || actor.initial_health > 1.0) {
        scenario.validation_errors.push_back(
            "Actor '" + actor.id + "' health out of range [0,1]");
        last_error_ = scenario.validation_errors.back();
        return false;
    }
    
    if (actor.initial_supply < 0.0 || actor.initial_supply > 1.0) {
        scenario.validation_errors.push_back(
            "Actor '" + actor.id + "' supply out of range [0,1]");
        last_error_ = scenario.validation_errors.back();
        return false;
    }
    
    return true;
}

bool ScenarioLoader::validate_bounds(const Scenario& scenario) {
    if (!scenario.spatial_bounds.valid()) {
        last_error_ = "Invalid spatial bounds";
        return false;
    }
    return true;
}

bool ScenarioLoader::validate_references(Scenario& scenario) {
    // Build set of actor IDs
    std::set<std::string> actor_ids;
    for (const auto& actor : scenario.actors) {
        if (actor_ids.count(actor.id) > 0) {
            scenario.validation_errors.push_back(
                "Duplicate actor ID: " + actor.id);
            last_error_ = scenario.validation_errors.back();
            return false;
        }
        actor_ids.insert(actor.id);
    }
    
    // Validate parent references
    for (const auto& actor : scenario.actors) {
        if (actor.parent_id.has_value()) {
            if (actor_ids.count(*actor.parent_id) == 0) {
                scenario.validation_errors.push_back(
                    "Actor '" + actor.id + "' references unknown parent: " + 
                    *actor.parent_id);
                last_error_ = scenario.validation_errors.back();
                return false;
            }
        }
        
        for (const auto& sub_id : actor.subordinate_ids) {
            if (actor_ids.count(sub_id) == 0) {
                scenario.validation_errors.push_back(
                    "Actor '" + actor.id + "' references unknown subordinate: " + 
                    sub_id);
                last_error_ = scenario.validation_errors.back();
                return false;
            }
        }
    }
    
    return true;
}

// =============================================================================
// ScenarioConverter Implementation
// =============================================================================

Result<usize> ScenarioConverter::convert(const Scenario& scenario,
                                         EntityManager& entities,
                                         Rng& rng) {
    if (!scenario.is_valid) {
        return Error(ErrorCode::INVALID_ARGUMENT, "Scenario is not valid");
    }
    
    actor_to_entity_.clear();
    usize created = 0;
    
    for (const auto& actor : scenario.actors) {
        EntityId eid = entities.create();
        if (eid == INVALID_ENTITY) {
            return Error(ErrorCode::ENTITY_LIMIT_REACHED,
                "Entity limit reached while converting scenario");
        }
        
        actor_to_entity_[actor.id] = eid;
        
        auto& storage = entities.storage();
        usize idx = storage.count - 1;  // Just created
        
        // Set side
        if (actor.side == "blue") {
            storage.side[idx] = Side::BLUE;
        } else if (actor.side == "red") {
            storage.side[idx] = Side::RED;
        } else if (actor.side == "green") {
            storage.side[idx] = Side::GREEN;
        } else if (actor.side == "orange") {
            storage.side[idx] = Side::ORANGE;
        } else if (actor.side == "yellow") {
            storage.side[idx] = Side::YELLOW;
        } else {
            storage.side[idx] = Side::NEUTRAL;
        }
        
        // Set unit type from actor.type (for display / NATO symbol)
        storage.unit_type[idx] = unit_type_from_string(actor.type);
        
        // Store platform_id reference
        storage.platform_id[idx] = actor.platform_id;
        
        // Set position (convert lat/lon to meters for simulation)
        // Using simple equirectangular projection centered at scenario center
        f64 center_lat = (scenario.spatial_bounds.min_lat + 
                          scenario.spatial_bounds.max_lat) / 2.0;
        f64 center_lon = (scenario.spatial_bounds.min_lon + 
                          scenario.spatial_bounds.max_lon) / 2.0;
        
        constexpr f64 EARTH_RADIUS = 6371000.0;  // meters
        constexpr f64 DEG_TO_RAD = 3.14159265358979323846 / 180.0;
        
        f64 lat_rad = actor.initial_position.lat * DEG_TO_RAD;
        f64 lon_rad = actor.initial_position.lon * DEG_TO_RAD;
        f64 center_lat_rad = center_lat * DEG_TO_RAD;
        f64 center_lon_rad = center_lon * DEG_TO_RAD;
        
        storage.pos_x[idx] = EARTH_RADIUS * (lon_rad - center_lon_rad) * 
                             std::cos(center_lat_rad);
        storage.pos_y[idx] = EARTH_RADIUS * (lat_rad - center_lat_rad);
        storage.pos_z[idx] = actor.initial_position.alt;
        
        // Set initial state
        storage.health[idx] = actor.initial_health;
        storage.supply[idx] = actor.initial_supply;
        storage.morale[idx] = actor.initial_morale;
        storage.readiness[idx] = actor.initial_readiness;
        
        // =====================================================================
        // Derive combat attributes from platform spec OR manual capabilities
        // =====================================================================
        
        const PlatformSpec* spec = nullptr;
        if (!actor.platform_id.empty() && platform_db_) {
            spec = platform_db_->get(actor.platform_id);
        }
        
        auto& cp = storage.combat[idx];
        
        if (spec) {
            // === PLATFORM-DRIVEN: derive everything from real specs ===
            cp.has_platform_data = true;
            
            // Armament: use best available round from main gun
            if (spec->armament.main_gun.has_value()) {
                const auto& gun = spec->armament.main_gun.value();
                cp.caliber_mm = gun.caliber_mm;
                cp.muzzle_vel_mps = gun.muzzle_velocity_mps;
                cp.rate_of_fire_rpm = gun.rate_of_fire_rpm;
                cp.effective_range_m = gun.effective_range_m;
                cp.ammo_carried = gun.ammunition_carried;
                cp.penetration_mm = gun.penetration_mm_rha;
            }
            // Check small_arms for infantry weapons
            if (spec->small_arms.has_value()) {
                const auto& sa = spec->small_arms.value();
                cp.muzzle_vel_mps = sa.muzzle_velocity_mps;
                cp.rate_of_fire_rpm = sa.rate_of_fire_rpm;
                cp.effective_range_m = sa.effective_range_m;
                cp.ammo_carried = sa.magazine_capacity;
                cp.caliber_mm = 0.0; // parse from caliber string if needed
            }
            
            // Protection
            // Use the higher of turret/hull front
            f64 turret_front = spec->protection.front_mm_rha;
            f64 hull_front = spec->protection.front_mm_rha; // Same field in current schema
            cp.armor_front_mm = std::max(turret_front, hull_front);
            cp.armor_side_mm = spec->protection.side_mm_rha;
            cp.armor_rear_mm = spec->protection.rear_mm_rha;
            cp.armor_top_mm = spec->protection.top_mm_rha;
            cp.has_aps = spec->protection.aps_active;
            cp.has_era = (spec->protection.armor_type.find("reactive") != std::string::npos ||
                          spec->protection.armor_type.find("ERA") != std::string::npos);
            
            // Sensors
            cp.thermal_gen = spec->sensors.thermal_generation;
            cp.has_lrf = spec->sensors.laser_rangefinder;
            cp.has_radar = (spec->sensors.radar_range_km > 0.0);
            // Detection range from calculated value or sensors
            cp.sensor_range_m = spec->detection_range_m;
            if (cp.sensor_range_m <= 0.0 && spec->sensors.radar_range_km > 0.0) {
                cp.sensor_range_m = spec->sensors.radar_range_km * 1000.0;
            }
            if (cp.sensor_range_m <= 0.0) {
                // Baseline: 2km visual, +1km per thermal gen
                cp.sensor_range_m = 2000.0 + cp.thermal_gen * 1000.0;
            }
            
            // Mobility
            cp.max_speed_road_mps = spec->mobility.max_speed_kmh / 3.6;
            cp.max_speed_offroad_mps = spec->mobility.max_speed_offroad_kmh / 3.6;
            if (cp.max_speed_offroad_mps <= 0.0) {
                cp.max_speed_offroad_mps = cp.max_speed_road_mps * 0.6;
            }
            cp.range_km = spec->mobility.range_km;

            // v1.2.3: drive_type code for MovementType inference
            if (spec->mobility.drive_type == "tracked")      cp.drive_type_code = 2;
            else if (spec->mobility.drive_type == "wheeled")  cp.drive_type_code = 1;
            else if (spec->mobility.drive_type == "fixed_wing") cp.drive_type_code = 3;
            else if (spec->mobility.drive_type == "helicopter") cp.drive_type_code = 4;
            else if (spec->mobility.drive_type == "naval")    cp.drive_type_code = 5;
            else                                              cp.drive_type_code = 0; // foot

            // v1.2.0: Indirect fire inference
            // Artillery, MLRS, mortars are indirect fire platforms.
            // Also set splash radius and minimum range based on caliber.
            if (spec->category == PlatformCategory::ARTILLERY ||
                spec->category == PlatformCategory::MLRS) {
                cp.is_indirect = true;
                // Splash radius from caliber: 155mm ~ 50m, 122mm ~ 35m, 203mm ~ 70m
                cp.splash_radius_m = cp.caliber_mm * 0.35;
                // Min range: artillery can't fire at close targets (dead zone)
                // ~500m for mortars (small caliber), ~2000m for large artillery
                cp.min_range_m = std::max(500.0, cp.caliber_mm * 10.0);
            }
            // Mortars (infantry-carried, typically < 120mm, treated as small_arms category)
            if (spec->category == PlatformCategory::SMALL_ARMS &&
                cp.caliber_mm >= 60.0 && cp.effective_range_m > 3000.0) {
                // Likely a mortar system (high caliber for "small arms" + long range)
                cp.is_indirect = true;
                cp.splash_radius_m = cp.caliber_mm * 0.30;
                cp.min_range_m = 200.0;
            }
            
            // Populate legacy scalar fields from platform data
            // (so existing systems that read firepower/engagement_range still work)
            storage.firepower[idx] = spec->firepower_rating;
            storage.engagement_range[idx] = cp.effective_range_m > 0.0
                ? cp.effective_range_m : 3000.0;
            storage.detection_range[idx] = cp.sensor_range_m;
            
            // Auto-detect UnitType from PlatformCategory if actor.type is generic
            if (actor.type == "unit" || actor.type.empty()) {
                switch (spec->category) {
                    case PlatformCategory::TANK:       storage.unit_type[idx] = UnitType::ARMOR; break;
                    case PlatformCategory::IFV:        storage.unit_type[idx] = UnitType::MECHANIZED; break;
                    case PlatformCategory::APC:        storage.unit_type[idx] = UnitType::MECHANIZED; break;
                    case PlatformCategory::ARTILLERY:   storage.unit_type[idx] = UnitType::ARTILLERY; break;
                    case PlatformCategory::MLRS:       storage.unit_type[idx] = UnitType::ARTILLERY; break;
                    case PlatformCategory::SAM:        storage.unit_type[idx] = UnitType::AIR_DEFENSE; break;
                    case PlatformCategory::SMALL_ARMS: storage.unit_type[idx] = UnitType::LIGHT_INFANTRY; break;
                    case PlatformCategory::AIRCRAFT:   storage.unit_type[idx] = UnitType::FIGHTER; break;
                    case PlatformCategory::HELICOPTER: storage.unit_type[idx] = UnitType::ATTACK_HELO; break;
                    case PlatformCategory::BOMBER:     storage.unit_type[idx] = UnitType::FIGHTER; break;
                    case PlatformCategory::UAV:        storage.unit_type[idx] = UnitType::RECON; break;
                    case PlatformCategory::SHIP:       storage.unit_type[idx] = UnitType::NAVAL; break;
                    case PlatformCategory::SUBMARINE:  storage.unit_type[idx] = UnitType::NAVAL; break;
                    case PlatformCategory::ENGINEERING:storage.unit_type[idx] = UnitType::ENGINEER; break;
                    default: break;
                }
            }
        } else {
            // === LEGACY: manual capabilities (backward compatible) ===
            cp.has_platform_data = false;
            
            if (actor.firepower.has_value()) {
                storage.firepower[idx] = actor.firepower->base_firepower;
                storage.engagement_range[idx] = actor.firepower->range_km * 1000.0;
                cp.effective_range_m = actor.firepower->range_km * 1000.0;
            } else {
                storage.firepower[idx] = 5.0;
                storage.engagement_range[idx] = 3000.0;
                cp.effective_range_m = 3000.0;
            }
            
            if (actor.sensors.has_value()) {
                storage.detection_range[idx] = actor.sensors->detection_range_km * 1000.0;
                cp.sensor_range_m = actor.sensors->detection_range_km * 1000.0;
            } else {
                storage.detection_range[idx] = 5000.0;
                cp.sensor_range_m = 5000.0;
            }
            
            if (actor.logistics.has_value()) {
                storage.fuel[idx] = 1.0;
                storage.ammo[idx] = 1.0;
                storage.consumption_rate[idx] = actor.logistics->consumption_rate;
            } else {
                storage.fuel[idx] = 1.0;
                storage.ammo[idx] = 1.0;
                storage.consumption_rate[idx] = 0.01;
            }
        }
        
        // Velocity starts at zero
        storage.vel_x[idx] = 0.0;
        storage.vel_y[idx] = 0.0;
        storage.vel_z[idx] = 0.0;
        
        // Set flags
        storage.flags[idx] = entity_flags::ACTIVE;
        
        created++;
    }
    
    return created;
}

}  // namespace athena
