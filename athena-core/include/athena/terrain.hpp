// ATHENA Core - Terrain System (Physical Layer)
// Contract: Deterministic procedural terrain generation.
//
// ARCHITECTURE:
// Layer 1 (Physical) - This file
//   - Elevation, slope, water, obstacles
//   - Immutable during simulation
//   - Generated from seed + config
//
// Layer 2 (Environment) - environment.hpp
//   - Weather, temperature, visibility
//   - Modifies cost functions, not terrain
//   - Can vary with time
//
// Layer 3 (Semantic) - Derived at runtime
//   - Defense advantage, movement cost, concealment
//   - NEVER stored, always computed from L1 + L2
//
// RULES:
// - All functions are deterministic
// - No external data dependencies
// - Perlin noise for procedural generation
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_TERRAIN_HPP
#define ATHENA_TERRAIN_HPP

#include "athena/types.hpp"
#include <vector>
#include <string>
#include <map>

namespace athena {

// =============================================================================
// Terrain Base Types
// =============================================================================

enum class TerrainBase : u8 {
    FLAT = 0,       // Plains, minimal elevation change
    HILLS,          // Rolling hills, moderate elevation
    MOUNTAINS,      // High elevation, steep slopes
    VALLEY,         // Depression between elevations
    PLATEAU         // Elevated flat area
};

enum class OverlayType : u8 {
    NONE = 0,
    FOREST,         // Dense vegetation
    URBAN,          // Built-up area
    SWAMP,          // Wetland
    RIVER,          // Water obstacle (linear)
    LAKE,           // Water obstacle (area)
    ROAD,           // Improved movement
    BRIDGE          // River crossing point
};

// =============================================================================
// Configuration Structures (DSL)
// =============================================================================

struct OverlayConfig {
    OverlayType type = OverlayType::NONE;
    f64 density = 0.5;      // 0-1, coverage fraction
    f64 param1 = 0.0;       // Type-specific (e.g., river width in meters)
    Seed seed_offset = 0;   // Added to main seed for this overlay
};

struct ConnectivityConfig {
    TerrainBase north = TerrainBase::FLAT;
    TerrainBase south = TerrainBase::FLAT;
    TerrainBase east = TerrainBase::FLAT;
    TerrainBase west = TerrainBase::FLAT;
};

struct TerrainPhysicalConfig {
    TerrainBase base = TerrainBase::FLAT;
    f64 roughness = 0.5;            // 0-1, intensity of elevation variation
    Seed seed = 0;                  // Master seed for generation
    
    f64 base_elevation_m = 100.0;   // Base elevation in meters
    f64 elevation_range_m = 200.0;  // Max elevation variation
    
    std::vector<OverlayConfig> overlays;
    ConnectivityConfig connectivity;
    
    // Dimensions
    f64 width_m = 100000.0;         // Width in meters
    f64 height_m = 100000.0;        // Height in meters
};

// =============================================================================
// Terrain Query Results
// =============================================================================

struct TerrainSample {
    f64 elevation;          // Meters above sea level
    f64 slope;              // Degrees (0-90)
    f64 slope_direction;    // Degrees (0-360), direction of steepest descent
    bool is_water;          // True if water obstacle
    f64 obstacle;           // 0 = clear, 1 = impassable
    OverlayType primary_overlay;  // Dominant overlay at this point
};

// =============================================================================
// Perlin Noise Generator
// =============================================================================

class PerlinNoise {
public:
    PerlinNoise();
    explicit PerlinNoise(Seed seed);
    
    void reseed(Seed seed);
    
    // Single octave noise, returns [-1, 1]
    f64 noise(f64 x, f64 y) const;
    
    // Fractional Brownian Motion (multiple octaves)
    // Returns [-1, 1] range
    f64 fbm(f64 x, f64 y, int octaves, f64 persistence = 0.5) const;
    
    // Ridge noise (for mountains)
    f64 ridge(f64 x, f64 y, int octaves, f64 persistence = 0.5) const;

private:
    static constexpr int TABLE_SIZE = 256;
    int perm_[TABLE_SIZE * 2];
    
