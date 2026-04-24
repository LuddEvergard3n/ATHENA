// ATHENA Core - Platform Loader Implementation
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/platform_loader.hpp"
#include "athena/json.hpp"
#include <algorithm>
#include <cmath>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace athena {

// =============================================================================
// Category Conversion
// =============================================================================

const char* category_to_string(PlatformCategory cat) {
    switch (cat) {
        case PlatformCategory::TANK:       return "tanks";
        case PlatformCategory::IFV:        return "ifv";
        case PlatformCategory::APC:        return "apc";
        case PlatformCategory::ARTILLERY:  return "artillery";
        case PlatformCategory::MLRS:       return "mlrs";
        case PlatformCategory::SAM:        return "sam";
        case PlatformCategory::ATGM:       return "atgm";
        case PlatformCategory::MANPADS:    return "manpads";
        case PlatformCategory::COUNTER_UAS: return "counter-uas";
        case PlatformCategory::AIRCRAFT:   return "aircraft";
        case PlatformCategory::HELICOPTER: return "helicopters";
        case PlatformCategory::BOMBER:     return "bombers";
        case PlatformCategory::UAV:        return "uav";
        case PlatformCategory::SHIP:       return "ships";
        case PlatformCategory::SUBMARINE:  return "submarines";
        case PlatformCategory::SMALL_ARMS: return "small-arms";
        case PlatformCategory::BODY_ARMOR: return "body-armor";
        case PlatformCategory::OPTICS:     return "optics";
        case PlatformCategory::RADAR:      return "radars";
        case PlatformCategory::EW_SYSTEM:  return "ew-systems";
        case PlatformCategory::COMMS:      return "comms";
        case PlatformCategory::ENGINEERING: return "engineering";
        case PlatformCategory::REGIONAL:   return "regional";
        case PlatformCategory::MISSILE:    return "missiles";
        case PlatformCategory::MUNITION:   return "munitions";
        default:                           return "unknown";
    }
}

PlatformCategory string_to_category(const std::string& str) {
    if (str == "tanks")       return PlatformCategory::TANK;
    if (str == "ifv")         return PlatformCategory::IFV;
    if (str == "apc")         return PlatformCategory::APC;
    if (str == "artillery")   return PlatformCategory::ARTILLERY;
    if (str == "mlrs")        return PlatformCategory::MLRS;
    if (str == "sam")         return PlatformCategory::SAM;
    if (str == "atgm")        return PlatformCategory::ATGM;
    if (str == "manpads")     return PlatformCategory::MANPADS;
    if (str == "counter-uas") return PlatformCategory::COUNTER_UAS;
    if (str == "aircraft")    return PlatformCategory::AIRCRAFT;
    if (str == "helicopters") return PlatformCategory::HELICOPTER;
    if (str == "bombers")     return PlatformCategory::BOMBER;
    if (str == "uav")         return PlatformCategory::UAV;
    if (str == "ships")       return PlatformCategory::SHIP;
    if (str == "submarines")  return PlatformCategory::SUBMARINE;
    if (str == "small-arms")  return PlatformCategory::SMALL_ARMS;
    if (str == "body-armor")  return PlatformCategory::BODY_ARMOR;
    if (str == "optics")      return PlatformCategory::OPTICS;
    if (str == "radars")      return PlatformCategory::RADAR;
    if (str == "ew-systems")  return PlatformCategory::EW_SYSTEM;
    if (str == "comms")       return PlatformCategory::COMMS;
    if (str == "engineering") return PlatformCategory::ENGINEERING;
    if (str == "regional")    return PlatformCategory::REGIONAL;
    if (str == "missiles")    return PlatformCategory::MISSILE;
    if (str == "munitions")   return PlatformCategory::MUNITION;
    return PlatformCategory::UNKNOWN;
}

// =============================================================================
// File Utilities
// =============================================================================

std::vector<std::string> list_json_files(const std::string& dir_path) {
    std::vector<std::string> files;
    
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) {
        return files;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() > 5 && name.substr(name.size() - 5) == ".json") {
            files.push_back(dir_path + "/" + name);
        }
    }
    closedir(dir);
    
    // Sort alphabetically for deterministic loading
    std::sort(files.begin(), files.end());
    return files;
}

Result<std::string> read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return Error{ErrorCode::FILE_NOT_FOUND, "Cannot open file: " + path};
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// =============================================================================
// Helper: Safe JSON field extraction
// =============================================================================

