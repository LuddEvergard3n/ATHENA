// Test loading of new platform categories (v0.8.2)
// SPDX-License-Identifier: Proprietary

#include "athena/platform_loader.hpp"
#include <iostream>
#include <cassert>

using namespace athena;

void test_category_strings() {
    std::cout << "Testing category string conversions..." << std::endl;
    
    // Test all new categories
    assert(category_to_string(PlatformCategory::SMALL_ARMS) == std::string("small-arms"));
    assert(category_to_string(PlatformCategory::RADAR) == std::string("radars"));
    assert(category_to_string(PlatformCategory::EW_SYSTEM) == std::string("ew-systems"));
    assert(category_to_string(PlatformCategory::COUNTER_UAS) == std::string("counter-uas"));
    assert(category_to_string(PlatformCategory::COMMS) == std::string("comms"));
    assert(category_to_string(PlatformCategory::BODY_ARMOR) == std::string("body-armor"));
    assert(category_to_string(PlatformCategory::OPTICS) == std::string("optics"));
    assert(category_to_string(PlatformCategory::ENGINEERING) == std::string("engineering"));
    assert(category_to_string(PlatformCategory::REGIONAL) == std::string("regional"));
    
    // Test reverse mapping
    assert(string_to_category("small-arms") == PlatformCategory::SMALL_ARMS);
    assert(string_to_category("radars") == PlatformCategory::RADAR);
    assert(string_to_category("ew-systems") == PlatformCategory::EW_SYSTEM);
    assert(string_to_category("counter-uas") == PlatformCategory::COUNTER_UAS);
    assert(string_to_category("comms") == PlatformCategory::COMMS);
    assert(string_to_category("body-armor") == PlatformCategory::BODY_ARMOR);
    assert(string_to_category("optics") == PlatformCategory::OPTICS);
    assert(string_to_category("engineering") == PlatformCategory::ENGINEERING);
    assert(string_to_category("regional") == PlatformCategory::REGIONAL);
    
    std::cout << "  PASS: All category strings correct" << std::endl;
}

void test_load_all_categories(const std::string& data_path) {
    std::cout << "Testing full database load..." << std::endl;
    
    PlatformDatabase db;
    auto result = db.load_all(data_path);
    
    if (!result.ok()) {
        std::cerr << "  FAIL: " << result.error.message << std::endl;
        return;
    }
    
    usize total = result.get();
    std::cout << "  Loaded " << total << " platforms" << std::endl;
    
    // Check each category
    struct CatCheck {
        PlatformCategory cat;
        const char* name;
        usize min_expected;
    };
    
    CatCheck checks[] = {
        {PlatformCategory::TANK, "tanks", 80},
        {PlatformCategory::AIRCRAFT, "aircraft", 70},
        {PlatformCategory::SHIP, "ships", 60},
        {PlatformCategory::UAV, "uav", 40},
        {PlatformCategory::SMALL_ARMS, "small-arms", 80},
        {PlatformCategory::RADAR, "radars", 15},
        {PlatformCategory::EW_SYSTEM, "ew-systems", 15},
        {PlatformCategory::COUNTER_UAS, "counter-uas", 15},
        {PlatformCategory::OPTICS, "optics", 15},
        {PlatformCategory::BODY_ARMOR, "body-armor", 10},
        {PlatformCategory::COMMS, "comms", 10},
        {PlatformCategory::ENGINEERING, "engineering", 10},
        {PlatformCategory::REGIONAL, "regional", 40},
    };
    
    bool all_pass = true;
    for (const auto& check : checks) {
        usize count = db.count_by_category(check.cat);
        bool pass = count >= check.min_expected;
        std::cout << "  " << check.name << ": " << count 
                  << " (min: " << check.min_expected << ") " 
                  << (pass ? "PASS" : "FAIL") << std::endl;
        if (!pass) all_pass = false;
    }
    
    // Validate database
    auto validation = db.validate();
    if (!validation.valid) {
        std::cout << "  Validation errors:" << std::endl;
        for (const auto& err : validation.errors) {
            std::cout << "    ERROR: " << err << std::endl;
        }
        all_pass = false;
    }
    
    if (validation.warnings.size() > 0) {
        std::cout << "  Warnings: " << validation.warnings.size() << std::endl;
    }
    
    std::cout << (all_pass ? "  ALL PASS" : "  SOME FAILURES") << std::endl;
}

void test_specific_platforms(const std::string& data_path) {
    std::cout << "Testing specific platform lookups..." << std::endl;
    
    PlatformDatabase db;
    db.load_all(data_path);
    
    // Test small arms
    const auto* m4 = db.get("us-m4a1");
    if (m4) {
        std::cout << "  Found: " << m4->name << std::endl;
        if (m4->small_arms.has_value()) {
            std::cout << "    Caliber: " << m4->small_arms->caliber << std::endl;
            std::cout << "    ROF: " << m4->small_arms->rate_of_fire_rpm << " rpm" << std::endl;
        }
    } else {
        std::cout << "  Warning: us-m4a1 not found" << std::endl;
    }
    
    // Test radar
    const auto* spy6 = db.get("us-spy6");
    if (spy6) {
        std::cout << "  Found: " << spy6->name << std::endl;
        if (spy6->radar.has_value()) {
            std::cout << "    Range: " << spy6->radar->range_km << " km" << std::endl;
        }
    } else {
        std::cout << "  Warning: us-spy6 not found" << std::endl;
    }
    
    // Test C-UAS
    const auto* dd = db.get("il-drone-dome");
    if (dd) {
        std::cout << "  Found: " << dd->name << std::endl;
        if (dd->cuas.has_value()) {
            std::cout << "    Detection: " << dd->cuas->detection_range_km << " km" << std::endl;
        }
    } else {
        std::cout << "  Warning: il-drone-dome not found" << std::endl;
    }
    
    std::cout << "  Done" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "=== ATHENA Platform Loader Test (v0.8.2 categories) ===" << std::endl;
    
    std::string data_path = "data/platforms";
    if (argc > 1) {
        data_path = argv[1];
    }
    
    test_category_strings();
    test_load_all_categories(data_path);
    test_specific_platforms(data_path);
    
    std::cout << "=== Tests Complete ===" << std::endl;
    return 0;
}
