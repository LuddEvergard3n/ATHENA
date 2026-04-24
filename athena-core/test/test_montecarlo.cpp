// ATHENA Core - Monte Carlo Tests
// Contract: Deterministic Monte Carlo execution.
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/types.hpp"
#include "athena/json.hpp"
#include "athena/scenario.hpp"
#include "athena/analysis.hpp"

#include <iostream>
#include <cmath>

using namespace athena;
using namespace athena::analysis;

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
// Test Scenario
// =============================================================================

static const char* TEST_SCENARIO_JSON = R"({
    "id": "mc-test-scenario",
    "name": "Monte Carlo Test",
    "scale": "meso",
    "temporal": {
        "tick_duration_seconds": 3600,
        "max_ticks": 24
    },
    "spatial_bounds": {
        "min_lat": 49.9,
        "max_lat": 50.1,
        "min_lon": 11.9,
        "max_lon": 12.1
    },
    "actors": [
        {
            "id": "blue-1",
            "side": "blue",
            "position": {"lat": 50.0, "lon": 12.0, "alt": 0},
            "firepower": {"base_firepower": 10, "range_km": 50},
            "health": 1.0,
            "supply": 1.0
        },
        {
            "id": "blue-2",
            "side": "blue",
            "position": {"lat": 50.001, "lon": 12.001, "alt": 0},
            "firepower": {"base_firepower": 8, "range_km": 50},
            "health": 1.0
        },
        {
            "id": "red-1",
            "side": "red",
            "position": {"lat": 50.002, "lon": 12.002, "alt": 0},
            "firepower": {"base_firepower": 12, "range_km": 50},
            "health": 1.0
        },
        {
            "id": "red-2",
            "side": "red",
            "position": {"lat": 50.003, "lon": 12.003, "alt": 0},
            "firepower": {"base_firepower": 9, "range_km": 50},
            "health": 1.0
        }
    ],
    "parameters": {
        "attrition_coefficient": {
            "value": 0.15,
            "uncertainty": {
                "type": "uniform",
                "min": 0.10,
                "max": 0.20
            },
            "sensitivity": true
        }
    }
})";

// =============================================================================
// Tests
// =============================================================================

bool test_monte_carlo_determinism() {
    // Parse scenario
    auto json_result = json::parse(TEST_SCENARIO_JSON);
    TEST_ASSERT(json_result.ok(), "JSON parse");
    
    ScenarioLoader loader;
    auto scenario_result = loader.load(json_result.get());
    TEST_ASSERT(scenario_result.ok(), "Scenario load");
    
    const auto& scenario = scenario_result.get();
    
    // First batch
    BatchConfig config1;
    config1.master_seed = 0xDEADBEEF;
    config1.num_iterations = 10;
    config1.max_ticks_per_iteration = 24;
    config1.entity_capacity = 100;
    
    MonteCarloExecutor exec1;
    exec1.configure(config1);
    exec1.set_scenario(scenario);
    
    auto status1 = exec1.execute();
    TEST_ASSERT(status1.ok(), "Batch 1 execute");
    TEST_ASSERT(exec1.results().size() == 10, "Batch 1 results count");
    
    // Second batch (same config)
    MonteCarloExecutor exec2;
    exec2.configure(config1);
    exec2.set_scenario(scenario);
    
    auto status2 = exec2.execute();
    TEST_ASSERT(status2.ok(), "Batch 2 execute");
    
    // Compare results
    const auto& r1 = exec1.results();
    const auto& r2 = exec2.results();
    
    TEST_ASSERT(r1.size() == r2.size(), "Result count match");
    
    for (usize i = 0; i < r1.size(); ++i) {
        TEST_ASSERT(r1[i].seed == r2[i].seed, "Seed match");
        TEST_ASSERT(r1[i].ticks_executed == r2[i].ticks_executed, 
            "Ticks match for iteration " + std::to_string(i));
        TEST_ASSERT(r1[i].blue_surviving == r2[i].blue_surviving,
            "Blue surviving match");
        TEST_ASSERT(r1[i].red_surviving == r2[i].red_surviving,
            "Red surviving match");
        TEST_ASSERT(r1[i].blue_total_health == r2[i].blue_total_health,
            "Blue health match");
        TEST_ASSERT(r1[i].red_total_health == r2[i].red_total_health,
            "Red health match");
    }
    
    return true;
}

