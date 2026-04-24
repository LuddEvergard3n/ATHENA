// ATHENA Core - Monte Carlo Profiling Benchmark
// Measures throughput for 100k simulation iterations.
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/analysis/montecarlo.hpp"
#include "athena/scenario.hpp"
#include "athena/platform_loader.hpp"
#include <chrono>
#include <cstdio>

using namespace athena;
using namespace athena::analysis;

// Build a minimal test scenario: 4 Blue vs 4 Red tanks
static Scenario make_test_scenario() {
    Scenario sc;
    sc.name = "Benchmark: 4v4 Tank Engagement";
    sc.temporal.max_ticks = 72;
    sc.temporal.tick_duration_seconds = 3600.0;
    sc.spatial_bounds.min_lat = 48.0;
    sc.spatial_bounds.max_lat = 48.1;
    sc.spatial_bounds.min_lon = 11.0;
    sc.spatial_bounds.max_lon = 11.1;

    auto make_actor = [](const char* id, const char* name, const char* side,
                         f64 lat, f64 lon, const char* platform) {
        ActorDefinition a;
        a.id = id;
        a.name = name;
        a.type = "unit";
        a.side = side;
        a.platform_id = platform;
        a.initial_position = {lat, lon, 0.0};
        a.initial_health = 1.0;
        a.initial_supply = 1.0;
        a.initial_morale = 1.0;
        a.initial_readiness = 1.0;
        // Fallback capabilities (for when platform DB not loaded)
        FirepowerCapability fp;
        fp.base_firepower = 10.0;
        fp.range_km = 4.0;
        a.firepower = fp;
        SensorCapability sn;
        sn.detection_range_km = 5.0;
        a.sensors = sn;
        return a;
    };

    // Blue team (west)
    sc.actors.push_back(make_actor("b1", "Blue-1", "blue", 48.05, 11.02, "de-leopard2a4"));
    sc.actors.push_back(make_actor("b2", "Blue-2", "blue", 48.04, 11.02, "de-leopard2a4"));
    sc.actors.push_back(make_actor("b3", "Blue-3", "blue", 48.06, 11.02, "de-leopard2a4"));
    sc.actors.push_back(make_actor("b4", "Blue-4", "blue", 48.03, 11.02, "de-leopard2a4"));

    // Red team (east)
    sc.actors.push_back(make_actor("r1", "Red-1", "red", 48.05, 11.08, "ru-t72b3"));
    sc.actors.push_back(make_actor("r2", "Red-2", "red", 48.04, 11.08, "ru-t72b3"));
    sc.actors.push_back(make_actor("r3", "Red-3", "red", 48.06, 11.08, "ru-t72b3"));
    sc.actors.push_back(make_actor("r4", "Red-4", "red", 48.03, 11.08, "ru-t72b3"));

    return sc;
}

