// ATHENA Core - Tactical AI System
// Simple seek-nearest-enemy behavior for unit movement.
// Used by both interactive simulation and Monte Carlo executor.
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_SYSTEMS_TACTICAL_AI_HPP
#define ATHENA_SYSTEMS_TACTICAL_AI_HPP

#include "athena/types.hpp"
#include "athena/entities.hpp"
#include "athena/rng.hpp"
#include "athena/terrain_semantics.hpp"
#include "athena/pathfinding.hpp"
#include <atomic>
#include <vector>
#include <utility>

namespace athena {
namespace systems {

// Dt_seconds used by tactical AI for displacement calculation.
// Must be set before registering the AI with a scheduler.
// Atomic so it can be set from any thread before MC iterations.
extern std::atomic<f64> g_tactical_ai_dt_seconds;

// Optional terrain pointer for obstacle avoidance.
// Set before simulation start. Null = no terrain avoidance.
extern const TerrainSemantics* g_tactical_ai_terrain;

// v1.2.5: Optional pathfinder for A* obstacle navigation.
// Set before simulation start. Null = fallback to straight-line + offset avoidance.
extern const Pathfinder* g_tactical_ai_pathfinder;

// v1.2.5: Per-entity cached path state (managed by seek_enemy_ai_update)
struct EntityPathCache {
    std::vector<std::pair<f64, f64>> waypoints;
    usize current_wp = 0;
    f64 goal_x = 0, goal_y = 0;   // Target when path was computed
    Tick path_tick = 0;            // Tick when path was last computed
    bool has_path = false;
};

// v1.2.5: Initialize/resize path cache for N entities. Call before simulation.
void tactical_ai_init_path_cache(usize entity_count);
// v1.2.5: Clear all cached paths. Call on simulation reset.
void tactical_ai_clear_path_cache();

// Movement parameters
constexpr f64 CRUISE_SPEED_MPS  = 5.0;    // ~18 km/h (infantry march)
constexpr f64 APPROACH_FACTOR   = 0.5;    // Slow down when in engagement range
constexpr f64 STANDOFF_FACTOR   = 0.8;    // Hold at 80% of engagement range

// Seek-enemy AI: each entity moves toward nearest detected enemy.
// Stops at standoff distance (80% of engagement range) and holds.
// Deterministic: processes entities in index order.
Status seek_enemy_ai_update(EntityStorage& storage, Rng& rng, Tick tick);

// Morale propagation: nearby friendly units boost morale via morale_impact.
// Runs as POST_TICK. Morale capped at [0.0, 1.0].
// Range: 5000m radius. Effect diminishes with distance.
Status morale_propagation_update(EntityStorage& storage, Rng& rng, Tick tick);

}  // namespace systems
}  // namespace athena

#endif  // ATHENA_SYSTEMS_TACTICAL_AI_HPP
