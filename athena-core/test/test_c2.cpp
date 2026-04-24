// ATHENA Core - C2 System Tests
// Contract: Verify command hierarchy, orders, and communication.
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/systems/c2.hpp"
#include "athena/entities.hpp"
#include "athena/rng.hpp"

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
// Helper: Setup entities
// =============================================================================

void setup_hierarchy(EntityStorage& storage) {
    storage.init(10);
    storage.count = 4;
    
    // Entity 0: Commander (HQ)
    storage.id[0] = 100;
    storage.flags[0] = entity_flags::ACTIVE;
    storage.side[0] = Side::BLUE;
    storage.pos_x[0] = 0.0;
    storage.pos_y[0] = 0.0;
    storage.pos_z[0] = 0.0;
    storage.health[0] = 1.0;
    
    // Entity 1: Sub-commander (Battalion)
    storage.id[1] = 101;
    storage.flags[1] = entity_flags::ACTIVE;
    storage.side[1] = Side::BLUE;
    storage.pos_x[1] = 5000.0;
    storage.pos_y[1] = 0.0;
    storage.pos_z[1] = 0.0;
    storage.health[1] = 1.0;
    
    // Entity 2: Unit A (Company)
    storage.id[2] = 102;
    storage.flags[2] = entity_flags::ACTIVE;
    storage.side[2] = Side::BLUE;
    storage.pos_x[2] = 10000.0;
    storage.pos_y[2] = 0.0;
    storage.pos_z[2] = 0.0;
    storage.health[2] = 1.0;
    
    // Entity 3: Unit B (Company)
    storage.id[3] = 103;
    storage.flags[3] = entity_flags::ACTIVE;
    storage.side[3] = Side::BLUE;
    storage.pos_x[3] = 10000.0;
    storage.pos_y[3] = 5000.0;
    storage.pos_z[3] = 0.0;
    storage.health[3] = 1.0;
}

// =============================================================================
// C2 System Tests
// =============================================================================

bool test_c2_init() {
    C2System c2;
    C2Config config;
    
    c2.init(100, config);
    
    TEST_ASSERT(c2.stats().orders_issued == 0, "No orders initially");
    TEST_ASSERT(c2.stats().units_autonomous == 0, "No autonomous units");
    
    return true;
}

bool test_hierarchy_setup() {
    C2System c2;
    C2Config config;
    c2.init(100, config);
    
    // HQ -> Battalion -> Companies
    c2.set_commander(101, 100);  // Battalion under HQ
    c2.set_commander(102, 101);  // Company A under Battalion
    c2.set_commander(103, 101);  // Company B under Battalion
    
    TEST_ASSERT(c2.get_commander(101) == 100, "Battalion commander is HQ");
    TEST_ASSERT(c2.get_commander(102) == 101, "Company A commander is Battalion");
    TEST_ASSERT(c2.get_commander(103) == 101, "Company B commander is Battalion");
    
    auto subs = c2.get_subordinates(101);
    TEST_ASSERT(subs.size() == 2, "Battalion has 2 subordinates");
    
    return true;
}

bool test_echelon_levels() {
    C2System c2;
    C2Config config;
    c2.init(100, config);
    
    // Build hierarchy
    c2.set_commander(101, 100);
    c2.set_commander(102, 101);
    c2.set_commander(103, 101);
    
    // HQ has no commander, so echelon is 0
    TEST_ASSERT(c2.get_echelon_level(100) == 0, "HQ is echelon 0");
    
    // Battalion reports to HQ, so echelon is 1
    TEST_ASSERT(c2.get_echelon_level(101) == 1, "Battalion is echelon 1");
    
    // Companies report to Battalion, so echelon is 2
    TEST_ASSERT(c2.get_echelon_level(102) == 2, "Company is echelon 2");
    TEST_ASSERT(c2.get_echelon_level(103) == 2, "Company is echelon 2");
    
    return true;
}

bool test_issue_order() {
    C2System c2;
    C2Config config;
    config.base_command_delay_ticks = 1.0;
    c2.init(100, config);
    
    c2.set_commander(101, 100);
    
    Order order;
    order.type = OrderType::MOVE;
    order.target_x = 5000.0;
    order.target_y = 5000.0;
    order.priority = 1;
    
    bool success = c2.issue_order(100, 101, order, 0);
    TEST_ASSERT(success, "Order should be issued successfully");
    TEST_ASSERT(c2.stats().orders_issued == 1, "One order issued");
    TEST_ASSERT(c2.has_pending_orders(101), "Order should be pending");
    
    return true;
}

