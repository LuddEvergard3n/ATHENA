// ATHENA Core - AI/Doctrine System
// Contract: Behavior tree-based AI with configurable military doctrines.
//
// ARCHITECTURE:
//   BehaviorNode (abstract) -> Composite/Decorator/Leaf nodes
//   DoctrineConfig          -> Parameters that shape tree construction
//   BehaviorTreeFactory     -> Creates doctrine-specific trees
//   AISystem                -> Per-entity tree management, perception, action dispatch
//
// RULES:
// - All entity access via EntityManager::storage()
// - storage.is_active(idx) is a FUNCTION, not an array
// - Rng (not RNG) for random number generation
// - Deterministic iteration order (by entity index)
// - No exceptions -- uses NodeStatus for control flow
// - No allocation during tick -- trees pre-built at assignment
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_AI_DOCTRINE_HPP
#define ATHENA_AI_DOCTRINE_HPP

#include "athena/types.hpp"
#include "athena/rng.hpp"
#include "athena/entities.hpp"

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <utility>
#include <cmath>

namespace athena {
namespace ai {

// =============================================================================
// Node Status (Behavior Tree return values)
// =============================================================================

enum class NodeStatus : u8 {
    SUCCESS = 0,
    FAILURE = 1,
    RUNNING = 2
};

// =============================================================================
// Action Result (output of action leaf nodes)
// =============================================================================

struct ActionResult {
    enum class Type : u8 {
        NONE = 0,
        HOLD_POSITION,
        ATTACK_ENTITY,
        RETREAT,
        MOVE_TO_POSITION,
        DEFEND_POSITION,
        REGROUP,
        RESUPPLY
    };

    Type type            = Type::NONE;
    EntityId target_entity = INVALID_ENTITY;
    f64 target_x         = 0.0;
    f64 target_y         = 0.0;
    f64 priority         = 0.0;
};

/// Global action queue -- action leaf nodes push here during tick().
/// The AISystem clears and collects per-entity after each tree tick.
extern std::vector<ActionResult> g_action_queue;

// =============================================================================
// Blackboard (per-entity typed key-value store)
// =============================================================================

class Blackboard {
public:
    void set_f64(const std::string& key, f64 val);
    f64  get_f64(const std::string& key, f64 def = 0.0) const;

    void set_i32(const std::string& key, i32 val);
    i32  get_i32(const std::string& key, i32 def = 0) const;

    void set_bool(const std::string& key, bool val);
    bool get_bool(const std::string& key, bool def = false) const;

    void     set_entity(const std::string& key, EntityId id);
    EntityId get_entity(const std::string& key, EntityId def = INVALID_ENTITY) const;

    /// Returns true if key exists in any typed map.
    bool has(const std::string& key) const;

    void clear();

private:
    std::map<std::string, f64>      f64_data_;
    std::map<std::string, i32>      i32_data_;
    std::map<std::string, bool>     bool_data_;
    std::map<std::string, EntityId> entity_data_;
};

// =============================================================================
// Behavior Context (passed to every node on tick)
// =============================================================================

struct BehaviorContext {
    usize self = 0;                              // Entity storage index
    EntityManager* entities = nullptr;           // Access via entities->storage()
    Blackboard* blackboard = nullptr;
    Rng* rng = nullptr;
    Tick current_tick = 0;

    // Pre-computed by AISystem (or test setup) before tick
    std::vector<usize> visible_enemies;          // Storage indices of detected hostiles
    std::vector<usize> visible_friendlies;       // Storage indices of same-side allies
    f64 local_force_ratio = 1.0;                 // (1 + friendlies) / max(1, enemies)
};

// =============================================================================
// Behavior Node (abstract base)
// =============================================================================

class BehaviorNode {
public:
    explicit BehaviorNode(const std::string& name = "") : name_(name) {}
    virtual ~BehaviorNode() = default;

    virtual NodeStatus tick(BehaviorContext& ctx) = 0;
    virtual void reset() {}

