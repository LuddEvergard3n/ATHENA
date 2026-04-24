// ATHENA Core - Terrain System Tests
// Contract: Verify deterministic terrain generation and 3-layer architecture.
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/terrain.hpp"
#include "athena/environment.hpp"
#include "athena/terrain_semantics.hpp"

#include <iostream>
#include <cmath>

using namespace athena;

// =============================================================================
// Test Utilities
// =============================================================================

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << "\n"; \
            std::cerr << "  at " << __FILE__ << ":" << __LINE__ << "\n"; \
            return false; \
        } \
    } while(0)

#define RUN_TEST(fn) \
    do { \
        std::cout << "Running " << #fn << "... "; \
        std::cout.flush(); \
        if (fn()) { \
            std::cout << "PASS\n"; \
            passed++; \
        } else { \
            failed++; \
        } \
    } while(0)

// =============================================================================
// Layer 1: Physical Terrain Tests
// =============================================================================

bool test_perlin_determinism() {
    PerlinNoise noise1(12345);
    PerlinNoise noise2(12345);
    
    // Same seed should produce identical results
    for (int i = 0; i < 100; ++i) {
        f64 x = i * 0.1;
        f64 y = i * 0.07;
        TEST_ASSERT(noise1.noise(x, y) == noise2.noise(x, y), 
            "Perlin noise must be deterministic");
    }
    
    // Different seeds should produce different results
    PerlinNoise noise3(54321);
    int different_count = 0;
    for (int i = 0; i < 100; ++i) {
        f64 x = i * 0.1;
        f64 y = i * 0.07;
        if (std::abs(noise1.noise(x, y) - noise3.noise(x, y)) > 0.001) {
            different_count++;
        }
    }
    TEST_ASSERT(different_count > 50, "Different seeds should produce mostly different noise");
    
    return true;
}

bool test_perlin_range() {
    PerlinNoise noise(42);
    
    // Noise should be in [-1, 1] range
    for (int i = 0; i < 1000; ++i) {
        f64 x = (i % 100) * 0.17;
        f64 y = (i / 100) * 0.23;
        f64 value = noise.noise(x, y);
        TEST_ASSERT(value >= -1.0 && value <= 1.0, 
            "Perlin noise should be in [-1, 1]");
    }
    
    return true;
}

bool test_terrain_generation_determinism() {
    TerrainPhysicalConfig config;
    config.base = TerrainBase::HILLS;
    config.roughness = 0.6;
    config.seed = 0xABCDEF;
    config.width_m = 10000.0;
    config.height_m = 10000.0;
    
    PhysicalTerrain terrain1;
    auto s1 = terrain1.init(config);
    TEST_ASSERT(s1.ok(), "Terrain 1 init");
    
    PhysicalTerrain terrain2;
    auto s2 = terrain2.init(config);
    TEST_ASSERT(s2.ok(), "Terrain 2 init");
    
    // Same config should produce identical terrain
    for (int i = 0; i < 100; ++i) {
        f64 x = (i % 10) * 1000.0;
        f64 y = (i / 10) * 1000.0;
        
        TEST_ASSERT(terrain1.elevation(x, y) == terrain2.elevation(x, y),
            "Elevation must be deterministic");
        TEST_ASSERT(terrain1.slope(x, y) == terrain2.slope(x, y),
            "Slope must be deterministic");
    }
    
    return true;
}

bool test_terrain_base_types() {
    TerrainPhysicalConfig config;
    config.seed = 12345;
    config.roughness = 0.7;
    config.width_m = 10000.0;
    config.height_m = 10000.0;
    config.base_elevation_m = 100.0;
    config.elevation_range_m = 500.0;
    
    // Test FLAT - should have minimal variation
    config.base = TerrainBase::FLAT;
    PhysicalTerrain flat;
    flat.init(config);
    
    f64 flat_min = 9999.0, flat_max = -9999.0;
    for (int i = 0; i < 100; ++i) {
        f64 e = flat.elevation(i * 100.0, i * 100.0);
        flat_min = std::min(flat_min, e);
        flat_max = std::max(flat_max, e);
    }
    f64 flat_range = flat_max - flat_min;
    
    // Test MOUNTAINS - should have large variation
    config.base = TerrainBase::MOUNTAINS;
    PhysicalTerrain mountains;
    mountains.init(config);
    
    f64 mtn_min = 9999.0, mtn_max = -9999.0;
    for (int i = 0; i < 100; ++i) {
        f64 e = mountains.elevation(i * 100.0, i * 100.0);
        mtn_min = std::min(mtn_min, e);
        mtn_max = std::max(mtn_max, e);
    }
    f64 mtn_range = mtn_max - mtn_min;
    
    TEST_ASSERT(mtn_range > flat_range, 
        "Mountains should have more elevation variation than flat");
    
    return true;
}

