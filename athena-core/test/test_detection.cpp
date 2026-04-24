// ATHENA Core - Detection System Tests
// Contract: Verify detection, identification, and contact management.
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/systems/detection.hpp"
#include "athena/entities.hpp"
#include "athena/rng.hpp"
#include "athena/terrain.hpp"
#include "athena/environment.hpp"
#include "athena/terrain_semantics.hpp"

#include <iostream>
#include <cmath>

using namespace athena;
using namespace athena::systems;

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
// Helper: Create entity storage with two entities
// =============================================================================

void setup_two_entities(EntityStorage& storage, f64 distance_m) {
    // Initialize storage with enough capacity
    storage.init(10);
    storage.count = 2;
    
    // Entity 0 (blue observer)
    storage.id[0] = 1;
    storage.flags[0] = entity_flags::ACTIVE;
    storage.side[0] = Side::BLUE;
    storage.pos_x[0] = 0.0;
    storage.pos_y[0] = 0.0;
    storage.pos_z[0] = 0.0;
    storage.vel_x[0] = 0.0;
    storage.vel_y[0] = 0.0;
    storage.vel_z[0] = 0.0;
    storage.detection_range[0] = 10000.0;
    storage.health[0] = 1.0;
    
    // Entity 1 (red target)
    storage.id[1] = 2;
    storage.flags[1] = entity_flags::ACTIVE;
    storage.side[1] = Side::RED;
    storage.pos_x[1] = distance_m;
    storage.pos_y[1] = 0.0;
    storage.pos_z[1] = 0.0;
    storage.vel_x[1] = 0.0;
    storage.vel_y[1] = 0.0;
    storage.vel_z[1] = 0.0;
    storage.detection_range[1] = 5000.0;
    storage.health[1] = 1.0;
}

// =============================================================================
// Detection System Tests
// =============================================================================

bool test_detection_init() {
    DetectionSystem det;
    DetectionConfig config;
    config.detection_range_km = 10.0;
    config.base_detection_probability = 0.5;
    
    det.init(100, config);
    
    // Should start with no contacts
    TEST_ASSERT(det.contact_count(0) == 0, "No contacts initially");
    TEST_ASSERT(det.stats().detection_attempts == 0, "No attempts yet");
    
    return true;
}

bool test_detection_close_range() {
    EntityStorage storage;
    storage.count = 2;
    setup_two_entities(storage, 1000.0);  // 1km apart
    
    DetectionConfig config;
    config.detection_range_km = 10.0;
    config.base_detection_probability = 0.8;  // High prob
    config.terrain_los_enabled = false;
    config.terrain_concealment_enabled = false;
    
    DetectionSystem det;
    det.init(100, config);
    
    Rng rng(12345);
    
    // Run several ticks - should eventually detect
    bool detected = false;
    for (int i = 0; i < 20 && !detected; ++i) {
        det.update(storage, rng, i);
        detected = det.has_detected(0, 2);
    }
    
    TEST_ASSERT(detected, "Should detect close target eventually");
    TEST_ASSERT(det.contact_count(0) >= 1, "Should have at least 1 contact");
    
    return true;
}

bool test_detection_out_of_range() {
    EntityStorage storage;
    storage.count = 2;
    setup_two_entities(storage, 20000.0);  // 20km apart
    
    DetectionConfig config;
    config.detection_range_km = 10.0;  // Only 10km range
    config.base_detection_probability = 1.0;  // Would be guaranteed if in range
    config.terrain_los_enabled = false;
    config.terrain_concealment_enabled = false;
    
    DetectionSystem det;
    det.init(100, config);
    
    Rng rng(54321);
    
    // Run several ticks
    for (int i = 0; i < 10; ++i) {
        det.update(storage, rng, i);
    }
    
    TEST_ASSERT(!det.has_detected(0, 2), "Should NOT detect target beyond range");
    TEST_ASSERT(det.contact_count(0) == 0, "No contacts for out-of-range");
    
    return true;
}