namespace {

// Get string with default
std::string get_str(const json::Value& obj, const std::string& key, const std::string& def = "") {
    if (obj.is_object() && obj.has(key) && obj[key].is_string()) {
        return obj[key].as_string();
    }
    return def;
}

// Get number with default
f64 get_num(const json::Value& obj, const std::string& key, f64 def = 0.0) {
    if (obj.is_object() && obj.has(key) && obj[key].is_number()) {
        return obj[key].as_number();
    }
    return def;
}

// Get int with default
i32 get_int(const json::Value& obj, const std::string& key, i32 def = 0) {
    if (obj.is_object() && obj.has(key) && obj[key].is_number()) {
        return static_cast<i32>(obj[key].as_number());
    }
    return def;
}

// Get bool with default
bool get_bool(const json::Value& obj, const std::string& key, bool def = false) {
    if (obj.is_object() && obj.has(key) && obj[key].is_bool()) {
        return obj[key].as_bool();
    }
    return def;
}

// Parse mobility from JSON (ground vehicle)
MobilitySpec parse_mobility_ground(const json::Value& mob) {
    MobilitySpec spec;
    
    if (!mob.is_object()) return spec;
    
    // Engine
    if (mob.has("engine") && mob["engine"].is_object()) {
        const auto& eng = mob["engine"];
        spec.engine_model = get_str(eng, "model");
        spec.engine_type = get_str(eng, "type");
        spec.power_hp = get_num(eng, "power_hp");
        spec.power_kw = get_num(eng, "power_kw");
    }
    
    // Speed
    spec.max_speed_kmh = get_num(mob, "max_speed_road_kmh");
    if (spec.max_speed_kmh == 0.0) {
        spec.max_speed_kmh = get_num(mob, "max_speed_kmh");
    }
    spec.max_speed_offroad_kmh = get_num(mob, "max_speed_offroad_kmh");
    spec.cruise_speed_kmh = get_num(mob, "cruise_speed_kmh");
    
    // Range
    spec.range_km = get_num(mob, "range_road_km");
    if (spec.range_km == 0.0) {
        spec.range_km = get_num(mob, "range_km");
    }
    spec.combat_radius_km = get_num(mob, "combat_radius_km");
    spec.ferry_range_km = get_num(mob, "ferry_range_km");
    
    // Fuel
    spec.fuel_capacity_liters = get_num(mob, "fuel_capacity_liters");
    spec.fuel_consumption_l_per_100km = get_num(mob, "fuel_consumption_l_per_100km");
    
    // Terrain
    spec.fording_depth_m = get_num(mob, "fording_depth_m");
    spec.gradient_percent = get_num(mob, "gradient_percent");
    spec.trench_crossing_m = get_num(mob, "trench_crossing_m");
    spec.amphibious = get_bool(mob, "amphibious");

    // v1.2.3: drive_type from JSON (if present, overrides category inference)
    spec.drive_type = get_str(mob, "drive_type");
    
    return spec;
}

// Parse mobility from JSON (aircraft/helicopter)
MobilitySpec parse_mobility_air(const json::Value& perf, const json::Value& power) {
    MobilitySpec spec;
    
    // Performance
    if (perf.is_object()) {
        spec.max_speed_kmh = get_num(perf, "max_speed_kmh");
        spec.max_speed_mach = get_num(perf, "max_speed_mach");
        spec.cruise_speed_kmh = get_num(perf, "cruise_speed_kmh");
        spec.combat_radius_km = get_num(perf, "combat_radius_km");
        spec.ferry_range_km = get_num(perf, "ferry_range_km");
        spec.service_ceiling_m = get_num(perf, "service_ceiling_m");
        spec.rate_of_climb_mps = get_num(perf, "rate_of_climb_mps");
        spec.max_g_load = get_num(perf, "max_g_load");
    }
    
    // Powerplant
    if (power.is_object()) {
        spec.engine_model = get_str(power, "type");
        spec.engine_type = get_str(power, "type");
        spec.power_hp = get_num(power, "power_shp");
        if (spec.power_hp == 0.0) {
            // Aircraft use thrust in kN, convert roughly
            f64 thrust_kn = get_num(power, "thrust_afterburner_kn");
            if (thrust_kn == 0.0) thrust_kn = get_num(power, "thrust_dry_kn");
            spec.power_hp = thrust_kn * 100.0;  // Rough conversion for comparison
        }
    }
    
    return spec;
}

// Parse weapon from JSON
WeaponSpec parse_weapon(const json::Value& weap) {
    WeaponSpec spec;
    if (!weap.is_object()) return spec;
    
    spec.designation = get_str(weap, "designation");
    if (spec.designation.empty()) spec.designation = get_str(weap, "type");
    spec.type = get_str(weap, "type");
    spec.caliber_mm = get_num(weap, "caliber_mm");
    spec.rate_of_fire_rpm = get_num(weap, "rate_of_fire_rpm");
    spec.muzzle_velocity_mps = get_num(weap, "muzzle_velocity_ms");
    if (spec.muzzle_velocity_mps == 0.0) {
        spec.muzzle_velocity_mps = get_num(weap, "muzzle_velocity_mps");
    }
    spec.effective_range_m = get_num(weap, "effective_range_m");
    spec.max_range_m = get_num(weap, "max_range_m");
    spec.ammunition_carried = get_int(weap, "ammunition_carried");
    if (spec.ammunition_carried == 0) {
        spec.ammunition_carried = get_int(weap, "rounds");
    }
    spec.penetration_mm_rha = get_num(weap, "penetration_mm_rha");
    
    return spec;
}

// Parse armament from JSON (ground vehicle)
ArmamentSpec parse_armament_ground(const json::Value& arm) {
    ArmamentSpec spec;
    if (!arm.is_object()) return spec;
    
    // Main gun
    if (arm.has("main") && arm["main"].is_object()) {
        WeaponSpec main = parse_weapon(arm["main"]);
        
        // Get penetration from ammunition types if available
        if (arm["main"].has("ammunition_types") && arm["main"]["ammunition_types"].is_array()) {
            const auto& ammo = arm["main"]["ammunition_types"].as_array();
            for (const auto& a : ammo) {
                f64 pen = get_num(a, "penetration_mm_rha");
                if (pen > main.penetration_mm_rha) {
                    main.penetration_mm_rha = pen;
                }
            }
        }
        spec.main_gun = main;
    }
    
    // Secondary weapons
    if (arm.has("secondary") && arm["secondary"].is_array()) {
        for (const auto& sec : arm["secondary"].as_array()) {
            spec.secondary.push_back(parse_weapon(sec));
        }
    }
    
    return spec;
}

// Parse armament from JSON (aircraft/helicopter)
ArmamentSpec parse_armament_air(const json::Value& arm) {
    ArmamentSpec spec;
    if (!arm.is_object()) return spec;
    
    // Gun
    if (arm.has("gun") && arm["gun"].is_object()) {
        spec.main_gun = parse_weapon(arm["gun"]);
    }
    
    // Missiles (ATGMs, AAMs)
    if (arm.has("missiles") && arm["missiles"].is_object()) {
        const auto& miss = arm["missiles"];
        
        // ATGMs
        if (miss.has("atgm") && miss["atgm"].is_array()) {
            for (const auto& m : miss["atgm"].as_array()) {
                WeaponSpec ws;
                if (m.is_string()) {
                    ws.designation = m.as_string();
                    ws.type = "atgm";
                } else if (m.is_object()) {
                    ws = parse_weapon(m);
                    ws.type = "atgm";
                }
                spec.missiles.push_back(ws);
            }
        }
        
        // AAMs
        if (miss.has("aam") && miss["aam"].is_array()) {
            for (const auto& m : miss["aam"].as_array()) {
                WeaponSpec ws;
                if (m.is_string()) {
                    ws.designation = m.as_string();
                    ws.type = "aam";
                } else if (m.is_object()) {
                    ws = parse_weapon(m);
                    ws.type = "aam";
                }
                spec.missiles.push_back(ws);
            }
        }
        
        // Get range if specified
        f64 range = get_num(miss, "range_hellfire_km");
        if (range == 0.0) range = get_num(miss, "range_vikhr_km");
        if (range == 0.0) range = get_num(miss, "range_km");
        if (range > 0.0 && !spec.missiles.empty()) {
            spec.missiles[0].max_range_m = range * 1000.0;
        }
    }
    
    // Rockets
    if (arm.has("rockets") && arm["rockets"].is_object()) {
        WeaponSpec ws;
        ws.type = "rocket";
        ws.designation = get_str(arm["rockets"], "type");
        ws.ammunition_carried = get_int(arm["rockets"], "quantity");
        spec.rockets.push_back(ws);
    }
    
    spec.max_payload_kg = get_num(arm, "max_payload_kg");
    spec.hardpoints = get_int(arm, "hardpoints");
    
    return spec;
}

// Parse protection from JSON
ProtectionSpec parse_protection(const json::Value& prot) {
    ProtectionSpec spec;
    if (!prot.is_object()) return spec;
    
    spec.armor_type = get_str(prot, "armor_type");
    spec.front_mm_rha = get_num(prot, "protection_front_turret_mm_rha");
    if (spec.front_mm_rha == 0.0) {
        spec.front_mm_rha = get_num(prot, "protection_front_hull_mm_rha");
    }
    spec.side_mm_rha = get_num(prot, "protection_side_mm_rha");
    spec.rear_mm_rha = get_num(prot, "protection_rear_mm_rha");
    spec.top_mm_rha = get_num(prot, "protection_top_mm_rha");
    spec.nbc_protection = get_bool(prot, "nbc_protection");
    
    // APS
    if (prot.has("active_protection") && prot["active_protection"].is_object()) {
        spec.aps_active = true;
        spec.aps_type = get_str(prot["active_protection"], "type");
    }
    
    return spec;
}

// Parse sensors from JSON
SensorsSpec parse_sensors(const json::Value& sens, const json::Value& avio) {
    SensorsSpec spec;
    
    // Ground vehicle sensors
    if (sens.is_object()) {
        spec.fcs_name = get_str(sens, "fire_control_system");
        spec.laser_rangefinder = get_bool(sens, "laser_rangefinder");
        spec.laser_range_m = get_num(sens, "laser_rangefinder_range_m");
        spec.ballistic_computer = get_bool(sens, "ballistic_computer");
        
        if (sens.has("gunner_sight") && sens["gunner_sight"].is_object()) {
            spec.thermal_sight = get_bool(sens["gunner_sight"], "thermal");
            spec.thermal_generation = get_int(sens["gunner_sight"], "thermal_generation");
        }
    }
    
    // Aircraft avionics
    if (avio.is_object()) {
        spec.radar_name = get_str(avio, "radar");
        spec.radar_range_km = get_num(avio, "radar_range_km");
        spec.irst = get_bool(avio, "irst") || avio.has("irst");
        spec.datalink = get_str(avio, "datalink");
        
        // Check for AESA
        std::string radar = spec.radar_name;
        std::transform(radar.begin(), radar.end(), radar.begin(), ::tolower);
        spec.aesa_radar = radar.find("aesa") != std::string::npos;
    }
    
    return spec;
}

// Parse stealth from JSON (aircraft)
void parse_stealth(ProtectionSpec& prot, const json::Value& stealth) {
    if (!stealth.is_object()) return;
    
    prot.rcs_m2 = get_num(stealth, "rcs_estimate_m2");
}

// =============================================================================
// NEW CATEGORY PARSERS
// =============================================================================

// Parse small arms specifications
SmallArmsSpec parse_small_arms(const json::Value& specs, const std::string& type) {
    SmallArmsSpec sa;
    if (!specs.is_object()) return sa;
    
    sa.caliber = get_str(specs, "caliber");
    sa.action = get_str(specs, "action");
    sa.rate_of_fire_rpm = get_num(specs, "rate_of_fire_rpm");
    sa.muzzle_velocity_mps = get_num(specs, "muzzle_velocity_mps");
    if (sa.muzzle_velocity_mps == 0.0) {
        sa.muzzle_velocity_mps = get_num(specs, "muzzle_velocity_ms");
    }
    sa.effective_range_m = get_num(specs, "effective_range_m");
    sa.max_range_m = get_num(specs, "max_range_m");
    sa.magazine_capacity = get_int(specs, "magazine_capacity");
    sa.barrel_length_mm = get_num(specs, "barrel_length_mm");
    sa.subcategory = type;
    
    return sa;
}

// Parse radar specifications
RadarSpec parse_radar(const json::Value& specs) {
    RadarSpec r;
    if (!specs.is_object()) return r;
    
    r.radar_type = get_str(specs, "type");
    r.band = get_str(specs, "band");
    r.range_km = get_num(specs, "range_km");
    if (r.range_km == 0.0) {
        r.range_km = get_num(specs, "detection_range_km");
    }
    r.altitude_km = get_num(specs, "altitude_km");
    r.tracking_capacity = get_int(specs, "tracking_capacity");
    r.counter_stealth = get_bool(specs, "counter_stealth");
    r.counter_battery = get_bool(specs, "counter_battery");
    r.mobile = get_bool(specs, "mobile");
    
    // Parse modes array
    if (specs.has("modes") && specs["modes"].is_array()) {
        for (const auto& mode : specs["modes"].as_array()) {
            if (mode.is_string()) {
                r.modes.push_back(mode.as_string());
            }
        }
    }
    
    return r;
}

// Parse EW specifications
EWSpec parse_ew(const json::Value& specs) {
    EWSpec ew;
    if (!specs.is_object()) return ew;
    
    ew.ew_type = get_str(specs, "type");
    ew.range_km = get_num(specs, "range_km");
    ew.mobile = get_bool(specs, "mobile");
    ew.platform = get_str(specs, "platform");
    
    // Parse targets array
    if (specs.has("targets") && specs["targets"].is_array()) {
        for (const auto& t : specs["targets"].as_array()) {
            if (t.is_string()) {
                ew.targets.push_back(t.as_string());
            }
        }
    }
    
    return ew;
}

// Parse Counter-UAS specifications
CUASSpec parse_cuas(const json::Value& specs) {
    CUASSpec c;
    if (!specs.is_object()) return c;
    
    c.detection_range_km = get_num(specs, "detection_range_km");
    c.defeat_range_km = get_num(specs, "defeat_range_km");
    c.mobile = get_bool(specs, "mobile");
    
    // Parse defeat_methods array
    if (specs.has("defeat_methods") && specs["defeat_methods"].is_array()) {
        for (const auto& m : specs["defeat_methods"].as_array()) {
            if (m.is_string()) {
                c.defeat_methods.push_back(m.as_string());
            }
        }
    }
    
    // Parse sensors array
    if (specs.has("sensors") && specs["sensors"].is_array()) {
        for (const auto& s : specs["sensors"].as_array()) {
            if (s.is_string()) {
                c.sensors.push_back(s.as_string());
            }
        }
    }
    
    return c;
}

// Parse communications specifications
CommsSpec parse_comms(const json::Value& specs) {
    CommsSpec co;
    if (!specs.is_object()) return co;
    
    co.comms_type = get_str(specs, "type");
    co.frequency_range = get_str(specs, "frequency");
    if (co.frequency_range.empty()) {
        co.frequency_range = get_str(specs, "frequency_range");
    }
    co.power_w = get_num(specs, "power_w");
    co.encryption = get_bool(specs, "encryption");
    co.mesh_capable = get_bool(specs, "mesh_capable");
    if (!co.mesh_capable) {
        // Check for "networking" field with "mesh" mention
        std::string net = get_str(specs, "networking");
        co.mesh_capable = net.find("mesh") != std::string::npos || 
                         net.find("Mesh") != std::string::npos;
    }
    co.satcom = get_bool(specs, "satcom");
    
    // Parse waveforms array
    if (specs.has("waveforms") && specs["waveforms"].is_array()) {
        for (const auto& w : specs["waveforms"].as_array()) {
            if (w.is_string()) {
                co.waveforms.push_back(w.as_string());
            }
        }
    }
    
    return co;
}

// Parse body armor specifications
ArmorSpec parse_body_armor(const json::Value& specs, const std::string& type) {
    ArmorSpec a;
    if (!specs.is_object()) return a;
    
    a.armor_level = get_str(specs, "protection_level");
    a.armor_subcategory = type;
    a.weight_kg = get_num(specs, "weight_kg");
    a.material = get_str(specs, "material");
    
    // Parse features array
    if (specs.has("features") && specs["features"].is_array()) {
        for (const auto& f : specs["features"].as_array()) {
            if (f.is_string()) {
                a.features.push_back(f.as_string());
            }
        }
    }
    
    return a;
}

// Parse optics specifications
OpticsSpec parse_optics(const json::Value& specs, const std::string& type) {
    OpticsSpec o;
    if (!specs.is_object()) return o;
    
    o.optics_type = type;
    o.generation = get_int(specs, "generation");
    o.magnification = get_num(specs, "magnification");
    o.fov_degrees = get_num(specs, "fov_degrees");
    o.nv_compatible = get_bool(specs, "nv_compatible");
    o.resolution = get_str(specs, "resolution");
    
    return o;
}

}  // anonymous namespace