bool test_terrain_overlays() {
    TerrainPhysicalConfig config;
    config.base = TerrainBase::HILLS;
    config.seed = 99999;
    config.roughness = 0.5;
    config.width_m = 10000.0;
    config.height_m = 10000.0;
    
    // Add forest overlay
    OverlayConfig forest;
    forest.type = OverlayType::FOREST;
    forest.density = 0.5;
    config.overlays.push_back(forest);
    
    PhysicalTerrain terrain;
    terrain.init(config);
    
    // Should have some forest and some open areas
    int forest_count = 0;
    int samples = 0;
    
    // Sample more points across the terrain
    for (int i = 0; i < 400; ++i) {
        f64 x = (i % 20) * 500.0;
        f64 y = (i / 20) * 500.0;
        samples++;
        
        if (terrain.overlay_at(x, y) == OverlayType::FOREST) {
            forest_count++;
        }
    }
    
    // With 50% density, should have a mix
    // Due to noise thresholding, actual coverage may be lower
    TEST_ASSERT(forest_count > 5, "Should have some forest");
    TEST_ASSERT(forest_count < samples - 5, "Should have some open areas");
    
    return true;
}

bool test_terrain_slope() {
    TerrainPhysicalConfig config;
    config.base = TerrainBase::MOUNTAINS;
    config.seed = 11111;
    config.roughness = 0.8;
    config.width_m = 10000.0;
    config.height_m = 10000.0;
    config.elevation_range_m = 1000.0;
    
    PhysicalTerrain terrain;
    terrain.init(config);
    
    // Mountains should have some steep slopes
    bool found_steep = false;
    for (int i = 0; i < 1000; ++i) {
        f64 x = (i % 100) * 100.0;
        f64 y = (i / 100) * 100.0;
        f64 slope = terrain.slope(x, y);
        
        // Slope should be >= 0
        TEST_ASSERT(slope >= 0.0, "Slope must be non-negative");
        
        if (slope > 20.0) {
            found_steep = true;
        }
    }
    
    TEST_ASSERT(found_steep, "Mountains should have some steep slopes");
    
    return true;
}

// =============================================================================
// Layer 2: Environment Tests
// =============================================================================

bool test_environment_init() {
    TerrainEnvironmentConfig config;
    config.climate = Climate::TEMPERATE;
    config.weather = Weather::RAIN;
    config.temperature_c = 10.0;
    config.visibility_km = 5.0;
    
    Environment env;
    auto status = env.init(config);
    TEST_ASSERT(status.ok(), "Environment init");
    TEST_ASSERT(env.is_initialized(), "Should be initialized");
    
    return true;
}

bool test_weather_visibility() {
    TerrainEnvironmentConfig config;
    config.visibility_km = 10.0;
    
    Environment env;
    
    // Clear weather
    config.weather = Weather::CLEAR;
    env.init(config);
    f64 vis_clear = env.visibility(0, 0, 0);
    
    // Foggy weather
    config.weather = Weather::FOG;
    env.init(config);
    f64 vis_fog = env.visibility(0, 0, 0);
    
    TEST_ASSERT(vis_fog < vis_clear, "Fog should reduce visibility");
    TEST_ASSERT(vis_fog < 2.0, "Fog visibility should be very low");
    
    return true;
}

bool test_weather_modifiers() {
    // Rain should increase movement cost
    auto rain_mods = Environment::weather_modifiers(Weather::RAIN);
    TEST_ASSERT(rain_mods.movement_modifier > 1.0, 
        "Rain should increase movement cost");
    
    // Blizzard should be worse
    auto blizzard_mods = Environment::weather_modifiers(Weather::BLIZZARD);
    TEST_ASSERT(blizzard_mods.movement_modifier > rain_mods.movement_modifier,
        "Blizzard should be worse than rain");
    TEST_ASSERT(blizzard_mods.personnel_attrition > 0.0,
        "Blizzard should cause attrition");
    
    // Clear should be neutral
    auto clear_mods = Environment::weather_modifiers(Weather::CLEAR);
    TEST_ASSERT(clear_mods.movement_modifier == 1.0,
        "Clear weather should have no movement penalty");
    
    return true;
}

bool test_events() {
    TerrainEnvironmentConfig config;
    config.weather = Weather::CLEAR;
    
    Environment env;
    env.init(config);
    
    // Add flood event
    EventConfig flood;
    flood.type = EventType::FLOOD;
    flood.center_x = 5000.0;
    flood.center_y = 5000.0;
    flood.radius_m = 1000.0;
    flood.start_tick = 0;
    flood.duration_ticks = 100;
    flood.intensity = 0.8;
    
    env.add_event(flood);
    
    // Inside flood zone should be denied
    TEST_ASSERT(env.is_denied(5000.0, 5000.0, 10), 
        "Flood center should be denied");
    
    // Outside flood zone should be fine
    TEST_ASSERT(!env.is_denied(0.0, 0.0, 10),
        "Far from flood should be accessible");
    
    // After flood ends, should be accessible
    env.update(200);  // Update to after flood
    TEST_ASSERT(!env.is_denied(5000.0, 5000.0, 200),
        "After flood ends should be accessible");
    
    return true;
}

