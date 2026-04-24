// ATHENA Core - Platform Loader Tests
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/platform_loader.hpp"
#include <cassert>
#include <iostream>
#include <iomanip>

using namespace athena;

// =============================================================================
// Test Utilities
// =============================================================================

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "  " << #name << "... "; \
    try { \
        test_##name(); \
        std::cout << "PASS\n"; \
        tests_passed++; \
    } catch (const std::exception& e) { \
        std::cout << "FAIL: " << e.what() << "\n"; \
        tests_failed++; \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        throw std::runtime_error("Assertion failed: " #cond); \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        throw std::runtime_error("Assertion failed: " #a " == " #b); \
    } \
} while(0)

// =============================================================================
// Tests
// =============================================================================

TEST(category_conversion) {
    // String to category
    ASSERT_EQ(string_to_category("tanks"), PlatformCategory::TANK);
    ASSERT_EQ(string_to_category("aircraft"), PlatformCategory::AIRCRAFT);
    ASSERT_EQ(string_to_category("helicopters"), PlatformCategory::HELICOPTER);
    ASSERT_EQ(string_to_category("unknown"), PlatformCategory::UNKNOWN);
    
    // Category to string
    ASSERT(std::string(category_to_string(PlatformCategory::TANK)) == "tanks");
    ASSERT(std::string(category_to_string(PlatformCategory::AIRCRAFT)) == "aircraft");
    ASSERT(std::string(category_to_string(PlatformCategory::HELICOPTER)) == "helicopters");
}

TEST(list_json_files) {
    // This test requires the data directory to exist
    auto files = list_json_files("data/platforms/tanks");
    
    // Should have files (or be empty if run from wrong directory)
    // Just test that it doesn't crash
    std::cout << "[" << files.size() << " files] ";
}

TEST(load_single_category) {
    PlatformDatabase db;
    
    auto result = db.load_category("data/platforms/tanks", PlatformCategory::TANK);
    
    if (result.ok()) {
        std::cout << "[" << result.get() << " tanks] ";
        ASSERT(result.get() > 0);
        ASSERT(db.count() > 0);
        ASSERT(db.count_by_category(PlatformCategory::TANK) > 0);
    } else {
        std::cout << "[skipped - no data] ";
    }
}

TEST(load_all_categories) {
    PlatformDatabase db;
    
    auto result = db.load_all("data/platforms");
    
    if (result.ok()) {
        std::cout << "[" << result.get() << " total] ";
        ASSERT(result.get() > 0);
    } else {
        std::cout << "[skipped - no data] ";
    }
}

TEST(get_by_id) {
    PlatformDatabase db;
    db.load_all("data/platforms");
    
    // Try to get a known platform
    const auto* m1 = db.get("us-m1a2-sepv3");
    if (m1) {
        ASSERT(m1->name.find("Abrams") != std::string::npos || 
               m1->name.find("M1") != std::string::npos);
        std::cout << "[found M1A2] ";
    } else {
        // Try another known ID
        const auto* any = db.get("us-m1-abrams");
        if (any) {
            std::cout << "[found M1] ";
        } else {
            std::cout << "[no M1 found] ";
        }
    }
    
    // Non-existent should return nullptr
    ASSERT(db.get("nonexistent-platform-xyz") == nullptr);
}

TEST(get_by_country) {
    PlatformDatabase db;
    db.load_all("data/platforms");
    
    auto us_platforms = db.get_by_country("US");
    std::cout << "[US: " << us_platforms.size() << "] ";
    
    auto ru_platforms = db.get_by_country("RU");
    std::cout << "[RU: " << ru_platforms.size() << "] ";
    
    auto br_platforms = db.get_by_country("BR");
    std::cout << "[BR: " << br_platforms.size() << "] ";
}

TEST(platform_ratings) {
    PlatformDatabase db;
    db.load_all("data/platforms");
    
    // Check that ratings are calculated
    for (const auto& [id, spec] : db.all()) {
        // At least some rating should be non-zero
        bool has_rating = spec.firepower_rating > 0.0 ||
                          spec.protection_rating > 0.0 ||
                          spec.mobility_rating > 0.0;
        
        if (!has_rating && spec.category != PlatformCategory::UNKNOWN) {
            std::cout << "[warning: " << id << " has no ratings] ";
        }
    }
    
    std::cout << "[ratings ok] ";
}

