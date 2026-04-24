// ATHENA Core - AI/Doctrine System Tests
//
// API corrections from original stub:
// - Rng (not RNG)
// - Entity data via em.storage().field[idx] (not em.field[id])
// - storage.is_active(idx) is a function (not array)
// - create() already sets ACTIVE flag (no manual assignment needed)
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/ai_doctrine.hpp"

#include <cassert>
#include <iostream>
#include <cmath>

using namespace athena;
using namespace athena::ai;

// =============================================================================
// Test Helpers
// =============================================================================

/// Create a minimal entity set for testing:
///   Indices 0-4: BLUE at (1000+i*100, 1000)
///   Indices 5-9: RED  at (5000+i*100, 5000)
/// All entities: health=1, supply=1, morale=1
static EntityManager create_test_entities() {
    EntityManager em;
    em.init(100);

    auto& s = em.storage();

    // Blue entities (indices 0-4)
    for (int i = 0; i < 5; ++i) {
        EntityId id = em.create();  // id == i, idx == i
        s.pos_x[id]  = 1000.0 + i * 100.0;
        s.pos_y[id]  = 1000.0;
        s.health[id] = 1.0;
        s.supply[id] = 1.0;
        s.morale[id] = 1.0;
        s.side[id]   = Side::BLUE;
        // ACTIVE flag already set by create()
    }

    // Red entities (indices 5-9)
    for (int i = 0; i < 5; ++i) {
        EntityId id = em.create();
        s.pos_x[id]  = 5000.0 + i * 100.0;
        s.pos_y[id]  = 5000.0;
        s.health[id] = 1.0;
        s.supply[id] = 1.0;
        s.morale[id] = 1.0;
        s.side[id]   = Side::RED;
    }

    return em;
}

// =============================================================================
// Behavior Node Tests
// =============================================================================

void test_sequence_node() {
    std::cout << "  test_sequence_node... ";

    EntityManager em = create_test_entities();
    Blackboard bb;
    Rng rng(12345);

    BehaviorContext ctx;
    ctx.self       = 0;
    ctx.entities   = &em;
    ctx.blackboard = &bb;
    ctx.rng        = &rng;
    ctx.current_tick = 0;

    // Sequence with all SUCCESS (health=1.0 > 0.0 for both children)
    auto seq = std::make_unique<SequenceNode>("TestSeq");
    seq->add_child(std::make_unique<HealthAboveNode>(0.0));
    seq->add_child(std::make_unique<HealthAboveNode>(0.0));
    assert(seq->tick(ctx) == NodeStatus::SUCCESS);

    // Sequence with one FAILURE (health=1.0, NOT > 1.0)
    auto seq2 = std::make_unique<SequenceNode>("TestSeq2");
    seq2->add_child(std::make_unique<HealthAboveNode>(0.0));  // SUCCESS
    seq2->add_child(std::make_unique<HealthAboveNode>(1.0));  // FAILURE
    assert(seq2->tick(ctx) == NodeStatus::FAILURE);

    std::cout << "OK\n";
}

void test_selector_node() {
    std::cout << "  test_selector_node... ";

    EntityManager em = create_test_entities();
    Blackboard bb;
    Rng rng(12345);

    BehaviorContext ctx;
    ctx.self       = 0;
    ctx.entities   = &em;
    ctx.blackboard = &bb;
    ctx.rng        = &rng;
    ctx.current_tick = 0;

    // Selector: first child SUCCESS -> returns SUCCESS immediately
    auto sel = std::make_unique<SelectorNode>("TestSel");
    sel->add_child(std::make_unique<HealthAboveNode>(0.0));  // SUCCESS
    sel->add_child(std::make_unique<HealthAboveNode>(2.0));  // FAILURE
    assert(sel->tick(ctx) == NodeStatus::SUCCESS);

    // Selector: all FAILURE -> returns FAILURE
    auto sel2 = std::make_unique<SelectorNode>("TestSel2");
    sel2->add_child(std::make_unique<HealthAboveNode>(2.0));  // FAILURE
    sel2->add_child(std::make_unique<HealthAboveNode>(3.0));  // FAILURE
    assert(sel2->tick(ctx) == NodeStatus::FAILURE);

    std::cout << "OK\n";
}

