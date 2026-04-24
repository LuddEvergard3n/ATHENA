// ATHENA Core - Terrain System Implementation
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/terrain.hpp"
#include <cmath>
#include <algorithm>

namespace athena {

// =============================================================================
// Perlin Noise Implementation
// =============================================================================

PerlinNoise::PerlinNoise() {
    reseed(0);
}

PerlinNoise::PerlinNoise(Seed seed) {
    reseed(seed);
}

void PerlinNoise::reseed(Seed seed) {
    // Initialize permutation table
    for (int i = 0; i < TABLE_SIZE; ++i) {
        perm_[i] = i;
    }
    
    // Fisher-Yates shuffle with deterministic RNG
    u64 state = seed == 0 ? 0x853c49e6748fea9bULL : seed;
    
    for (int i = TABLE_SIZE - 1; i > 0; --i) {
        // Simple LCG for shuffling
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        int j = static_cast<int>((state >> 33) % (i + 1));
        std::swap(perm_[i], perm_[j]);
    }
    
    // Duplicate for overflow handling
    for (int i = 0; i < TABLE_SIZE; ++i) {
        perm_[TABLE_SIZE + i] = perm_[i];
    }
}

f64 PerlinNoise::fade(f64 t) const {
    // 6t^5 - 15t^4 + 10t^3 (smootherstep)
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

f64 PerlinNoise::lerp(f64 a, f64 b, f64 t) const {
    return a + t * (b - a);
}

f64 PerlinNoise::grad(int hash, f64 x, f64 y) const {
    // Convert low 2 bits of hash to gradient direction
    int h = hash & 3;
    f64 u = h < 2 ? x : y;
    f64 v = h < 2 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

f64 PerlinNoise::noise(f64 x, f64 y) const {
    // Find unit square containing point
    int xi = static_cast<int>(std::floor(x)) & 255;
    int yi = static_cast<int>(std::floor(y)) & 255;
    
    // Relative position in square
    f64 xf = x - std::floor(x);
    f64 yf = y - std::floor(y);
    
    // Fade curves
    f64 u = fade(xf);
    f64 v = fade(yf);
    
    // Hash coordinates of square corners
    int aa = perm_[perm_[xi] + yi];
    int ab = perm_[perm_[xi] + yi + 1];
    int ba = perm_[perm_[xi + 1] + yi];
    int bb = perm_[perm_[xi + 1] + yi + 1];
    
    // Blend gradients
    f64 x1 = lerp(grad(aa, xf, yf), grad(ba, xf - 1.0, yf), u);
    f64 x2 = lerp(grad(ab, xf, yf - 1.0), grad(bb, xf - 1.0, yf - 1.0), u);
    
    return lerp(x1, x2, v);
}

f64 PerlinNoise::fbm(f64 x, f64 y, int octaves, f64 persistence) const {
    f64 total = 0.0;
    f64 amplitude = 1.0;
    f64 frequency = 1.0;
    f64 max_value = 0.0;
    
    for (int i = 0; i < octaves; ++i) {
        total += noise(x * frequency, y * frequency) * amplitude;
        max_value += amplitude;
        amplitude *= persistence;
        frequency *= 2.0;
    }
    
    return total / max_value;
}

f64 PerlinNoise::ridge(f64 x, f64 y, int octaves, f64 persistence) const {
    f64 total = 0.0;
    f64 amplitude = 1.0;
    f64 frequency = 1.0;
    f64 max_value = 0.0;
    
    for (int i = 0; i < octaves; ++i) {
        // Ridge noise: abs and invert
        f64 n = 1.0 - std::abs(noise(x * frequency, y * frequency));
        n = n * n;  // Sharpen ridges
        total += n * amplitude;
        max_value += amplitude;
        amplitude *= persistence;
        frequency *= 2.0;
    }
    
    return total / max_value;
}

// =============================================================================
// PhysicalTerrain Implementation
// =============================================================================

PhysicalTerrain::PhysicalTerrain()
    : config_()
    , noise_()
    , initialized_(false)
    , overlay_noise_()
{
}

Status PhysicalTerrain::init(const TerrainPhysicalConfig& config) {
    if (config.seed == 0) {
        return Error(ErrorCode::INVALID_ARGUMENT, "Terrain seed must be non-zero");
    }
    
    config_ = config;
    noise_.reseed(config.seed);
    
    // Initialize overlay noise generators with offset seeds
    overlay_noise_.clear();
    for (usize i = 0; i < config.overlays.size(); ++i) {
        PerlinNoise overlay_noise(config.seed + config.overlays[i].seed_offset + i + 1);
        overlay_noise_.push_back(overlay_noise);
    }
    
    initialized_ = true;
    return Status();
}

void PhysicalTerrain::reset() {
    config_ = TerrainPhysicalConfig();
    overlay_noise_.clear();
    initialized_ = false;
}

f64 PhysicalTerrain::normalize_x(f64 x) const {
    return x / config_.width_m;
}

f64 PhysicalTerrain::normalize_y(f64 y) const {
    return y / config_.height_m;
}

f64 PhysicalTerrain::compute_base_elevation(f64 x, f64 y) const {
    f64 nx = normalize_x(x);
    f64 ny = normalize_y(y);
    
    // Scale for noise sampling (higher = more detail)
    f64 scale = 4.0;
    
    f64 elev = 0.0;
    
    switch (config_.base) {
        case TerrainBase::FLAT:
            // Very low frequency, minimal variation
            elev = noise_.fbm(nx * scale * 0.5, ny * scale * 0.5, 2, 0.3);
            elev *= 0.1;  // Reduce amplitude
            break;
            
        case TerrainBase::HILLS:
            // Medium frequency rolling hills
            elev = noise_.fbm(nx * scale, ny * scale, 4, 0.5);
            break;
            
        case TerrainBase::MOUNTAINS:
            // Ridge noise for sharp peaks
            elev = noise_.ridge(nx * scale, ny * scale, 6, 0.5);
            break;
            
        case TerrainBase::VALLEY:
            // Invert hills - low in center
            elev = -std::abs(noise_.fbm(nx * scale, ny * scale, 4, 0.5));
            elev += 0.5;  // Shift up
            break;
            
        case TerrainBase::PLATEAU:
            // Flat top with steep edges
            {
                f64 raw = noise_.fbm(nx * scale * 0.5, ny * scale * 0.5, 3, 0.5);
                // Sigmoid to flatten top and bottom
                elev = 1.0 / (1.0 + std::exp(-raw * 4.0));
            }
            break;
    }
    
    // Apply roughness
    elev *= config_.roughness;
    
    // Map to elevation range
    // elev is in [-1, 1] or [0, 1] depending on type
    // Normalize to [0, 1] then apply range
    f64 normalized = (elev + 1.0) * 0.5;
    return config_.base_elevation_m + normalized * config_.elevation_range_m;
}

f64 PhysicalTerrain::apply_connectivity_blend(f64 x, f64 y, f64 base_elev) const {
    // Blend edges to match neighboring terrain types
    f64 nx = normalize_x(x);
    f64 ny = normalize_y(y);
    
    constexpr f64 BLEND_ZONE = 0.1;  // 10% of map at each edge
    
    f64 blend_factor = 1.0;
    f64 target_elev = base_elev;
    
    // North edge (y near height)
    if (ny > (1.0 - BLEND_ZONE)) {
        f64 t = (ny - (1.0 - BLEND_ZONE)) / BLEND_ZONE;
        // Adjust based on connectivity.north
        // For now, simple linear blend toward base elevation
        target_elev = config_.base_elevation_m + config_.elevation_range_m * 0.5;
        blend_factor = 1.0 - t;
    }
    // South edge (y near 0)
    else if (ny < BLEND_ZONE) {
        f64 t = (BLEND_ZONE - ny) / BLEND_ZONE;
        target_elev = config_.base_elevation_m + config_.elevation_range_m * 0.5;
        blend_factor = 1.0 - t;
    }
    // East edge (x near width)
    else if (nx > (1.0 - BLEND_ZONE)) {
        f64 t = (nx - (1.0 - BLEND_ZONE)) / BLEND_ZONE;
        target_elev = config_.base_elevation_m + config_.elevation_range_m * 0.5;
        blend_factor = 1.0 - t;
    }
    // West edge (x near 0)
    else if (nx < BLEND_ZONE) {
        f64 t = (BLEND_ZONE - nx) / BLEND_ZONE;
        target_elev = config_.base_elevation_m + config_.elevation_range_m * 0.5;
        blend_factor = 1.0 - t;
    }
    
    return base_elev * blend_factor + target_elev * (1.0 - blend_factor);
}

f64 PhysicalTerrain::compute_overlay_density(f64 x, f64 y, usize overlay_idx) const {
    if (overlay_idx >= overlay_noise_.size()) return 0.0;
    
    const auto& overlay = config_.overlays[overlay_idx];
    const auto& noise = overlay_noise_[overlay_idx];
    
    f64 nx = normalize_x(x);
    f64 ny = normalize_y(y);
    
    // Generate noise for this overlay
    f64 n = noise.fbm(nx * 8.0, ny * 8.0, 4, 0.5);
    n = (n + 1.0) * 0.5;  // Map to [0, 1]
    
    // Threshold based on density
    // Higher density = more of the overlay passes threshold
    f64 threshold = 1.0 - overlay.density;
    
    if (n > threshold) {
        // Return how much above threshold (0 at threshold, 1 at max)
        return (n - threshold) / (1.0 - threshold);
    }
    
    return 0.0;
}

f64 PhysicalTerrain::elevation(f64 x, f64 y) const {
    if (!initialized_) return 0.0;
    
    f64 base = compute_base_elevation(x, y);
    return apply_connectivity_blend(x, y, base);
}

f64 PhysicalTerrain::slope(f64 x, f64 y) const {
    if (!initialized_) return 0.0;
    
    // Compute slope using finite differences
    constexpr f64 DELTA = 10.0;  // 10 meter sample distance
    
    f64 e_center = elevation(x, y);
    f64 e_east = elevation(x + DELTA, y);
    f64 e_north = elevation(x, y + DELTA);
    
    f64 dx = (e_east - e_center) / DELTA;
    f64 dy = (e_north - e_center) / DELTA;
    
    f64 gradient = std::sqrt(dx * dx + dy * dy);
    return std::atan(gradient) * (180.0 / 3.14159265358979323846);
}

f64 PhysicalTerrain::slope_direction(f64 x, f64 y) const {
    if (!initialized_) return 0.0;
    
    constexpr f64 DELTA = 10.0;
    
    f64 e_center = elevation(x, y);
    f64 e_east = elevation(x + DELTA, y);
    f64 e_north = elevation(x, y + DELTA);
    
    f64 dx = (e_east - e_center) / DELTA;
    f64 dy = (e_north - e_center) / DELTA;
    
    // Direction of steepest descent (opposite of gradient)
    f64 angle = std::atan2(-dy, -dx) * (180.0 / 3.14159265358979323846);
    if (angle < 0) angle += 360.0;
    
    return angle;
}

bool PhysicalTerrain::is_water(f64 x, f64 y) const {
    if (!initialized_) return false;
    
    for (usize i = 0; i < config_.overlays.size(); ++i) {
        const auto& overlay = config_.overlays[i];
        if (overlay.type == OverlayType::RIVER || overlay.type == OverlayType::LAKE) {
            if (compute_overlay_density(x, y, i) > 0.5) {
                return true;
            }
        }
    }
    
    return false;
}

f64 PhysicalTerrain::obstacle(f64 x, f64 y) const {
    if (!initialized_) return 0.0;
    
    // Water is obstacle
    if (is_water(x, y)) return 1.0;
    
    // Very steep slopes are obstacles
    f64 s = slope(x, y);
    if (s > 60.0) return 1.0;
    if (s > 45.0) return 0.8;
    if (s > 30.0) return 0.5;
    
    // Dense urban can be partial obstacle
    for (usize i = 0; i < config_.overlays.size(); ++i) {
        const auto& overlay = config_.overlays[i];
        if (overlay.type == OverlayType::URBAN) {
            f64 density = compute_overlay_density(x, y, i);
            if (density > 0.8) return 0.3;  // Dense urban
        }
    }
    
    return 0.0;
}

OverlayType PhysicalTerrain::overlay_at(f64 x, f64 y) const {
    if (!initialized_) return OverlayType::NONE;
    
    OverlayType dominant = OverlayType::NONE;
    f64 max_density = 0.0;
    
    for (usize i = 0; i < config_.overlays.size(); ++i) {
        f64 density = compute_overlay_density(x, y, i);
        if (density > max_density) {
            max_density = density;
            dominant = config_.overlays[i].type;
        }
    }
    
    // Only return if above minimum threshold
    if (max_density > 0.3) {
        return dominant;
    }
    
    return OverlayType::NONE;
}

TerrainSample PhysicalTerrain::sample(f64 x, f64 y) const {
    TerrainSample s;
    s.elevation = elevation(x, y);
    s.slope = slope(x, y);
    s.slope_direction = slope_direction(x, y);
    s.is_water = is_water(x, y);
    s.obstacle = obstacle(x, y);
    s.primary_overlay = overlay_at(x, y);
    return s;
}

std::vector<TerrainSample> PhysicalTerrain::sample_line(
    f64 x1, f64 y1, f64 x2, f64 y2, f64 interval_m) const {
    
    std::vector<TerrainSample> samples;
    
    f64 dx = x2 - x1;
    f64 dy = y2 - y1;
    f64 length = std::sqrt(dx * dx + dy * dy);
    
    if (length < interval_m) {
        samples.push_back(sample(x1, y1));
        samples.push_back(sample(x2, y2));
        return samples;
    }
    
    int num_samples = static_cast<int>(length / interval_m) + 1;
    samples.reserve(num_samples);
    
    for (int i = 0; i < num_samples; ++i) {
        f64 t = static_cast<f64>(i) / (num_samples - 1);
        f64 x = x1 + dx * t;
        f64 y = y1 + dy * t;
        samples.push_back(sample(x, y));
    }
    
    return samples;
}

// =============================================================================
// String parsing
// =============================================================================

TerrainBase parse_terrain_base(const std::string& name) {
    if (name == "flat") return TerrainBase::FLAT;
    if (name == "hills") return TerrainBase::HILLS;
    if (name == "mountains") return TerrainBase::MOUNTAINS;
    if (name == "valley") return TerrainBase::VALLEY;
    if (name == "plateau") return TerrainBase::PLATEAU;
    return TerrainBase::FLAT;  // default
}

OverlayType parse_overlay_type(const std::string& name) {
    if (name == "forest") return OverlayType::FOREST;
    if (name == "urban") return OverlayType::URBAN;
    if (name == "swamp") return OverlayType::SWAMP;
    if (name == "river") return OverlayType::RIVER;
    if (name == "lake") return OverlayType::LAKE;
    if (name == "road") return OverlayType::ROAD;
    if (name == "bridge") return OverlayType::BRIDGE;
    return OverlayType::NONE;
}

}  // namespace athena