    const std::string& name() const { return name_; }

protected:
    std::string name_;
};

// =============================================================================
// Composite Nodes
// =============================================================================

/// Runs children in order. Stops on first FAILURE or RUNNING.
/// Returns SUCCESS only if ALL children return SUCCESS.
class SequenceNode : public BehaviorNode {
public:
    explicit SequenceNode(const std::string& name = "Sequence")
        : BehaviorNode(name) {}

    void add_child(std::unique_ptr<BehaviorNode> child) {
        children_.push_back(std::move(child));
    }

    NodeStatus tick(BehaviorContext& ctx) override {
        for (auto& child : children_) {
            NodeStatus s = child->tick(ctx);
            if (s != NodeStatus::SUCCESS) return s;
        }
        return NodeStatus::SUCCESS;
    }

    void reset() override {
        for (auto& c : children_) c->reset();
    }

private:
    std::vector<std::unique_ptr<BehaviorNode>> children_;
};

/// Runs children in order. Stops on first SUCCESS or RUNNING.
/// Returns FAILURE only if ALL children return FAILURE.
class SelectorNode : public BehaviorNode {
public:
    explicit SelectorNode(const std::string& name = "Selector")
        : BehaviorNode(name) {}

    void add_child(std::unique_ptr<BehaviorNode> child) {
        children_.push_back(std::move(child));
    }

    NodeStatus tick(BehaviorContext& ctx) override {
        for (auto& child : children_) {
            NodeStatus s = child->tick(ctx);
            if (s != NodeStatus::FAILURE) return s;
        }
        return NodeStatus::FAILURE;
    }

    void reset() override {
        for (auto& c : children_) c->reset();
    }

private:
    std::vector<std::unique_ptr<BehaviorNode>> children_;
};

// =============================================================================
// Decorator Nodes
// =============================================================================

/// Inverts SUCCESS <-> FAILURE. RUNNING passes through unchanged.
class InverterNode : public BehaviorNode {
public:
    explicit InverterNode(std::unique_ptr<BehaviorNode> child,
                          const std::string& name = "Inverter")
        : BehaviorNode(name), child_(std::move(child)) {}

    NodeStatus tick(BehaviorContext& ctx) override {
        NodeStatus s = child_->tick(ctx);
        if (s == NodeStatus::SUCCESS) return NodeStatus::FAILURE;
        if (s == NodeStatus::FAILURE) return NodeStatus::SUCCESS;
        return NodeStatus::RUNNING;
    }

    void reset() override { child_->reset(); }

private:
    std::unique_ptr<BehaviorNode> child_;
};

// =============================================================================
// Condition Nodes (leaf, read-only -- no side effects)
// =============================================================================

/// SUCCESS if entity health > threshold
class HealthAboveNode : public BehaviorNode {
public:
    explicit HealthAboveNode(f64 threshold)
        : BehaviorNode("HealthAbove"), threshold_(threshold) {}

    NodeStatus tick(BehaviorContext& ctx) override {
        auto& s = ctx.entities->storage();
        return (s.health[ctx.self] > threshold_)
            ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
    }

private:
    f64 threshold_;
};

/// SUCCESS if entity supply > threshold
class SupplyAboveNode : public BehaviorNode {
public:
    explicit SupplyAboveNode(f64 threshold)
        : BehaviorNode("SupplyAbove"), threshold_(threshold) {}

    NodeStatus tick(BehaviorContext& ctx) override {
        auto& s = ctx.entities->storage();
        return (s.supply[ctx.self] > threshold_)
            ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
    }

private:
    f64 threshold_;
};

/// SUCCESS if entity morale > threshold
class MoraleAboveNode : public BehaviorNode {
public:
    explicit MoraleAboveNode(f64 threshold)
        : BehaviorNode("MoraleAbove"), threshold_(threshold) {}

    NodeStatus tick(BehaviorContext& ctx) override {
        auto& s = ctx.entities->storage();
        return (s.morale[ctx.self] > threshold_)
            ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
    }

private:
    f64 threshold_;
};

/// SUCCESS if visible_enemies is non-empty
class EnemiesVisibleNode : public BehaviorNode {
public:
    EnemiesVisibleNode() : BehaviorNode("EnemiesVisible") {}