bool test_monte_carlo_different_seeds() {
    auto json_result = json::parse(TEST_SCENARIO_JSON);
    TEST_ASSERT(json_result.ok(), "JSON parse");
    
    ScenarioLoader loader;
    auto scenario_result = loader.load(json_result.get());
    TEST_ASSERT(scenario_result.ok(), "Scenario load");
    
    const auto& scenario = scenario_result.get();
    
    // Batch with seed A
    BatchConfig config_a;
    config_a.master_seed = 12345;
    config_a.num_iterations = 5;
    config_a.max_ticks_per_iteration = 24;
    config_a.entity_capacity = 100;
    
    MonteCarloExecutor exec_a;
    exec_a.configure(config_a);
    exec_a.set_scenario(scenario);
    exec_a.execute();
    
    // Batch with seed B
    BatchConfig config_b;
    config_b.master_seed = 54321;
    config_b.num_iterations = 5;
    config_b.max_ticks_per_iteration = 24;
    config_b.entity_capacity = 100;
    
    MonteCarloExecutor exec_b;
    exec_b.configure(config_b);
    exec_b.set_scenario(scenario);
    exec_b.execute();
    
    // Results should differ (health values at minimum should vary due to RNG)
    const auto& ra = exec_a.results();
    const auto& rb = exec_b.results();
    
    bool found_difference = false;
    for (usize i = 0; i < ra.size() && !found_difference; ++i) {
        if (ra[i].blue_surviving != rb[i].blue_surviving ||
            ra[i].red_surviving != rb[i].red_surviving ||
            ra[i].blue_total_health != rb[i].blue_total_health ||
            ra[i].red_total_health != rb[i].red_total_health) {
            found_difference = true;
        }
    }
    
    // If no combat happened, results might be identical - that's also valid
    // The key is that with same seed, results ARE identical (tested elsewhere)
    // Different seeds can produce same results by chance, so this test is lenient
    if (!found_difference) {
        // Check if any combat happened at all
        bool any_damage = false;
        for (const auto& r : ra) {
            if (r.blue_total_health < 2.0 || r.red_total_health < 2.0) {
                any_damage = true;
                break;
            }
        }
        // If combat happened but results are identical, that's suspicious
        // But if no combat, identical results are expected
        TEST_ASSERT(!any_damage || found_difference, 
            "Different seeds with combat should produce different results");
    }
    
    return true;
}

bool test_monte_carlo_statistics() {
    auto json_result = json::parse(TEST_SCENARIO_JSON);
    TEST_ASSERT(json_result.ok(), "JSON parse");
    
    ScenarioLoader loader;
    auto scenario_result = loader.load(json_result.get());
    TEST_ASSERT(scenario_result.ok(), "Scenario load");
    
    const auto& scenario = scenario_result.get();
    
    BatchConfig config;
    config.master_seed = 0xCAFEBABE;
    config.num_iterations = 20;
    config.max_ticks_per_iteration = 48;
    config.entity_capacity = 100;
    
    MonteCarloExecutor exec;
    exec.configure(config);
    exec.set_scenario(scenario);
    exec.execute();
    
    // Compute statistics
    auto stats = exec.compute_statistics();
    
    TEST_ASSERT(stats.total_iterations == 20, "Total iterations");
    TEST_ASSERT(stats.completed_iterations > 0, "Some iterations completed");
    TEST_ASSERT(stats.blue_wins + stats.red_wins + stats.draws == 
                stats.completed_iterations, "Outcome sum");
    
    // Check metric stats are reasonable
    TEST_ASSERT(stats.ticks_to_completion.min <= stats.ticks_to_completion.max,
        "Ticks min <= max");
    TEST_ASSERT(stats.ticks_to_completion.p5 <= stats.ticks_to_completion.p95,
        "Ticks p5 <= p95");
    
    return true;
}

bool test_monte_carlo_json_export() {
    auto json_result = json::parse(TEST_SCENARIO_JSON);
    TEST_ASSERT(json_result.ok(), "JSON parse");
    
    ScenarioLoader loader;
    auto scenario_result = loader.load(json_result.get());
    TEST_ASSERT(scenario_result.ok(), "Scenario load");
    
    const auto& scenario = scenario_result.get();
    
    BatchConfig config;
    config.master_seed = 0x12345678;
    config.num_iterations = 5;
    config.max_ticks_per_iteration = 24;
    config.entity_capacity = 100;
    
    MonteCarloExecutor exec;
    exec.configure(config);
    exec.set_scenario(scenario);
    exec.execute();
    
    // Export to JSON
    std::string json_str = exec.export_json();
    
    TEST_ASSERT(!json_str.empty(), "JSON not empty");
    TEST_ASSERT(json_str.find("\"results\"") != std::string::npos, "Has results");
    TEST_ASSERT(json_str.find("\"statistics\"") != std::string::npos, "Has statistics");
    
    // Parse and verify
    auto parsed = json::parse(json_str);
    TEST_ASSERT(parsed.ok(), "Exported JSON is valid");
    
    const auto& root = parsed.get();
    TEST_ASSERT(root["results"].is_array(), "Results is array");
    TEST_ASSERT(root["results"].size() == 5, "Results count");
    
    return true;
}