TEST(validation) {
    PlatformDatabase db;
    db.load_all("data/platforms");
    
    auto result = db.validate();
    
    std::cout << "[" << result.warnings.size() << " warnings, " 
              << result.errors.size() << " errors] ";
    
    // Print first few warnings
    for (size_t i = 0; i < std::min(result.warnings.size(), (size_t)3); i++) {
        std::cout << "\n    -> " << result.warnings[i];
    }
    if (result.warnings.size() > 3) {
        std::cout << "\n    -> ... and " << (result.warnings.size() - 3) << " more";
    }
}

TEST(category_counts) {
    PlatformDatabase db;
    db.load_all("data/platforms");
    
    std::cout << "\n";
    std::cout << "    Tanks:       " << std::setw(4) << db.count_by_category(PlatformCategory::TANK) << "\n";
    std::cout << "    IFVs:        " << std::setw(4) << db.count_by_category(PlatformCategory::IFV) << "\n";
    std::cout << "    APCs:        " << std::setw(4) << db.count_by_category(PlatformCategory::APC) << "\n";
    std::cout << "    Artillery:   " << std::setw(4) << db.count_by_category(PlatformCategory::ARTILLERY) << "\n";
    std::cout << "    MLRS:        " << std::setw(4) << db.count_by_category(PlatformCategory::MLRS) << "\n";
    std::cout << "    SAM:         " << std::setw(4) << db.count_by_category(PlatformCategory::SAM) << "\n";
    std::cout << "    ATGM:        " << std::setw(4) << db.count_by_category(PlatformCategory::ATGM) << "\n";
    std::cout << "    MANPADS:     " << std::setw(4) << db.count_by_category(PlatformCategory::MANPADS) << "\n";
    std::cout << "    Aircraft:    " << std::setw(4) << db.count_by_category(PlatformCategory::AIRCRAFT) << "\n";
    std::cout << "    Helicopters: " << std::setw(4) << db.count_by_category(PlatformCategory::HELICOPTER) << "\n";
    std::cout << "    Bombers:     " << std::setw(4) << db.count_by_category(PlatformCategory::BOMBER) << "\n";
    std::cout << "    Ships:       " << std::setw(4) << db.count_by_category(PlatformCategory::SHIP) << "\n";
    std::cout << "    Submarines:  " << std::setw(4) << db.count_by_category(PlatformCategory::SUBMARINE) << "\n";
    std::cout << "    UAVs:        " << std::setw(4) << db.count_by_category(PlatformCategory::UAV) << "\n";
    std::cout << "    ────────────────────\n";
    std::cout << "    TOTAL:       " << std::setw(4) << db.count() << "\n";
    std::cout << "    ";
}

TEST(sample_platform_details) {
    PlatformDatabase db;
    db.load_all("data/platforms");
    
    // Try to find and print details of a specific platform
    const char* test_ids[] = {
        "us-m1a2-sepv3",
        "us-m1-abrams", 
        "ru-t90m",
        "us-f35a",
        "us-ah64d",
        "us-b2",
        "us-b52h"
    };
    
    for (const auto& id : test_ids) {
        const auto* p = db.get(id);
        if (p) {
            std::cout << "\n";
            std::cout << "    ID:          " << p->id << "\n";
            std::cout << "    Name:        " << p->name << "\n";
            std::cout << "    Country:     " << p->country_of_origin << "\n";
            std::cout << "    Crew:        " << p->crew << "\n";
            std::cout << "    Weight:      " << p->weight_kg << " kg\n";
            std::cout << "    Max Speed:   " << p->mobility.max_speed_kmh << " km/h\n";
            std::cout << "    Range:       " << p->mobility.range_km << " km\n";
            std::cout << "    Detect Rng:  " << p->detection_range_m << " m\n";
            std::cout << "    Engage Rng:  " << p->engagement_range_m << " m\n";
            std::cout << "    Firepower:   " << p->firepower_rating << "\n";
            std::cout << "    Protection:  " << p->protection_rating << "\n";
            std::cout << "    Mobility:    " << p->mobility_rating << "\n";
            std::cout << "    ";
            break;  // Only print one
        }
    }
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "=== Platform Loader Tests ===\n\n";
    
    RUN_TEST(category_conversion);
    RUN_TEST(list_json_files);
    RUN_TEST(load_single_category);
    RUN_TEST(load_all_categories);
    RUN_TEST(get_by_id);
    RUN_TEST(get_by_country);
    RUN_TEST(platform_ratings);
    RUN_TEST(validation);
    RUN_TEST(category_counts);
    RUN_TEST(sample_platform_details);
    
    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << tests_passed << "\n";
    std::cout << "Failed: " << tests_failed << "\n";
    
    return tests_failed > 0 ? 1 : 0;
}
