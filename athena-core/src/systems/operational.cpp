// ATHENA Core - Operational Wear System Implementation (v1.1.8)
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/systems/operational.hpp"
#include <cmath>

namespace athena {
namespace systems {

// =============================================================================
// PRE_TICK: Capture health snapshot for casualty rate computation
// =============================================================================

Status operational_snapshot_update(EntityStorage& storage, Rng& /*rng*/, Tick /*tick*/) {
    // Resize if needed (first tick or capacity change)
    if (g_prev_tick_health.size() < storage.count) {
        g_prev_tick_health.resize(storage.capacity, 1.0);
    }

    // Snapshot current health BEFORE combat resolves this tick
    for (usize i = 0; i < storage.count; ++i) {
        g_prev_tick_health[i] = storage.health[i];
    }

    return Status();
}

// =============================================================================
// POST_TICK: Accumulate fatigue, compute cohesion, check collapse
// =============================================================================

Status operational_wear_update(EntityStorage& storage, Rng& /*rng*/, Tick /*tick*/) {
    const auto& cfg = g_operational_config;

    // v1.2.1: dt-scaling — all rates are tuned for 1-hour ticks (3600s).
    // Scale linearly so 15min ticks accumulate 1/4 per tick, etc.
    const f64 dt_scale = cfg.dt_seconds / 3600.0;

    // v1.2.3: rest threshold in ticks derived from rest_seconds_for_recovery
    // At 1h ticks: 10800s / 3600s = 3 ticks.  At 15min ticks: 10800s / 900s = 12 ticks.
    const u32 rest_ticks_needed = static_cast<u32>(
        std::max(1.0, cfg.rest_seconds_for_recovery / cfg.dt_seconds));

    for (usize i = 0; i < storage.count; ++i) {
        if (!storage.is_active(i)) continue;

        const bool is_moving  = (storage.flags[i] & entity_flags::MOVING)  != 0;
        const bool is_engaged = (storage.flags[i] & entity_flags::ENGAGED) != 0;
        const bool is_stressed = is_moving || is_engaged;

        // =================================================================
        // A) Casualty rate — rolling accumulator
        // =================================================================
        f64 prev_health = (i < g_prev_tick_health.size()) ? g_prev_tick_health[i] : 1.0;
        f64 damage_this_tick = prev_health - storage.health[i];
        if (damage_this_tick < 0.0) damage_this_tick = 0.0;

        // Decay uses dt_scale so shorter ticks decay proportionally less per tick
        f64 decay_per_tick = 1.0 - (1.0 - cfg.casualty_decay_rate) * dt_scale;
        storage.casualty_rate[i] = storage.casualty_rate[i] * decay_per_tick
                                 + damage_this_tick;
        storage.casualty_rate[i] = safe::clamp(storage.casualty_rate[i], 0.0, 1.0);

        // =================================================================
        // B) Stress / rest tick counters
        // =================================================================
        if (is_stressed) {
            storage.stress_ticks[i]++;
            storage.rest_ticks[i] = 0;
        } else {
            storage.rest_ticks[i]++;
            if (storage.stress_ticks[i] > 0 &&
                storage.rest_ticks[i] >= rest_ticks_needed) {
                storage.stress_ticks[i]--;
            }
        }

        // =================================================================
        // C) Fatigue accumulation (dt-scaled)
        // =================================================================
        if (is_engaged) {
            storage.fatigue[i] += cfg.fatigue_rate_engaged * dt_scale;
        } else if (is_moving) {
            storage.fatigue[i] += cfg.fatigue_rate_moving * dt_scale;
        } else {
            storage.fatigue[i] += cfg.fatigue_rate_idle * dt_scale;
        }

        // Recovery (dt-scaled)
        if (!is_stressed && storage.rest_ticks[i] >= rest_ticks_needed) {
            storage.fatigue[i] -= cfg.fatigue_recovery_rate * dt_scale;
        }

        storage.fatigue[i] = safe::clamp(storage.fatigue[i], 0.0, 1.0);

        // Set/clear FATIGUED flag
        if (storage.fatigue[i] >= cfg.fatigue_threshold) {
            storage.flags[i] |= entity_flags::FATIGUED;
        } else {
            storage.flags[i] &= ~entity_flags::FATIGUED;
        }

        // =================================================================
        // C2) v1.2.0: Suppression decay (dt-scaled)
        // =================================================================
        if (is_engaged) {
            storage.suppression[i] -= 0.05 * dt_scale;
        } else {
            storage.suppression[i] -= 0.25 * dt_scale;
        }
        storage.suppression[i] = safe::clamp(storage.suppression[i], 0.0, 1.0);

        // =================================================================
        // D) Cohesion computation (dt-scaled)
        // =================================================================
        f64 cohesion_delta = 0.0;

        // D.1) Casualty pressure
        cohesion_delta -= storage.casualty_rate[i] * cfg.cohesion_casualty_factor * dt_scale;

        // D.2) Fatigue pressure
        if (storage.fatigue[i] > 0.5) {
            f64 excess = storage.fatigue[i] - 0.5;
            cohesion_delta -= excess * cfg.cohesion_fatigue_factor * dt_scale;
        }

        // D.3) Isolation check
        Side my_side = storage.side[i];
        f64 my_x = storage.pos_x[i];
        f64 my_y = storage.pos_y[i];
        f64 radius_sq = cfg.cohesion_friendly_radius * cfg.cohesion_friendly_radius;

        bool has_nearby_friendly = false;
        u32 nearby_count = 0;
        for (usize j = 0; j < storage.count; ++j) {
            if (i == j) continue;
            if (!storage.is_active(j)) continue;
            if (storage.side[j] != my_side) continue;

            f64 dx = storage.pos_x[j] - my_x;
            f64 dy = storage.pos_y[j] - my_y;
            if (dx * dx + dy * dy <= radius_sq) {
                has_nearby_friendly = true;
                nearby_count++;
            }
        }

        if (!has_nearby_friendly) {
            cohesion_delta -= cfg.cohesion_isolation_penalty * dt_scale;
        }

        // D.4) Recovery
        bool can_recover = !is_engaged
                        && has_nearby_friendly
                        && storage.casualty_rate[i] < 0.01
                        && storage.cohesion[i] >= cfg.cohesion_min_for_recovery;
        if (can_recover) {
            f64 support_factor = std::min(1.0, nearby_count * 0.25);
            cohesion_delta += cfg.cohesion_recovery_rate * (1.0 + support_factor) * dt_scale;
        }

        storage.cohesion[i] += cohesion_delta;
        storage.cohesion[i] = safe::clamp(storage.cohesion[i], 0.0, 1.0);

        // =================================================================
        // E) Cohesion collapse check
        // =================================================================
        if (storage.cohesion[i] < cfg.cohesion_collapse_threshold) {
            storage.flags[i] |= entity_flags::BROKEN;
            storage.vel_x[i] = 0.0;
            storage.vel_y[i] = 0.0;
        } else {
            storage.flags[i] &= ~entity_flags::BROKEN;
        }
    }

    return Status();
}

}  // namespace systems
}  // namespace athena