    NodeStatus tick(BehaviorContext& ctx) override {
        return ctx.visible_enemies.empty()
            ? NodeStatus::FAILURE : NodeStatus::SUCCESS;
    }
};

/// SUCCESS if local_force_ratio > threshold
class ForceRatioAboveNode : public BehaviorNode {
public:
    explicit ForceRatioAboveNode(f64 threshold)
        : BehaviorNode("ForceRatioAbove"), threshold_(threshold) {}

    NodeStatus tick(BehaviorContext& ctx) override {
        return (ctx.local_force_ratio > threshold_)
            ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
    }

private:
    f64 threshold_;
};

// =============================================================================
// Action Nodes (leaf -- push to g_action_queue)
// =============================================================================

/// Push HOLD_POSITION, always SUCCESS.
class HoldPositionNode : public BehaviorNode {
public:
    HoldPositionNode() : BehaviorNode("HoldPosition") {}

    NodeStatus tick(BehaviorContext& /*ctx*/) override {
        ActionResult a;
        a.type = ActionResult::Type::HOLD_POSITION;
        g_action_queue.push_back(a);
        return NodeStatus::SUCCESS;
    }
};

/// Push ATTACK_ENTITY targeting nearest visible enemy. FAILURE if none visible.
class AttackNearestNode : public BehaviorNode {
public:
    AttackNearestNode() : BehaviorNode("AttackNearest") {}

    NodeStatus tick(BehaviorContext& ctx) override {
        if (ctx.visible_enemies.empty()) return NodeStatus::FAILURE;

        auto& s = ctx.entities->storage();
        f64 self_x = s.pos_x[ctx.self];
        f64 self_y = s.pos_y[ctx.self];

        // Find nearest enemy by squared Euclidean distance
        usize nearest = ctx.visible_enemies[0];
        f64 best_d2 = 1e30;
        for (usize idx : ctx.visible_enemies) {
            f64 dx = s.pos_x[idx] - self_x;
            f64 dy = s.pos_y[idx] - self_y;
            f64 d2 = dx * dx + dy * dy;
            if (d2 < best_d2) {
                best_d2 = d2;
                nearest = idx;
            }
        }

        ActionResult a;
        a.type = ActionResult::Type::ATTACK_ENTITY;
        a.target_entity = s.id[nearest];
        a.target_x = s.pos_x[nearest];
        a.target_y = s.pos_y[nearest];
        g_action_queue.push_back(a);
        return NodeStatus::SUCCESS;
    }
};

/// Push RETREAT away from centroid of visible enemies. Always SUCCESS.
class RetreatNode : public BehaviorNode {
public:
    RetreatNode() : BehaviorNode("Retreat") {}

    NodeStatus tick(BehaviorContext& ctx) override {
        auto& s = ctx.entities->storage();
        f64 self_x = s.pos_x[ctx.self];
        f64 self_y = s.pos_y[ctx.self];

        f64 rx = self_x;
        f64 ry = self_y;

        if (!ctx.visible_enemies.empty()) {
            f64 cx = 0.0, cy = 0.0;
            for (usize idx : ctx.visible_enemies) {
                cx += s.pos_x[idx];
                cy += s.pos_y[idx];
            }
            f64 n = static_cast<f64>(ctx.visible_enemies.size());
            cx /= n;
            cy /= n;

            // Retreat 2 km in direction away from enemy centroid
            f64 dx = self_x - cx;
            f64 dy = self_y - cy;
            f64 len = std::sqrt(dx * dx + dy * dy);
            if (len > 1.0) {
                rx = self_x + (dx / len) * 2000.0;
                ry = self_y + (dy / len) * 2000.0;
            }
        } else {
            // No enemies: retreat 1 km south (deterministic fallback)
            ry -= 1000.0;
        }

        ActionResult a;
        a.type = ActionResult::Type::RETREAT;
        a.target_x = rx;
        a.target_y = ry;
        g_action_queue.push_back(a);
        return NodeStatus::SUCCESS;
    }
};

/// Push MOVE_TO_POSITION for current waypoint. Cycles through waypoints.
/// Returns RUNNING while patrolling (never completes on its own).
class PatrolNode : public BehaviorNode {
public:
    explicit PatrolNode(std::vector<std::pair<f64, f64>> waypoints)
        : BehaviorNode("Patrol"), waypoints_(std::move(waypoints)), wp_idx_(0) {}