void test_inverter_node() {
    std::cout << "  test_inverter_node... ";

    EntityManager em = create_test_entities();
    Blackboard bb;
    Rng rng(12345);

    BehaviorContext ctx;
    ctx.self       = 0;
    ctx.entities   = &em;
    ctx.blackboard = &bb;
    ctx.rng        = &rng;
    ctx.current_tick = 0;

    // Invert SUCCESS -> FAILURE
    auto inv1 = std::make_unique<InverterNode>(
        std::make_unique<HealthAboveNode>(0.0));  // child returns SUCCESS
    assert(inv1->tick(ctx) == NodeStatus::FAILURE);

    // Invert FAILURE -> SUCCESS
    auto inv2 = std::make_unique<InverterNode>(
        std::make_unique<HealthAboveNode>(2.0));  // child returns FAILURE
    assert(inv2->tick(ctx) == NodeStatus::SUCCESS);

    std::cout << "OK\n";
}

void test_condition_nodes() {
    std::cout << "  test_condition_nodes... ";

    EntityManager em = create_test_entities();
    Blackboard bb;
    Rng rng(12345);

    BehaviorContext ctx;
    ctx.self       = 0;
    ctx.entities   = &em;
    ctx.blackboard = &bb;
    ctx.rng        = &rng;
    ctx.current_tick = 0;

    // HealthAbove: entity 0 has health=1.0
    auto health_ok = std::make_unique<HealthAboveNode>(0.5);
    assert(health_ok->tick(ctx) == NodeStatus::SUCCESS);

    auto health_high = std::make_unique<HealthAboveNode>(1.5);
    assert(health_high->tick(ctx) == NodeStatus::FAILURE);

    // SupplyAbove: entity 0 has supply=1.0
    auto supply_ok = std::make_unique<SupplyAboveNode>(0.5);
    assert(supply_ok->tick(ctx) == NodeStatus::SUCCESS);

    // EnemiesVisible: initially empty
    ctx.visible_enemies.clear();
    auto enemies = std::make_unique<EnemiesVisibleNode>();
    assert(enemies->tick(ctx) == NodeStatus::FAILURE);

    // Add some enemy indices
    ctx.visible_enemies.push_back(5);
    ctx.visible_enemies.push_back(6);
    assert(enemies->tick(ctx) == NodeStatus::SUCCESS);

    // ForceRatioAbove
    ctx.local_force_ratio = 2.0;
    auto ratio_ok = std::make_unique<ForceRatioAboveNode>(1.5);
    assert(ratio_ok->tick(ctx) == NodeStatus::SUCCESS);

    auto ratio_high = std::make_unique<ForceRatioAboveNode>(3.0);
    assert(ratio_high->tick(ctx) == NodeStatus::FAILURE);

    std::cout << "OK\n";
}

// =============================================================================
// Blackboard Tests
// =============================================================================

void test_blackboard() {
    std::cout << "  test_blackboard... ";

    Blackboard bb;

    // f64
    bb.set_f64("damage", 50.5);
    assert(std::abs(bb.get_f64("damage") - 50.5) < 0.001);
    assert(std::abs(bb.get_f64("nonexistent", 0.0) - 0.0) < 0.001);

    // i32
    bb.set_i32("count", 42);
    assert(bb.get_i32("count") == 42);
    assert(bb.get_i32("nonexistent", -1) == -1);

    // bool
    bb.set_bool("active", true);
    assert(bb.get_bool("active") == true);
    assert(bb.get_bool("nonexistent", false) == false);

    // EntityId
    bb.set_entity("target", 7);
    assert(bb.get_entity("target") == 7);
    assert(bb.get_entity("nonexistent", INVALID_ENTITY) == INVALID_ENTITY);

    // has
    assert(bb.has("damage"));
    assert(!bb.has("nonexistent"));

    // clear
    bb.clear();
    assert(!bb.has("damage"));

    std::cout << "OK\n";
}

// =============================================================================
// Doctrine Config Tests
// =============================================================================

void test_doctrine_configs() {
    std::cout << "  test_doctrine_configs... ";

    auto nato = doctrines::nato_defensive();
    assert(nato.name == "NATO Defensive");
    assert(nato.aggression < 0.5);
    assert(nato.discipline > 0.5);

    auto soviet = doctrines::soviet_offensive();
    assert(soviet.name == "Soviet Offensive");
    assert(soviet.aggression > 0.7);

    auto guer = doctrines::guerrilla();
    assert(guer.self_preservation > 0.8);
    assert(guer.min_force_ratio_attack > 1.5);

    auto idf = doctrines::idf_aggressive();
    assert(idf.aggression > 0.8);
    assert(idf.initiative > 0.8);

    std::cout << "OK\n";
}

// =============================================================================
// Behavior Tree Factory Tests
// =============================================================================