// =============================================================================
// PlatformDatabase Implementation
// =============================================================================

Result<PlatformSpec> PlatformDatabase::parse_platform(const json::Value& json, PlatformCategory category) {
    if (!json.is_object()) {
        return Error{ErrorCode::INVALID_FORMAT, "Platform must be an object"};
    }
    
    PlatformSpec spec;
    spec.category = category;
    
    // === Identity ===
    spec.id = get_str(json, "id");
    if (spec.id.empty()) {
        return Error{ErrorCode::INVALID_FORMAT, "Platform missing required field: id"};
    }
    
    spec.name = get_str(json, "name");
    if (spec.name.empty()) {
        return Error{ErrorCode::INVALID_FORMAT, "Platform missing required field: name"};
    }
    
    spec.type = get_str(json, "type");
    spec.country_of_origin = get_str(json, "country_of_origin");
    spec.manufacturer = get_str(json, "manufacturer");
    spec.nato_name = get_str(json, "nato_name");
    
    // === Physical ===
    if (json.has("specifications") && json["specifications"].is_object()) {
        const auto& specs = json["specifications"];
        spec.crew = get_int(specs, "crew");
        spec.passengers = get_int(specs, "passengers");
        spec.weight_kg = get_num(specs, "weight_kg");
        if (spec.weight_kg == 0.0) {
            spec.weight_kg = get_num(specs, "empty_weight_kg");
        }
        if (spec.weight_kg == 0.0) {
            spec.weight_kg = get_num(specs, "max_takeoff_weight_kg");
        }
        spec.length_m = get_num(specs, "length_m");
        spec.width_m = get_num(specs, "width_m");
        if (spec.width_m == 0.0) {
            spec.width_m = get_num(specs, "wingspan_m");
        }
        if (spec.width_m == 0.0) {
            spec.width_m = get_num(specs, "rotor_diameter_m");
        }
        spec.height_m = get_num(specs, "height_m");
    }
    
    // === Category-specific parsing ===
    bool is_ground = (category == PlatformCategory::TANK || 
                      category == PlatformCategory::IFV ||
                      category == PlatformCategory::APC ||
                      category == PlatformCategory::ARTILLERY ||
                      category == PlatformCategory::MLRS);
    
    bool is_air = (category == PlatformCategory::AIRCRAFT ||
                   category == PlatformCategory::HELICOPTER ||
                   category == PlatformCategory::UAV);
    
    // Mobility
    if (is_ground && json.has("mobility")) {
        spec.mobility = parse_mobility_ground(json["mobility"]);
    } else if (is_air) {
        spec.mobility = parse_mobility_air(
            json.has("performance") ? json["performance"] : json::null_value(),
            json.has("powerplant") ? json["powerplant"] : json::null_value()
        );
    }
    
    // Armament
    if (json.has("armament")) {
        if (is_ground) {
            spec.armament = parse_armament_ground(json["armament"]);
        } else if (is_air) {
            spec.armament = parse_armament_air(json["armament"]);
        }
    }
    
    // Protection
    if (json.has("protection")) {
        spec.protection = parse_protection(json["protection"]);
    }
    if (json.has("stealth")) {
        parse_stealth(spec.protection, json["stealth"]);
    }
    if (json.has("survivability")) {
        // Helicopters use "survivability" instead of "protection"
        const auto& surv = json["survivability"];
        if (surv.is_object()) {
            spec.protection.armor_type = get_str(surv, "armor");
        }
    }
    
    // Sensors
    spec.sensors = parse_sensors(
        json.has("sensors_and_fcs") ? json["sensors_and_fcs"] : json::null_value(),
        json.has("avionics") ? json["avionics"] : json::null_value()
    );
    
    // === Economics ===
    spec.unit_cost_usd = get_num(json, "unit_cost_usd");
    spec.cost_year = get_int(json, "cost_year");
    
    // === Metadata ===
    spec.confidence = get_str(json, "confidence");
    spec.notes = get_str(json, "notes");
    
    if (json.has("sources") && json["sources"].is_array()) {
        for (const auto& src : json["sources"].as_array()) {
            if (src.is_string()) {
                spec.sources.push_back(src.as_string());
            }
        }
    }
    
    // === Category-specific optional specs ===
    const auto& specs_obj = json.has("specifications") ? json["specifications"] : json;
    
    switch (category) {
        case PlatformCategory::SMALL_ARMS:
            spec.small_arms = parse_small_arms(specs_obj, spec.type);
            // For small arms, set engagement range from effective range
            if (spec.small_arms.has_value()) {
                spec.engagement_range_m = spec.small_arms->effective_range_m;
            }
            break;
            
        case PlatformCategory::RADAR:
            spec.radar = parse_radar(specs_obj);
            // For radars, set detection range
            if (spec.radar.has_value()) {
                spec.detection_range_m = spec.radar->range_km * 1000.0;
            }
            break;
            
        case PlatformCategory::EW_SYSTEM:
            spec.ew = parse_ew(specs_obj);
            if (spec.ew.has_value()) {
                spec.engagement_range_m = spec.ew->range_km * 1000.0;
            }
            break;
            
        case PlatformCategory::COUNTER_UAS:
            spec.cuas = parse_cuas(specs_obj);
            if (spec.cuas.has_value()) {
                spec.detection_range_m = spec.cuas->detection_range_km * 1000.0;
                spec.engagement_range_m = spec.cuas->defeat_range_km * 1000.0;
            }
            break;
            
        case PlatformCategory::COMMS:
            spec.comms = parse_comms(specs_obj);
            break;
            
        case PlatformCategory::BODY_ARMOR:
            spec.body_armor = parse_body_armor(specs_obj, spec.type);
            break;
            
        case PlatformCategory::OPTICS:
            spec.optics = parse_optics(specs_obj, spec.type);
            break;
            
        default:
            // Standard categories use existing parsing
            break;
    }
    
    // ---- Fill missing fields with coherent defaults ----
    // Every platform must have: description, type, role, mobility, sensor, protection
    if (spec.type.empty()) {
        // Derive from category
        switch (category) {
            case PlatformCategory::TANK: spec.type = "main_battle_tank"; break;
            case PlatformCategory::APC: spec.type = "armored_personnel_carrier"; break;
            case PlatformCategory::IFV: spec.type = "infantry_fighting_vehicle"; break;
            case PlatformCategory::ARTILLERY: spec.type = "artillery"; break;
            case PlatformCategory::SAM: spec.type = "air_defense"; break;
            case PlatformCategory::HELICOPTER: spec.type = "helicopter"; break;
            case PlatformCategory::AIRCRAFT: spec.type = "fixed_wing"; break;
            case PlatformCategory::SUBMARINE: spec.type = "submarine"; break;
            case PlatformCategory::SHIP: spec.type = "surface_combatant"; break;
            default: spec.type = "generic"; break;
        }
    }

    // Default mobility if none parsed
    if (spec.mobility.max_speed_kmh == 0.0) {
        switch (category) {
            case PlatformCategory::TANK: spec.mobility.max_speed_kmh = 55.0; break;
            case PlatformCategory::APC:
            case PlatformCategory::IFV: spec.mobility.max_speed_kmh = 65.0; break;
            case PlatformCategory::ARTILLERY: spec.mobility.max_speed_kmh = 35.0; break;
            case PlatformCategory::HELICOPTER: spec.mobility.max_speed_kmh = 250.0; break;
            case PlatformCategory::AIRCRAFT: spec.mobility.max_speed_kmh = 900.0; break;
            default: spec.mobility.max_speed_kmh = 40.0; break;
        }
    }
    if (spec.mobility.range_km == 0.0) {
        switch (category) {
            case PlatformCategory::TANK:
            case PlatformCategory::IFV: spec.mobility.range_km = 450.0; break;
            case PlatformCategory::HELICOPTER: spec.mobility.range_km = 500.0; break;
            case PlatformCategory::AIRCRAFT: spec.mobility.range_km = 2000.0; break;
            default: spec.mobility.range_km = 300.0; break;
        }
    }

    // Default detection range if zero
    if (spec.detection_range_m == 0.0) {
        switch (category) {
            case PlatformCategory::SAM: spec.detection_range_m = 80000.0; break;
            case PlatformCategory::AIRCRAFT: spec.detection_range_m = 150000.0; break;
            case PlatformCategory::HELICOPTER: spec.detection_range_m = 15000.0; break;
            case PlatformCategory::TANK: spec.detection_range_m = 4000.0; break;
            default: spec.detection_range_m = 5000.0; break;
        }
    }

    // Default off-road speed (60% of road speed if not specified)
    if (spec.mobility.max_speed_offroad_kmh == 0.0 && spec.mobility.max_speed_kmh > 0.0) {
        spec.mobility.max_speed_offroad_kmh = spec.mobility.max_speed_kmh * 0.6;
    }

    // v1.2.3: Infer drive_type from category if not set in JSON
    if (spec.mobility.drive_type.empty()) {
        switch (spec.category) {
            case PlatformCategory::TANK:
            case PlatformCategory::IFV:
            case PlatformCategory::ARTILLERY:
            case PlatformCategory::MLRS:
            case PlatformCategory::ENGINEERING:
                spec.mobility.drive_type = "tracked";
                break;
            case PlatformCategory::APC:
            case PlatformCategory::SAM:
            case PlatformCategory::ATGM:
            case PlatformCategory::RADAR:
            case PlatformCategory::EW_SYSTEM:
            case PlatformCategory::COMMS:
                spec.mobility.drive_type = "wheeled";
                break;
            case PlatformCategory::AIRCRAFT:
            case PlatformCategory::BOMBER:
            case PlatformCategory::UAV:
                spec.mobility.drive_type = "fixed_wing";
                break;
            case PlatformCategory::HELICOPTER:
                spec.mobility.drive_type = "helicopter";
                break;
            case PlatformCategory::SHIP:
            case PlatformCategory::SUBMARINE:
                spec.mobility.drive_type = "naval";
                break;
            default:
                spec.mobility.drive_type = "foot";
                break;
        }
    }

    // Default confidence if empty
    if (spec.confidence.empty()) {
        spec.confidence = "estimated";
    }

    // Default protection if no armor parsed
    if (spec.protection.front_mm_rha == 0.0 && spec.protection.armor_type.empty()) {
        switch (category) {
            case PlatformCategory::TANK:
                spec.protection.front_mm_rha = 500.0;
                spec.protection.side_mm_rha = 100.0;
                spec.protection.armor_type = "composite";
                break;
            case PlatformCategory::IFV:
                spec.protection.front_mm_rha = 40.0;
                spec.protection.side_mm_rha = 15.0;
                spec.protection.armor_type = "aluminum";
                break;
            case PlatformCategory::APC:
                spec.protection.front_mm_rha = 15.0;
                spec.protection.side_mm_rha = 10.0;
                spec.protection.armor_type = "steel";
                break;
            default: break;
        }
    }

    // Calculate derived ratings
    calculate_ratings(spec);
    
    return spec;
}

