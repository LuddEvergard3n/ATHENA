// ATHENA Core - Integration Test
// Contract: Full simulation with all systems, deterministic results.
//
// This test runs a complete scenario through the simulation pipeline
// and verifies determinism across multiple runs.
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/types.hpp"
#include "athena/rng.hpp"
#include "athena/context.hpp"
#include "athena/entities.hpp"
#include "athena/scheduler.hpp"
#include "athena/scenario.hpp"
#include "athena/systems.hpp"
#include "athena/terrain.hpp"
#include "athena/environment.hpp"
#include "athena/terrain_semantics.hpp"

#include <iostream>
#include <vector>
#include <cmath>

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
// State Snapshot (for determinism verification)
// =============================================================================

struct StateSnapshot {
    std::vector<f64> pos_x;
    std::vector<f64> pos_y;
    std::vector<f64> pos_z;
    std::vector<f64> health;
    std::vector<f64> supply;
    std::vector<f64> morale;
    std::vector<u8> flags;
    
    void capture(const EntityStorage& storage) {
        pos_x.resize(storage.count);
        pos_y.resize(storage.count);
        pos_z.resize(storage.count);
        health.resize(storage.count);
        supply.resize(storage.count);
        morale.resize(storage.count);
        flags.resize(storage.count);
        
        for (usize i = 0; i < storage.count; ++i) {
            pos_x[i] = storage.pos_x[i];
            pos_y[i] = storage.pos_y[i];
            pos_z[i] = storage.pos_z[i];
            health[i] = storage.health[i];
            supply[i] = storage.supply[i];
            morale[i] = storage.morale[i];
            flags[i] = storage.flags[i];
        }
    }
    
    bool equals(const StateSnapshot& other) const {
        if (pos_x.size() != other.pos_x.size()) return false;
        
        for (usize i = 0; i < pos_x.size(); ++i) {
            if (pos_x[i] != other.pos_x[i]) return false;
            if (pos_y[i] != other.pos_y[i]) return false;
            if (pos_z[i] != other.pos_z[i]) return false;
            if (health[i] != other.health[i]) return false;
            if (supply[i] != other.supply[i]) return false;
            if (morale[i] != other.morale[i]) return false;
            if (flags[i] != other.flags[i]) return false;
        }
        
        return true;
    }
};

// =============================================================================
// Test Scenario Setup
// =============================================================================

struct TestSetup {
    std::unique_ptr<Context> ctx;
    EntityManager entities;
    Scheduler scheduler;
    systems::SystemsBundle systems;
    
    bool init(Seed seed, usize max_ticks) {
        // Create context
        ContextConfig ctx_cfg;
        ctx_cfg.seed = seed;
        ctx_cfg.max_entities = 1000;
        
        auto result = Context::create(ctx_cfg);
        if (!result.ok()) return false;
        ctx = std::move(result.get());
        
        // Initialize entities
        entities.init(1000);
        
        // Initialize systems with test configs
        systems::MovementConfig move_cfg;
        move_cfg.dt_seconds = 3600.0;  // 1 hour ticks
        move_cfg.terrain_effects_enabled = true;
        move_cfg.fuel_required_for_movement = true;
        
        systems::CombatConfig combat_cfg;
        combat_cfg.base_attrition_rate = 0.1;
        combat_cfg.terrain_defense_bonus = 1.5;
        
        systems::LogisticsConfig log_cfg;
        log_cfg.base_fuel_consumption = 0.01;
        log_cfg.base_ammo_consumption = 0.005;
        
        systems.init(1000, move_cfg, combat_cfg, log_cfg);
        
        // Initialize scheduler
        SchedulerConfig sched_cfg;
        sched_cfg.max_ticks = max_ticks;
        sched_cfg.compact_per_tick = true;
        
        auto status = scheduler.init(*ctx, entities, sched_cfg);
        if (!status.ok()) return false;
        
        // Register systems
        status = systems::register_all_systems(scheduler);
        if (!status.ok()) return false;
        
        return true;
    }
    