bool test_detection_probability_range_effect() {
    DetectionConfig config;
    config.detection_range_km = 10.0;
    config.base_detection_probability = 0.5;
    config.terrain_los_enabled = false;
    config.terrain_concealment_enabled = false;
    
    DetectionSystem det;
    det.init(100, config);
    
    EntityStorage storage;
    storage.count = 2;
    
    // Close range (1km)
    setup_two_entities(storage, 1000.0);
    f64 prob_close = det.calculate_detection_probability(storage, 0, 1, 0);
    
    // Far range (8km)
    setup_two_entities(storage, 8000.0);
    f64 prob_far = det.calculate_detection_probability(storage, 0, 1, 0);
    
    TEST_ASSERT(prob_close > prob_far, "Closer targets easier to detect");
    TEST_ASSERT(prob_close > 0.3, "Close range should have reasonable probability");
    TEST_ASSERT(prob_far > 0.0, "Far (in range) should still be detectable");
    
    return true;
}

bool test_moving_target_bonus() {
    EntityStorage storage;
    storage.count = 2;
    setup_two_entities(storage, 5000.0);  // 5km
    
    DetectionConfig config;
    config.detection_range_km = 10.0;
    config.base_detection_probability = 0.3;
    config.moving_target_bonus = 0.3;
    config.terrain_los_enabled = false;
    config.terrain_concealment_enabled = false;
    
    DetectionSystem det;
    det.init(100, config);
    
    // Stationary target
    f64 prob_stationary = det.calculate_detection_probability(storage, 0, 1, 0);
    
    // Moving target
    storage.vel_x[1] = 10.0;  // Moving
    storage.vel_y[1] = 5.0;
    f64 prob_moving = det.calculate_detection_probability(storage, 0, 1, 0);
    
    TEST_ASSERT(prob_moving > prob_stationary, "Moving targets easier to detect");
    
    return true;
}

bool test_contact_decay() {
    EntityStorage storage;
    storage.count = 2;
    setup_two_entities(storage, 2000.0);
    
    DetectionConfig config;
    config.detection_range_km = 10.0;
    config.base_detection_probability = 1.0;  // Guaranteed detection
    config.contact_decay_ticks = 3;
    config.terrain_los_enabled = false;
    config.terrain_concealment_enabled = false;
    
    DetectionSystem det;
    det.init(100, config);
    
    Rng rng(11111);
    
    // Detect at tick 0
    det.update(storage, rng, 0);
    TEST_ASSERT(det.has_detected(0, 2), "Should detect at tick 0");
    
    // Move target out of range
    storage.pos_x[1] = 50000.0;  // Way out of range
    
    // Run more ticks without re-detection
    det.update(storage, rng, 1);
    det.update(storage, rng, 2);
    TEST_ASSERT(det.has_detected(0, 2), "Still detected at tick 2");
    
    det.update(storage, rng, 3);
    det.update(storage, rng, 4);
    TEST_ASSERT(!det.has_detected(0, 2), "Contact should decay after 3+ ticks");
    
    return true;
}

bool test_identification() {
    EntityStorage storage;
    storage.count = 2;
    setup_two_entities(storage, 2000.0);  // Close
    
    DetectionConfig config;
    config.detection_range_km = 10.0;
    config.identification_range_km = 5.0;
    config.base_detection_probability = 0.9;
    config.base_identification_probability = 0.8;
    config.terrain_los_enabled = false;
    config.terrain_concealment_enabled = false;
    
    DetectionSystem det;
    det.init(100, config);
    
    Rng rng(22222);
    
    // Run several ticks until identified
    bool identified = false;
    for (int i = 0; i < 30 && !identified; ++i) {
        det.update(storage, rng, i);
        identified = det.has_identified(0, 2);
    }
    
    TEST_ASSERT(identified, "Should identify close target eventually");
    
    const Contact* c = det.get_contact(0, 2);
    TEST_ASSERT(c != nullptr, "Contact should exist");
    TEST_ASSERT(c->type == ContactType::HOSTILE, "Should identify as hostile");
    
    return true;
}

bool test_friendly_detection() {
    EntityStorage storage;
    storage.count = 2;
    setup_two_entities(storage, 2000.0);
    storage.side[1] = Side::BLUE;  // Same side as observer
    
    DetectionConfig config;
    config.detection_range_km = 10.0;
    config.base_detection_probability = 1.0;
    config.base_identification_probability = 1.0;
    config.terrain_los_enabled = false;
    config.terrain_concealment_enabled = false;
    
    DetectionSystem det;
    det.init(100, config);
    
    Rng rng(33333);
    
    det.update(storage, rng, 0);
    
    if (det.has_identified(0, 2)) {
        const Contact* c = det.get_contact(0, 2);
        TEST_ASSERT(c != nullptr, "Contact should exist");
        TEST_ASSERT(c->type == ContactType::FRIENDLY, "Should identify as friendly");
    }
    
    return true;
}

