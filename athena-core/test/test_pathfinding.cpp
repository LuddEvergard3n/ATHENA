// ATHENA Core - Pathfinding Tests
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/pathfinding.hpp"
#include "athena/terrain.hpp"
#include "athena/environment.hpp"
#include "athena/terrain_semantics.hpp"

#include <iostream>
#include <cmath>

using namespace athena;

// =============================================================================
// Test Macros
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
        if (fn()) { \
            std::cout << "PASS\n"; \
            passed++; \
        } else { \
            failed++; \
        } \
    } while(0)

// =============================================================================
// Test Helpers
// =============================================================================

struct TestTerrain {
    PhysicalTerrain physical;
    Environment environment;
    TerrainSemantics semantics;
    
    bool init_flat() {
        TerrainPhysicalConfig phys;
        phys.base = TerrainBase::FLAT;
        phys.seed = 12345;
        phys.roughness = 0.1;  // Very smooth
        phys.width_m = 10000.0;
        phys.height_m = 10000.0;
        
        if (!physical.init(phys).ok()) return false;
        
        TerrainEnvironmentConfig env;
        env.climate = Climate::TEMPERATE;
        env.weather = Weather::CLEAR;
        
        if (!environment.init(env).ok()) return false;
        
        return semantics.init(&physical, &environment).ok();
    }
    
    bool init_with_river() {
        TerrainPhysicalConfig phys;
        phys.base = TerrainBase::FLAT;
        phys.seed = 12345;
        phys.roughness = 0.1;
        phys.width_m = 10000.0;
        phys.height_m = 10000.0;
        
        // Add river overlay across the middle
        OverlayConfig river;
        river.type = OverlayType::RIVER;
        river.density = 0.3;
        river.param1 = 500.0;  // width
        phys.overlays.push_back(river);
        
        if (!physical.init(phys).ok()) return false;
        
        TerrainEnvironmentConfig env;
        if (!environment.init(env).ok()) return false;
        
        return semantics.init(&physical, &environment).ok();
    }
    
    bool init_with_forest() {
        TerrainPhysicalConfig phys;
        phys.base = TerrainBase::FLAT;
        phys.seed = 12345;
        phys.roughness = 0.1;
        phys.width_m = 10000.0;
        phys.height_m = 10000.0;
        
        // Dense forest
        OverlayConfig forest;
        forest.type = OverlayType::FOREST;
        forest.density = 0.8;
        phys.overlays.push_back(forest);
        
        if (!physical.init(phys).ok()) return false;
        
        TerrainEnvironmentConfig env;
        if (!environment.init(env).ok()) return false;
        
        return semantics.init(&physical, &environment).ok();
    }
    
    bool init_with_road() {
        TerrainPhysicalConfig phys;
        phys.base = TerrainBase::HILLS;
        phys.seed = 12345;
        phys.roughness = 0.5;
        phys.width_m = 10000.0;
        phys.height_m = 10000.0;
        
        // Road (faster travel)
        OverlayConfig road;
        road.type = OverlayType::ROAD;
        road.density = 0.2;
        phys.overlays.push_back(road);
        
        if (!physical.init(phys).ok()) return false;
        
        TerrainEnvironmentConfig env;
        if (!environment.init(env).ok()) return false;
        
        return semantics.init(&physical, &environment).ok();
    }
};

// =============================================================================
// Basic Tests
// =============================================================================

bool test_pathfinder_init() {
    TestTerrain t;
    TEST_ASSERT(t.init_flat(), "Terrain init");
    
    Pathfinder pf;
    TEST_ASSERT(!pf.initialized(), "Not initialized initially");
    
    PathfindingConfig config;
    config.cell_size = 100.0;
    
    auto status = pf.init(&t.semantics, config);
    TEST_ASSERT(status.ok(), "Pathfinder init");
    TEST_ASSERT(pf.initialized(), "Is initialized");
    
    return true;
}

bool test_pathfinder_null_terrain() {
    Pathfinder pf;
    PathfindingConfig config;
    
    auto status = pf.init(nullptr, config);
    TEST_ASSERT(!status.ok(), "Should fail with null terrain");
    
    return true;
}

bool test_trivial_path() {
    TestTerrain t;
    TEST_ASSERT(t.init_flat(), "Terrain init");
    
    PathfindingConfig config;
    config.cell_size = 100.0;
    
    Pathfinder pf;
    TEST_ASSERT(pf.init(&t.semantics, config).ok(), "Pathfinder init");
    
    // Same start and goal
    PathResult r = pf.find_path(500.0, 500.0, 500.0, 500.0, 0, MovementType::TRACKED);
    TEST_ASSERT(r.found, "Trivial path found");
    TEST_ASSERT(r.path.size() >= 2, "Path has waypoints");
    
    return true;
}

// =============================================================================
// Pathfinding Tests
// =============================================================================