    void create_test_entities() {
        auto& storage = entities.storage();
        
        // Blue force - 3 units
        for (int i = 0; i < 3; ++i) {
            (void)entities.create();  // Suppress unused warning
            usize idx = storage.count - 1;
            
            storage.side[idx] = Side::BLUE;
            storage.pos_x[idx] = 0.0 + i * 1000.0;
            storage.pos_y[idx] = 0.0;
            storage.pos_z[idx] = 0.0;
            storage.health[idx] = 1.0;
            storage.supply[idx] = 1.0;
            storage.fuel[idx] = 1.0;
            storage.ammo[idx] = 1.0;
            storage.morale[idx] = 0.9;
            storage.readiness[idx] = 0.85;
            storage.firepower[idx] = 10.0;
            storage.engagement_range[idx] = 5000.0;
            storage.detection_range[idx] = 10000.0;
            storage.consumption_rate[idx] = 1.0;
            
            // Set movement
            systems.movement.set_max_speed(idx, 10.0);  // 10 m/s
            systems.movement.set_target(idx, 50000.0, 0.0, 0.0);  // Move east
        }
        
        // Red force - 3 units
        for (int i = 0; i < 3; ++i) {
            (void)entities.create();  // Returns EntityId
            usize idx = storage.count - 1;
            
            storage.side[idx] = Side::RED;
            storage.pos_x[idx] = 100000.0 - i * 1000.0;
            storage.pos_y[idx] = 0.0;
            storage.pos_z[idx] = 0.0;
            storage.health[idx] = 1.0;
            storage.supply[idx] = 1.0;
            storage.fuel[idx] = 1.0;
            storage.ammo[idx] = 1.0;
            storage.morale[idx] = 0.8;
            storage.readiness[idx] = 0.9;
            storage.firepower[idx] = 12.0;
            storage.engagement_range[idx] = 6000.0;
            storage.detection_range[idx] = 8000.0;
            storage.consumption_rate[idx] = 1.0;
            
            // Set movement
            systems.movement.set_max_speed(idx, 8.0);  // 8 m/s
            systems.movement.set_target(idx, 50000.0, 0.0, 0.0);  // Move west
        }
    }
};

// =============================================================================
// Integration Tests
// =============================================================================

bool test_full_simulation_determinism() {
    constexpr Seed SEED = 0xDEADBEEF12345678ULL;
    constexpr usize TICKS = 24;  // 24 hours of simulation
    
    // First run
    TestSetup setup1;
    TEST_ASSERT(setup1.init(SEED, TICKS), "Setup 1 init");
    setup1.create_test_entities();
    
    auto status1 = setup1.scheduler.run_until_complete();
    // Note: May complete early due to max_ticks
    
    StateSnapshot snapshot1;
    snapshot1.capture(setup1.entities.storage());
    
    // Second run (same seed)
    TestSetup setup2;
    TEST_ASSERT(setup2.init(SEED, TICKS), "Setup 2 init");
    setup2.create_test_entities();
    
    auto status2 = setup2.scheduler.run_until_complete();
    
    StateSnapshot snapshot2;
    snapshot2.capture(setup2.entities.storage());
    
    // Compare
    TEST_ASSERT(snapshot1.equals(snapshot2), 
        "Simulation results must be deterministic");
    
    return true;
}