bool test_order_delivery_delay() {
    EntityStorage storage;
    setup_hierarchy(storage);
    
    C2System c2;
    C2Config config;
    config.base_command_delay_ticks = 1.0;  // 1 tick delay
    config.base_comm_reliability = 1.0;      // Perfect comms
    config.delay_per_echelon = 0.0;          // No echelon delay
    c2.init(100, config);
    
    c2.set_commander(101, 100);
    
    Rng rng(12345);
    
    // First update to establish comms
    c2.update(storage, rng, 0);
    
    Order order;
    order.type = OrderType::ATTACK;
    order.target_entity = 200;
    
    // Issue order at tick 0
    bool issued = c2.issue_order(100, 101, order, 0);
    TEST_ASSERT(issued, "Order should be issued");
    TEST_ASSERT(c2.has_pending_orders(101), "Should have pending order");
    
    // Tick 1: Should be delivered (0 + ceil(1) = 1)
    c2.update(storage, rng, 1);
    
    const Order& current = c2.get_current_order(101);
    TEST_ASSERT(current.type == OrderType::ATTACK, "Order should be delivered at tick 1");
    TEST_ASSERT(c2.stats().orders_delivered == 1, "One order delivered");
    
    return true;
}

bool test_communication_reliability() {
    EntityStorage storage;
    setup_hierarchy(storage);
    
    C2System c2;
    C2Config config;
    config.base_comm_reliability = 1.0;  // Perfect comms
    config.max_comm_range_km = 100.0;
    c2.init(100, config);
    
    c2.set_commander(101, 100);
    
    Rng rng(54321);
    
    // Units are 5km apart, should be connected
    c2.update(storage, rng, 0);
    TEST_ASSERT(c2.get_comm_status(101) == CommStatus::CONNECTED, "Should be connected");
    
    return true;
}

bool test_out_of_range() {
    EntityStorage storage;
    storage.init(10);
    storage.count = 2;
    
    // Commander at origin
    storage.id[0] = 100;
    storage.flags[0] = entity_flags::ACTIVE;
    storage.side[0] = Side::BLUE;
    storage.pos_x[0] = 0.0;
    storage.pos_y[0] = 0.0;
    storage.pos_z[0] = 0.0;
    storage.health[0] = 1.0;
    
    // Subordinate 100km away
    storage.id[1] = 101;
    storage.flags[1] = entity_flags::ACTIVE;
    storage.side[1] = Side::BLUE;
    storage.pos_x[1] = 100000.0;  // 100km
    storage.pos_y[1] = 0.0;
    storage.pos_z[1] = 0.0;
    storage.health[1] = 1.0;
    
    C2System c2;
    C2Config config;
    config.max_comm_range_km = 50.0;  // Only 50km range
    c2.init(100, config);
    
    c2.set_commander(101, 100);
    
    Rng rng(11111);
    c2.update(storage, rng, 0);
    
    TEST_ASSERT(c2.get_comm_status(101) == CommStatus::OUT_OF_RANGE, 
        "Should be out of range");
    
    return true;
}

bool test_jamming() {
    EntityStorage storage;
    setup_hierarchy(storage);
    
    C2System c2;
    C2Config config;
    config.base_comm_reliability = 0.95;
    config.jamming_susceptibility = 0.5;
    c2.init(100, config);
    
    c2.set_commander(101, 100);
    
    // Set jamming at subordinate position
    c2.set_jamming(5000.0, 0.0, 2000.0, 1.0);  // Full jamming
    
    Rng rng(22222);
    
    // Run several ticks
    for (int i = 0; i < 5; ++i) {
        c2.update(storage, rng, i);
    }
    
    // Effectiveness should be reduced
    f64 eff = c2.get_effectiveness_multiplier(101);
    TEST_ASSERT(eff < 1.0, "Effectiveness should be reduced under jamming");
    
    return true;
}

bool test_autonomy() {
    EntityStorage storage;
    setup_hierarchy(storage);
    
    C2System c2;
    C2Config config;
    config.base_comm_reliability = 0.0;  // No comms (force autonomy)
    config.autonomy_threshold_ticks = 3;
    config.autonomous_effectiveness = 0.6;
    c2.init(100, config);
    
    c2.set_commander(101, 100);
    
    Rng rng(33333);
    
    // Initially not autonomous
    TEST_ASSERT(!c2.is_autonomous(101), "Not autonomous initially");
    
    // Run past threshold
    for (Tick t = 0; t <= 5; ++t) {
        c2.update(storage, rng, t);
    }
    
    TEST_ASSERT(c2.is_autonomous(101), "Should be autonomous after threshold");
    TEST_ASSERT(c2.get_effectiveness_multiplier(101) == config.autonomous_effectiveness,
        "Effectiveness should be reduced when autonomous");
    
    return true;
}