    NodeStatus tick(BehaviorContext& ctx) override {
        if (waypoints_.empty()) return NodeStatus::FAILURE;

        auto& [wx, wy] = waypoints_[wp_idx_];

        ActionResult a;
        a.type = ActionResult::Type::MOVE_TO_POSITION;
        a.target_x = wx;
        a.target_y = wy;
        g_action_queue.push_back(a);

        // Advance to next waypoint if within 50 m of current one
        auto& s = ctx.entities->storage();
        f64 dx = s.pos_x[ctx.self] - wx;
        f64 dy = s.pos_y[ctx.self] - wy;
        if (std::sqrt(dx * dx + dy * dy) < 50.0) {
            wp_idx_ = (wp_idx_ + 1) % waypoints_.size();
        }

        return NodeStatus::RUNNING;
    }

    void reset() override { wp_idx_ = 0; }

private:
    std::vector<std::pair<f64, f64>> waypoints_;
    usize wp_idx_;
};

// =============================================================================
// Behavior Tree (wrapper around root node)
// =============================================================================

class BehaviorTree {
public:
    BehaviorTree() = default;
    explicit BehaviorTree(std::unique_ptr<BehaviorNode> root)
        : root_(std::move(root)) {}

    // Move-only (owns the node graph)
    BehaviorTree(BehaviorTree&&) = default;
    BehaviorTree& operator=(BehaviorTree&&) = default;
    BehaviorTree(const BehaviorTree&) = delete;
    BehaviorTree& operator=(const BehaviorTree&) = delete;

    NodeStatus tick(BehaviorContext& ctx) {
        if (!root_) return NodeStatus::FAILURE;
        return root_->tick(ctx);
    }

    void reset() {
        if (root_) root_->reset();
    }

    bool valid() const { return root_ != nullptr; }

private:
    std::unique_ptr<BehaviorNode> root_;
};

// =============================================================================
// Doctrine Configuration
// =============================================================================

struct DoctrineConfig {
    std::string name = "default";

    // Behavioral parameters [0, 1]
    f64 aggression        = 0.5;   // Willingness to engage
    f64 discipline        = 0.5;   // Adherence to orders/formation
    f64 initiative        = 0.5;   // Ability to act independently
    f64 self_preservation = 0.5;   // Willingness to retreat
    f64 coordination      = 0.5;   // Multi-unit coordination quality

    // Decision thresholds
    f64 retreat_health_threshold = 0.2;   // Health <= this triggers retreat
    f64 retreat_supply_threshold = 0.1;   // Supply <= this triggers retreat
    f64 retreat_morale_threshold = 0.2;   // Morale <= this triggers retreat
    f64 min_force_ratio_attack   = 1.0;   // Required friendly:enemy ratio to attack