bool test_straight_path_flat_terrain() {
    TestTerrain t;
    TEST_ASSERT(t.init_flat(), "Terrain init");
    
    PathfindingConfig config;
    config.cell_size = 100.0;
    config.allow_diagonal = true;
    
    Pathfinder pf;
    TEST_ASSERT(pf.init(&t.semantics, config).ok(), "Pathfinder init");
    
    // Path across flat terrain
    PathResult r = pf.find_path(1000.0, 1000.0, 5000.0, 1000.0, 0, MovementType::TRACKED);
    
    TEST_ASSERT(r.found, "Path found");
    TEST_ASSERT(r.path.size() >= 2, "Path has waypoints");
    TEST_ASSERT(r.total_cost > 0.0, "Has cost");
    TEST_ASSERT(r.path_length_m > 3500.0, "Path length reasonable");  // ~4000m
    TEST_ASSERT(r.path_length_m < 5000.0, "Path length not too long");
    
    // First and last should match start/goal
    f64 start_dist = std::sqrt(
        std::pow(r.path.front().first - 1000.0, 2) +
        std::pow(r.path.front().second - 1000.0, 2));
    TEST_ASSERT(start_dist < 1.0, "Start position correct");
    
    f64 goal_dist = std::sqrt(
        std::pow(r.path.back().first - 5000.0, 2) +
        std::pow(r.path.back().second - 1000.0, 2));
    TEST_ASSERT(goal_dist < 1.0, "Goal position correct");
    
    return true;
}

bool test_diagonal_path() {
    TestTerrain t;
    TEST_ASSERT(t.init_flat(), "Terrain init");
    
    PathfindingConfig config;
    config.cell_size = 100.0;
    config.allow_diagonal = true;
    
    Pathfinder pf;
    TEST_ASSERT(pf.init(&t.semantics, config).ok(), "Pathfinder init");
    
    // Diagonal path
    PathResult r = pf.find_path(1000.0, 1000.0, 3000.0, 3000.0, 0, MovementType::FOOT);
    
    TEST_ASSERT(r.found, "Diagonal path found");
    TEST_ASSERT(r.path.size() >= 2, "Path has waypoints");
    
    // Diagonal should be shorter than L-shaped
    f64 direct_dist = std::sqrt(2.0) * 2000.0;  // ~2828m
    TEST_ASSERT(r.path_length_m < direct_dist + 500.0, "Path reasonably short");
    
    return true;
}

bool test_path_with_forest() {
    TestTerrain t;
    TEST_ASSERT(t.init_with_forest(), "Terrain init");
    
    PathfindingConfig config;
    config.cell_size = 100.0;
    
    Pathfinder pf;
    TEST_ASSERT(pf.init(&t.semantics, config).ok(), "Pathfinder init");
    
    // Path through forest (tracked vehicles slower)
    PathResult r_tracked = pf.find_path(1000.0, 5000.0, 5000.0, 5000.0, 0, MovementType::TRACKED);
    
    TEST_ASSERT(r_tracked.found, "Path found for tracked");
    TEST_ASSERT(r_tracked.total_cost > 4000.0, "Forest increases cost");
    
    // Compare with foot (foot is faster in forest)
    PathResult r_foot = pf.find_path(1000.0, 5000.0, 5000.0, 5000.0, 0, MovementType::FOOT);
    
    TEST_ASSERT(r_foot.found, "Path found for foot");
    
    // Both should find paths, but costs differ
    TEST_ASSERT(r_tracked.found && r_foot.found, "Both find paths");
    
    return true;
}

bool test_different_movement_types() {
    TestTerrain t;
    TEST_ASSERT(t.init_flat(), "Terrain init");
    
    PathfindingConfig config;
    config.cell_size = 100.0;
    
    Pathfinder pf;
    TEST_ASSERT(pf.init(&t.semantics, config).ok(), "Pathfinder init");
    
    // Same path, different movement types
    f64 start_x = 1000.0, start_y = 5000.0;
    f64 goal_x = 5000.0, goal_y = 5000.0;
    
    PathResult r_foot = pf.find_path(start_x, start_y, goal_x, goal_y, 0, MovementType::FOOT);
    PathResult r_wheeled = pf.find_path(start_x, start_y, goal_x, goal_y, 0, MovementType::WHEELED);
    PathResult r_tracked = pf.find_path(start_x, start_y, goal_x, goal_y, 0, MovementType::TRACKED);
    
    TEST_ASSERT(r_foot.found, "Foot path found");
    TEST_ASSERT(r_wheeled.found, "Wheeled path found");
    TEST_ASSERT(r_tracked.found, "Tracked path found");
    
    // All should have similar lengths on flat terrain
    TEST_ASSERT(std::abs(r_foot.path_length_m - r_wheeled.path_length_m) < 500.0, 
                "Similar path lengths");
    
    return true;
}

bool test_path_exists_utility() {
    TestTerrain t;
    TEST_ASSERT(t.init_flat(), "Terrain init");
    
    PathfindingConfig config;
    config.cell_size = 100.0;
    
    Pathfinder pf;
    TEST_ASSERT(pf.init(&t.semantics, config).ok(), "Pathfinder init");
    
    // Should exist
    bool exists = pf.path_exists(1000.0, 1000.0, 5000.0, 5000.0, 0, MovementType::TRACKED);
    TEST_ASSERT(exists, "Path should exist");
    
    return true;
}

