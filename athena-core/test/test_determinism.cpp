// ATHENA Core - Determinism Tests
// Contract: Two runs with same seed MUST produce identical results.
//
// These tests are MANDATORY before any feature merge.
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/types.hpp"
#include "athena/rng.hpp"
#include "athena/context.hpp"
#include "athena/entities.hpp"
#include "athena/scheduler.hpp"
#include "athena/manifest.hpp"

#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cassert>

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

#define TEST_ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            std::cerr << "FAIL: " << msg << "\n"; \
            std::cerr << "  expected: " << (b) << "\n"; \
            std::cerr << "  actual:   " << (a) << "\n"; \
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
// RNG Determinism Tests
// =============================================================================

bool test_rng_determinism() {
    // Two RNGs with same seed must produce identical sequences
    constexpr Seed SEED = 0xDEADBEEF12345678ULL;
    
    Rng rng1(SEED, 0);
    Rng rng2(SEED, 0);
    
    // Generate 1000 values, all must match
    for (int i = 0; i < 1000; ++i) {
        u64 v1 = rng1.next_u64();
        u64 v2 = rng2.next_u64();
        TEST_ASSERT_EQ(v1, v2, "RNG values must match");
    }
    
    return true;
}

bool test_rng_different_seeds() {
    // Different seeds must produce different sequences
    Rng rng1(12345, 0);
    Rng rng2(12346, 0);
    
    // At least one of first 10 values should differ
    bool found_difference = false;
    for (int i = 0; i < 10; ++i) {
        if (rng1.next_u64() != rng2.next_u64()) {
            found_difference = true;
            break;
        }
    }
    
    TEST_ASSERT(found_difference, "Different seeds must produce different values");
    return true;
}

bool test_rng_split_determinism() {
    // Split must be deterministic
    constexpr Seed SEED = 0xCAFEBABE;
    
    Rng master1(SEED, 0);
    Rng master2(SEED, 0);
    
    Rng child1 = master1.split(0xABCD);
    Rng child2 = master2.split(0xABCD);
    
    for (int i = 0; i < 100; ++i) {
        TEST_ASSERT_EQ(child1.next_u64(), child2.next_u64(), 
            "Split RNGs must produce identical values");
    }
    
    return true;
}

bool test_rng_split_independence() {
    // Different split IDs must produce different sequences
    Rng master(0x12345678, 0);
    
    Rng child1 = master.split(1);
    Rng child2 = master.split(2);
    
    bool found_difference = false;
    for (int i = 0; i < 10; ++i) {
        if (child1.next_u64() != child2.next_u64()) {
            found_difference = true;
            break;
        }
    }
    
    TEST_ASSERT(found_difference, "Different split IDs must produce different sequences");
    return true;
}

bool test_rng_stream_independence() {
    // Different streams must be independent
    Rng rng1(12345, 0);
    Rng rng2(12345, 1);
    
    bool found_difference = false;
    for (int i = 0; i < 10; ++i) {
        if (rng1.next_u64() != rng2.next_u64()) {
            found_difference = true;
            break;
        }
    }
    
    TEST_ASSERT(found_difference, "Different streams must produce different sequences");
    return true;
}

bool test_rng_normal_determinism() {
    // Normal distribution must be deterministic
    constexpr Seed SEED = 0xFEEDFACE;
    
    Rng rng1(SEED, 0);
    Rng rng2(SEED, 0);
    
    for (int i = 0; i < 100; ++i) {
        f64 v1 = rng1.next_normal();
        f64 v2 = rng2.next_normal();
        TEST_ASSERT(v1 == v2, "Normal distribution must be deterministic");
    }
    
    return true;
}

// =============================================================================
// Context Determinism Tests
// =============================================================================

bool test_context_creation() {
    ContextConfig config;
    config.seed = 12345;
    config.max_entities = 1000;
    
    auto result = Context::create(config);
    TEST_ASSERT(result.ok(), "Context creation should succeed");
    
    auto& ctx = *result.get();
    TEST_ASSERT(ctx.is_initialized(), "Context should be initialized");
    TEST_ASSERT_EQ(ctx.config().seed, (Seed)12345, "Seed should match");
    
    return true;
}

bool test_context_zero_seed_rejected() {
    ContextConfig config;
    config.seed = 0;  // Invalid
    
    auto result = Context::create(config);
    TEST_ASSERT(!result.ok(), "Zero seed should be rejected");
    TEST_ASSERT(result.error.code == ErrorCode::RNG_INVALID_SEED, 
        "Error code should be RNG_INVALID_SEED");
    
    return true;
}

bool test_context_reset_determinism() {
    ContextConfig config;
    config.seed = 0xABCDEF;
    
    auto result = Context::create(config);
    TEST_ASSERT(result.ok(), "Context creation should succeed");
    
    auto& ctx = *result.get();
    
    // Generate some random values
    std::vector<u64> values1;
    for (int i = 0; i < 10; ++i) {
        values1.push_back(ctx.master_rng().next_u64());
    }
    
    // Reset
    ctx.reset();
    
    // Generate again - should match
    for (int i = 0; i < 10; ++i) {
        u64 v = ctx.master_rng().next_u64();
        TEST_ASSERT_EQ(v, values1[i], "Reset should restore RNG state");
    }
    
    return true;
}

