// ATHENA Core - Command & Control (C2) System
// Contract: Command hierarchy, order propagation, communication delays.
//
// RULES:
// - Commands propagate with delay based on hierarchy depth
// - Communication failures affect coordination
// - Jamming degrades command effectiveness
// - Units without C2 contact operate autonomously
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_SYSTEMS_C2_HPP
#define ATHENA_SYSTEMS_C2_HPP

#include "athena/types.hpp"
#include "athena/rng.hpp"
#include "athena/entities.hpp"
#include <vector>
#include <map>
#include <queue>

namespace athena {
namespace systems {

// =============================================================================
// C2 Configuration
// =============================================================================

struct C2Config {
    // Command delays (ticks per echelon level)
    f64 base_command_delay_ticks = 1.0;
    f64 delay_per_echelon = 0.5;
    
    // Communication reliability
    f64 base_comm_reliability = 0.95;      // Base success rate per tick
    f64 range_reliability_falloff = 0.01;  // Reliability loss per km
    f64 max_comm_range_km = 50.0;          // Beyond this, no communication
    
    // Jamming effects
    f64 jamming_susceptibility = 0.1;      // How much jamming affects reliability
    bool jamming_enabled = true;
    
    // Autonomy
    u32 autonomy_threshold_ticks = 6;      // Ticks without contact before autonomous
    f64 autonomous_effectiveness = 0.7;    // Effectiveness multiplier when autonomous
    
    // Hierarchy
    u32 max_subordinates = 8;              // Max direct reports per commander
    u32 max_echelon_depth = 5;             // Max hierarchy depth
};

// =============================================================================
// Order Types
// =============================================================================

enum class OrderType : u8 {
    NONE = 0,
    MOVE,           // Move to position
    ATTACK,         // Engage target
    DEFEND,         // Hold position
    WITHDRAW,       // Retreat
    SUPPORT,        // Support another unit
    REGROUP,        // Rally to commander
    HOLD_FIRE,      // Cease engagement
    WEAPONS_FREE    // Engage at will
};

struct Order {
    OrderType type = OrderType::NONE;
    EntityId issuer;            // Who issued the order
    EntityId target_entity;     // Target entity (for ATTACK, SUPPORT)
    f64 target_x = 0.0;         // Target position
    f64 target_y = 0.0;
    f64 target_z = 0.0;
    Tick issued_tick = 0;       // When order was issued
    Tick received_tick = 0;     // When order was received (after delay)
    u8 priority = 0;            // Higher = more important
    bool acknowledged = false;
};

// =============================================================================
// Communication Status
// =============================================================================

enum class CommStatus : u8 {
    CONNECTED = 0,      // Normal communication
    DEGRADED,           // Intermittent communication
    JAMMED,             // Communication jammed
    OUT_OF_RANGE,       // Beyond communication range
    DESTROYED           // Commander destroyed
};

struct CommLink {
    EntityId superior;          // Commander
    EntityId subordinate;       // Unit
    CommStatus status = CommStatus::CONNECTED;
    Tick last_contact_tick = 0;
    f64 current_reliability = 1.0;
    u32 failed_attempts = 0;
};

// =============================================================================
// C2 System
// =============================================================================

class C2System {
public:
    C2System();
    
    /// Initialize C2 system
    void init(usize capacity, const C2Config& config);
    
    /// Reset to initial state
    void reset();
    
    /// Main update
    Status update(EntityStorage& storage, Rng& rng, Tick tick);
    
    // =========================================================================
    // Hierarchy Management
    // =========================================================================
    
    /// Set command relationship
    void set_commander(EntityId subordinate, EntityId commander);
    
    /// Remove command relationship
    void remove_commander(EntityId subordinate);
    
    /// Get commander of entity
    EntityId get_commander(EntityId entity) const;
    
    /// Get all subordinates
    std::vector<EntityId> get_subordinates(EntityId commander) const;
    
    /// Get echelon level (0 = top, higher = lower in hierarchy)
    u32 get_echelon_level(EntityId entity) const;
    
    // =========================================================================
    // Order Management
    // =========================================================================
    
    /// Issue order from commander to subordinate
    bool issue_order(EntityId commander, EntityId subordinate, const Order& order, Tick tick);
    