bool test_movement_system() {
    TestSetup setup;
    TEST_ASSERT(setup.init(12345, 10), "Setup init");
    
    // Create single entity
    auto& storage = setup.entities.storage();
    (void)setup.entities.create();
    usize idx = 0;
    
    storage.side[idx] = Side::BLUE;
    storage.pos_x[idx] = 0.0;
    storage.pos_y[idx] = 0.0;
    storage.pos_z[idx] = 0.0;
    storage.fuel[idx] = 1.0;
    storage.ammo[idx] = 1.0;
    storage.supply[idx] = 1.0;
    storage.consumption_rate[idx] = 1.0;
    
    // Set movement: 10 m/s for 1 hour = 36000 meters
    setup.systems.movement.set_max_speed(idx, 10.0);
    setup.systems.movement.set_target(idx, 100000.0, 0.0, 0.0);
    
    // Run 1 tick
    setup.ctx->start();
    auto status = setup.scheduler.run_tick();
    TEST_ASSERT(status.ok(), "Run tick");
    
    // Check movement (should have moved ~36000m in 1 hour tick)
    f64 expected_x = 10.0 * 3600.0;  // speed * dt
    f64 actual_x = storage.pos_x[idx];
    
    TEST_ASSERT(std::abs(actual_x - expected_x) < 1.0, 
        "Movement distance should match expected");
    
    return true;
}

bool test_combat_system() {
    TestSetup setup;
    TEST_ASSERT(setup.init(54321, 10), "Setup init");
    
    auto& storage = setup.entities.storage();
    
    // Create two opposing units within range
    (void)setup.entities.create();  // Blue entity
    usize blue_idx = 0;
    storage.side[blue_idx] = Side::BLUE;
    storage.pos_x[blue_idx] = 0.0;
    storage.pos_y[blue_idx] = 0.0;
    storage.health[blue_idx] = 1.0;
    storage.supply[blue_idx] = 1.0;
    storage.fuel[blue_idx] = 1.0;
    storage.ammo[blue_idx] = 1.0;
    storage.morale[blue_idx] = 1.0;
    storage.readiness[blue_idx] = 1.0;
    storage.firepower[blue_idx] = 10.0;
    storage.engagement_range[blue_idx] = 10000.0;
    
    (void)setup.entities.create();  // Red entity
    usize red_idx = 1;
    storage.side[red_idx] = Side::RED;
    storage.pos_x[red_idx] = 5000.0;  // 5km away (within range)
    storage.pos_y[red_idx] = 0.0;
    storage.health[red_idx] = 1.0;
    storage.supply[red_idx] = 1.0;
    storage.fuel[red_idx] = 1.0;
    storage.ammo[red_idx] = 1.0;
    storage.morale[red_idx] = 1.0;
    storage.readiness[red_idx] = 1.0;
    storage.firepower[red_idx] = 10.0;
    storage.engagement_range[red_idx] = 10000.0;
    
    // Run combat
    setup.ctx->start();
    auto status = setup.scheduler.run_tick();
    TEST_ASSERT(status.ok(), "Run tick");
    
    // Both should have taken damage
    TEST_ASSERT(storage.health[blue_idx] < 1.0, "Blue should take damage");
    TEST_ASSERT(storage.health[red_idx] < 1.0, "Red should take damage");
    
    // Check engagements recorded
    const auto& engagements = setup.systems.combat.last_engagements();
    TEST_ASSERT(!engagements.empty(), "Should have engagements");
    
    return true;
}

bool test_logistics_system() {
    TestSetup setup;
    TEST_ASSERT(setup.init(11111, 10), "Setup init");
    
    auto& storage = setup.entities.storage();
    
    // Create unit with consumption
    (void)setup.entities.create();
    usize idx = 0;
    storage.side[idx] = Side::BLUE;
    storage.fuel[idx] = 0.5;
    storage.ammo[idx] = 0.5;
    storage.supply[idx] = 0.5;
    storage.morale[idx] = 0.8;
    storage.consumption_rate[idx] = 1.0;
    
    f64 initial_fuel = storage.fuel[idx];
    f64 initial_ammo = storage.ammo[idx];
    
    // Run logistics
    setup.ctx->start();
    auto status = setup.scheduler.run_tick();
    TEST_ASSERT(status.ok(), "Run tick");
    
    // Should have consumed supplies
    TEST_ASSERT(storage.fuel[idx] < initial_fuel, "Fuel should decrease");
    TEST_ASSERT(storage.ammo[idx] < initial_ammo, "Ammo should decrease");
    
    return true;
}

