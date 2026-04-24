// ATHENA Core - Platform Loader
// Contract: Load platform specifications from JSON database.
//
// This module loads platform data from JSON files and provides
// specifications for entity instantiation in simulation.
//
// RULES:
// - All loading happens at init (not during simulation)
// - Deterministic loading order (alphabetical)
// - Validates required fields
// - Clear error reporting
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_PLATFORM_LOADER_HPP
#define ATHENA_PLATFORM_LOADER_HPP

#include "types.hpp"
#include "json.hpp"
#include "entities.hpp"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstring>

namespace athena {

// =============================================================================
// Platform Category
// =============================================================================

enum class PlatformCategory : u8 {
    UNKNOWN = 0,
    // Ground Combat Vehicles
    TANK,
    IFV,
    APC,
    ARTILLERY,
    MLRS,
    // Air Defense
    SAM,
    ATGM,
    MANPADS,
    COUNTER_UAS,        // NEW: Counter-drone systems
    // Aviation
    AIRCRAFT,
    HELICOPTER,
    BOMBER,
    UAV,
    // Naval
    SHIP,
    SUBMARINE,
    // Infantry Equipment
    SMALL_ARMS,         // NEW: Rifles, SMGs, pistols, MGs, snipers
    BODY_ARMOR,         // NEW: Helmets, plate carriers
    OPTICS,             // NEW: NVG, thermal, combat optics
    // Support Systems
    RADAR,              // NEW: Ground radars, fire control
    EW_SYSTEM,          // NEW: Electronic warfare
    COMMS,              // NEW: Tactical radios
    ENGINEERING,        // NEW: Combat engineering vehicles
    // Regional/Mixed
    REGIONAL,           // NEW: Regional platforms (mixed types)
    // Missiles & Munitions
    MISSILE,            // NEW: Ballistic, cruise missiles
    MUNITION,           // NEW: PGMs, loitering munitions
    MAX_CATEGORIES
};

// Convert category to string (for paths)
const char* category_to_string(PlatformCategory cat);
// Convert string to category
PlatformCategory string_to_category(const std::string& str);

// =============================================================================
// Mobility Specification
// =============================================================================

struct MobilitySpec {
    // Engine
    std::string engine_model;
    std::string engine_type;      // "diesel", "turbine", "turbofan", etc.
    f64 power_hp = 0.0;
    f64 power_kw = 0.0;
    
    // Speed
    f64 max_speed_kmh = 0.0;
    f64 max_speed_offroad_kmh = 0.0;
    f64 cruise_speed_kmh = 0.0;
    
    // Range
    f64 range_km = 0.0;
    f64 combat_radius_km = 0.0;
    f64 ferry_range_km = 0.0;
    
    // Fuel
    f64 fuel_capacity_liters = 0.0;
    f64 fuel_consumption_l_per_100km = 0.0;
    
    // Terrain capability (ground vehicles)
    f64 fording_depth_m = 0.0;
    f64 gradient_percent = 0.0;
    f64 trench_crossing_m = 0.0;
    bool amphibious = false;

    // v1.2.3: Drive type for MovementType inference
    // Values: "tracked", "wheeled", "foot", "fixed_wing", "helicopter", "naval"
    std::string drive_type;
    
    // Air performance
    f64 max_speed_mach = 0.0;
    f64 service_ceiling_m = 0.0;
    f64 rate_of_climb_mps = 0.0;
    f64 max_g_load = 0.0;
};

// =============================================================================
// Armament Specification
// =============================================================================

struct WeaponSpec {
    std::string designation;
    std::string type;             // "gun", "atgm", "aam", "rocket", etc.
    f64 caliber_mm = 0.0;
    f64 rate_of_fire_rpm = 0.0;
    f64 muzzle_velocity_mps = 0.0;
    f64 effective_range_m = 0.0;
    f64 max_range_m = 0.0;
    i32 ammunition_carried = 0;
    f64 penetration_mm_rha = 0.0; // For AT weapons
};

struct ArmamentSpec {
    std::optional<WeaponSpec> main_gun;
    std::vector<WeaponSpec> secondary;
    std::vector<WeaponSpec> missiles;
    std::vector<WeaponSpec> rockets;
    f64 max_payload_kg = 0.0;
    i32 hardpoints = 0;
};

// =============================================================================
// Protection Specification
// =============================================================================

struct ProtectionSpec {
    std::string armor_type;       // "composite", "steel", "reactive", etc.
    f64 front_mm_rha = 0.0;
    f64 side_mm_rha = 0.0;
    f64 rear_mm_rha = 0.0;
    f64 top_mm_rha = 0.0;
    bool nbc_protection = false;
    bool aps_active = false;      // Active Protection System
    std::string aps_type;
    f64 rcs_m2 = 0.0;             // Radar cross section (aircraft/ships)
};

// =============================================================================
// Sensors Specification
// =============================================================================

struct SensorsSpec {
    // Fire control
    std::string fcs_name;
    bool thermal_sight = false;
    i32 thermal_generation = 0;
    bool laser_rangefinder = false;
    f64 laser_range_m = 0.0;
    bool ballistic_computer = false;
    
