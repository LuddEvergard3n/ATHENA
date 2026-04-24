// ATHENA Core - Scenario Tests
// Contract: Scenario loading must be deterministic and correct.
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/types.hpp"
#include "athena/json.hpp"
#include "athena/scenario.hpp"
#include "athena/entities.hpp"
#include "athena/rng.hpp"

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
        if (fn()) { \
            std::cout << "PASS\n"; \
            passed++; \
        } else { \
            failed++; \
        } \
    } while(0)

// =============================================================================
// JSON Parser Tests
// =============================================================================

bool test_json_parse_object() {
    std::string json = R"({
        "name": "test",
        "value": 42,
        "enabled": true,
        "data": null
    })";
    
    auto result = json::parse(json);
    TEST_ASSERT(result.ok(), "Parse should succeed");
    
    const auto& v = result.get();
    TEST_ASSERT(v.is_object(), "Root should be object");
    TEST_ASSERT(v["name"].is_string(), "name should be string");
    TEST_ASSERT(v["name"].as_string() == "test", "name value");
    TEST_ASSERT(v["value"].is_number(), "value should be number");
    TEST_ASSERT(v["value"].as_number() == 42.0, "value should be 42");
    TEST_ASSERT(v["enabled"].is_bool(), "enabled should be bool");
    TEST_ASSERT(v["enabled"].as_bool() == true, "enabled value");
    TEST_ASSERT(v["data"].is_null(), "data should be null");
    
    return true;
}

bool test_json_parse_array() {
    std::string json = R"([1, 2, 3, "four", true, null])";
    
    auto result = json::parse(json);
    TEST_ASSERT(result.ok(), "Parse should succeed");
    
    const auto& v = result.get();
    TEST_ASSERT(v.is_array(), "Root should be array");
    TEST_ASSERT(v.size() == 6, "Array size");
    TEST_ASSERT(v[0].as_number() == 1.0, "First element");
    TEST_ASSERT(v[3].as_string() == "four", "Fourth element");
    TEST_ASSERT(v[4].as_bool() == true, "Fifth element");
    TEST_ASSERT(v[5].is_null(), "Sixth element");
    
    return true;
}

bool test_json_parse_nested() {
    std::string json = R"({
        "level1": {
            "level2": {
                "value": 123
            }
        }
    })";
    
    auto result = json::parse(json);
    TEST_ASSERT(result.ok(), "Parse should succeed");
    
    const auto& v = result.get();
    TEST_ASSERT(v["level1"]["level2"]["value"].as_number() == 123.0, "Nested value");
    
    return true;
}

bool test_json_parse_escapes() {
    std::string json = R"({"text": "line1\nline2\ttab\"quote\\"})";
    
    auto result = json::parse(json);
    TEST_ASSERT(result.ok(), "Parse should succeed");
    
    const auto& v = result.get();
    std::string expected = "line1\nline2\ttab\"quote\\";
    TEST_ASSERT(v["text"].as_string() == expected, "Escaped string");
    
    return true;
}

bool test_json_parse_numbers() {
    std::string json = R"({
        "int": 42,
        "neg": -17,
        "float": 3.14159,
        "exp": 1.5e10,
        "neg_exp": 2.5e-3
    })";
    
    auto result = json::parse(json);
    TEST_ASSERT(result.ok(), "Parse should succeed");
    
    const auto& v = result.get();
    TEST_ASSERT(v["int"].as_number() == 42.0, "Integer");
    TEST_ASSERT(v["neg"].as_number() == -17.0, "Negative");
    TEST_ASSERT(std::abs(v["float"].as_number() - 3.14159) < 1e-10, "Float");
    TEST_ASSERT(v["exp"].as_number() == 1.5e10, "Exponent");
    TEST_ASSERT(std::abs(v["neg_exp"].as_number() - 2.5e-3) < 1e-10, "Neg exponent");
    
    return true;
}

bool test_json_serialize_roundtrip() {
    std::string original = R"({"a":1,"b":"two","c":[1,2,3],"d":{"nested":true}})";
    
    auto result = json::parse(original);
    TEST_ASSERT(result.ok(), "Parse should succeed");
    
    std::string serialized = json::to_string(result.get());
    
    auto result2 = json::parse(serialized);
    TEST_ASSERT(result2.ok(), "Re-parse should succeed");
    
    // Values should match
    const auto& v1 = result.get();
    const auto& v2 = result2.get();
    
    TEST_ASSERT(v1["a"].as_number() == v2["a"].as_number(), "a value");
    TEST_ASSERT(v1["b"].as_string() == v2["b"].as_string(), "b value");
    TEST_ASSERT(v1["c"].size() == v2["c"].size(), "c size");
    TEST_ASSERT(v1["d"]["nested"].as_bool() == v2["d"]["nested"].as_bool(), "nested value");
    
    return true;
}