int main(int argc, char** argv) {
    u32 num_iterations = 100000;
    if (argc > 1) {
        num_iterations = static_cast<u32>(std::atoi(argv[1]));
    }

    printf("=== ATHENA Monte Carlo Benchmark ===\n");
    printf("Iterations: %u\n", num_iterations);

    // Load platform database
    PlatformDatabase db;
    auto db_result = db.load_all("data/platforms");
    if (db_result.ok()) {
        printf("Platforms loaded: %zu\n", db.count());
    } else {
        printf("Platform DB not loaded (running without platform data)\n");
    }

    Scenario scenario = make_test_scenario();
    printf("Scenario: %zu actors\n", scenario.actors.size());

    // Configure MC
    BatchConfig config;
    config.num_iterations = num_iterations;
    config.master_seed = 42;
    config.stop_on_decisive = true;
    config.decisive_threshold = 0.01;
    config.max_ticks_per_iteration = 72;
    config.entity_capacity = 64;  // 8 entities, 64 is plenty
    config.record_timeline = false;

    // Diagnostic: run 1 iteration manually and inspect
    if (num_iterations <= 10) {
        printf("\n--- Diagnostic: Manual single iteration ---\n");
        EntityManager em;
        em.init(64);
        Rng conv_rng(42, 0x5C3A1210ULL);
        ScenarioConverter converter;
        if (db_result.ok()) converter.set_platform_database(&db);
        auto conv = converter.convert(scenario, em, conv_rng);
        if (conv.ok()) {
            auto& st = em.storage();
            printf("Entities created: %zu\n", st.count);
            for (usize i = 0; i < st.count; ++i) {
                auto& c = st.combat[i];
                printf("  [%zu] side=%d platform=%d pen=%.0f armor_f=%.0f eng_range=%.0f det=%.0f speed=%.1f\n",
                    i, (int)st.side[i], (int)c.has_platform_data,
                    c.penetration_mm, c.armor_front_mm,
                    st.engagement_range[i], st.detection_range[i],
                    c.max_speed_offroad_mps);
            }
        } else {
            printf("Convert failed!\n");
        }
        printf("--- End diagnostic ---\n\n");
    }

    MonteCarloExecutor executor;
    executor.configure(config);
    executor.set_scenario(scenario);
    if (db_result.ok()) {
        executor.set_platform_database(&db);
    }

    // Run benchmark
    printf("\nRunning %u iterations...\n", num_iterations);
    auto t0 = std::chrono::high_resolution_clock::now();

    auto status = executor.execute();

    auto t1 = std::chrono::high_resolution_clock::now();
    f64 elapsed_ms = std::chrono::duration<f64, std::milli>(t1 - t0).count();
    f64 elapsed_s = elapsed_ms / 1000.0;

    if (!status.ok()) {
        printf("ERROR: MC execution failed\n");
        return 1;
    }

    // Results
    auto stats = executor.compute_statistics();
    f64 iter_per_sec = num_iterations / elapsed_s;
    f64 us_per_iter = (elapsed_ms * 1000.0) / num_iterations;

    printf("\n=== Results ===\n");
    printf("Time:         %.2f s\n", elapsed_s);
    printf("Throughput:   %.0f iterations/sec\n", iter_per_sec);
    printf("Per-iter:     %.1f us\n", us_per_iter);
    printf("Completed:    %u / %u\n", stats.completed_iterations, stats.total_iterations);
    printf("Blue wins:    %u (%.1f%%)\n", stats.blue_wins, 100.0 * stats.blue_wins / stats.completed_iterations);
    printf("Red wins:     %u (%.1f%%)\n", stats.red_wins, 100.0 * stats.red_wins / stats.completed_iterations);
    printf("Draws:        %u (%.1f%%)\n", stats.draws, 100.0 * stats.draws / stats.completed_iterations);
    printf("Avg ticks:    %.1f (median: %.1f)\n",
           stats.ticks_to_completion.mean, stats.ticks_to_completion.median);
    printf("Blue surv:    %.1f%% [%.1f%% - %.1f%% CI90]\n",
           stats.blue_survival_rate.mean * 100,
           stats.blue_survival_rate.p5 * 100,
           stats.blue_survival_rate.p95 * 100);
    printf("Red surv:     %.1f%% [%.1f%% - %.1f%% CI90]\n",
           stats.red_survival_rate.mean * 100,
           stats.red_survival_rate.p5 * 100,
           stats.red_survival_rate.p95 * 100);

    // Performance breakdown estimate
    printf("\n=== Performance Analysis ===\n");
    printf("Entity cap:   %zu per iteration\n", config.entity_capacity);
    printf("Max ticks:    %lu per iteration\n", config.max_ticks_per_iteration);
    printf("Avg ticks:    %.0f (auto-stopped on decisive)\n", stats.ticks_to_completion.mean);
    printf("Target 100k:  %s\n", num_iterations >= 100000 ? "YES" : "NO");
    if (num_iterations >= 100000) {
        printf("100k time:    %.2f s\n", elapsed_s);
    } else {
        f64 est_100k = (100000.0 / num_iterations) * elapsed_s;
        printf("Est 100k:     %.2f s\n", est_100k);
    }

    return 0;
}