    /// Issue order to all subordinates
    void issue_order_to_all(EntityId commander, const Order& order, Tick tick);
    
    /// Get current order for entity
    const Order& get_current_order(EntityId entity) const;
    
    /// Check if entity has pending orders
    bool has_pending_orders(EntityId entity) const;
    
    // =========================================================================
    // Communication Status
    // =========================================================================
    
    /// Get communication status with superior
    CommStatus get_comm_status(EntityId entity) const;
    
    /// Check if entity is operating autonomously
    bool is_autonomous(EntityId entity) const;
    
    /// Get effectiveness multiplier (reduced if autonomous/jammed)
    f64 get_effectiveness_multiplier(EntityId entity) const;
    
    /// Set jamming level at position (0-1)
    void set_jamming(f64 x, f64 y, f64 radius, f64 intensity);
    
    /// Clear all jamming
    void clear_jamming();
    
    // =========================================================================
    // Statistics
    // =========================================================================
    
    struct Stats {
        u64 orders_issued;
        u64 orders_delivered;
        u64 orders_failed;
        u64 comm_checks;
        u64 comm_failures;
        u64 units_autonomous;
    };
    
    const Stats& stats() const { return stats_; }
    void reset_stats();

private:
    C2Config config_;
    
    // Hierarchy: subordinate -> commander
    std::map<EntityId, EntityId> commanders_;
    
    // Reverse lookup: commander -> subordinates
    std::map<EntityId, std::vector<EntityId>> subordinates_;
    
    // Communication links
    std::map<EntityId, CommLink> comm_links_;
    
    // Current orders per entity
    std::map<EntityId, Order> current_orders_;
    
    // Pending orders (in transit)
    struct PendingOrder {
        EntityId target;
        Order order;
        Tick delivery_tick;
    };
    std::vector<PendingOrder> pending_orders_;
    
    // Jamming zones
    struct JammingZone {
        f64 x, y;
        f64 radius;
        f64 intensity;  // 0-1
    };
    std::vector<JammingZone> jamming_zones_;
    
    // Autonomy tracking
    std::map<EntityId, Tick> last_contact_;
    std::map<EntityId, bool> autonomous_mode_;
    
    Stats stats_;
    bool initialized_ = false;
    
    // Internal methods
    f64 calculate_comm_reliability(
        const EntityStorage& storage,
        EntityId from,
        EntityId to,
        Tick tick) const;
    
    f64 get_jamming_at(f64 x, f64 y) const;
    
    u32 calculate_delay(EntityId from, EntityId to) const;
    
    void process_pending_orders(Tick tick);
    
    void update_comm_status(EntityStorage& storage, Rng& rng, Tick tick);
    
    void check_autonomy(Tick tick);
};

// =============================================================================
// String Conversions
// =============================================================================

inline const char* order_type_name(OrderType type) {
    switch (type) {
        case OrderType::NONE:         return "none";
        case OrderType::MOVE:         return "move";
        case OrderType::ATTACK:       return "attack";
        case OrderType::DEFEND:       return "defend";
        case OrderType::WITHDRAW:     return "withdraw";
        case OrderType::SUPPORT:      return "support";
        case OrderType::REGROUP:      return "regroup";
        case OrderType::HOLD_FIRE:    return "hold_fire";
        case OrderType::WEAPONS_FREE: return "weapons_free";
        default:                      return "unknown";
    }
}

inline const char* comm_status_name(CommStatus status) {
    switch (status) {
        case CommStatus::CONNECTED:    return "connected";
        case CommStatus::DEGRADED:     return "degraded";
        case CommStatus::JAMMED:       return "jammed";
        case CommStatus::OUT_OF_RANGE: return "out_of_range";
        case CommStatus::DESTROYED:    return "destroyed";
        default:                       return "unknown";
    }
}

// =============================================================================
// Global System Pointer (for scheduler integration)
// =============================================================================

inline C2System* g_c2_system = nullptr;

/// System update function for scheduler
inline Status c2_system_update(EntityStorage& storage, Rng& rng, Tick tick) {
    if (!g_c2_system) {
        return Error(ErrorCode::INTERNAL_ERROR, "C2 system not initialized");
    }
    return g_c2_system->update(storage, rng, tick);
}

}  // namespace systems
}  // namespace athena

#endif  // ATHENA_SYSTEMS_C2_HPP