void PlatformDatabase::calculate_ratings(PlatformSpec& spec) {
    // === Firepower Rating ===
    // Based on main weapon caliber, penetration, and range
    f64 fp = 0.0;
    if (spec.armament.main_gun.has_value()) {
        const auto& gun = spec.armament.main_gun.value();
        fp += gun.caliber_mm * 0.5;
        fp += gun.penetration_mm_rha * 0.3;
        fp += gun.effective_range_m * 0.01;
    }
    // Add missile capability
    for (const auto& m : spec.armament.missiles) {
        if (m.type == "atgm") fp += 50.0;
        if (m.type == "aam") fp += 30.0;
    }
    spec.firepower_rating = fp;
    
    // === Protection Rating ===
    // Based on armor values
    f64 pr = 0.0;
    pr += spec.protection.front_mm_rha * 0.4;
    pr += spec.protection.side_mm_rha * 0.3;
    pr += spec.protection.top_mm_rha * 0.2;
    if (spec.protection.aps_active) pr += 100.0;
    if (spec.protection.nbc_protection) pr += 20.0;
    // Stealth (inverse RCS)
    if (spec.protection.rcs_m2 > 0.0 && spec.protection.rcs_m2 < 1.0) {
        pr += (1.0 / spec.protection.rcs_m2) * 10.0;
    }
    spec.protection_rating = pr;
    
    // === Mobility Rating ===
    // Based on speed and range
    f64 mr = 0.0;
    mr += spec.mobility.max_speed_kmh * 0.5;
    mr += spec.mobility.range_km * 0.1;
    if (spec.mobility.max_speed_mach > 0.0) {
        mr += spec.mobility.max_speed_mach * 100.0;
    }
    if (spec.mobility.amphibious) mr += 50.0;
    spec.mobility_rating = mr;
    
    // === Detection Range ===
    if (spec.sensors.radar_range_km > 0.0) {
        spec.detection_range_m = spec.sensors.radar_range_km * 1000.0;
    } else if (spec.sensors.thermal_sight) {
        spec.detection_range_m = 4000.0 + spec.sensors.thermal_generation * 1000.0;
    } else {
        spec.detection_range_m = 2000.0;  // Visual baseline
    }
    
    // === Engagement Range ===
    if (spec.armament.main_gun.has_value()) {
        spec.engagement_range_m = spec.armament.main_gun.value().effective_range_m;
        if (spec.engagement_range_m == 0.0) {
            spec.engagement_range_m = spec.armament.main_gun.value().max_range_m * 0.6;
        }
    }
    for (const auto& m : spec.armament.missiles) {
        if (m.max_range_m > spec.engagement_range_m) {
            spec.engagement_range_m = m.max_range_m;
        }
    }
    if (spec.engagement_range_m == 0.0) {
        spec.engagement_range_m = 500.0;  // Fallback
    }
}