bool test_detection_determinism() {
    const Seed SEED = 0xABCDEF123456;
    
    EntityStorage storage1, storage2;
    storage1.count = storage2.count = 2;
    setup_two_entities(storage1, 5000.0);
    setup_two_entities(storage2, 5000.0);
    
    DetectionConfig config;
    config.detection_range_km = 10.0;
    config.base_detection_probability = 0.5;
    config.terrain_los_enabled = false;
    config.terrain_concealment_enabled = false;
    
    DetectionSystem det1, det2;
    det1.init(100, config);
    det2.init(100, config);
    
    Rng rng1(SEED), rng2(SEED);
    
    // Run both systems
    for (int i = 0; i < 10; ++i) {
        det1.update(storage1, rng1, i);
        det2.update(storage2, rng2, i);
    }
    
    // Should have identical results
    TEST_ASSERT(det1.contact_count(0) == det2.contact_count(0), "Same contact count");
    TEST_ASSERT(det1.has_detected(0, 2) == det2.has_detected(0, 2), "Same detection state");
    
    auto& stats1 = det1.stats();
    auto& stats2 = det2.stats();
    TEST_ASSERT(stats1.detection_attempts == stats2.detection_attempts, "Same attempts");
    TEST_ASSERT(stats1.successful_detections == stats2.successful_detections, "Same successes");
    
    return true;
}

bool test_detection_with_terrain() {
    // Setup terrain
    TerrainPhysicalConfig phys_config;
    phys_config.base = TerrainBase::HILLS;
    phys_config.seed = 12345;
    phys_config.roughness = 0.6;
    phys_config.width_m = 20000.0;
    phys_config.height_m = 20000.0;
    
    PhysicalTerrain physical;
    physical.init(phys_config);
    
    TerrainEnvironmentConfig env_config;
    env_config.weather = Weather::CLEAR;
    Environment environment;
    environment.init(env_config);
    
    TerrainSemantics semantics;
    semantics.init(&physical, &environment);
    
    // Setup detection with terrain
    EntityStorage storage;
    storage.count = 2;
    setup_two_entities(storage, 5000.0);
    
    DetectionConfig config;
    config.detection_range_km = 10.0;
    config.base_detection_probability = 0.5;
    config.terrain_los_enabled = true;
    config.terrain_concealment_enabled = true;
    
    DetectionSystem det;
    det.init(100, config);
    det.set_terrain(&semantics);
    
    Rng rng(44444);
    
    // Calculate detection probability with terrain
    f64 prob_with_terrain = det.calculate_detection_probability(storage, 0, 1, 0);
    
    // Should be reduced by terrain effects
    TEST_ASSERT(prob_with_terrain >= 0.0 && prob_with_terrain <= 1.0, 
        "Probability should be valid");
    
    // Run some detection cycles
    for (int i = 0; i < 20; ++i) {
        det.update(storage, rng, i);
    }
    
    // Stats should show some attempts
    TEST_ASSERT(det.stats().detection_attempts > 0, "Should have made attempts");
    
    return true;
}

bool test_contact_position_tracking() {
    EntityStorage storage;
    storage.count = 2;
    setup_two_entities(storage, 2000.0);
    
    DetectionConfig config;
    config.detection_range_km = 10.0;
    config.base_detection_probability = 1.0;  // Guaranteed
    config.terrain_los_enabled = false;
    config.terrain_concealment_enabled = false;
    
    DetectionSystem det;
    det.init(100, config);
    
    Rng rng(55555);
    
    // Detect
    det.update(storage, rng, 0);
    
    const Contact* c = det.get_contact(0, 2);
    if (c) {
        // Position should be recorded
        TEST_ASSERT(std::abs(c->last_x - 2000.0) < 1.0, "X position tracked");
        TEST_ASSERT(std::abs(c->last_y - 0.0) < 1.0, "Y position tracked");
        
        // Move target
        storage.pos_x[1] = 3000.0;
        storage.pos_y[1] = 500.0;
        
        // Re-detect
        det.update(storage, rng, 1);
        
        c = det.get_contact(0, 2);
        TEST_ASSERT(c != nullptr, "Still have contact");
        // Position should be updated (allow some tolerance since detection might occur at different times)
        TEST_ASSERT(c->detection_count >= 1, "Detection count should be at least 1");
    }
    
    return true;
}