bool test_distribution_sampler() {
    Rng rng(42, 0);
    
    // Point distribution
    Distribution point = Distribution::point(5.0);
    for (int i = 0; i < 10; ++i) {
        f64 v = DistributionSampler::sample(point, rng);
        TEST_ASSERT(v == 5.0, "Point distribution always returns value");
    }
    
    // Uniform distribution
    Distribution uniform = Distribution::uniform(0.0, 10.0);
    for (int i = 0; i < 100; ++i) {
        f64 v = DistributionSampler::sample(uniform, rng);
        TEST_ASSERT(v >= 0.0 && v <= 10.0, "Uniform in range");
    }
    
    // Normal distribution - check mean is approximately correct
    Distribution normal = Distribution::normal(100.0, 10.0);
    f64 sum = 0.0;
    for (int i = 0; i < 1000; ++i) {
        sum += DistributionSampler::sample(normal, rng);
    }
    f64 mean = sum / 1000.0;
    TEST_ASSERT(std::abs(mean - 100.0) < 5.0, "Normal mean approximately correct");
    
    return true;
}

bool test_parameter_override() {
    auto json_result = json::parse(TEST_SCENARIO_JSON);
    TEST_ASSERT(json_result.ok(), "JSON parse");
    
    ScenarioLoader loader;
    auto scenario_result = loader.load(json_result.get());
    TEST_ASSERT(scenario_result.ok(), "Scenario load");
    
    const auto& scenario = scenario_result.get();
    
    // Run with high attrition
    BatchConfig config;
    config.master_seed = 0xAAAABBBB;
    config.num_iterations = 10;
    config.max_ticks_per_iteration = 24;
    config.entity_capacity = 100;
    
    MonteCarloExecutor exec_high;
    exec_high.configure(config);
    exec_high.set_scenario(scenario);
    exec_high.set_parameter("attrition_coefficient", 0.3);  // High attrition
    exec_high.execute();
    
    // Run with low attrition
    MonteCarloExecutor exec_low;
    exec_low.configure(config);
    exec_low.set_scenario(scenario);
    exec_low.set_parameter("attrition_coefficient", 0.05);  // Low attrition
    exec_low.execute();
    
    // High attrition should result in more damage dealt
    f64 high_damage = 0.0, low_damage = 0.0;
    for (const auto& r : exec_high.results()) {
        high_damage += (2.0 - r.blue_total_health) + (2.0 - r.red_total_health);
    }
    for (const auto& r : exec_low.results()) {
        low_damage += (2.0 - r.blue_total_health) + (2.0 - r.red_total_health);
    }
    
    // High attrition should result in more total damage
    // Note: if no combat occurs (entities out of range), both will be zero
    TEST_ASSERT(high_damage >= low_damage,
        "High attrition should result in >= damage dealt");
    
    return true;
}

bool test_progress_callback() {
    auto json_result = json::parse(TEST_SCENARIO_JSON);
    TEST_ASSERT(json_result.ok(), "JSON parse");
    
    ScenarioLoader loader;
    auto scenario_result = loader.load(json_result.get());
    TEST_ASSERT(scenario_result.ok(), "Scenario load");
    
    const auto& scenario = scenario_result.get();
    
    int callback_count = 0;
    u32 last_completed = 0;
    
    BatchConfig config;
    config.master_seed = 0x11111111;
    config.num_iterations = 10;
    config.max_ticks_per_iteration = 12;
    config.entity_capacity = 100;
    config.progress_callback = [&](u32 completed, u32 total) {
        callback_count++;
        if (completed > 0) last_completed = completed;
        (void)total;  // Suppress unused warning
    };
    
    MonteCarloExecutor exec;
    exec.configure(config);
    exec.set_scenario(scenario);
    exec.execute();
    
    TEST_ASSERT(callback_count == 10, "Callback called for each iteration");
    TEST_ASSERT(last_completed == 10, "Final progress is complete");
    
    return true;
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "=== ATHENA Monte Carlo Tests ===\n\n";
    
    int passed = 0;
    int failed = 0;
    
    std::cout << "--- Core Monte Carlo Tests ---\n";
    RUN_TEST(test_monte_carlo_determinism);
    RUN_TEST(test_monte_carlo_different_seeds);
    RUN_TEST(test_monte_carlo_statistics);
    RUN_TEST(test_monte_carlo_json_export);
    
    std::cout << "\n--- Distribution Tests ---\n";
    RUN_TEST(test_distribution_sampler);
    
    std::cout << "\n--- Parameter Tests ---\n";
    RUN_TEST(test_parameter_override);
    RUN_TEST(test_progress_callback);
    
    // Summary
    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    
    if (failed > 0) {
        std::cout << "\n*** MONTE CARLO TESTS FAILED ***\n";
        return 1;
    }
    
    std::cout << "\nAll Monte Carlo tests passed.\n";
    return 0;
}