Result<usize> PlatformDatabase::load_category(const std::string& dir_path, PlatformCategory category) {
    auto files = list_json_files(dir_path);
    if (files.empty()) {
        return Error{ErrorCode::FILE_NOT_FOUND, "No JSON files found in: " + dir_path};
    }
    
    usize loaded = 0;
    
    for (const auto& file : files) {
        // Read file
        auto content_result = read_file(file);
        if (!content_result.ok()) {
            fprintf(stderr, "[PlatformDB] Failed to read: %s\n", file.c_str());
            continue;
        }
        
        // Parse JSON
        auto json_result = json::parse(content_result.get());
        if (!json_result.ok()) {
            fprintf(stderr, "[PlatformDB] JSON parse error in: %s\n", file.c_str());
            continue;
        }
        
        const auto& root = json_result.get();
        
        // Check for "platforms" array
        if (!root.has("platforms") || !root["platforms"].is_array()) {
            fprintf(stderr, "[PlatformDB] No 'platforms' array in: %s\n", file.c_str());
            continue;
        }
        
        // Parse each platform
        for (const auto& platform_json : root["platforms"].as_array()) {
            auto spec_result = parse_platform(platform_json, category);
            if (spec_result.ok()) {
                const auto& spec = spec_result.get();
                
                // Check for duplicates
                if (platforms_.find(spec.id) != platforms_.end()) {
                    continue;  // Skip duplicates silently (common with expanded sets)
                }
                
                // Add to database
                std::string id = spec.id;
                std::string country = spec.country_of_origin;
                
                platforms_[id] = spec;
                by_category_[category].push_back(id);
                by_country_[country].push_back(id);
                loaded++;
            }
        }
    }
    
    return loaded;
}