    // Radar (aircraft/ships/SAM)
    std::string radar_name;
    f64 radar_range_km = 0.0;
    bool aesa_radar = false;
    
    // Other
    bool irst = false;
    bool maws = false;            // Missile Approach Warning
    bool rwr = false;             // Radar Warning Receiver
    std::string datalink;
};

// =============================================================================
// Small Arms Specification (NEW)
// =============================================================================

struct SmallArmsSpec {
    std::string caliber;          // "5.56x45mm NATO", "7.62x39mm", etc.
    std::string action;           // "Gas-operated", "Blowback", "Bolt-action"
    f64 rate_of_fire_rpm = 0.0;
    f64 muzzle_velocity_mps = 0.0;
    f64 effective_range_m = 0.0;
    f64 max_range_m = 0.0;
    i32 magazine_capacity = 0;
    f64 barrel_length_mm = 0.0;
    bool suppressor_compatible = false;
    std::string subcategory;      // "assault-rifle", "smg", "pistol", "sniper", "mg"
};

// =============================================================================
// Radar System Specification (NEW)
// =============================================================================

struct RadarSpec {
    std::string radar_type;       // "AESA", "PESA", "mechanically-scanned"
    std::string band;             // "S-band", "X-band", "L-band", "VHF"
    f64 range_km = 0.0;
    f64 altitude_km = 0.0;
    i32 tracking_capacity = 0;
    bool counter_stealth = false;
    bool counter_battery = false;
    bool mobile = false;
    std::vector<std::string> modes;  // "Air Defense", "Ground Surveillance", etc.
};

// =============================================================================
// Electronic Warfare Specification (NEW)
// =============================================================================

struct EWSpec {
    std::string ew_type;          // "jammer", "sigint", "cyber", "integrated"
    f64 range_km = 0.0;
    std::vector<std::string> bands_covered;
    std::vector<std::string> targets;  // "radar", "comms", "gps", "datalink"
    bool mobile = false;
    std::string platform;         // "ground", "airborne", "naval", "man-portable"
};

// =============================================================================
// Counter-UAS Specification (NEW)
// =============================================================================

struct CUASSpec {
    f64 detection_range_km = 0.0;
    f64 defeat_range_km = 0.0;
    std::vector<std::string> defeat_methods;  // "RF jamming", "laser", "kinetic", "cyber"
    std::vector<std::string> sensors;         // "radar", "EO/IR", "RF analyzer"
    bool mobile = false;
};

// =============================================================================
// Communications Specification (NEW)
// =============================================================================

struct CommsSpec {
    std::string comms_type;       // "handheld", "manpack", "vehicle", "network"
    std::string frequency_range;  // "30 MHz - 2 GHz"
    std::vector<std::string> waveforms;  // "SINCGARS", "HAVEQUICK", etc.
    f64 power_w = 0.0;
    bool encryption = false;
    bool mesh_capable = false;
    bool satcom = false;
};

// =============================================================================
// Body Armor Specification (NEW)
// =============================================================================

struct ArmorSpec {
    std::string armor_level;      // "NIJ IV", "GOST 5", etc.
    std::string armor_subcategory;// "plate-carrier", "helmet", "assault-helmet"
    f64 weight_kg = 0.0;
    std::string material;         // "Kevlar", "UHMWPE", "Ceramic"
    std::vector<std::string> features;
};

// =============================================================================
// Optics Specification (NEW)
// =============================================================================

struct OpticsSpec {
    std::string optics_type;      // "nvg", "thermal", "red-dot", "magnified"
    i32 generation = 0;           // NVG generation (1, 2, 3, 4)
    f64 magnification = 0.0;
    f64 fov_degrees = 0.0;
    bool nv_compatible = false;
    std::string resolution;       // For thermal: "640x480"
};

// =============================================================================
// Platform Specification (full)
// =============================================================================

struct PlatformSpec {
    // === Identity ===
    std::string id;               // Unique ID (e.g., "us-m1a2-sepv3")
    std::string name;             // Display name (e.g., "M1A2 SEPv3 Abrams")
    std::string type;             // Type within category (e.g., "mbt", "fighter")
    PlatformCategory category = PlatformCategory::UNKNOWN;
    std::string country_of_origin;
    std::string manufacturer;
    std::string nato_name;        // NATO reporting name (optional)
    