    // Engagement parameters
    f64 engagement_range_factor = 1.0;    // Multiplier on base engagement range
    f64 pursuit_distance_km     = 5.0;    // Max pursuit distance (km)
};

// =============================================================================
// Pre-built Doctrine Presets
// =============================================================================

namespace doctrines {

/// NATO combined arms defense -- disciplined, coordinated, cautious.
/// Reference: ADP 3-90 (Offense and Defense), FM 3-21.20
DoctrineConfig nato_defensive();

/// NATO offensive operations -- aggressive but coordinated.
/// Reference: ADP 3-90
DoctrineConfig nato_offensive();

/// Soviet/Russian deep battle -- mass, aggression, acceptable losses.
/// Reference: FM 100-2-1 (Soviet Army Operations and Tactics)
DoctrineConfig soviet_offensive();

/// Guerrilla/insurgent -- hit-and-run, high self-preservation.
/// Reference: Mao Zedong, "On Guerrilla Warfare"
DoctrineConfig guerrilla();

/// IDF aggressive maneuver -- high initiative, decisive engagement.
/// Reference: IDF Combined Arms doctrine, Yom Kippur War lessons
DoctrineConfig idf_aggressive();

}  // namespace doctrines

// =============================================================================
// Behavior Tree Factory
// =============================================================================

/// Constructs behavior trees shaped by doctrine parameters.
/// Trees are built once per entity, then ticked each simulation step.
class BehaviorTreeFactory {
public:
    /// Create combat behavior tree.
    ///
    /// Structure:
    ///   Selector (CombatRoot)
    ///   +-- Sequence (CriticalRetreat)
    ///   |   +-- Inverter(HealthAbove(retreat_health_threshold))
    ///   |   +-- Retreat
    ///   +-- Sequence (LowMoraleRetreat)
    ///   |   +-- Inverter(MoraleAbove(retreat_morale_threshold))
    ///   |   +-- Retreat
    ///   +-- Sequence (Attack)
    ///   |   +-- EnemiesVisible
    ///   |   +-- ForceRatioAbove(min_force_ratio_attack)
    ///   |   +-- AttackNearest
    ///   +-- Sequence (DefendAgainstContact)
    ///   |   +-- EnemiesVisible
    ///   |   +-- HoldPosition
    ///   +-- HoldPosition (fallback: no contact, no threat)
    static BehaviorTree create_combat_tree(const DoctrineConfig& doctrine);

    /// Create patrol behavior tree with waypoint list.
    ///
    /// Structure:
    ///   Selector (PatrolRoot)
    ///   +-- Sequence (ReactToContact)
    ///   |   +-- EnemiesVisible
    ///   |   +-- AttackNearest
    ///   +-- Patrol(waypoints)  [RUNNING while patrolling]
    static BehaviorTree create_patrol_tree(
        const std::vector<std::pair<f64, f64>>& waypoints);
};

// =============================================================================
// AI System (orchestrates per-entity behavior)
// =============================================================================

/// Manages behavior trees and action dispatch for all entities.
///
/// Usage:
///   AISystem ai;
///   ai.init(entity_capacity);
///   ai.assign_doctrine_to_side(Side::BLUE, doctrine, entity_manager);
///   ai.update(entity_manager, rng, tick);
///   auto actions = ai.get_actions(entity_id);
class AISystem {
public:
    AISystem();

    /// Allocate internal storage for up to @p capacity entities.
    void init(usize capacity);

    /// Clear all trees, blackboards, and actions.
    void reset();

    /// Assign a doctrine to a single entity by storage index.
    void assign_doctrine(usize entity_idx, const DoctrineConfig& doctrine);

    /// Assign a doctrine to all active entities of the given side.
    /// Creates a combat behavior tree per entity.
    void assign_doctrine_to_side(Side side, const DoctrineConfig& doctrine,
                                 EntityManager& entities);

    /// Tick all assigned behavior trees.
    /// Computes perception (visible enemies/friendlies, force ratio)
    /// and dispatches actions per entity.
    void update(EntityManager& entities, Rng& rng, Tick tick);

    /// Get actions produced for entity during last update().
    /// Returns empty vector if entity has no tree or produced no actions.
    std::vector<ActionResult> get_actions(EntityId entity_id) const;

private:
    usize capacity_ = 0;

    // Per storage-index state
    std::vector<BehaviorTree> trees_;
    std::vector<Blackboard>   blackboards_;
    std::vector<bool>         has_tree_;

    // Actions collected during last update(), keyed by EntityId
    std::map<EntityId, std::vector<ActionResult>> actions_;

    /// Populate ctx.visible_enemies, visible_friendlies, local_force_ratio.
    /// Simple model: all active entities of opposite side are visible.
    /// (Real sensor modeling is handled by DetectionSystem.)
    void compute_perception(usize entity_idx, const EntityStorage& storage,
                            BehaviorContext& ctx) const;
};

}  // namespace ai
}  // namespace athena

#endif  // ATHENA_AI_DOCTRINE_HPP
