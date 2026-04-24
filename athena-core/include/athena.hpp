// ATHENA Core - Main Header
// Contract: This is the public API. Changes require version bump.
//
// Include this header to access all ATHENA core functionality.
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_HPP
#define ATHENA_HPP

// Core types and error handling
#include "athena/types.hpp"

// JSON parsing
#include "athena/json.hpp"

// Random number generation
#include "athena/rng.hpp"

// Simulation context
#include "athena/context.hpp"

// Entity storage (SoA)
#include "athena/entities.hpp"

// Deterministic scheduler
#include "athena/scheduler.hpp"

// Execution manifest
#include "athena/manifest.hpp"

// Scenario loading
#include "athena/scenario.hpp"

// Terrain and pathfinding
#include "athena/terrain.hpp"
#include "athena/terrain_semantics.hpp"
#include "athena/pathfinding.hpp"

// Platform database
#include "athena/platform_loader.hpp"

// Simulation systems
#include "athena/systems.hpp"

// Analysis framework
#include "athena/analysis.hpp"

// Serialization
#include "athena/serialization.hpp"

// Integrated application (combines all features)
#include "athena/integrated.hpp"

namespace athena {

// =============================================================================
// Version Information
// =============================================================================

struct Version {
    static constexpr int MAJOR = 0;
    static constexpr int MINOR = 1;
    static constexpr int PATCH = 0;
    
    static constexpr const char* STRING = "0.1.0";
    static constexpr const char* SCHEMA = "1.0";
};

// =============================================================================
// Quick Start Example
// =============================================================================

/*
 * Basic usage:
 *
 *   #include <athena.hpp>
 *
 *   int main() {
 *       // 1. Create context with explicit seed
 *       athena::ContextConfig config;
 *       config.seed = 12345;  // REQUIRED - must be non-zero
 *       
 *       auto ctx_result = athena::Context::create(config);
 *       if (!ctx_result.ok()) {
 *           // Handle error
 *           return 1;
 *       }
 *       auto& ctx = *ctx_result.get();
 *
 *       // 2. Initialize entities
 *       athena::EntityManager entities;
 *       entities.init(1000);
 *       
 *       // Create some entities
 *       for (int i = 0; i < 100; ++i) {
 *           athena::EntityId id = entities.create();
 *           // Configure entity...
 *       }
 *
 *       // 3. Setup scheduler
 *       athena::Scheduler scheduler;
 *       athena::SchedulerConfig sched_config;
 *       sched_config.max_ticks = 1000;
 *       
 *       scheduler.init(ctx, entities, sched_config);
 *       
 *       // Register systems
 *       scheduler.register_system(
 *           athena::Scheduler::Phase::MOVEMENT,
 *           athena::systems::movement,
 *           "movement"
 *       );
 *
 *       // 4. Run simulation
 *       auto status = scheduler.run_until_complete();
 *       if (!status.ok()) {
 *           // Handle error
 *       }
 *
 *       // 5. Generate manifest for audit trail
 *       athena::ManifestBuilder builder;
 *       auto manifest = builder
 *           .capture_platform()
 *           .set_config(ctx, exec_config)
 *           .set_results(results)
 *           .build();
 *
 *       // Output results...
 *       return 0;
 *   }
 *
 * DETERMINISM GUARANTEE:
 *   Two runs with identical seed, binary, and platform will produce
 *   bitwise-identical results. This is verified by the test suite.
 */

}  // namespace athena

#endif  // ATHENA_HPP