// =============================================================================
// Layer 3: Semantic Tests
// =============================================================================

bool test_semantics_init() {
    TerrainPhysicalConfig phys_config;
    phys_config.base = TerrainBase::HILLS;
    phys_config.seed = 12345;
    phys_config.width_m = 10000.0;
    phys_config.height_m = 10000.0;
    
    PhysicalTerrain physical;
    physical.init(phys_config);
    
    TerrainEnvironmentConfig env_config;
    env_config.weather = Weather::CLEAR;
    
    Environment environment;
    environment.init(env_config);
    
    TerrainSemantics semantics;
    auto status = semantics.init(&physical, &environment);
    TEST_ASSERT(status.ok(), "Semantics init");
    
    return true;
}

bool test_movement_cost_derivation() {
    TerrainPhysicalConfig phys_config;
    phys_config.base = TerrainBase::HILLS;
    phys_config.seed = 12345;
    phys_config.width_m = 10000.0;
    phys_config.height_m = 10000.0;
    
    // Add swamp
    OverlayConfig swamp;
    swamp.type = OverlayType::SWAMP;
    swamp.density = 0.8;
    phys_config.overlays.push_back(swamp);
    
    PhysicalTerrain physical;
    physical.init(phys_config);
    
    Environment environment;
    environment.init(TerrainEnvironmentConfig{});
    
    TerrainSemantics semantics;
    semantics.init(&physical, &environment);
    
    // Find a swamp point
    f64 swamp_x = 0, swamp_y = 0;
    for (int i = 0; i < 100; ++i) {
        f64 x = (i % 10) * 1000.0;
        f64 y = (i / 10) * 1000.0;
        if (physical.overlay_at(x, y) == OverlayType::SWAMP) {
            swamp_x = x;
            swamp_y = y;
            break;
        }
    }
    
    // Wheeled vehicles should have high cost in swamp
    auto wheeled_cost = semantics.movement_cost(swamp_x, swamp_y, 0, MovementType::WHEELED);
    TEST_ASSERT(!wheeled_cost.passable || wheeled_cost.cost > 5.0,
        "Wheeled vehicles should struggle in swamp");
    
    // Helicopters should be unaffected
    auto heli_cost = semantics.movement_cost(swamp_x, swamp_y, 0, MovementType::HELICOPTER);
    TEST_ASSERT(heli_cost.passable && heli_cost.cost <= 1.5,
        "Helicopters should ignore swamp");
    
    return true;
}

bool test_defense_value_derivation() {
    TerrainPhysicalConfig phys_config;
    phys_config.base = TerrainBase::FLAT;
    phys_config.seed = 54321;
    phys_config.width_m = 10000.0;
    phys_config.height_m = 10000.0;
    
    // Add urban area
    OverlayConfig urban;
    urban.type = OverlayType::URBAN;
    urban.density = 0.7;
    phys_config.overlays.push_back(urban);
    
    PhysicalTerrain physical;
    physical.init(phys_config);
    
    TerrainSemantics semantics;
    semantics.init(&physical, nullptr);  // No environment
    
    // Find urban vs open
    f64 urban_x = 0, urban_y = 0;
    f64 open_x = 0, open_y = 0;
    bool found_urban = false, found_open = false;
    
    for (int i = 0; i < 100 && (!found_urban || !found_open); ++i) {
        f64 x = (i % 10) * 1000.0;
        f64 y = (i / 10) * 1000.0;
        if (!found_urban && physical.overlay_at(x, y) == OverlayType::URBAN) {
            urban_x = x;
            urban_y = y;
            found_urban = true;
        }
        if (!found_open && physical.overlay_at(x, y) == OverlayType::NONE) {
            open_x = x;
            open_y = y;
            found_open = true;
        }
    }
    
    if (found_urban && found_open) {
        auto urban_defense = semantics.defense_value(urban_x, urban_y, 0);
        auto open_defense = semantics.defense_value(open_x, open_y, 0);
        
        TEST_ASSERT(urban_defense.cover > open_defense.cover,
            "Urban should provide more cover than open");
        TEST_ASSERT(urban_defense.concealment > open_defense.concealment,
            "Urban should provide more concealment than open");
    }
    
    return true;
}

