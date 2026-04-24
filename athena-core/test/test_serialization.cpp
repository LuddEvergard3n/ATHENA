// ATHENA Core - Serialization Tests
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/serialization.hpp"
#include "athena/analysis/montecarlo.hpp"

#include <iostream>
#include <cstdio>
#include <cmath>

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
            return false; \
        } \
    } while(0)

using namespace athena;

// =============================================================================
// CRC32 Tests
// =============================================================================

bool test_crc32_empty() {
    u32 crc = CRC32::compute(nullptr, 0);
    // CRC32 of empty data should be 0
    TEST_ASSERT(crc == 0, "CRC32 of empty should be 0");
    return true;
}

bool test_crc32_known_value() {
    // "123456789" has known CRC32 = 0xCBF43926
    const char* data = "123456789";
    u32 crc = CRC32::compute(data, 9);
    TEST_ASSERT(crc == 0xCBF43926, "CRC32 of '123456789' should be 0xCBF43926");
    return true;
}

bool test_crc32_determinism() {
    const char* data = "ATHENA binary format test data for CRC32";
    u32 crc1 = CRC32::compute(data, std::strlen(data));
    u32 crc2 = CRC32::compute(data, std::strlen(data));
    TEST_ASSERT(crc1 == crc2, "CRC32 should be deterministic");
    return true;
}

bool test_crc32_incremental() {
    const char* data = "ATHENA";
    
    // Compute in one pass
    u32 crc_full = CRC32::compute(data, 6);
    
    // Compute incrementally
    CRC32 crc;
    crc.update(data, 3);    // "ATH"
    crc.update(data + 3, 3); // "ENA"
    u32 crc_inc = crc.finalize();
    
    TEST_ASSERT(crc_full == crc_inc, "Incremental CRC should match full CRC");
    return true;
}

// =============================================================================
// BinaryWriter/Reader Tests
// =============================================================================

bool test_binary_primitives() {
    const char* path = "/tmp/athena_test_primitives.bin";
    
    // Write
    {
        BinaryWriter w(path);
        TEST_ASSERT(w.is_open(), "Writer should open file");
        
        w.write_u8(0x42);
        w.write_u16(0x1234);
        w.write_u32(0xDEADBEEF);
        w.write_u64(0x123456789ABCDEF0ULL);
        w.write_i32(-12345);
        w.write_i64(-9876543210LL);
        w.write_f32(3.14159f);
        w.write_f64(2.718281828459045);
    }
    
    // Read
    {
        BinaryReader r(path);
        TEST_ASSERT(r.is_open(), "Reader should open file");
        
        TEST_ASSERT(r.read_u8() == 0x42, "u8 mismatch");
        TEST_ASSERT(r.read_u16() == 0x1234, "u16 mismatch");
        TEST_ASSERT(r.read_u32() == 0xDEADBEEF, "u32 mismatch");
        TEST_ASSERT(r.read_u64() == 0x123456789ABCDEF0ULL, "u64 mismatch");
        TEST_ASSERT(r.read_i32() == -12345, "i32 mismatch");
        TEST_ASSERT(r.read_i64() == -9876543210LL, "i64 mismatch");
        
        f32 f32_val = r.read_f32();
        TEST_ASSERT(std::abs(f32_val - 3.14159f) < 1e-5f, "f32 mismatch");
        
        f64 f64_val = r.read_f64();
        TEST_ASSERT(std::abs(f64_val - 2.718281828459045) < 1e-12, "f64 mismatch");
    }
    
    std::remove(path);
    return true;
}

bool test_binary_strings() {
    const char* path = "/tmp/athena_test_strings.bin";
    
    {
        BinaryWriter w(path);
        w.write_string("");
        w.write_string("Hello");
        w.write_string("ATHENA Binary Format");
    }
    
    {
        BinaryReader r(path);
        TEST_ASSERT(r.read_string() == "", "Empty string mismatch");
        TEST_ASSERT(r.read_string() == "Hello", "String 'Hello' mismatch");
        TEST_ASSERT(r.read_string() == "ATHENA Binary Format", "Long string mismatch");
    }
    
    std::remove(path);
    return true;
}

