// ATHENA Core - Tactical AI System Implementation
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/systems/tactical_ai.hpp"
#include "athena/systems/detection.hpp"
#include <cmath>

namespace athena {
namespace systems {

std::atomic<f64> g_tactical_ai_dt_seconds{3600.0};
const TerrainSemantics* g_tactical_ai_terrain = nullptr;
const Pathfinder* g_tactical_ai_pathfinder = nullptr;

// v1.2.5: Path cache — one per entity, resized at sim start
static std::vector<EntityPathCache> s_path_cache;

void tactical_ai_init_path_cache(usize entity_count) {
    s_path_cache.clear();
    s_path_cache.resize(entity_count);
}

void tactical_ai_clear_path_cache() {
    s_path_cache.clear();
}

// v1.2.5: Pathfinding parameters
constexpr Tick   REPATH_INTERVAL   = 5;     // Re-path every N ticks
constexpr f64    REPATH_DIST_SQ    = 500.0 * 500.0;  // Re-path if target moved >500m
constexpr f64    WP_ARRIVAL_DIST   = 150.0; // Advance to next waypoint when within 150m

// Check if a world position is passable for ground units
static bool is_passable_at(f64 x, f64 y) {
    if (!g_tactical_ai_terrain) return true;
    return g_tactical_ai_terrain->is_passable(x, y, 0, MovementType::TRACKED);
}

Status seek_enemy_ai_update(EntityStorage& storage, Rng& /*rng*/, Tick /*tick*/) {
    const f64 dt = g_tactical_ai_dt_seconds.load();

    for (usize i = 0; i < storage.count; ++i) {
        if (!storage.is_active(i)) continue;

        // v1.1.8: Broken units cannot act — they're operationally dead
        if ((storage.flags[i] & entity_flags::BROKEN) != 0) {
            storage.vel_x[i] = 0.0;
            storage.vel_y[i] = 0.0;
            continue;
        }

        Side my_side = storage.side[i];
        f64 my_x = storage.pos_x[i];
        f64 my_y = storage.pos_y[i];
        f64 detect_range = storage.detection_range[i];
        f64 engage_range = storage.engagement_range[i];

        // Use real platform speed if available, else UnitType modifier
        f64 unit_cruise;
        const auto& cp = storage.combat[i];
        if (cp.has_platform_data && cp.max_speed_offroad_mps > 0.0) {
            unit_cruise = cp.max_speed_offroad_mps;
        } else {
            unit_cruise = CRUISE_SPEED_MPS *
                get_unit_modifiers(storage.unit_type[i]).mobility_factor;
        }

        // v1.1.8: Fatigue reduces movement speed
        f64 fatigue_factor = 1.0 - storage.fatigue[i] * 0.4;  // Up to 40% speed loss
        unit_cruise *= fatigue_factor;

        // =====================================================================
        // Target acquisition — v1.1.8: uses detection contacts, not global knowledge
        //
        // Phase 1 (Direct): find nearest enemy within own sensor range (LOS)
        // Phase 2 (Intel):  use detection system contacts (shared via recon network)
        // Phase 3 (Blind):  no contacts → hold position
        // =====================================================================
        f64 best_tac_dist = 1e18;
        usize best_tac_target = SIZE_MAX;
        f64 best_intel_dist = 1e18;
        f64 best_intel_x = 0.0, best_intel_y = 0.0;
        bool has_intel_target = false;

        f64 eff_detect = (detect_range > 0.0) ? detect_range : 1e18;

        // Phase 1: Direct detection — within own sensor range
        for (usize j = 0; j < storage.count; ++j) {
            if (i == j) continue;
            if (!storage.is_active(j)) continue;
            if (storage.side[j] == my_side) continue;
            if (storage.side[j] == Side::NEUTRAL) continue;

            f64 dx = storage.pos_x[j] - my_x;
            f64 dy = storage.pos_y[j] - my_y;
            f64 dist = std::sqrt(dx * dx + dy * dy);

            if (dist < best_tac_dist && dist < eff_detect) {
                best_tac_dist = dist;
                best_tac_target = j;
            }
        }

        // Phase 2: Intel from detection system (shared contacts via recon propagation)
        if (best_tac_target == SIZE_MAX && g_detection_system) {
            const auto& contacts = g_detection_system->get_contacts(i);
            for (const auto& c : contacts) {
                if (c.type == ContactType::FRIENDLY) continue;
                // Use last known position from contact
                f64 dx = c.last_x - my_x;
                f64 dy = c.last_y - my_y;
                f64 dist = std::sqrt(dx * dx + dy * dy);
                if (dist < best_intel_dist) {
                    best_intel_dist = dist;
                    best_intel_x = c.last_x;
                    best_intel_y = c.last_y;
                    has_intel_target = true;
                }
            }
        }

        // Determine target and mode
        f64 target_x, target_y;
        bool is_tactical;

        if (best_tac_target != SIZE_MAX) {
            // Direct contact — can engage
            target_x = storage.pos_x[best_tac_target];
            target_y = storage.pos_y[best_tac_target];
            is_tactical = true;
        } else if (has_intel_target) {
            // Intel contact — march toward last known position
            target_x = best_intel_x;
            target_y = best_intel_y;
            is_tactical = false;
        } else {
            // Phase 3: No intel — hold position
            storage.vel_x[i] = 0.0;
            storage.vel_y[i] = 0.0;
            continue;
        }

        f64 dx = target_x - my_x;
        f64 dy = target_y - my_y;
        f64 dist = std::sqrt(dx * dx + dy * dy);
        if (dist < 1.0) dist = 1.0;

        // Decide speed based on distance to target and awareness phase
        f64 standoff = engage_range * STANDOFF_FACTOR;
        f64 speed_mps;

        if (is_tactical) {
            if (dist <= standoff) {
                speed_mps = 0.0;
            } else if (dist <= engage_range) {
                speed_mps = unit_cruise * APPROACH_FACTOR;
            } else {
                speed_mps = unit_cruise;
            }
        } else {
            // Intel phase: march toward last known position at cruise speed
            speed_mps = unit_cruise;
        }

        // Displacement = speed * dt
        f64 disp = speed_mps * dt;

        // v1.2.3: Suppression affects movement — suppressed units halt or crawl
        f64 supp = storage.suppression[i];
        if (supp > 0.7) {
            disp = 0.0;
            speed_mps = 0.0;
        } else if (supp > 0.3) {
            f64 supp_factor = 1.0 - (supp - 0.3) / 0.4;
            disp *= supp_factor;
            speed_mps *= supp_factor;
        }

        // Don't overshoot the standoff distance (tactical only)
        if (is_tactical && dist - disp < standoff && dist > standoff) {
            disp = dist - standoff;
        }

        // =====================================================================
        // v1.2.5: A* pathfinding — compute direction from waypoint path
        //
        // If pathfinder is available: compute A* path to target, follow waypoints.
        // Fallback: straight-line with angular offset obstacle avoidance (legacy).
        // Path is cached per entity, recomputed when target moves or path is stale.
        // =====================================================================

        f64 nx, ny;   // Movement direction
        f64 next_x, next_y;

        bool use_pathfinding = (g_tactical_ai_pathfinder != nullptr &&
                                i < s_path_cache.size() &&
                                disp > 0.0);

        if (use_pathfinding) {
            auto& pc = s_path_cache[i];
            Tick current_tick = static_cast<Tick>(dt);  // Approximate

            // Determine if path needs recomputing
            bool need_repath = false;
            if (!pc.has_path) {
                need_repath = true;
            } else if (pc.current_wp >= pc.waypoints.size()) {
                need_repath = true;  // Reached end of path
            } else {
                // Target moved significantly?
                f64 gdx = target_x - pc.goal_x;
                f64 gdy = target_y - pc.goal_y;
                if (gdx * gdx + gdy * gdy > REPATH_DIST_SQ) {
                    need_repath = true;
                }
            }

            // Compute new A* path
            if (need_repath) {
                // Determine movement type for this entity
                MovementType mt = MovementType::FOOT;
                if (cp.drive_type_code == 1) mt = MovementType::WHEELED;
                else if (cp.drive_type_code == 2) mt = MovementType::TRACKED;

                auto result = g_tactical_ai_pathfinder->find_path(
                    my_x, my_y, target_x, target_y, 0, mt);

                if (result.found && result.path.size() >= 2) {
                    // Smooth path to reduce waypoints
                    pc.waypoints = g_tactical_ai_pathfinder->smooth_path(
                        result.path, 0, mt);
                    pc.current_wp = 1;  // Skip first waypoint (current pos)
                    pc.goal_x = target_x;
                    pc.goal_y = target_y;
                    pc.has_path = true;
                } else {
                    // No path found — clear and fall through to legacy
                    pc.has_path = false;
                }
            }

            // Follow waypoint path
            if (pc.has_path && pc.current_wp < pc.waypoints.size()) {
                auto [wp_x, wp_y] = pc.waypoints[pc.current_wp];

                // Check if we've arrived at current waypoint
                f64 wp_dx = wp_x - my_x;
                f64 wp_dy = wp_y - my_y;
                f64 wp_dist = std::sqrt(wp_dx * wp_dx + wp_dy * wp_dy);

                if (wp_dist < WP_ARRIVAL_DIST && pc.current_wp + 1 < pc.waypoints.size()) {
                    pc.current_wp++;
                    auto [nwp_x, nwp_y] = pc.waypoints[pc.current_wp];
                    wp_dx = nwp_x - my_x;
                    wp_dy = nwp_y - my_y;
                    wp_dist = std::sqrt(wp_dx * wp_dx + wp_dy * wp_dy);
                }

                if (wp_dist > 0.01) {
                    nx = wp_dx / wp_dist;
                    ny = wp_dy / wp_dist;
                } else {
                    nx = dx / dist;
                    ny = dy / dist;
                }

                // Clamp displacement to not overshoot waypoint
                if (disp > wp_dist) disp = wp_dist;

                next_x = my_x + nx * disp;
                next_y = my_y + ny * disp;
            } else {
                // Path exhausted or empty — direct line (close to target)
                nx = dx / dist;
                ny = dy / dist;
                next_x = my_x + nx * disp;
                next_y = my_y + ny * disp;
            }
        } else {
            // Legacy: straight-line movement with angular offset avoidance
            nx = dx / dist;
            ny = dy / dist;
            next_x = my_x + nx * disp;
            next_y = my_y + ny * disp;

            if (disp > 0.0 && !is_passable_at(next_x, next_y)) {
                constexpr f64 OFFSETS[] = {
                    0.3927, -0.3927,
                    0.7854, -0.7854,
                    1.1781, -1.1781,
                    1.5708, -1.5708
                };
                f64 base_angle = std::atan2(ny, nx);
                bool found = false;

                for (f64 off : OFFSETS) {
                    f64 a = base_angle + off;
                    f64 try_x = my_x + std::cos(a) * disp;
                    f64 try_y = my_y + std::sin(a) * disp;
                    if (is_passable_at(try_x, try_y)) {
                        nx = std::cos(a);
                        ny = std::sin(a);
                        next_x = try_x;
                        next_y = try_y;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    storage.vel_x[i] = 0.0;
                    storage.vel_y[i] = 0.0;
                    storage.heading[i] = std::atan2(dy, dx);
                    continue;
                }
            }
        }

        storage.pos_x[i] = next_x;
        storage.pos_y[i] = next_y;
        storage.vel_x[i] = nx * speed_mps;
        storage.vel_y[i] = ny * speed_mps;

        // v1.2.1: Threat-facing — orient toward target even when stationary.
        // Moving units get heading from movement system (velocity vector).
        // Stationary units (at standoff, holding) face the threat directly.
        if (speed_mps < 0.01) {
            storage.heading[i] = std::atan2(dy, dx);
        }
    }

    return Status();
}

// =============================================================================
// Morale Propagation
// =============================================================================
// Each entity gains morale from nearby friendly units based on their UnitType's
// morale_impact. Medical units boost morale the most. Effect decays with distance.
// Also: seeing friendly casualties reduces morale.

Status morale_propagation_update(EntityStorage& storage, Rng& /*rng*/, Tick /*tick*/) {
    constexpr f64 MORALE_RADIUS = 5000.0;       // 5km influence radius
    constexpr f64 MORALE_RADIUS_SQ = MORALE_RADIUS * MORALE_RADIUS;
    constexpr f64 MORALE_RECOVERY_RATE = 0.005;  // Per-tick passive recovery
    constexpr f64 CASUALTY_MORALE_PENALTY = 0.02; // Per nearby dead friendly

    for (usize i = 0; i < storage.count; ++i) {
        if (!storage.is_active(i)) continue;

        Side my_side = storage.side[i];
        f64 my_x = storage.pos_x[i];
        f64 my_y = storage.pos_y[i];

        f64 morale_boost = MORALE_RECOVERY_RATE;  // Passive baseline
        f64 morale_penalty = 0.0;

        for (usize j = 0; j < storage.count; ++j) {
            if (i == j) continue;
            if (storage.side[j] != my_side) continue;

            f64 dx = storage.pos_x[j] - my_x;
            f64 dy = storage.pos_y[j] - my_y;
            f64 dist_sq = dx * dx + dy * dy;
            if (dist_sq > MORALE_RADIUS_SQ) continue;

            f64 dist = std::sqrt(dist_sq);
            f64 proximity = 1.0 - (dist / MORALE_RADIUS);  // 1.0 at 0m, 0.0 at 5km

            if (storage.is_active(j)) {
                // Alive friendly: morale boost based on their type
                f64 impact = get_unit_modifiers(storage.unit_type[j]).morale_impact;
                morale_boost += impact * proximity * 0.1;
            } else {
                // Dead friendly: morale penalty
                morale_penalty += CASUALTY_MORALE_PENALTY * proximity;
            }
        }

        storage.morale[i] += morale_boost - morale_penalty;

        // Clamp to [0, 1]
        if (storage.morale[i] > 1.0) storage.morale[i] = 1.0;
        if (storage.morale[i] < 0.0) storage.morale[i] = 0.0;
    }

    return Status();
}

}  // namespace systems
}  // namespace athena