    // === Physical ===
    i32 crew = 0;
    i32 passengers = 0;
    f64 weight_kg = 0.0;
    f64 length_m = 0.0;
    f64 width_m = 0.0;
    f64 height_m = 0.0;
    
    // === Core Subsystems (for vehicles/aircraft) ===
    MobilitySpec mobility;
    ArmamentSpec armament;
    ProtectionSpec protection;
    SensorsSpec sensors;
    
    // === Category-Specific Specs (optional, based on category) ===
    std::optional<SmallArmsSpec> small_arms;   // For SMALL_ARMS category
    std::optional<RadarSpec> radar;            // For RADAR category
    std::optional<EWSpec> ew;                  // For EW_SYSTEM category
    std::optional<CUASSpec> cuas;              // For COUNTER_UAS category
    std::optional<CommsSpec> comms;            // For COMMS category
    std::optional<ArmorSpec> body_armor;       // For BODY_ARMOR category
    std::optional<OpticsSpec> optics;          // For OPTICS category
    
    // === Economics ===
    f64 unit_cost_usd = 0.0;
    i32 cost_year = 0;
    
    // === Metadata ===
    std::string confidence;       // "high", "medium", "low"
    std::vector<std::string> sources;
    std::string notes;
    
    // === Calculated values for simulation ===
    f64 firepower_rating = 0.0;   // Calculated composite score
    f64 protection_rating = 0.0;  // Calculated composite score
    f64 mobility_rating = 0.0;    // Calculated composite score
    f64 detection_range_m = 0.0;  // Effective detection range
    f64 engagement_range_m = 0.0; // Effective engagement range
};

// =============================================================================
// Infer UnitType string from PlatformCategory + type
// Returns a unit type string compatible with unit_type_from_string() in entities.hpp
// =============================================================================

inline const char* infer_unit_type_str(PlatformCategory cat, const std::string& type) {
    // Secondary type refinements (override category-based default)
    if (type.find("recon") != std::string::npos ||
        type.find("reconnaissance") != std::string::npos)
        return "recon";
    if (type.find("medical") != std::string::npos || type.find("ambulance") != std::string::npos)
        return "medical";
    if (type.find("logistics") != std::string::npos || type.find("supply") != std::string::npos)
        return "logistics";
    if (type.find("special_forces") != std::string::npos || type.find("sof") != std::string::npos)
        return "special_forces";

    // Primary mapping by category
    switch (cat) {
        case PlatformCategory::TANK:        return "armor";
        case PlatformCategory::IFV:         return "mechanized";
        case PlatformCategory::APC:         return "mechanized";
        case PlatformCategory::ARTILLERY:   return "artillery";
        case PlatformCategory::MLRS:        return "artillery";
        case PlatformCategory::SAM:         return "air_defense";
        case PlatformCategory::MANPADS:     return "air_defense";
        case PlatformCategory::COUNTER_UAS: return "air_defense";
        case PlatformCategory::ATGM:        return "heavy_infantry";
        case PlatformCategory::AIRCRAFT:    return "fighter";
        case PlatformCategory::BOMBER:      return "fighter";
        case PlatformCategory::HELICOPTER:
            // Differentiate attack vs transport helicopters
            if (type.find("attack") != std::string::npos ||
                type.find("armed") != std::string::npos)
                return "attack_helo";
            if (type.find("transport") != std::string::npos ||
                type.find("utility") != std::string::npos)
                return "logistics";
            return "attack_helo";  // default for helicopters
        case PlatformCategory::UAV:         return "recon";
        case PlatformCategory::SHIP:        return "naval";
        case PlatformCategory::SUBMARINE:   return "naval";
        case PlatformCategory::SMALL_ARMS:  return "light_infantry";
        case PlatformCategory::BODY_ARMOR:  return "heavy_infantry";
        case PlatformCategory::OPTICS:      return "recon";
        case PlatformCategory::RADAR:       return "recon";
        case PlatformCategory::EW_SYSTEM:   return "recon";
        case PlatformCategory::COMMS:       return "logistics";
        case PlatformCategory::ENGINEERING: return "engineer";
        case PlatformCategory::MISSILE:     return "artillery";
        case PlatformCategory::MUNITION:    return "artillery";
        case PlatformCategory::REGIONAL:
            // Secondary check for regional platforms
            if (type.find("mbt") != std::string::npos) return "armor";
            if (type.find("ifv") != std::string::npos) return "mechanized";
            if (type.find("apc") != std::string::npos) return "mechanized";
            if (type.find("sam") != std::string::npos) return "air_defense";
            if (type.find("artillery") != std::string::npos) return "artillery";
            return "unit";  // generic for ambiguous regional
        default: return "unit";
    }
}

/// Default spacing (meters) between units in formation, based on inferred type
inline f64 default_formation_spacing(const char* unit_type_str) {
    // Armor/mechanized: wider spacing (100-200m)
    if (std::strcmp(unit_type_str, "armor") == 0)       return 200.0;
    if (std::strcmp(unit_type_str, "mechanized") == 0)   return 150.0;
    if (std::strcmp(unit_type_str, "artillery") == 0)    return 300.0;
    if (std::strcmp(unit_type_str, "air_defense") == 0)  return 250.0;
    if (std::strcmp(unit_type_str, "naval") == 0)        return 2000.0;
    if (std::strcmp(unit_type_str, "fighter") == 0)      return 5000.0;
    if (std::strcmp(unit_type_str, "attack_helo") == 0)  return 1000.0;
    // Infantry: tighter
    if (std::strcmp(unit_type_str, "light_infantry") == 0)  return 50.0;
    if (std::strcmp(unit_type_str, "heavy_infantry") == 0)  return 75.0;
    if (std::strcmp(unit_type_str, "recon") == 0)           return 500.0;
    return 100.0;  // default
}

// =============================================================================
// Platform Database
// =============================================================================

class PlatformDatabase {
public:
    PlatformDatabase() = default;
    