bool test_binary_arrays() {
    const char* path = "/tmp/athena_test_arrays.bin";
    
    std::vector<f64> f64_arr = {1.0, 2.0, 3.0, 4.5, 5.5};
    std::vector<u32> u32_arr = {10, 20, 30, 40, 50};
    
    {
        BinaryWriter w(path);
        w.write_f64_array(f64_arr);
        w.write_u32_array(u32_arr);
    }
    
    {
        BinaryReader r(path);
        auto read_f64 = r.read_f64_array();
        auto read_u32 = r.read_u32_array();
        
        TEST_ASSERT(read_f64.size() == f64_arr.size(), "f64 array size mismatch");
        TEST_ASSERT(read_u32.size() == u32_arr.size(), "u32 array size mismatch");
        
        for (usize i = 0; i < f64_arr.size(); ++i) {
            TEST_ASSERT(std::abs(read_f64[i] - f64_arr[i]) < 1e-12, "f64 array value mismatch");
        }
        
        for (usize i = 0; i < u32_arr.size(); ++i) {
            TEST_ASSERT(read_u32[i] == u32_arr[i], "u32 array value mismatch");
        }
    }
    
    std::remove(path);
    return true;
}

// =============================================================================
// ABF Format Tests
// =============================================================================

bool test_abf_header() {
    ABFHeader header;
    TEST_ASSERT(header.magic == abf::MAGIC, "Default magic should be ATHENABF");
    TEST_ASSERT(header.version == abf::VERSION, "Default version should be current");
    TEST_ASSERT(header.validate(), "Default header should be valid");
    
    // Invalid magic
    ABFHeader bad;
    bad.magic = 0x12345678;
    TEST_ASSERT(!bad.validate(), "Invalid magic should fail validation");
    
    return true;
}

bool test_abf_roundtrip_small() {
    const char* path = "/tmp/athena_test_abf_small.abf";
    
    // Create test data
    analysis::BatchConfig config;
    config.master_seed = 0xDEADBEEF12345678ULL;
    config.num_iterations = 10;
    config.max_ticks_per_iteration = 100;
    config.stop_on_decisive = false;
    config.decisive_threshold = 0.3;
    
    std::vector<analysis::IterationResult> results;
    for (u32 i = 0; i < 10; ++i) {
        analysis::IterationResult r;
        r.iteration_id = i;
        r.seed = config.master_seed + i;
        r.ticks_executed = 100;
        r.completed_normally = true;
        r.blue_surviving = 5 + i;
        r.red_surviving = 3 + i;
        r.neutral_surviving = 0;
        r.blue_total_health = 0.5 + i * 0.01;
        r.red_total_health = 0.4 + i * 0.01;
        r.blue_avg_supply = 0.8;
        r.red_avg_supply = 0.7;
        r.blue_avg_morale = 0.9;
        r.red_avg_morale = 0.85;
        r.total_engagements = 10;
        r.total_blue_damage = 1.5;
        r.total_red_damage = 1.2;
        r.time_to_first_casualty = 5;
        r.time_to_decisive = 80;
        results.push_back(r);
    }
    
    analysis::BatchStatistics stats;
    stats.total_iterations = 10;
    stats.completed_iterations = 10;
    stats.failed_iterations = 0;
    stats.blue_wins = 6;
    stats.red_wins = 3;
    stats.draws = 1;
    
    // Initialize MetricStats
    stats.ticks_to_completion = {100.0, 0.0, 100.0, 100.0, 100.0, 100.0, 100.0};
    stats.blue_survival_rate = {0.55, 0.05, 0.5, 0.6, 0.55, 0.5, 0.6};
    stats.red_survival_rate = {0.45, 0.04, 0.4, 0.5, 0.45, 0.4, 0.5};
    stats.blue_final_health = {0.55, 0.05, 0.5, 0.6, 0.55, 0.5, 0.6};
    stats.red_final_health = {0.45, 0.04, 0.4, 0.5, 0.45, 0.4, 0.5};
    
    // Write
    auto write_result = save_results_abf(path, config, results, stats);
    TEST_ASSERT(write_result.ok(), "Write should succeed");
    
    // Read
    analysis::BatchConfig read_config;
    std::vector<analysis::IterationResult> read_results;
    analysis::BatchStatistics read_stats;
    
    auto read_result = load_results_abf(path, read_config, read_results, read_stats);
    TEST_ASSERT(read_result.ok(), "Read should succeed");
    
    // Verify config
    TEST_ASSERT(read_config.master_seed == config.master_seed, "master_seed mismatch");
    TEST_ASSERT(read_config.num_iterations == config.num_iterations, "num_iterations mismatch");
    TEST_ASSERT(read_config.max_ticks_per_iteration == config.max_ticks_per_iteration, "max_ticks mismatch");
    
    // Verify results
    TEST_ASSERT(read_results.size() == results.size(), "Results count mismatch");
    for (usize i = 0; i < results.size(); ++i) {
        TEST_ASSERT(read_results[i].iteration_id == results[i].iteration_id, "iteration_id mismatch");
        TEST_ASSERT(read_results[i].seed == results[i].seed, "seed mismatch");
        TEST_ASSERT(read_results[i].blue_surviving == results[i].blue_surviving, "blue_surviving mismatch");
    }
    
    // Verify stats
    TEST_ASSERT(read_stats.total_iterations == stats.total_iterations, "total_iterations mismatch");
    TEST_ASSERT(read_stats.blue_wins == stats.blue_wins, "blue_wins mismatch");
    TEST_ASSERT(std::abs(read_stats.blue_survival_rate.mean - stats.blue_survival_rate.mean) < 1e-12, "blue_survival_rate.mean mismatch");
    
    std::remove(path);
    return true;
}