bool test_line_of_sight() {
    TerrainPhysicalConfig phys_config;
    phys_config.base = TerrainBase::MOUNTAINS;
    phys_config.seed = 77777;
    phys_config.roughness = 0.9;
    phys_config.width_m = 10000.0;
    phys_config.height_m = 10000.0;
    phys_config.elevation_range_m = 500.0;
    
    PhysicalTerrain physical;
    physical.init(phys_config);
    
    TerrainSemantics semantics;
    semantics.init(&physical, nullptr);
    
    // LOS across mountainous terrain should be < 1.0 sometimes
    int blocked_count = 0;
    for (int i = 0; i < 50; ++i) {
        f64 x1 = (i % 5) * 2000.0;
        f64 y1 = (i / 5) * 1000.0;
        f64 x2 = x1 + 3000.0;
        f64 y2 = y1 + 3000.0;
        
        f64 los = semantics.line_of_sight(x1, y1, x2, y2, 0);
        if (los < 1.0) {
            blocked_count++;
        }
    }
    
    TEST_ASSERT(blocked_count > 0, 
        "Mountains should sometimes block LOS");
    
    return true;
}

bool test_environment_affects_semantics() {
    TerrainPhysicalConfig phys_config;
    phys_config.base = TerrainBase::FLAT;
    phys_config.seed = 33333;
    phys_config.width_m = 10000.0;
    phys_config.height_m = 10000.0;
    
    PhysicalTerrain physical;
    physical.init(phys_config);
    
    // Clear weather
    TerrainEnvironmentConfig env_clear;
    env_clear.weather = Weather::CLEAR;
    Environment env1;
    env1.init(env_clear);
    
    // Blizzard
    TerrainEnvironmentConfig env_storm;
    env_storm.weather = Weather::BLIZZARD;
    Environment env2;
    env2.init(env_storm);
    
    TerrainSemantics sem_clear;
    sem_clear.init(&physical, &env1);
    
    TerrainSemantics sem_storm;
    sem_storm.init(&physical, &env2);
    
    // Same terrain, different weather
    auto cost_clear = sem_clear.movement_cost(5000, 5000, 0, MovementType::WHEELED);
    auto cost_storm = sem_storm.movement_cost(5000, 5000, 0, MovementType::WHEELED);
    
    TEST_ASSERT(cost_storm.cost > cost_clear.cost,
        "Blizzard should increase movement cost");
    
    return true;
}

bool test_operational_suitability() {
    TerrainPhysicalConfig phys_config;
    phys_config.base = TerrainBase::HILLS;
    phys_config.seed = 88888;
    phys_config.width_m = 10000.0;
    phys_config.height_m = 10000.0;
    
    // Add forest
    OverlayConfig forest;
    forest.type = OverlayType::FOREST;
    forest.density = 0.6;
    phys_config.overlays.push_back(forest);
    
    PhysicalTerrain physical;
    physical.init(phys_config);
    
    TerrainSemantics semantics;
    semantics.init(&physical, nullptr);
    
    // Find forest point
    f64 forest_x = 5000, forest_y = 5000;
    for (int i = 0; i < 100; ++i) {
        f64 x = (i % 10) * 1000.0;
        f64 y = (i / 10) * 1000.0;
        if (physical.overlay_at(x, y) == OverlayType::FOREST) {
            forest_x = x;
            forest_y = y;
            break;
        }
    }
    
    f64 armor_suit = semantics.armor_suitability(forest_x, forest_y, 0);
    f64 inf_suit = semantics.infantry_suitability(forest_x, forest_y, 0);
    
    // Forest favors infantry over armor
    TEST_ASSERT(inf_suit > armor_suit,
        "Forest should favor infantry over armor");
    
    return true;
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "=== ATHENA Terrain System Tests ===\n\n";
    
    int passed = 0;
    int failed = 0;
    
    std::cout << "--- Layer 1: Physical Terrain ---\n";
    RUN_TEST(test_perlin_determinism);
    RUN_TEST(test_perlin_range);
    RUN_TEST(test_terrain_generation_determinism);
    RUN_TEST(test_terrain_base_types);
    RUN_TEST(test_terrain_overlays);
    RUN_TEST(test_terrain_slope);
    
    std::cout << "\n--- Layer 2: Environment ---\n";
    RUN_TEST(test_environment_init);
    RUN_TEST(test_weather_visibility);
    RUN_TEST(test_weather_modifiers);
    RUN_TEST(test_events);
    
    std::cout << "\n--- Layer 3: Semantics (Derived) ---\n";
    RUN_TEST(test_semantics_init);
    RUN_TEST(test_movement_cost_derivation);
    RUN_TEST(test_defense_value_derivation);
    RUN_TEST(test_line_of_sight);
    RUN_TEST(test_environment_affects_semantics);
    RUN_TEST(test_operational_suitability);
    
    // Summary
    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    
    if (failed > 0) {
        std::cout << "\n*** TERRAIN TESTS FAILED ***\n";
        return 1;
    }
    
    std::cout << "\nAll terrain tests passed.\n";
    return 0;
}
