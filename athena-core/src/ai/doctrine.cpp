// ATHENA Core - AI/Doctrine Implementation
//
// Implements: Blackboard, doctrine presets, BehaviorTreeFactory, AISystem.
// All entity access through EntityManager::storage().
// All RNG through Rng (PCG64).
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/ai_doctrine.hpp"

#include <algorithm>

namespace athena {
namespace ai {

// =============================================================================
// Global Action Queue
// =============================================================================

std::vector<ActionResult> g_action_queue;

// =============================================================================
// Blackboard
// =============================================================================

void Blackboard::set_f64(const std::string& key, f64 val) {
    f64_data_[key] = val;
}

f64 Blackboard::get_f64(const std::string& key, f64 def) const {
    auto it = f64_data_.find(key);
    return (it != f64_data_.end()) ? it->second : def;
}

void Blackboard::set_i32(const std::string& key, i32 val) {
    i32_data_[key] = val;
}

i32 Blackboard::get_i32(const std::string& key, i32 def) const {
    auto it = i32_data_.find(key);
    return (it != i32_data_.end()) ? it->second : def;
}

void Blackboard::set_bool(const std::string& key, bool val) {
    bool_data_[key] = val;
}

bool Blackboard::get_bool(const std::string& key, bool def) const {
    auto it = bool_data_.find(key);
    return (it != bool_data_.end()) ? it->second : def;
}

void Blackboard::set_entity(const std::string& key, EntityId id) {
    entity_data_[key] = id;
}

EntityId Blackboard::get_entity(const std::string& key, EntityId def) const {
    auto it = entity_data_.find(key);
    return (it != entity_data_.end()) ? it->second : def;
}

bool Blackboard::has(const std::string& key) const {
    return f64_data_.count(key) > 0 ||
           i32_data_.count(key) > 0 ||
           bool_data_.count(key) > 0 ||
           entity_data_.count(key) > 0;
}

void Blackboard::clear() {
    f64_data_.clear();
    i32_data_.clear();
    bool_data_.clear();
    entity_data_.clear();
}

// =============================================================================
// Doctrine Presets
// =============================================================================

namespace doctrines {

DoctrineConfig nato_defensive() {
    DoctrineConfig d;
    d.name                    = "NATO Defensive";
    d.aggression              = 0.3;
    d.discipline              = 0.8;
    d.initiative              = 0.5;
    d.self_preservation       = 0.7;
    d.coordination            = 0.8;
    d.retreat_health_threshold = 0.3;
    d.retreat_supply_threshold = 0.15;
    d.retreat_morale_threshold = 0.3;
    d.min_force_ratio_attack   = 1.5;
    d.engagement_range_factor  = 1.2;
    d.pursuit_distance_km      = 2.0;
    return d;
}

DoctrineConfig nato_offensive() {
    DoctrineConfig d;
    d.name                    = "NATO Offensive";
    d.aggression              = 0.7;
    d.discipline              = 0.7;
    d.initiative              = 0.6;
    d.self_preservation       = 0.4;
    d.coordination            = 0.8;
    d.retreat_health_threshold = 0.2;
    d.retreat_supply_threshold = 0.1;
    d.retreat_morale_threshold = 0.2;
    d.min_force_ratio_attack   = 1.0;
    d.engagement_range_factor  = 1.0;
    d.pursuit_distance_km      = 5.0;
    return d;
}

DoctrineConfig soviet_offensive() {
    DoctrineConfig d;
    d.name                    = "Soviet Offensive";
    d.aggression              = 0.85;
    d.discipline              = 0.7;
    d.initiative              = 0.3;
    d.self_preservation       = 0.2;
    d.coordination            = 0.6;
    d.retreat_health_threshold = 0.1;
    d.retreat_supply_threshold = 0.05;
    d.retreat_morale_threshold = 0.15;
    d.min_force_ratio_attack   = 0.8;
    d.engagement_range_factor  = 0.9;
    d.pursuit_distance_km      = 8.0;
    return d;
}

DoctrineConfig guerrilla() {
    DoctrineConfig d;
    d.name                    = "Guerrilla";
    d.aggression              = 0.4;
    d.discipline              = 0.4;
    d.initiative              = 0.8;
    d.self_preservation       = 0.9;
    d.coordination            = 0.3;
    d.retreat_health_threshold = 0.5;
    d.retreat_supply_threshold = 0.3;
    d.retreat_morale_threshold = 0.4;
    d.min_force_ratio_attack   = 2.0;
    d.engagement_range_factor  = 0.7;
    d.pursuit_distance_km      = 1.0;
    return d;
}

DoctrineConfig idf_aggressive() {
    DoctrineConfig d;
    d.name                    = "IDF Aggressive";
    d.aggression              = 0.85;
    d.discipline              = 0.7;
    d.initiative              = 0.9;
    d.self_preservation       = 0.3;
    d.coordination            = 0.7;
    d.retreat_health_threshold = 0.15;
    d.retreat_supply_threshold = 0.1;
    d.retreat_morale_threshold = 0.2;
    d.min_force_ratio_attack   = 0.7;
    d.engagement_range_factor  = 1.1;
    d.pursuit_distance_km      = 10.0;
    return d;
}

}  // namespace doctrines

// =============================================================================
// Behavior Tree Factory
// =============================================================================

BehaviorTree BehaviorTreeFactory::create_combat_tree(const DoctrineConfig& doctrine) {
    //
    // Tree structure:
    //
    //   Selector "CombatRoot"
    //   +-- Sequence "CriticalRetreat"
    //   |   +-- Inverter(HealthAbove(retreat_health_threshold))
    //   |   +-- Retreat
    //   +-- Sequence "LowMoraleRetreat"
    //   |   +-- Inverter(MoraleAbove(retreat_morale_threshold))
    //   |   +-- Retreat
    //   +-- Sequence "Attack"
    //   |   +-- EnemiesVisible
    //   |   +-- ForceRatioAbove(min_force_ratio_attack)
    //   |   +-- AttackNearest
    //   +-- Sequence "DefendAgainstContact"
    //   |   +-- EnemiesVisible
    //   |   +-- HoldPosition
    //   +-- HoldPosition (fallback)
    //

    auto root = std::make_unique<SelectorNode>("CombatRoot");

    // Branch 1: Critical health retreat
    {
        auto seq = std::make_unique<SequenceNode>("CriticalRetreat");
        seq->add_child(std::make_unique<InverterNode>(
            std::make_unique<HealthAboveNode>(doctrine.retreat_health_threshold)));
        seq->add_child(std::make_unique<RetreatNode>());
        root->add_child(std::move(seq));
    }

    // Branch 2: Low morale retreat
    {
        auto seq = std::make_unique<SequenceNode>("LowMoraleRetreat");
        seq->add_child(std::make_unique<InverterNode>(
            std::make_unique<MoraleAboveNode>(doctrine.retreat_morale_threshold)));
        seq->add_child(std::make_unique<RetreatNode>());
        root->add_child(std::move(seq));
    }

    // Branch 3: Attack when enemies visible and force ratio acceptable
    {
        auto seq = std::make_unique<SequenceNode>("Attack");
        seq->add_child(std::make_unique<EnemiesVisibleNode>());
        seq->add_child(std::make_unique<ForceRatioAboveNode>(
            doctrine.min_force_ratio_attack));
        seq->add_child(std::make_unique<AttackNearestNode>());
        root->add_child(std::move(seq));
    }

    // Branch 4: Enemies visible but force ratio too low -- defend in place
    {
        auto seq = std::make_unique<SequenceNode>("DefendAgainstContact");
        seq->add_child(std::make_unique<EnemiesVisibleNode>());
        seq->add_child(std::make_unique<HoldPositionNode>());
        root->add_child(std::move(seq));
    }

    // Branch 5: No threats -- hold position (fallback)
    root->add_child(std::make_unique<HoldPositionNode>());

    return BehaviorTree(std::move(root));
}

BehaviorTree BehaviorTreeFactory::create_patrol_tree(
    const std::vector<std::pair<f64, f64>>& waypoints)
{
    //
    // Tree structure:
    //
    //   Selector "PatrolRoot"
    //   +-- Sequence "ReactToContact"
    //   |   +-- EnemiesVisible
    //   |   +-- AttackNearest
    //   +-- Patrol(waypoints)
    //

    auto root = std::make_unique<SelectorNode>("PatrolRoot");

    // Branch 1: Interrupt patrol to engage enemies
    {
        auto seq = std::make_unique<SequenceNode>("ReactToContact");
        seq->add_child(std::make_unique<EnemiesVisibleNode>());
        seq->add_child(std::make_unique<AttackNearestNode>());
        root->add_child(std::move(seq));
    }

    // Branch 2: Patrol waypoints (returns RUNNING indefinitely)
    root->add_child(std::make_unique<PatrolNode>(waypoints));

    return BehaviorTree(std::move(root));
}

// =============================================================================
// AI System
// =============================================================================

AISystem::AISystem() = default;

void AISystem::init(usize capacity) {
    capacity_ = capacity;
    trees_.resize(capacity);
    blackboards_.resize(capacity);
    has_tree_.resize(capacity, false);
    actions_.clear();
}

void AISystem::reset() {
    for (usize i = 0; i < capacity_; ++i) {
        trees_[i] = BehaviorTree();   // Release node graph
        blackboards_[i].clear();
        has_tree_[i] = false;
    }
    actions_.clear();
}

void AISystem::assign_doctrine(usize entity_idx, const DoctrineConfig& doctrine) {
    if (entity_idx >= capacity_) return;
    trees_[entity_idx] = BehaviorTreeFactory::create_combat_tree(doctrine);
    blackboards_[entity_idx].clear();
    has_tree_[entity_idx] = true;
}

void AISystem::assign_doctrine_to_side(Side side, const DoctrineConfig& doctrine,
                                       EntityManager& entities) {
    auto& storage = entities.storage();
    for (usize i = 0; i < storage.count; ++i) {
        if (storage.is_active(i) && storage.side[i] == side) {
            assign_doctrine(i, doctrine);
        }
    }
}

void AISystem::update(EntityManager& entities, Rng& rng, Tick tick) {
    actions_.clear();

    auto& storage = entities.storage();

    for (usize i = 0; i < storage.count; ++i) {
        if (!storage.is_active(i)) continue;
        if (!has_tree_[i]) continue;

        // Derive a deterministic per-entity RNG stream from the master RNG.
        // Uses entity index as split_id for reproducibility.
        Rng entity_rng = rng.split(static_cast<u64>(i));

        // Build context
        BehaviorContext ctx;
        ctx.self = i;
        ctx.entities = &entities;
        ctx.blackboard = &blackboards_[i];
        ctx.rng = &entity_rng;
        ctx.current_tick = tick;

        compute_perception(i, storage, ctx);

        // Clear the global queue, tick tree, then harvest actions
        g_action_queue.clear();
        trees_[i].tick(ctx);

        if (!g_action_queue.empty()) {
            EntityId eid = storage.id[i];
            actions_[eid] = g_action_queue;
        }
    }
}

std::vector<ActionResult> AISystem::get_actions(EntityId entity_id) const {
    auto it = actions_.find(entity_id);
    if (it != actions_.end()) return it->second;
    return {};
}

void AISystem::compute_perception(usize entity_idx, const EntityStorage& storage,
                                  BehaviorContext& ctx) const {
    // Simple perception model: all active entities of opposite side are visible.
    // Real sensor modeling is handled by systems::DetectionSystem.

    Side my_side = storage.side[entity_idx];

    ctx.visible_enemies.clear();
    ctx.visible_friendlies.clear();

    for (usize j = 0; j < storage.count; ++j) {
        if (j == entity_idx) continue;
        if (!storage.is_active(j)) continue;

        if (storage.side[j] != my_side) {
            ctx.visible_enemies.push_back(j);
        } else {
            ctx.visible_friendlies.push_back(j);
        }
    }

    // Force ratio: (self + friendlies) / max(1, enemies)
    f64 friendly_count = 1.0 + static_cast<f64>(ctx.visible_friendlies.size());
    f64 enemy_count    = static_cast<f64>(ctx.visible_enemies.size());
    ctx.local_force_ratio = (enemy_count > 0.0)
        ? (friendly_count / enemy_count)
        : 999.0;  // No enemies = overwhelming advantage
}

}  // namespace ai
}  // namespace athena