bool test_abf_validation() {
    const char* path = "/tmp/athena_test_abf_valid.abf";
    
    // Create minimal valid file
    analysis::BatchConfig config;
    config.master_seed = 12345;
    config.num_iterations = 1;
    
    std::vector<analysis::IterationResult> results(1);
    results[0].iteration_id = 0;
    results[0].seed = 12345;
    results[0].completed_normally = true;
    
    analysis::BatchStatistics stats;
    stats.total_iterations = 1;
    stats.completed_iterations = 1;
    
    auto write_result = save_results_abf(path, config, results, stats);
    TEST_ASSERT(write_result.ok(), "Write should succeed");
    
    // Validate
    auto valid_result = validate_abf(path);
    TEST_ASSERT(valid_result.ok(), "Validation should not error");
    TEST_ASSERT(valid_result.get() == true, "File should be valid");
    
    std::remove(path);
    return true;
}

bool test_abf_large_dataset() {
    const char* path = "/tmp/athena_test_abf_large.abf";
    
    // Create larger dataset (1000 iterations)
    analysis::BatchConfig config;
    config.master_seed = 0xCAFEBABE;
    config.num_iterations = 1000;
    config.max_ticks_per_iteration = 500;
    
    std::vector<analysis::IterationResult> results;
    results.reserve(1000);
    for (u32 i = 0; i < 1000; ++i) {
        analysis::IterationResult r;
        r.iteration_id = i;
        r.seed = config.master_seed ^ (static_cast<u64>(i) << 32);
        r.ticks_executed = 400 + (i % 100);
        r.completed_normally = true;
        r.blue_surviving = 10 + (i % 20);
        r.red_surviving = 8 + (i % 15);
        r.neutral_surviving = 0;
        r.blue_total_health = 0.3 + (i % 50) * 0.01;
        r.red_total_health = 0.25 + (i % 40) * 0.01;
        r.blue_avg_supply = 0.6 + (i % 30) * 0.01;
        r.red_avg_supply = 0.5 + (i % 25) * 0.01;
        r.blue_avg_morale = 0.8;
        r.red_avg_morale = 0.75;
        r.total_engagements = i;
        r.total_blue_damage = 1.0;
        r.total_red_damage = 1.0;
        r.time_to_first_casualty = 5;
        r.time_to_decisive = 300;
        results.push_back(r);
    }
    
    analysis::BatchStatistics stats;
    stats.total_iterations = 1000;
    stats.completed_iterations = 1000;
    
    // Write
    auto write_result = save_results_abf(path, config, results, stats);
    TEST_ASSERT(write_result.ok(), "Write large dataset should succeed");
    
    // Read
    analysis::BatchConfig read_config;
    std::vector<analysis::IterationResult> read_results;
    analysis::BatchStatistics read_stats;
    
    auto read_result = load_results_abf(path, read_config, read_results, read_stats);
    TEST_ASSERT(read_result.ok(), "Read large dataset should succeed");
    
    TEST_ASSERT(read_results.size() == 1000, "Should have 1000 iterations");
    
    // Spot check
    TEST_ASSERT(read_results[500].iteration_id == 500, "Iteration 500 ID mismatch");
    TEST_ASSERT(read_results[999].iteration_id == 999, "Iteration 999 ID mismatch");
    
    std::remove(path);
    return true;
}