Result<usize> PlatformDatabase::load_all(const std::string& base_path) {
    usize total = 0;
    
    // All categories to load (in logical order)
    const PlatformCategory categories[] = {
        // Ground Combat Vehicles
        PlatformCategory::TANK,
        PlatformCategory::IFV,
        PlatformCategory::APC,
        PlatformCategory::ARTILLERY,
        PlatformCategory::MLRS,
        // Air Defense
        PlatformCategory::SAM,
        PlatformCategory::ATGM,
        PlatformCategory::MANPADS,
        PlatformCategory::COUNTER_UAS,
        // Aviation
        PlatformCategory::AIRCRAFT,
        PlatformCategory::HELICOPTER,
        PlatformCategory::BOMBER,
        PlatformCategory::UAV,
        // Naval
        PlatformCategory::SHIP,
        PlatformCategory::SUBMARINE,
        // Infantry Equipment
        PlatformCategory::SMALL_ARMS,
        PlatformCategory::BODY_ARMOR,
        PlatformCategory::OPTICS,
        // Support Systems
        PlatformCategory::RADAR,
        PlatformCategory::EW_SYSTEM,
        PlatformCategory::COMMS,
        PlatformCategory::ENGINEERING,
        // Regional/Mixed
        PlatformCategory::REGIONAL,
        // Missiles & Munitions
        PlatformCategory::MISSILE,
        PlatformCategory::MUNITION
    };
    
    for (auto cat : categories) {
        std::string dir = base_path + "/" + category_to_string(cat);
        auto result = load_category(dir, cat);
        if (result.ok()) {
            total += result.get();
        }
    }
    
    if (total == 0) {
        return Error{ErrorCode::INVALID_FORMAT, "No platforms loaded from: " + base_path};
    }
    
    return total;
}