bool test_json_error_handling() {
    // Missing closing brace
    auto r1 = json::parse(R"({"unclosed": true)");
    TEST_ASSERT(!r1.ok(), "Should fail on unclosed object");
    
    // Invalid number
    auto r2 = json::parse(R"({"bad": 1.2.3})");
    TEST_ASSERT(!r2.ok(), "Should fail on invalid number");
    
    // Trailing comma
    auto r3 = json::parse(R"([1, 2, 3,])");
    TEST_ASSERT(!r3.ok(), "Should fail on trailing comma");
    
    return true;
}

// =============================================================================
// Scenario Tests
// =============================================================================

bool test_scenario_minimal() {
    std::string json = R"({
        "id": "test-scenario-001",
        "name": "Minimal Test Scenario"
    })";
    
    auto json_result = json::parse(json);
    TEST_ASSERT(json_result.ok(), "JSON parse");
    
    ScenarioLoader loader;
    auto result = loader.load(json_result.get());
    TEST_ASSERT(result.ok(), "Scenario load: " + loader.last_error());
    
    const auto& scenario = result.get();
    TEST_ASSERT(scenario.id == "test-scenario-001", "Scenario ID");
    TEST_ASSERT(scenario.name == "Minimal Test Scenario", "Scenario name");
    TEST_ASSERT(scenario.is_valid, "Should be valid");
    
    return true;
}

bool test_scenario_with_actors() {
    std::string json = R"({
        "id": "scenario-002",
        "name": "Scenario with Actors",
        "scale": "meso",
        "spatial_bounds": {
            "min_lat": 48.0,
            "max_lat": 52.0,
            "min_lon": 10.0,
            "max_lon": 15.0
        },
        "actors": [
            {
                "id": "blue-001",
                "name": "Alpha Company",
                "type": "unit",
                "side": "blue",
                "position": {"lat": 50.0, "lon": 12.0, "alt": 0},
                "mobility": {
                    "max_speed_kmh": 60,
                    "cruise_speed_kmh": 40,
                    "movement_type": "ground"
                },
                "firepower": {
                    "base_firepower": 10.0,
                    "range_km": 5.0,
                    "accuracy": 0.7
                },
                "health": 1.0,
                "supply": 0.8
            },
            {
                "id": "red-001",
                "name": "Enemy Battalion",
                "type": "formation",
                "side": "red",
                "position": {"lat": 51.0, "lon": 13.0, "alt": 100},
                "health": 0.9,
                "supply": 1.0
            }
        ]
    })";
    
    auto json_result = json::parse(json);
    TEST_ASSERT(json_result.ok(), "JSON parse");
    
    ScenarioLoader loader;
    auto result = loader.load(json_result.get());
    TEST_ASSERT(result.ok(), "Scenario load: " + loader.last_error());
    
    const auto& scenario = result.get();
    TEST_ASSERT(scenario.actors.size() == 2, "Actor count");
    TEST_ASSERT(scenario.actors[0].id == "blue-001", "First actor ID");
    TEST_ASSERT(scenario.actors[0].side == "blue", "First actor side");
    TEST_ASSERT(scenario.actors[0].mobility.has_value(), "First actor has mobility");
    TEST_ASSERT(scenario.actors[0].firepower.has_value(), "First actor has firepower");
    TEST_ASSERT(scenario.actors[1].side == "red", "Second actor side");
    
    return true;
}

bool test_scenario_validation_out_of_bounds() {
    std::string json = R"({
        "id": "scenario-003",
        "name": "Out of Bounds Test",
        "spatial_bounds": {
            "min_lat": 48.0,
            "max_lat": 50.0,
            "min_lon": 10.0,
            "max_lon": 12.0
        },
        "actors": [
            {
                "id": "actor-001",
                "position": {"lat": 55.0, "lon": 11.0, "alt": 0}
            }
        ]
    })";
    
    auto json_result = json::parse(json);
    TEST_ASSERT(json_result.ok(), "JSON parse");
    
    ScenarioLoader loader;
    auto result = loader.load(json_result.get());
    TEST_ASSERT(!result.ok(), "Should fail validation - actor out of bounds");
    
    return true;
}

bool test_scenario_validation_invalid_state() {
    std::string json = R"({
        "id": "scenario-004",
        "name": "Invalid State Test",
        "actors": [
            {
                "id": "actor-001",
                "position": {"lat": 0.0, "lon": 0.0, "alt": 0},
                "health": 1.5
            }
        ]
    })";
    
    auto json_result = json::parse(json);
    TEST_ASSERT(json_result.ok(), "JSON parse");
    
    ScenarioLoader loader;
    auto result = loader.load(json_result.get());
    TEST_ASSERT(!result.ok(), "Should fail validation - health > 1.0");
    
    return true;
}

