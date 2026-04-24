// ATHENA Core - Execution Manifest
// Contract: Complete audit trail for reproducibility.
//
// RULES:
// - Every execution produces a manifest
// - Manifest contains ALL determinism-relevant data
// - No execution result valid without manifest
// - Manifest is hashable and signable
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_MANIFEST_HPP
#define ATHENA_MANIFEST_HPP

#include "types.hpp"
#include "context.hpp"
#include <string>
#include <vector>

namespace athena {

// =============================================================================
// Platform Information
// =============================================================================

struct PlatformInfo {
    std::string os_name;            // e.g., "Linux", "Windows"
    std::string os_version;         // e.g., "6.5.0"
    std::string cpu_vendor;         // e.g., "GenuineIntel", "AuthenticAMD"
    std::string cpu_brand;          // Full CPU name
    std::string cpu_microarch;      // e.g., "zen4", "alderlake"
    std::vector<std::string> cpu_features;  // e.g., ["avx2", "fma"]
    u32 cpu_cores;
    u32 cpu_threads;
    u64 ram_bytes;
};

// =============================================================================
// Build Information
// =============================================================================

struct BuildInfo {
    std::string version;            // ATHENA version
    std::string git_commit;         // Git commit hash (if available)
    std::string build_date;         // Build timestamp
    std::string compiler;           // e.g., "gcc-13.2.0"
    std::string compiler_flags;     // Compilation flags
    Hash256 binary_hash;            // Hash of executable
};

// =============================================================================
// Execution Configuration
// =============================================================================

struct ExecutionConfig {
    Seed master_seed;
    u64 rng_stream;
    u32 thread_count;
    Tick start_tick;
    Tick end_tick;
    u32 monte_carlo_iterations;     // For batch runs
    bool sensitivity_enabled;
    std::string sensitivity_method;
};

// =============================================================================
// Execution Results Summary
// =============================================================================

struct ExecutionResults {
    Tick ticks_executed;
    u64 entities_created;
    u64 entities_destroyed;
    f64 duration_seconds;
    bool completed_normally;
    std::string error_message;      // Empty if no error
};

// =============================================================================
// Manifest
// =============================================================================

struct Manifest {
    // === Metadata ===
    std::string manifest_version;   // Schema version
    std::string created_at;         // ISO 8601 timestamp
    std::string job_id;             // Unique job identifier
    
    // === Environment ===
    PlatformInfo platform;
    BuildInfo build;
    
    // === Configuration ===
    ExecutionConfig config;
    
    // === Input Hashes ===
    Hash256 scenario_hash;
    Hash256 model_hash;
    Hash256 parameters_hash;
    
    // === Results ===
    ExecutionResults results;
    
    // === Output Hashes ===
    Hash256 output_hash;
    
    // === Chain ===
    Hash256 previous_manifest_hash; // For linked executions
    u64 sequence_number;
    
    // === Integrity ===
    Hash256 manifest_hash;          // Hash of this manifest (excluding this field)
};

// =============================================================================
// Manifest Builder
// =============================================================================

class ManifestBuilder {
public:
    ManifestBuilder();
    
    // Set metadata
    ManifestBuilder& set_job_id(const std::string& job_id);
    
    // Capture platform info (auto-detected)
    ManifestBuilder& capture_platform();
    
    // Set build info
    ManifestBuilder& set_build_info(const BuildInfo& info);
    
    // Set execution config from context
    ManifestBuilder& set_config(const Context& ctx, const ExecutionConfig& exec_config);
    
    // Set input hashes
    ManifestBuilder& set_scenario_hash(const Hash256& hash);
    ManifestBuilder& set_model_hash(const Hash256& hash);
    ManifestBuilder& set_parameters_hash(const Hash256& hash);
    
    // Set results
    ManifestBuilder& set_results(const ExecutionResults& results);
    
    // Set output hash
    ManifestBuilder& set_output_hash(const Hash256& hash);
    
    // Set chain info
    ManifestBuilder& set_previous_manifest(const Hash256& hash, u64 seq);
    
    // Build final manifest (computes manifest_hash)
    Result<Manifest> build();

private:
    Manifest manifest_;
    bool platform_set_;
    bool build_set_;
    bool config_set_;
};

// =============================================================================
// Manifest Utilities
// =============================================================================

namespace manifest {

// Detect current platform
PlatformInfo detect_platform();

// Get current build info
BuildInfo get_build_info();

// Generate current timestamp (ISO 8601)
std::string current_timestamp();

// Generate unique job ID
std::string generate_job_id();

// Serialize manifest to JSON
std::string to_json(const Manifest& manifest);

// Parse manifest from JSON
Result<Manifest> from_json(const std::string& json);

// Compute hash of manifest (excluding manifest_hash field)
Hash256 compute_manifest_hash(const Manifest& manifest);

// Verify manifest integrity
bool verify_manifest(const Manifest& manifest);

}  // namespace manifest

}  // namespace athena

#endif  // ATHENA_MANIFEST_HPP