const PlatformSpec* PlatformDatabase::get(const std::string& id) const {
    auto it = platforms_.find(id);
    if (it != platforms_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<const PlatformSpec*> PlatformDatabase::get_by_category(PlatformCategory category) const {
    std::vector<const PlatformSpec*> result;
    
    auto it = by_category_.find(category);
    if (it != by_category_.end()) {
        for (const auto& id : it->second) {
            auto pit = platforms_.find(id);
            if (pit != platforms_.end()) {
                result.push_back(&pit->second);
            }
        }
    }
    
    return result;
}

std::vector<const PlatformSpec*> PlatformDatabase::get_by_country(const std::string& country_code) const {
    std::vector<const PlatformSpec*> result;
    
    auto it = by_country_.find(country_code);
    if (it != by_country_.end()) {
        for (const auto& id : it->second) {
            auto pit = platforms_.find(id);
            if (pit != platforms_.end()) {
                result.push_back(&pit->second);
            }
        }
    }
    
    return result;
}

usize PlatformDatabase::count_by_category(PlatformCategory category) const {
    auto it = by_category_.find(category);
    if (it != by_category_.end()) {
        return it->second.size();
    }
    return 0;
}

PlatformDatabase::ValidationResult PlatformDatabase::validate() const {
    ValidationResult result;
    
    for (const auto& [id, spec] : platforms_) {
        // Check required fields
        if (spec.name.empty()) {
            result.errors.push_back("Platform " + id + " missing name");
            result.valid = false;
        }
        
        if (spec.crew == 0 && spec.category != PlatformCategory::UAV) {
            result.warnings.push_back("Platform " + id + " has crew=0");
        }
        
        if (spec.weight_kg == 0.0) {
            result.warnings.push_back("Platform " + id + " has weight=0");
        }
        
        // Check ratings
        if (spec.engagement_range_m == 0.0) {
            result.warnings.push_back("Platform " + id + " has engagement_range=0");
        }
        
        // Check for reasonable values
        if (spec.mobility.max_speed_kmh > 3000.0) {
            result.warnings.push_back("Platform " + id + " has unusually high speed: " + 
                                      std::to_string(spec.mobility.max_speed_kmh));
        }
    }
    
    return result;
}

}  // namespace athena