    f64 fade(f64 t) const;
    f64 lerp(f64 a, f64 b, f64 t) const;
    f64 grad(int hash, f64 x, f64 y) const;
};

// =============================================================================
// Physical Terrain (Layer 1)
// =============================================================================

class PhysicalTerrain {
public:
    PhysicalTerrain();
    
    /// Initialize terrain from configuration
    /// PRE: config.seed != 0
    /// POST: Terrain ready for queries
    /// DETERMINISM: Same config = same terrain
    Status init(const TerrainPhysicalConfig& config);
    
    /// Reset to uninitialized state
    void reset();
    
    /// Check if initialized
    bool is_initialized() const { return initialized_; }
    
    // =========================================================================
    // Core Queries (Layer 1 - Physical)
    // =========================================================================
    
    /// Get elevation at point (meters above sea level)
    /// Coordinates in meters from origin (0,0 = southwest corner)
    f64 elevation(f64 x, f64 y) const;
    
    /// Get slope at point (degrees, 0-90)
    f64 slope(f64 x, f64 y) const;
    
    /// Get slope direction (degrees, 0-360, direction of steepest descent)
    f64 slope_direction(f64 x, f64 y) const;
    
    /// Check if point is water
    bool is_water(f64 x, f64 y) const;
    
    /// Get obstacle value (0 = clear, 1 = impassable)
    f64 obstacle(f64 x, f64 y) const;
    
    /// Get primary overlay type at point
    OverlayType overlay_at(f64 x, f64 y) const;
    
    /// Get full terrain sample at point
    TerrainSample sample(f64 x, f64 y) const;
    
    // =========================================================================
    // Bulk Queries (for efficiency)
    // =========================================================================
    
    /// Sample terrain along a line (for pathfinding, LOS)
    /// Returns samples at regular intervals
    std::vector<TerrainSample> sample_line(
        f64 x1, f64 y1, f64 x2, f64 y2, f64 interval_m) const;
    
    // =========================================================================
    // Configuration Access
    // =========================================================================
    
    const TerrainPhysicalConfig& config() const { return config_; }
    f64 width() const { return config_.width_m; }
    f64 height() const { return config_.height_m; }

private:
    TerrainPhysicalConfig config_;
    PerlinNoise noise_;
    bool initialized_;
    
    // Overlay-specific noise generators (different seeds)
    std::vector<PerlinNoise> overlay_noise_;
    
    // Internal helpers
    f64 compute_base_elevation(f64 x, f64 y) const;
    f64 apply_connectivity_blend(f64 x, f64 y, f64 base_elev) const;
    f64 compute_overlay_density(f64 x, f64 y, usize overlay_idx) const;
    
    // Normalize coordinates to [0, 1]
    f64 normalize_x(f64 x) const;
    f64 normalize_y(f64 y) const;
};

// =============================================================================
// Terrain Base to String
// =============================================================================

inline const char* terrain_base_name(TerrainBase base) {
    switch (base) {
        case TerrainBase::FLAT:      return "flat";
        case TerrainBase::HILLS:     return "hills";
        case TerrainBase::MOUNTAINS: return "mountains";
        case TerrainBase::VALLEY:    return "valley";
        case TerrainBase::PLATEAU:   return "plateau";
        default:                     return "unknown";
    }
}

inline const char* overlay_type_name(OverlayType type) {
    switch (type) {
        case OverlayType::NONE:   return "none";
        case OverlayType::FOREST: return "forest";
        case OverlayType::URBAN:  return "urban";
        case OverlayType::SWAMP:  return "swamp";
        case OverlayType::RIVER:  return "river";
        case OverlayType::LAKE:   return "lake";
        case OverlayType::ROAD:   return "road";
        case OverlayType::BRIDGE: return "bridge";
        default:                  return "unknown";
    }
}

// Parse from string (for JSON loading)
TerrainBase parse_terrain_base(const std::string& name);
OverlayType parse_overlay_type(const std::string& name);

}  // namespace athena

#endif  // ATHENA_TERRAIN_HPP