bool test_supply_point_resupply() {
    TestSetup setup;
    TEST_ASSERT(setup.init(22222, 10), "Setup init");
    
    auto& storage = setup.entities.storage();
    
    // Create unit with low supply
    (void)setup.entities.create();
    usize idx = 0;
    storage.side[idx] = Side::BLUE;
    storage.pos_x[idx] = 0.0;
    storage.pos_y[idx] = 0.0;
    storage.fuel[idx] = 0.1;
    storage.ammo[idx] = 0.1;
    storage.supply[idx] = 0.1;
    storage.morale[idx] = 0.5;
    storage.consumption_rate[idx] = 0.001;  // Very low consumption
    
    // Add supply depot at same location
    systems::SupplyPoint depot;
    depot.x = 0.0;
    depot.y = 0.0;
    depot.z = 0.0;
    depot.capacity = 1000.0;
    depot.current_supply = 1000.0;
    depot.resupply_rate = 0.0;
    depot.side = Side::BLUE;
    depot.active = true;
    setup.systems.logistics.add_supply_point(depot);
    
    f64 initial_fuel = storage.fuel[idx];
    
    // Run logistics
    setup.ctx->start();
    auto status = setup.scheduler.run_tick();
    TEST_ASSERT(status.ok(), "Run tick");
    
    // Should have been resupplied
    TEST_ASSERT(storage.fuel[idx] > initial_fuel, "Fuel should increase from resupply");
    
    return true;
}

bool test_scenario_to_simulation() {
    // Create scenario JSON
    std::string json = R"({
        "id": "integration-test",
        "name": "Integration Test Scenario",
        "scale": "meso",
        "spatial_bounds": {
            "min_lat": 49.0,
            "max_lat": 51.0,
            "min_lon": 11.0,
            "max_lon": 13.0
        },
        "actors": [
            {
                "id": "blue-1",
                "side": "blue",
                "position": {"lat": 50.0, "lon": 12.0, "alt": 0},
                "firepower": {"base_firepower": 10, "range_km": 5},
                "logistics": {"consumption_rate": 0.01}
            },
            {
                "id": "red-1",
                "side": "red",
                "position": {"lat": 50.05, "lon": 12.05, "alt": 0},
                "firepower": {"base_firepower": 12, "range_km": 6}
            }
        ]
    })";
    
    // Parse scenario
    auto json_result = athena::json::parse(json);
    TEST_ASSERT(json_result.ok(), "JSON parse");
    
    ScenarioLoader loader;
    auto scenario_result = loader.load(json_result.get());
    TEST_ASSERT(scenario_result.ok(), "Scenario load");
    
    const auto& scenario = scenario_result.get();
    
    // Create simulation
    TestSetup setup;
    TEST_ASSERT(setup.init(99999, 10), "Setup init");
    
    // Convert scenario to entities
    Rng conv_rng(99999, 0x12345);
    ScenarioConverter converter;
    auto count_result = converter.convert(scenario, setup.entities, conv_rng);
    TEST_ASSERT(count_result.ok(), "Convert scenario");
    TEST_ASSERT(*count_result == 2, "Should create 2 entities");
    
    // Run simulation
    setup.ctx->start();
    for (int i = 0; i < 5; ++i) {
        auto status = setup.scheduler.run_tick();
        TEST_ASSERT(status.ok(), "Run tick");
    }
    
    return true;
}

bool test_morale_and_readiness() {
    TestSetup setup;
    TEST_ASSERT(setup.init(33333, 50), "Setup init");
    
    auto& storage = setup.entities.storage();
    
    // Create unit with very low supply
    (void)setup.entities.create();
    usize idx = 0;
    storage.side[idx] = Side::BLUE;
    storage.fuel[idx] = 0.02;  // Below critical threshold
    storage.ammo[idx] = 0.02;
    storage.supply[idx] = 0.02;
    storage.morale[idx] = 0.8;
    storage.readiness[idx] = 0.8;
    storage.consumption_rate[idx] = 0.0;  // No further consumption
    
    f64 initial_morale = storage.morale[idx];
    
    // Run several ticks
    setup.ctx->start();
    for (int i = 0; i < 10; ++i) {
        setup.scheduler.run_tick();
    }
    
    // Morale should have decayed due to low supply
    TEST_ASSERT(storage.morale[idx] < initial_morale, 
        "Morale should decay with low supply");
    
    return true;
}