bool test_abf_metadata() {
    const char* path = "/tmp/athena_test_abf_meta.abf";
    
    analysis::BatchConfig config;
    config.master_seed = 42;
    config.num_iterations = 1;
    
    std::vector<analysis::IterationResult> results(1);
    analysis::BatchStatistics stats;
    stats.total_iterations = 1;
    
    // Write with metadata
    ResultSerializer serializer;
    serializer.set_metadata("scenario", "test_scenario_001");
    serializer.set_metadata("analyst", "unit_test");
    serializer.set_metadata("version", "0.3.8");
    
    auto write_result = serializer.write(path, config, results, stats);
    TEST_ASSERT(write_result.ok(), "Write with metadata should succeed");
    
    // Read and check metadata
    ResultSerializer reader;
    analysis::BatchConfig read_config;
    std::vector<analysis::IterationResult> read_results;
    analysis::BatchStatistics read_stats;
    
    auto read_result = reader.read(path, read_config, read_results, read_stats);
    TEST_ASSERT(read_result.ok(), "Read with metadata should succeed");
    
    TEST_ASSERT(reader.get_metadata("scenario") == "test_scenario_001", "scenario metadata mismatch");
    TEST_ASSERT(reader.get_metadata("analyst") == "unit_test", "analyst metadata mismatch");
    TEST_ASSERT(reader.get_metadata("version") == "0.3.8", "version metadata mismatch");
    
    std::remove(path);
    return true;
}

bool test_abf_determinism() {
    const char* path1 = "/tmp/athena_test_abf_det1.abf";
    const char* path2 = "/tmp/athena_test_abf_det2.abf";
    
    analysis::BatchConfig config;
    config.master_seed = 0x1234567890ABCDEFULL;
    config.num_iterations = 50;
    
    std::vector<analysis::IterationResult> results;
    for (u32 i = 0; i < 50; ++i) {
        analysis::IterationResult r;
        r.iteration_id = i;
        r.seed = config.master_seed + i;
        r.ticks_executed = 100;
        r.completed_normally = true;
        r.blue_surviving = i + 1;
        r.red_surviving = i;
        r.neutral_surviving = 0;
        r.blue_total_health = 0.5;
        r.red_total_health = 0.4;
        r.blue_avg_supply = 0.8;
        r.red_avg_supply = 0.7;
        r.blue_avg_morale = 0.9;
        r.red_avg_morale = 0.85;
        r.total_engagements = 10;
        r.total_blue_damage = 1.0;
        r.total_red_damage = 1.0;
        r.time_to_first_casualty = 5;
        r.time_to_decisive = 80;
        results.push_back(r);
    }
    
    analysis::BatchStatistics stats;
    stats.total_iterations = 50;
    
    // Write twice
    save_results_abf(path1, config, results, stats);
    save_results_abf(path2, config, results, stats);
    
    // Compare files byte-by-byte
    std::ifstream f1(path1, std::ios::binary);
    std::ifstream f2(path2, std::ios::binary);
    
    TEST_ASSERT(f1.is_open() && f2.is_open(), "Both files should open");
    
    f1.seekg(0, std::ios::end);
    f2.seekg(0, std::ios::end);
    
    TEST_ASSERT(f1.tellg() == f2.tellg(), "File sizes should match");
    
    f1.seekg(0);
    f2.seekg(0);
    
    char c1, c2;
    bool identical = true;
    while (f1.get(c1) && f2.get(c2)) {
        if (c1 != c2) {
            identical = false;
            break;
        }
    }
    
    TEST_ASSERT(identical, "Files should be byte-identical");
    
    std::remove(path1);
    std::remove(path2);
    return true;
}

// =============================================================================
// Test Runner
// =============================================================================

#define RUN_TEST(fn) \
    do { \
        std::cout << "Running " << #fn << "... "; \
        if (fn()) { \
            std::cout << "PASS\n"; \
            passed++; \
        } else { \
            std::cout << "FAIL\n"; \
            failed++; \
        } \
    } while(0)

int main() {
    int passed = 0;
    int failed = 0;
    
    std::cout << "\n=== Serialization Tests ===\n\n";
    
    // CRC32 Tests
    RUN_TEST(test_crc32_empty);
    RUN_TEST(test_crc32_known_value);
    RUN_TEST(test_crc32_determinism);
    RUN_TEST(test_crc32_incremental);
    
    // Binary I/O Tests
    RUN_TEST(test_binary_primitives);
    RUN_TEST(test_binary_strings);
    RUN_TEST(test_binary_arrays);
    
    // ABF Format Tests
    RUN_TEST(test_abf_header);
    RUN_TEST(test_abf_roundtrip_small);
    RUN_TEST(test_abf_validation);
    RUN_TEST(test_abf_large_dataset);
    RUN_TEST(test_abf_metadata);
    RUN_TEST(test_abf_determinism);
    
    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    
    return failed > 0 ? 1 : 0;
}