// =============================================================================
// Entity Storage Tests
// =============================================================================

bool test_entity_creation_determinism() {
    EntityManager mgr1;
    EntityManager mgr2;
    
    mgr1.init(100);
    mgr2.init(100);
    
    // Create entities - IDs must match
    for (int i = 0; i < 50; ++i) {
        EntityId id1 = mgr1.create();
        EntityId id2 = mgr2.create();
        TEST_ASSERT_EQ(id1, id2, "Entity IDs must match");
    }
    
    return true;
}

bool test_entity_compact_determinism() {
    EntityManager mgr;
    mgr.init(100);
    
    // Create entities
    for (int i = 0; i < 10; ++i) {
        mgr.create();
    }
    
    // Destroy some (indices 2, 5, 7)
    mgr.destroy(2);
    mgr.destroy(5);
    mgr.destroy(7);
    
    // Compact
    mgr.compact();
    
    // Check count
    TEST_ASSERT_EQ(mgr.storage().count, (usize)7, "Count after compact");
    
    // All remaining should be active
    for (usize i = 0; i < mgr.storage().count; ++i) {
        TEST_ASSERT(mgr.storage().is_active(i), "All remaining should be active");
    }
    
    return true;
}

// =============================================================================
// Scheduler Determinism Tests
// =============================================================================

// Simple test system that modifies state deterministically
static Status test_system_increment(EntityStorage& storage, Rng& /*rng*/, Tick /*tick*/) {
    for (usize i = 0; i < storage.count; ++i) {
        if (storage.is_active(i)) {
            storage.pos_x[i] += 1.0;
        }
    }
    return Status();
}

bool test_scheduler_determinism() {
    // Run same simulation twice, results must match
    constexpr Seed SEED = 0x123456789ABCDEF0ULL;
    constexpr Tick TICKS = 100;
    
    // First run
    ContextConfig config1;
    config1.seed = SEED;
    auto ctx1_result = Context::create(config1);
    TEST_ASSERT(ctx1_result.ok(), "Context 1 creation");
    auto& ctx1 = *ctx1_result.get();
    
    EntityManager entities1;
    entities1.init(100);
    for (int i = 0; i < 10; ++i) {
        entities1.create();
        // Assign to two opposing sides so early termination doesn't trigger
        entities1.storage().side[i] = (i < 5) ? Side::BLUE : Side::RED;
    }
    
    Scheduler sched1;
    SchedulerConfig sched_config;
    sched_config.max_ticks = TICKS;
    sched_config.compact_per_tick = false;
    
    TEST_ASSERT(sched1.init(ctx1, entities1, sched_config).ok(), "Scheduler 1 init");
    TEST_ASSERT(sched1.register_system(Scheduler::Phase::MOVEMENT, 
        test_system_increment, "increment").ok(), "Register system 1");
    TEST_ASSERT(sched1.run_until_complete().ok(), "Run 1");
    
    // Capture final state
    std::vector<f64> final_pos_x_1(entities1.storage().count);
    for (usize i = 0; i < entities1.storage().count; ++i) {
        final_pos_x_1[i] = entities1.storage().pos_x[i];
    }
    
    // Second run (identical setup)
    ContextConfig config2;
    config2.seed = SEED;
    auto ctx2_result = Context::create(config2);
    TEST_ASSERT(ctx2_result.ok(), "Context 2 creation");
    auto& ctx2 = *ctx2_result.get();
    
    EntityManager entities2;
    entities2.init(100);
    for (int i = 0; i < 10; ++i) {
        entities2.create();
        entities2.storage().side[i] = (i < 5) ? Side::BLUE : Side::RED;
    }
    
    Scheduler sched2;
    TEST_ASSERT(sched2.init(ctx2, entities2, sched_config).ok(), "Scheduler 2 init");
    TEST_ASSERT(sched2.register_system(Scheduler::Phase::MOVEMENT, 
        test_system_increment, "increment").ok(), "Register system 2");
    TEST_ASSERT(sched2.run_until_complete().ok(), "Run 2");
    
    // Compare
    TEST_ASSERT_EQ(entities1.storage().count, entities2.storage().count, "Entity count");
    
    for (usize i = 0; i < entities1.storage().count; ++i) {
        TEST_ASSERT(entities1.storage().pos_x[i] == entities2.storage().pos_x[i],
            "Position X must match");
    }
    
    // Verify expected value (started at 0, incremented TICKS times)
    TEST_ASSERT(final_pos_x_1[0] == static_cast<f64>(TICKS), 
        "Final position should equal tick count");
    
    return true;
}

// =============================================================================
// Hash Tests
// =============================================================================