bool test_entity_death() {
    TestSetup setup;
    TEST_ASSERT(setup.init(44444, 100), "Setup init");
    
    auto& storage = setup.entities.storage();
    
    // Create two units very close together with high firepower
    setup.systems.combat.set_attrition_rate(0.4);  // High attrition
    
    (void)setup.entities.create();  // Blue entity
    storage.side[0] = Side::BLUE;
    storage.pos_x[0] = 0.0;
    storage.health[0] = 1.0;
    storage.supply[0] = 1.0;
    storage.fuel[0] = 1.0;
    storage.ammo[0] = 1.0;
    storage.morale[0] = 1.0;
    storage.readiness[0] = 1.0;
    storage.firepower[0] = 100.0;  // Very high
    storage.engagement_range[0] = 10000.0;
    
    (void)setup.entities.create();  // Red entity
    storage.side[1] = Side::RED;
    storage.pos_x[1] = 100.0;  // Very close
    storage.health[1] = 0.2;   // Already damaged
    storage.supply[1] = 1.0;
    storage.fuel[1] = 1.0;
    storage.ammo[1] = 1.0;
    storage.morale[1] = 1.0;
    storage.readiness[1] = 1.0;
    storage.firepower[1] = 50.0;
    storage.engagement_range[1] = 10000.0;
    
    // Run combat until one dies
    setup.ctx->start();
    bool someone_died = false;
    for (int i = 0; i < 20 && !someone_died; ++i) {
        setup.scheduler.run_tick();
        
        if (storage.is_dead(0) || storage.is_dead(1)) {
            someone_died = true;
        }
    }
    
    TEST_ASSERT(someone_died, "At least one entity should die");
    
    return true;
}

// =============================================================================
// Terrain Integration Tests
// =============================================================================

bool test_terrain_semantics_queries() {
    TerrainPhysicalConfig phys_config;
    phys_config.base = TerrainBase::HILLS;
    phys_config.seed = 12345;
    phys_config.roughness = 0.6;
    phys_config.width_m = 10000.0;
    phys_config.height_m = 10000.0;
    
    PhysicalTerrain physical;
    TEST_ASSERT(physical.init(phys_config).ok(), "Physical terrain init");
    
    TerrainEnvironmentConfig env_config;
    env_config.climate = Climate::TEMPERATE;
    env_config.weather = Weather::CLEAR;
    env_config.visibility_km = 10.0;
    
    Environment environment;
    TEST_ASSERT(environment.init(env_config).ok(), "Environment init");
    
    TerrainSemantics semantics;
    TEST_ASSERT(semantics.init(&physical, &environment).ok(), "Semantics init");
    
    MovementCost cost = semantics.movement_cost(5000.0, 5000.0, 0, MovementType::TRACKED);
    TEST_ASSERT(cost.cost >= 0.5 && cost.cost <= 5.0, "Movement cost in range");
    TEST_ASSERT(cost.passable, "Hills passable for tracked");
    
    DefenseValue def = semantics.defense_value(5000.0, 5000.0, 0);
    TEST_ASSERT(def.cover >= 0.0 && def.cover <= 1.0, "Cover in range");
    TEST_ASSERT(def.concealment >= 0.0 && def.concealment <= 1.0, "Concealment in range");
    
    f64 los = semantics.line_of_sight(0.0, 0.0, 5000.0, 5000.0, 0);
    TEST_ASSERT(los >= 0.0 && los <= 1.0, "LOS in range");
    
    return true;
}

