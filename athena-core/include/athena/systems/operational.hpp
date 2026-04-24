// ATHENA Core - Operational Wear System (v1.1.8)
// Contract: Fatigue accumulation, force cohesion, attrition memory.
//
// This system models the cumulative degradation of units over time.
// Unlike health/morale which are per-tick reactive, operational wear
// accumulates across the entire simulation — units that fight for
// days become less effective regardless of supply or morale.
//
// Key concepts:
//   Fatigue:  Physical/mental exhaustion. Increases when moving or fighting,
//             recovers slowly when resting. Degrades all performance.
//   Cohesion: Organizational effectiveness. Degrades with casualties,
//             high fatigue, isolation from friendlies. Below threshold
//             (COHESION_COLLAPSE = 0.25), unit is BROKEN — physically
//             alive but operationally dead. Cannot fight or coordinate.
//   Casualty Rate: Rolling accumulator of health lost, decayed per tick.
//             Sustained combat degrades cohesion faster than a single hit.
//
// RULES:
// - All accumulation is deterministic (no RNG)
// - Runs as POST_TICK phase (after combat resolves)
// - Broken units still occupy space but cannot engage or move purposefully
// - Cohesion recovery is very slow (reorganization takes time)
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_SYSTEMS_OPERATIONAL_HPP
#define ATHENA_SYSTEMS_OPERATIONAL_HPP

#include "athena/types.hpp"
#include "athena/rng.hpp"
#include "athena/entities.hpp"

namespace athena {
namespace systems {

// =============================================================================
// Configuration
// =============================================================================

struct OperationalConfig {
    // --- Time scaling ---
    f64 dt_seconds = 3600.0;  // Tick duration in seconds (rates tuned for 3600s = 1h)

    // --- Fatigue ---
    f64 fatigue_rate_moving   = 0.015;   // Per tick when moving (at 1h ticks)
    f64 fatigue_rate_engaged  = 0.025;   // Per tick when in combat
    f64 fatigue_rate_idle     = 0.003;   // Per tick even when idle (alertness cost)
    f64 fatigue_recovery_rate = 0.010;   // Per tick when resting (not moving, not engaged)
    f64 rest_seconds_for_recovery = 10800.0; // 3 hours of rest before recovery starts
    f64 fatigue_threshold     = 0.7;     // Above this: FATIGUED flag set

    // --- Cohesion ---
    f64 cohesion_collapse_threshold = 0.25;  // Below this: BROKEN
    f64 cohesion_casualty_factor    = 0.30;  // How much casualty_rate degrades cohesion
    f64 cohesion_fatigue_factor     = 0.10;  // How much fatigue degrades cohesion
    f64 cohesion_isolation_penalty  = 0.02;  // Per tick when no friendlies within radius
    f64 cohesion_friendly_radius    = 5000.0; // Meters — friendlies within this stabilize
    f64 cohesion_recovery_rate      = 0.003;  // Very slow recovery when conditions good
    f64 cohesion_min_for_recovery   = 0.10;   // Cannot recover below this without reorganization

    // --- Casualty Rate ---
    f64 casualty_decay_rate = 0.5;  // Per tick — exponential decay of rolling average
    // casualty_rate[i] = casualty_rate[i] * decay + new_damage_this_tick
};

// =============================================================================
// Operational Wear System
// =============================================================================

/// Update operational wear for all entities.
/// Must run as POST_TICK (after combat has resolved, damage applied).
/// Reads: flags (MOVING, ENGAGED), health (for casualty tracking), position
/// Writes: fatigue, cohesion, stress_ticks, rest_ticks, casualty_rate, flags (FATIGUED, BROKEN)
Status operational_wear_update(EntityStorage& storage, Rng& rng, Tick tick);

// Global config — set before simulation starts
inline OperationalConfig g_operational_config;

// Previous-tick health snapshot for casualty rate computation.
// Must be captured at PRE_TICK before combat resolves.
inline std::vector<f64> g_prev_tick_health;

/// Capture health snapshot. Call as PRE_TICK system.
Status operational_snapshot_update(EntityStorage& storage, Rng& rng, Tick tick);

}  // namespace systems
}  // namespace athena

#endif  // ATHENA_SYSTEMS_OPERATIONAL_HPP