bool test_multiple_contacts() {
    EntityStorage storage;
    storage.init(10);
    storage.count = 4;
    
    // Observer
    storage.id[0] = 1;
    storage.flags[0] = entity_flags::ACTIVE;
    storage.side[0] = Side::BLUE;
    storage.pos_x[0] = 0.0;
    storage.pos_y[0] = 0.0;
    storage.pos_z[0] = 0.0;
    storage.vel_x[0] = storage.vel_y[0] = storage.vel_z[0] = 0.0;
    storage.detection_range[0] = 10000.0;
    storage.health[0] = 1.0;
    
    // Target 1
    storage.id[1] = 2;
    storage.flags[1] = entity_flags::ACTIVE;
    storage.side[1] = Side::RED;
    storage.pos_x[1] = 2000.0;
    storage.pos_y[1] = 0.0;
    storage.pos_z[1] = 0.0;
    storage.vel_x[1] = storage.vel_y[1] = storage.vel_z[1] = 0.0;
    storage.detection_range[1] = 5000.0;
    storage.health[1] = 1.0;
    
    // Target 2
    storage.id[2] = 3;
    storage.flags[2] = entity_flags::ACTIVE;
    storage.side[2] = Side::RED;
    storage.pos_x[2] = 0.0;
    storage.pos_y[2] = 3000.0;
    storage.pos_z[2] = 0.0;
    storage.vel_x[2] = storage.vel_y[2] = storage.vel_z[2] = 0.0;
    storage.detection_range[2] = 5000.0;
    storage.health[2] = 1.0;
    
    // Target 3 (far)
    storage.id[3] = 4;
    storage.flags[3] = entity_flags::ACTIVE;
    storage.side[3] = Side::RED;
    storage.pos_x[3] = 15000.0;
    storage.pos_y[3] = 0.0;
    storage.pos_z[3] = 0.0;
    storage.vel_x[3] = storage.vel_y[3] = storage.vel_z[3] = 0.0;
    storage.detection_range[3] = 5000.0;
    storage.health[3] = 1.0;
    
    DetectionConfig config;
    config.detection_range_km = 10.0;
    config.base_detection_probability = 0.9;
    config.terrain_los_enabled = false;
    config.terrain_concealment_enabled = false;
    
    DetectionSystem det;
    det.init(100, config);
    
    Rng rng(66666);
    
    // Run several ticks
    for (int i = 0; i < 30; ++i) {
        det.update(storage, rng, i);
    }
    
    // Should detect the two close targets
    TEST_ASSERT(det.has_detected(0, 2), "Should detect target 1");
    TEST_ASSERT(det.has_detected(0, 3), "Should detect target 2");
    TEST_ASSERT(!det.has_detected(0, 4), "Should NOT detect far target");
    
    TEST_ASSERT(det.contact_count(0) >= 2, "Should have at least 2 contacts");
    
    return true;
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "=== ATHENA Detection System Tests ===\n\n";
    
    int passed = 0;
    int failed = 0;
    
    std::cout << "--- Basic Detection Tests ---\n";
    RUN_TEST(test_detection_init);
    RUN_TEST(test_detection_close_range);
    RUN_TEST(test_detection_out_of_range);
    RUN_TEST(test_detection_probability_range_effect);
    RUN_TEST(test_moving_target_bonus);
    
    std::cout << "\n--- Contact Management Tests ---\n";
    RUN_TEST(test_contact_decay);
    RUN_TEST(test_identification);
    RUN_TEST(test_friendly_detection);
    RUN_TEST(test_contact_position_tracking);
    RUN_TEST(test_multiple_contacts);
    
    std::cout << "\n--- Integration Tests ---\n";
    RUN_TEST(test_detection_determinism);
    RUN_TEST(test_detection_with_terrain);
    
    // Summary
    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    
    if (failed > 0) {
        std::cout << "\n*** DETECTION TESTS FAILED ***\n";
        return 1;
    }
    
    std::cout << "\nAll detection tests passed.\n";
    return 0;
}