bool test_terrain_affects_movement() {
    TestSetup setup;
    TEST_ASSERT(setup.init(55555, 100), "Setup init");
    
    auto& storage = setup.entities.storage();
    
    TerrainPhysicalConfig phys_config;
    phys_config.base = TerrainBase::HILLS;
    phys_config.seed = 12345;
    phys_config.width_m = 50000.0;
    phys_config.height_m = 50000.0;
    
    PhysicalTerrain physical;
    TEST_ASSERT(physical.init(phys_config).ok(), "Physical init");
    
    TerrainEnvironmentConfig env_config;
    Environment environment;
    TEST_ASSERT(environment.init(env_config).ok(), "Environment init");
    
    TerrainSemantics semantics;
    TEST_ASSERT(semantics.init(&physical, &environment).ok(), "Semantics init");
    
    setup.systems.movement.set_terrain(&semantics);
    
    usize tank = setup.entities.create();
    storage.side[tank] = Side::BLUE;
    storage.pos_x[tank] = 1000.0;
    storage.pos_y[tank] = 1000.0;
    storage.pos_z[tank] = 0.0;
    storage.health[tank] = 1.0;
    storage.supply[tank] = 1.0;
    storage.fuel[tank] = 1.0;
    storage.consumption_rate[tank] = 1.0;
    
    setup.systems.movement.set_max_speed(tank, 10.0);
    setup.systems.movement.set_target(tank, 10000.0, 1000.0, 0.0);
    
    f64 initial_x = storage.pos_x[tank];
    
    setup.ctx->start();
    setup.scheduler.run_tick();
    
    TEST_ASSERT(storage.pos_x[tank] > initial_x, "Should move toward target");
    
    return true;
}

bool test_terrain_affects_combat() {
    TestSetup setup;
    TEST_ASSERT(setup.init(66666, 100), "Setup init");
    
    auto& storage = setup.entities.storage();
    
    TerrainPhysicalConfig phys_config;
    phys_config.base = TerrainBase::FLAT;
    phys_config.seed = 54321;
    phys_config.width_m = 50000.0;
    phys_config.height_m = 50000.0;
    
    OverlayConfig forest;
    forest.type = OverlayType::FOREST;
    forest.density = 0.7;
    phys_config.overlays.push_back(forest);
    
    PhysicalTerrain physical;
    TEST_ASSERT(physical.init(phys_config).ok(), "Physical init");
    
    TerrainEnvironmentConfig env_config;
    Environment environment;
    TEST_ASSERT(environment.init(env_config).ok(), "Environment init");
    
    TerrainSemantics semantics;
    TEST_ASSERT(semantics.init(&physical, &environment).ok(), "Semantics init");
    
    setup.systems.combat.set_terrain(&semantics);
    
    usize attacker = setup.entities.create();
    storage.side[attacker] = Side::RED;
    storage.pos_x[attacker] = 0.0;
    storage.health[attacker] = 1.0;
    storage.supply[attacker] = 1.0;
    storage.ammo[attacker] = 1.0;
    storage.morale[attacker] = 1.0;
    storage.readiness[attacker] = 1.0;
    storage.firepower[attacker] = 50.0;
    storage.engagement_range[attacker] = 10000.0;
    
    usize defender = setup.entities.create();
    storage.side[defender] = Side::BLUE;
    storage.pos_x[defender] = 3000.0;
    storage.health[defender] = 1.0;
    storage.supply[defender] = 1.0;
    storage.ammo[defender] = 1.0;
    storage.morale[defender] = 1.0;
    storage.readiness[defender] = 1.0;
    storage.firepower[defender] = 50.0;
    storage.engagement_range[defender] = 10000.0;
    
    setup.ctx->start();
    for (int i = 0; i < 10; ++i) {
        setup.scheduler.run_tick();
    }
    
    bool damage = (storage.health[attacker] < 1.0) || (storage.health[defender] < 1.0);
    TEST_ASSERT(damage, "Combat should cause damage");
    
    return true;
}