void test_behavior_tree_factory() {
    std::cout << "  test_behavior_tree_factory... ";

    EntityManager em = create_test_entities();
    Blackboard bb;
    Rng rng(12345);

    BehaviorContext ctx;
    ctx.self       = 0;
    ctx.entities   = &em;
    ctx.blackboard = &bb;
    ctx.rng        = &rng;
    ctx.current_tick = 0;
    ctx.visible_enemies.clear();
    ctx.visible_friendlies.clear();
    ctx.local_force_ratio = 1.0;

    auto doctrine = doctrines::nato_offensive();
    auto tree = BehaviorTreeFactory::create_combat_tree(doctrine);

    // --- Case 1: No enemies -> hold position
    g_action_queue.clear();
    NodeStatus status = tree.tick(ctx);
    assert(status == NodeStatus::SUCCESS);
    assert(!g_action_queue.empty());
    assert(g_action_queue.back().type == ActionResult::Type::HOLD_POSITION);

    // --- Case 2: Enemies visible, good force ratio -> attack
    tree.reset();
    g_action_queue.clear();
    ctx.visible_enemies.push_back(5);
    ctx.local_force_ratio = 2.0;

    status = tree.tick(ctx);
    assert(status == NodeStatus::SUCCESS);
    assert(!g_action_queue.empty());
    assert(g_action_queue.back().type == ActionResult::Type::ATTACK_ENTITY);

    // --- Case 3: Critical health -> retreat (regardless of enemies)
    tree.reset();
    g_action_queue.clear();
    em.storage().health[0] = 0.1;  // Below retreat_health_threshold (0.2)

    status = tree.tick(ctx);
    assert(status == NodeStatus::SUCCESS);
    assert(!g_action_queue.empty());
    assert(g_action_queue.back().type == ActionResult::Type::RETREAT);

    std::cout << "OK\n";
}

// =============================================================================
// Patrol Tree Tests
// =============================================================================

void test_patrol_tree() {
    std::cout << "  test_patrol_tree... ";

    EntityManager em = create_test_entities();
    Blackboard bb;
    Rng rng(12345);

    BehaviorContext ctx;
    ctx.self       = 0;
    ctx.entities   = &em;
    ctx.blackboard = &bb;
    ctx.rng        = &rng;
    ctx.current_tick = 0;
    ctx.visible_enemies.clear();

    std::vector<std::pair<f64, f64>> waypoints = {
        {2000.0, 2000.0},
        {3000.0, 2000.0},
        {3000.0, 3000.0}
    };

    auto tree = BehaviorTreeFactory::create_patrol_tree(waypoints);

    g_action_queue.clear();
    NodeStatus status = tree.tick(ctx);

    // Entity 0 is at (1000, 1000), far from first waypoint -> RUNNING
    assert(status == NodeStatus::RUNNING);
    assert(!g_action_queue.empty());
    assert(g_action_queue.back().type == ActionResult::Type::MOVE_TO_POSITION);

    std::cout << "OK\n";
}

// =============================================================================
// AI System Tests
// =============================================================================

void test_ai_system() {
    std::cout << "  test_ai_system... ";

    AISystem ai;
    ai.init(100);

    EntityManager em = create_test_entities();
    Rng rng(12345);

    // Assign doctrines to both sides
    ai.assign_doctrine_to_side(Side::BLUE, doctrines::nato_offensive(), em);
    ai.assign_doctrine_to_side(Side::RED, doctrines::soviet_offensive(), em);

    // Update -- should generate actions for all entities
    ai.update(em, rng, 0);

    // Verify at least some entities produced actions
    bool has_actions = false;
    for (EntityId i = 0; i < 10; ++i) {
        auto actions = ai.get_actions(i);
        if (!actions.empty()) {
            has_actions = true;
            break;
        }
    }
    assert(has_actions);

    // Reset should clear everything
    ai.reset();

    std::cout << "OK\n";
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "=== ATHENA AI/Doctrine System Tests ===\n\n";

    std::cout << "Behavior Node Tests:\n";
    test_sequence_node();
    test_selector_node();
    test_inverter_node();
    test_condition_nodes();

    std::cout << "\nBlackboard Tests:\n";
    test_blackboard();

    std::cout << "\nDoctrine Tests:\n";
    test_doctrine_configs();

    std::cout << "\nBehavior Tree Factory Tests:\n";
    test_behavior_tree_factory();
    test_patrol_tree();

    std::cout << "\nAI System Tests:\n";
    test_ai_system();

    std::cout << "\n=== All AI/Doctrine tests passed! ===\n";
    return 0;
}