bool test_scenario_conversion_determinism() {
    std::string json = R"({
        "id": "scenario-005",
        "name": "Conversion Test",
        "spatial_bounds": {
            "min_lat": 49.0,
            "max_lat": 51.0,
            "min_lon": 11.0,
            "max_lon": 13.0
        },
        "actors": [
            {"id": "a1", "side": "blue", "position": {"lat": 50.0, "lon": 12.0, "alt": 0}},
            {"id": "a2", "side": "red", "position": {"lat": 50.5, "lon": 12.5, "alt": 100}},
            {"id": "a3", "side": "neutral", "position": {"lat": 49.5, "lon": 11.5, "alt": 50}}
        ]
    })";
    
    auto json_result = json::parse(json);
    TEST_ASSERT(json_result.ok(), "JSON parse");
    
    ScenarioLoader loader;
    auto scenario_result = loader.load(json_result.get());
    TEST_ASSERT(scenario_result.ok(), "Scenario load");
    
    const auto& scenario = scenario_result.get();
    
    // First conversion
    EntityManager entities1;
    entities1.init(100);
    Rng rng1(12345, 0);
    ScenarioConverter conv1;
    auto count1 = conv1.convert(scenario, entities1, rng1);
    TEST_ASSERT(count1.ok(), "First conversion");
    TEST_ASSERT(*count1 == 3, "First conversion count");
    
    // Second conversion (same seed)
    EntityManager entities2;
    entities2.init(100);
    Rng rng2(12345, 0);
    ScenarioConverter conv2;
    auto count2 = conv2.convert(scenario, entities2, rng2);
    TEST_ASSERT(count2.ok(), "Second conversion");
    TEST_ASSERT(*count2 == 3, "Second conversion count");
    
    // Compare positions (should be identical)
    const auto& s1 = entities1.storage();
    const auto& s2 = entities2.storage();
    
    for (usize i = 0; i < s1.count; ++i) {
        TEST_ASSERT(s1.pos_x[i] == s2.pos_x[i], "Position X determinism");
        TEST_ASSERT(s1.pos_y[i] == s2.pos_y[i], "Position Y determinism");
        TEST_ASSERT(s1.pos_z[i] == s2.pos_z[i], "Position Z determinism");
        TEST_ASSERT(s1.side[i] == s2.side[i], "Side determinism");
    }
    
    return true;
}

bool test_scenario_with_parameters() {
    std::string json = R"({
        "id": "scenario-006",
        "name": "Parameters Test",
        "parameters": {
            "attrition_rate": {
                "value": 0.1,
                "uncertainty": {
                    "type": "uniform",
                    "min": 0.05,
                    "max": 0.15
                },
                "sensitivity": true,
                "source": "Historical data",
                "confidence": 0.8
            },
            "detection_modifier": {
                "value": 1.0,
                "uncertainty": {
                    "type": "normal",
                    "mean": 1.0,
                    "stddev": 0.1
                }
            }
        }
    })";
    
    auto json_result = json::parse(json);
    TEST_ASSERT(json_result.ok(), "JSON parse");
    
    ScenarioLoader loader;
    auto result = loader.load(json_result.get());
    TEST_ASSERT(result.ok(), "Scenario load: " + loader.last_error());
    
    const auto& scenario = result.get();
    TEST_ASSERT(scenario.parameters.size() == 2, "Parameter count");
    
    auto it = scenario.parameters.find("attrition_rate");
    TEST_ASSERT(it != scenario.parameters.end(), "attrition_rate exists");
    TEST_ASSERT(it->second.base_value == 0.1, "attrition_rate value");
    TEST_ASSERT(it->second.sensitivity_flag == true, "attrition_rate sensitivity");
    TEST_ASSERT(it->second.uncertainty.has_value(), "attrition_rate has uncertainty");
    TEST_ASSERT(it->second.uncertainty->type == DistributionType::UNIFORM, "uncertainty type");
    
    return true;
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "=== ATHENA Scenario Tests ===\n\n";
    
    int passed = 0;
    int failed = 0;
    
    // JSON tests
    std::cout << "--- JSON Parser Tests ---\n";
    RUN_TEST(test_json_parse_object);
    RUN_TEST(test_json_parse_array);
    RUN_TEST(test_json_parse_nested);
    RUN_TEST(test_json_parse_escapes);
    RUN_TEST(test_json_parse_numbers);
    RUN_TEST(test_json_serialize_roundtrip);
    RUN_TEST(test_json_error_handling);
    
    // Scenario tests
    std::cout << "\n--- Scenario Tests ---\n";
    RUN_TEST(test_scenario_minimal);
    RUN_TEST(test_scenario_with_actors);
    RUN_TEST(test_scenario_validation_out_of_bounds);
    RUN_TEST(test_scenario_validation_invalid_state);
    RUN_TEST(test_scenario_conversion_determinism);
    RUN_TEST(test_scenario_with_parameters);
    
    // Summary
    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    
    if (failed > 0) {
        std::cout << "\n*** TESTS FAILED ***\n";
        return 1;
    }
    
    std::cout << "\nAll tests passed.\n";
    return 0;
}