bool test_terrain_determinism() {
    auto run = [](Seed seed) -> f64 {
        TestSetup setup;
        if (!setup.init(seed, 50)) return -1.0;
        
        auto& storage = setup.entities.storage();
        
        TerrainPhysicalConfig phys;
        phys.base = TerrainBase::HILLS;
        phys.seed = 12345;
        phys.width_m = 50000.0;
        phys.height_m = 50000.0;
        
        PhysicalTerrain physical;
        if (!physical.init(phys).ok()) return -1.0;
        
        TerrainEnvironmentConfig env;
        Environment environment;
        if (!environment.init(env).ok()) return -1.0;
        
        TerrainSemantics semantics;
        if (!semantics.init(&physical, &environment).ok()) return -1.0;
        
        setup.systems.movement.set_terrain(&semantics);
        setup.systems.combat.set_terrain(&semantics);
        setup.systems.detection.set_terrain(&semantics);
        
        for (int i = 0; i < 5; ++i) {
            usize b = setup.entities.create();
            storage.side[b] = Side::BLUE;
            storage.pos_x[b] = 1000.0 + i * 500.0;
            storage.pos_y[b] = 1000.0;
            storage.health[b] = 1.0;
            storage.supply[b] = 1.0;
            storage.fuel[b] = 1.0;
            storage.ammo[b] = 1.0;
            storage.morale[b] = 1.0;
            storage.readiness[b] = 1.0;
            storage.firepower[b] = 30.0;
            storage.engagement_range[b] = 5000.0;
            storage.detection_range[b] = 3000.0;
            
            usize r = setup.entities.create();
            storage.side[r] = Side::RED;
            storage.pos_x[r] = 3000.0 + i * 500.0;
            storage.pos_y[r] = 1000.0;
            storage.health[r] = 1.0;
            storage.supply[r] = 1.0;
            storage.fuel[r] = 1.0;
            storage.ammo[r] = 1.0;
            storage.morale[r] = 1.0;
            storage.readiness[r] = 1.0;
            storage.firepower[r] = 30.0;
            storage.engagement_range[r] = 5000.0;
            storage.detection_range[r] = 3000.0;
        }
        
        setup.ctx->start();
        for (int i = 0; i < 20; ++i) {
            setup.scheduler.run_tick();
        }
        
        f64 total = 0.0;
        for (usize i = 0; i < storage.count; ++i) {
            total += storage.health[i];
        }
        return total;
    };
    
    const Seed SEED = 0xDEADBEEF12345678ULL;
    f64 r1 = run(SEED);
    f64 r2 = run(SEED);
    
    TEST_ASSERT(r1 >= 0.0, "Run 1 OK");
    TEST_ASSERT(r2 >= 0.0, "Run 2 OK");
    TEST_ASSERT(r1 == r2, "Bit-identical");
    
    return true;
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "=== ATHENA Integration Tests ===\n\n";
    
    int passed = 0;
    int failed = 0;
    
    std::cout << "--- Full Simulation Tests ---\n";
    RUN_TEST(test_full_simulation_determinism);
    RUN_TEST(test_scenario_to_simulation);
    
    std::cout << "\n--- Movement System Tests ---\n";
    RUN_TEST(test_movement_system);
    
    std::cout << "\n--- Combat System Tests ---\n";
    RUN_TEST(test_combat_system);
    RUN_TEST(test_entity_death);
    
    std::cout << "\n--- Logistics System Tests ---\n";
    RUN_TEST(test_logistics_system);
    RUN_TEST(test_supply_point_resupply);
    RUN_TEST(test_morale_and_readiness);
    
    std::cout << "\n--- Terrain Integration Tests ---\n";
    RUN_TEST(test_terrain_semantics_queries);
    RUN_TEST(test_terrain_affects_movement);
    RUN_TEST(test_terrain_affects_combat);
    RUN_TEST(test_terrain_determinism);
    
    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    
    if (failed > 0) {
        std::cout << "\n*** INTEGRATION TESTS FAILED ***\n";
        return 1;
    }
    
    std::cout << "\nAll integration tests passed.\n";
    return 0;
}