bool test_commander_destroyed() {
    EntityStorage storage;
    setup_hierarchy(storage);
    
    C2System c2;
    C2Config config;
    c2.init(100, config);
    
    c2.set_commander(101, 100);
    
    Rng rng(44444);
    
    // Kill the commander
    storage.flags[0] = entity_flags::DEAD;
    
    c2.update(storage, rng, 0);
    
    TEST_ASSERT(c2.get_comm_status(101) == CommStatus::DESTROYED,
        "Comm status should show commander destroyed");
    
    return true;
}

bool test_order_to_all_subordinates() {
    EntityStorage storage;
    setup_hierarchy(storage);
    
    C2System c2;
    C2Config config;
    config.base_command_delay_ticks = 0.0;  // Instant delivery for testing
    c2.init(100, config);
    
    // Battalion has 2 companies
    c2.set_commander(101, 100);
    c2.set_commander(102, 101);
    c2.set_commander(103, 101);
    
    Order order;
    order.type = OrderType::DEFEND;
    
    c2.issue_order_to_all(101, order, 0);
    
    TEST_ASSERT(c2.stats().orders_issued == 2, "Two orders issued");
    TEST_ASSERT(c2.has_pending_orders(102), "Company A has pending order");
    TEST_ASSERT(c2.has_pending_orders(103), "Company B has pending order");
    
    return true;
}

bool test_c2_determinism() {
    const Seed SEED = 0xC2C2C2123456ULL;
    
    EntityStorage storage1, storage2;
    setup_hierarchy(storage1);
    setup_hierarchy(storage2);
    
    C2Config config;
    config.base_comm_reliability = 0.8;  // Some randomness
    
    C2System c2_1, c2_2;
    c2_1.init(100, config);
    c2_2.init(100, config);
    
    // Same hierarchy
    c2_1.set_commander(101, 100);
    c2_1.set_commander(102, 101);
    c2_2.set_commander(101, 100);
    c2_2.set_commander(102, 101);
    
    // Same orders
    Order order;
    order.type = OrderType::MOVE;
    order.target_x = 1000.0;
    
    c2_1.issue_order(100, 101, order, 0);
    c2_2.issue_order(100, 101, order, 0);
    
    Rng rng1(SEED), rng2(SEED);
    
    // Run both
    for (Tick t = 0; t < 10; ++t) {
        c2_1.update(storage1, rng1, t);
        c2_2.update(storage2, rng2, t);
    }
    
    // Should have identical stats
    TEST_ASSERT(c2_1.stats().orders_delivered == c2_2.stats().orders_delivered,
        "Same orders delivered");
    TEST_ASSERT(c2_1.stats().comm_failures == c2_2.stats().comm_failures,
        "Same comm failures");
    TEST_ASSERT(c2_1.is_autonomous(101) == c2_2.is_autonomous(101),
        "Same autonomy state");
    
    return true;
}

bool test_remove_commander() {
    C2System c2;
    C2Config config;
    c2.init(100, config);
    
    c2.set_commander(101, 100);
    TEST_ASSERT(c2.get_commander(101) == 100, "Commander set");
    
    c2.remove_commander(101);
    TEST_ASSERT(c2.get_commander(101) == 0, "Commander removed");
    
    auto subs = c2.get_subordinates(100);
    TEST_ASSERT(subs.empty(), "No subordinates after removal");
    
    return true;
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "=== ATHENA C2 System Tests ===\n\n";
    
    int passed = 0;
    int failed = 0;
    
    std::cout << "--- Basic Tests ---\n";
    RUN_TEST(test_c2_init);
    RUN_TEST(test_hierarchy_setup);
    RUN_TEST(test_echelon_levels);
    RUN_TEST(test_remove_commander);
    
    std::cout << "\n--- Order Tests ---\n";
    RUN_TEST(test_issue_order);
    RUN_TEST(test_order_delivery_delay);
    RUN_TEST(test_order_to_all_subordinates);
    
    std::cout << "\n--- Communication Tests ---\n";
    RUN_TEST(test_communication_reliability);
    RUN_TEST(test_out_of_range);
    RUN_TEST(test_jamming);
    RUN_TEST(test_commander_destroyed);
    
    std::cout << "\n--- Autonomy Tests ---\n";
    RUN_TEST(test_autonomy);
    
    std::cout << "\n--- Determinism Tests ---\n";
    RUN_TEST(test_c2_determinism);
    
    // Summary
    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    
    if (failed > 0) {
        std::cout << "\n*** C2 TESTS FAILED ***\n";
        return 1;
    }
    
    std::cout << "\nAll C2 tests passed.\n";
    return 0;
}