bool test_hash256_roundtrip() {
    Hash256 original;
    for (int i = 0; i < 32; ++i) {
        original.bytes[i] = static_cast<u8>(i * 7 + 3);
    }
    
    std::string hex = original.to_hex();
    TEST_ASSERT_EQ(hex.size(), (usize)64, "Hex string should be 64 chars");
    
    auto parsed = Hash256::from_hex(hex);
    TEST_ASSERT(parsed.has_value(), "Parsing should succeed");
    TEST_ASSERT(parsed.value() == original, "Roundtrip should preserve value");
    
    return true;
}

bool test_hash256_invalid() {
    // Too short
    auto r1 = Hash256::from_hex("abcd");
    TEST_ASSERT(!r1.has_value(), "Short hex should fail");
    
    // Invalid char
    auto r2 = Hash256::from_hex(
        "000000000000000000000000000000000000000000000000000000000000000g");
    TEST_ASSERT(!r2.has_value(), "Invalid char should fail");
    
    return true;
}

// =============================================================================
// Manifest Tests
// =============================================================================

bool test_manifest_generation() {
    ContextConfig config;
    config.seed = 0xDEADBEEF;
    auto ctx_result = Context::create(config);
    TEST_ASSERT(ctx_result.ok(), "Context creation");
    auto& ctx = *ctx_result.get();
    
    ExecutionConfig exec_config;
    exec_config.master_seed = config.seed;
    exec_config.thread_count = 1;
    exec_config.start_tick = 0;
    exec_config.end_tick = 100;
    
    ExecutionResults results;
    results.ticks_executed = 100;
    results.completed_normally = true;
    results.duration_seconds = 1.5;
    
    ManifestBuilder builder;
    auto manifest_result = builder
        .capture_platform()
        .set_config(ctx, exec_config)
        .set_results(results)
        .build();
    
    TEST_ASSERT(manifest_result.ok(), "Manifest build");
    
    auto& manifest = manifest_result.get();
    TEST_ASSERT(!manifest.job_id.empty(), "Job ID should be set");
    TEST_ASSERT(!manifest.created_at.empty(), "Timestamp should be set");
    
    // Verify manifest hash
    TEST_ASSERT(manifest::verify_manifest(manifest), "Manifest should verify");
    
    return true;
}

bool test_manifest_json() {
    ContextConfig config;
    config.seed = 0x12345;
    auto ctx_result = Context::create(config);
    TEST_ASSERT(ctx_result.ok(), "Context creation");
    auto& ctx = *ctx_result.get();
    
    ExecutionConfig exec_config;
    exec_config.master_seed = config.seed;
    
    ExecutionResults results;
    results.ticks_executed = 50;
    results.completed_normally = true;
    
    ManifestBuilder builder;
    auto manifest_result = builder
        .set_config(ctx, exec_config)
        .set_results(results)
        .build();
    
    TEST_ASSERT(manifest_result.ok(), "Manifest build");
    
    std::string json = manifest::to_json(manifest_result.get());
    TEST_ASSERT(!json.empty(), "JSON should not be empty");
    TEST_ASSERT(json.find("\"manifest_version\"") != std::string::npos, 
        "JSON should contain manifest_version");
    
    return true;
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "=== ATHENA Core Determinism Tests ===\n\n";
    
    int passed = 0;
    int failed = 0;
    
    // RNG tests
    std::cout << "--- RNG Tests ---\n";
    RUN_TEST(test_rng_determinism);
    RUN_TEST(test_rng_different_seeds);
    RUN_TEST(test_rng_split_determinism);
    RUN_TEST(test_rng_split_independence);
    RUN_TEST(test_rng_stream_independence);
    RUN_TEST(test_rng_normal_determinism);
    
    // Context tests
    std::cout << "\n--- Context Tests ---\n";
    RUN_TEST(test_context_creation);
    RUN_TEST(test_context_zero_seed_rejected);
    RUN_TEST(test_context_reset_determinism);
    
    // Entity tests
    std::cout << "\n--- Entity Tests ---\n";
    RUN_TEST(test_entity_creation_determinism);
    RUN_TEST(test_entity_compact_determinism);
    
    // Scheduler tests
    std::cout << "\n--- Scheduler Tests ---\n";
    RUN_TEST(test_scheduler_determinism);
    
    // Hash tests
    std::cout << "\n--- Hash Tests ---\n";
    RUN_TEST(test_hash256_roundtrip);
    RUN_TEST(test_hash256_invalid);
    
    // Manifest tests
    std::cout << "\n--- Manifest Tests ---\n";
    RUN_TEST(test_manifest_generation);
    RUN_TEST(test_manifest_json);
    
    // Summary
    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    
    if (failed > 0) {
        std::cout << "\n*** DETERMINISM VIOLATION - DO NOT MERGE ***\n";
        return 1;
    }
    
    std::cout << "\nAll tests passed. Determinism verified.\n";
    return 0;
}