    // Load all platforms from a category directory
    // e.g., load_category("data/platforms/tanks", PlatformCategory::TANK)
    Result<usize> load_category(const std::string& dir_path, PlatformCategory category);
    
    // Load all categories from base directory
    // e.g., load_all("data/platforms")
    Result<usize> load_all(const std::string& base_path);
    
    // Lookup by ID (returns nullptr if not found)
    const PlatformSpec* get(const std::string& id) const;
    
    // Get all platforms of a category
    std::vector<const PlatformSpec*> get_by_category(PlatformCategory category) const;
    
    // Get all platforms from a country
    std::vector<const PlatformSpec*> get_by_country(const std::string& country_code) const;
    
    // Get all platforms (const iterators)
    const std::map<std::string, PlatformSpec>& all() const { return platforms_; }
    
    // Stats
    usize count() const { return platforms_.size(); }
    usize count_by_category(PlatformCategory category) const;
    
    // Validation
    struct ValidationResult {
        bool valid = true;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };
    ValidationResult validate() const;
    
private:
    // All platforms by ID
    std::map<std::string, PlatformSpec> platforms_;
    
    // Index by category
    std::map<PlatformCategory, std::vector<std::string>> by_category_;
    
    // Index by country
    std::map<std::string, std::vector<std::string>> by_country_;
    
    // Parse a single platform from JSON
    Result<PlatformSpec> parse_platform(const json::Value& json, PlatformCategory category);
    
    // Calculate derived values
    void calculate_ratings(PlatformSpec& spec);
};

// =============================================================================
// Platform Instance (for simulation)
// =============================================================================

// Configuration for creating an entity from a platform spec
struct PlatformInstance {
    std::string platform_id;      // Reference to PlatformSpec
    std::string unit_name;        // Display name for this instance
    Side side = Side::NEUTRAL;
    f64 pos_x = 0.0;
    f64 pos_y = 0.0;
    f64 pos_z = 0.0;
    f64 heading_deg = 0.0;
    f64 health = 1.0;
    f64 supply = 1.0;
    f64 morale = 1.0;
};

// =============================================================================
// Utility Functions
// =============================================================================

// List all JSON files in a directory (sorted alphabetically)
std::vector<std::string> list_json_files(const std::string& dir_path);

// Read entire file to string
Result<std::string> read_file(const std::string& path);

}  // namespace athena

#endif  // ATHENA_PLATFORM_LOADER_HPP