bool test_compute_path_cost() {
    TestTerrain t;
    TEST_ASSERT(t.init_flat(), "Terrain init");
    
    PathfindingConfig config;
    config.cell_size = 100.0;
    
    Pathfinder pf;
    TEST_ASSERT(pf.init(&t.semantics, config).ok(), "Pathfinder init");
    
    // Create simple path
    std::vector<std::pair<f64, f64>> path = {
        {1000.0, 1000.0},
        {2000.0, 1000.0},
        {3000.0, 1000.0}
    };
    
    f64 cost = pf.compute_path_cost(path, 0, MovementType::TRACKED);
    TEST_ASSERT(cost > 0.0, "Cost is positive");
    TEST_ASSERT(cost < 10000.0, "Cost is reasonable");  // 2000m with low multiplier
    
    return true;
}

bool test_smooth_path() {
    TestTerrain t;
    TEST_ASSERT(t.init_flat(), "Terrain init");
    
    PathfindingConfig config;
    config.cell_size = 100.0;
    
    Pathfinder pf;
    TEST_ASSERT(pf.init(&t.semantics, config).ok(), "Pathfinder init");
    
    // Find path first
    PathResult r = pf.find_path(1000.0, 1000.0, 5000.0, 1000.0, 0, MovementType::TRACKED);
    TEST_ASSERT(r.found, "Path found");
    
    // Smooth it
    auto smoothed = pf.smooth_path(r.path, 0, MovementType::TRACKED);
    TEST_ASSERT(!smoothed.empty(), "Smoothed path not empty");
    TEST_ASSERT(smoothed.size() <= r.path.size(), "Smoothed has fewer or equal waypoints");
    
    return true;
}

// =============================================================================
// Convenience Function Test
// =============================================================================

bool test_find_path_simple() {
    TestTerrain t;
    TEST_ASSERT(t.init_flat(), "Terrain init");
    
    PathResult r = find_path_simple(
        &t.semantics,
        1000.0, 1000.0,
        5000.0, 5000.0,
        0,
        MovementType::TRACKED,
        100.0);
    
    TEST_ASSERT(r.found, "Simple pathfinding works");
    TEST_ASSERT(r.path.size() >= 2, "Has waypoints");
    
    return true;
}

// =============================================================================
// Determinism Test
// =============================================================================

bool test_pathfinding_determinism() {
    auto run = [](u32 seed) -> f64 {
        TerrainPhysicalConfig phys;
        phys.base = TerrainBase::HILLS;
        phys.seed = seed;
        phys.roughness = 0.5;
        phys.width_m = 10000.0;
        phys.height_m = 10000.0;
        
        PhysicalTerrain physical;
        if (!physical.init(phys).ok()) return -1.0;
        
        TerrainEnvironmentConfig env;
        Environment environment;
        if (!environment.init(env).ok()) return -1.0;
        
        TerrainSemantics semantics;
        if (!semantics.init(&physical, &environment).ok()) return -1.0;
        
        PathfindingConfig config;
        config.cell_size = 100.0;
        
        Pathfinder pf;
        if (!pf.init(&semantics, config).ok()) return -1.0;
        
        PathResult r = pf.find_path(500.0, 500.0, 8000.0, 8000.0, 0, MovementType::TRACKED);
        if (!r.found) return -2.0;
        
        return r.total_cost;
    };
    
    const u32 SEED = 12345;
    f64 r1 = run(SEED);
    f64 r2 = run(SEED);
    
    TEST_ASSERT(r1 > 0.0, "Run 1 succeeded");
    TEST_ASSERT(r2 > 0.0, "Run 2 succeeded");
    TEST_ASSERT(r1 == r2, "Deterministic results");
    
    return true;
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "=== ATHENA Pathfinding Tests ===\n\n";
    
    int passed = 0;
    int failed = 0;
    
    std::cout << "--- Basic Tests ---\n";
    RUN_TEST(test_pathfinder_init);
    RUN_TEST(test_pathfinder_null_terrain);
    RUN_TEST(test_trivial_path);
    
    std::cout << "\n--- Pathfinding Tests ---\n";
    RUN_TEST(test_straight_path_flat_terrain);
    RUN_TEST(test_diagonal_path);
    RUN_TEST(test_path_with_forest);
    RUN_TEST(test_different_movement_types);
    
    std::cout << "\n--- Utility Tests ---\n";
    RUN_TEST(test_path_exists_utility);
    RUN_TEST(test_compute_path_cost);
    RUN_TEST(test_smooth_path);
    RUN_TEST(test_find_path_simple);
    
    std::cout << "\n--- Determinism Tests ---\n";
    RUN_TEST(test_pathfinding_determinism);
    
    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    
    if (failed > 0) {
        std::cout << "\n*** PATHFINDING TESTS FAILED ***\n";
        return 1;
    }
    
    std::cout << "\nAll pathfinding tests passed.\n";
    return 0;
}
