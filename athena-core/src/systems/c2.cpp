// ATHENA Core - C2 System Implementation
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/systems/c2.hpp"
#include <cmath>
#include <algorithm>

namespace athena {
namespace systems {

// =============================================================================
// C2System Implementation
// =============================================================================

C2System::C2System()
    : config_()
    , commanders_()
    , subordinates_()
    , comm_links_()
    , current_orders_()
    , pending_orders_()
    , jamming_zones_()
    , last_contact_()
    , autonomous_mode_()
    , stats_()
    , initialized_(false)
{
}

void C2System::init(usize capacity, const C2Config& config) {
    config_ = config;
    commanders_.clear();
    subordinates_.clear();
    comm_links_.clear();
    current_orders_.clear();
    pending_orders_.clear();
    jamming_zones_.clear();
    last_contact_.clear();
    autonomous_mode_.clear();
    reset_stats();
    initialized_ = true;
}

void C2System::reset() {
    commanders_.clear();
    subordinates_.clear();
    comm_links_.clear();
    current_orders_.clear();
    pending_orders_.clear();
    jamming_zones_.clear();
    last_contact_.clear();
    autonomous_mode_.clear();
    reset_stats();
}

void C2System::reset_stats() {
    stats_ = Stats{};
}

Status C2System::update(EntityStorage& storage, Rng& rng, Tick tick) {
    if (!initialized_) {
        return Error(ErrorCode::INTERNAL_ERROR, "C2System not initialized");
    }
    
    // 1. Update communication status for all links
    update_comm_status(storage, rng, tick);
    
    // 2. Process pending orders (deliver if delay elapsed)
    process_pending_orders(tick);
    
    // 3. Check for units going autonomous
    check_autonomy(tick);
    
    return Status();
}

// =============================================================================
// Hierarchy Management
// =============================================================================

void C2System::set_commander(EntityId subordinate, EntityId commander) {
    // Remove from old commander if exists
    auto it = commanders_.find(subordinate);
    if (it != commanders_.end()) {
        EntityId old_commander = it->second;
        auto& old_subs = subordinates_[old_commander];
        old_subs.erase(
            std::remove(old_subs.begin(), old_subs.end(), subordinate),
            old_subs.end()
        );
    }
    
    // Set new commander
    commanders_[subordinate] = commander;
    subordinates_[commander].push_back(subordinate);
    
    // Initialize comm link
    CommLink link;
    link.superior = commander;
    link.subordinate = subordinate;
    link.status = CommStatus::CONNECTED;
    link.current_reliability = config_.base_comm_reliability;
    comm_links_[subordinate] = link;
    
    // Initialize contact tracking
    last_contact_[subordinate] = 0;
    autonomous_mode_[subordinate] = false;
}

void C2System::remove_commander(EntityId subordinate) {
    auto it = commanders_.find(subordinate);
    if (it != commanders_.end()) {
        EntityId commander = it->second;
        
        // Remove from commander's subordinate list
        auto& subs = subordinates_[commander];
        subs.erase(
            std::remove(subs.begin(), subs.end(), subordinate),
            subs.end()
        );
        
        commanders_.erase(it);
        comm_links_.erase(subordinate);
    }
}

EntityId C2System::get_commander(EntityId entity) const {
    auto it = commanders_.find(entity);
    if (it != commanders_.end()) {
        return it->second;
    }
    return 0;  // No commander
}

std::vector<EntityId> C2System::get_subordinates(EntityId commander) const {
    auto it = subordinates_.find(commander);
    if (it != subordinates_.end()) {
        return it->second;
    }
    return {};
}

u32 C2System::get_echelon_level(EntityId entity) const {
    u32 level = 0;
    EntityId current = entity;
    
    while (level < config_.max_echelon_depth) {
        auto it = commanders_.find(current);
        if (it == commanders_.end()) {
            break;  // No more commanders
        }
        current = it->second;
        level++;
    }
    
    return level;
}

// =============================================================================
// Order Management
// =============================================================================

bool C2System::issue_order(EntityId commander, EntityId subordinate, const Order& order, Tick tick) {
    // Verify command relationship
    auto it = commanders_.find(subordinate);
    if (it == commanders_.end() || it->second != commander) {
        return false;  // No command authority
    }
    
    // Check communication status
    auto comm_it = comm_links_.find(subordinate);
    if (comm_it != comm_links_.end()) {
        CommStatus status = comm_it->second.status;
        if (status == CommStatus::DESTROYED || status == CommStatus::OUT_OF_RANGE) {
            stats_.orders_failed++;
            return false;
        }
    }
    
    stats_.orders_issued++;
    
    // Calculate delay
    u32 delay = calculate_delay(commander, subordinate);
    
    // Create pending order
    PendingOrder pending;
    pending.target = subordinate;
    pending.order = order;
    pending.order.issuer = commander;
    pending.order.issued_tick = tick;
    pending.delivery_tick = tick + delay;
    
    pending_orders_.push_back(pending);
    
    return true;
}

void C2System::issue_order_to_all(EntityId commander, const Order& order, Tick tick) {
    auto it = subordinates_.find(commander);
    if (it == subordinates_.end()) return;
    
    for (EntityId sub : it->second) {
        issue_order(commander, sub, order, tick);
    }
}

const Order& C2System::get_current_order(EntityId entity) const {
    static const Order empty_order{};
    auto it = current_orders_.find(entity);
    if (it != current_orders_.end()) {
        return it->second;
    }
    return empty_order;
}

bool C2System::has_pending_orders(EntityId entity) const {
    for (const auto& pending : pending_orders_) {
        if (pending.target == entity) {
            return true;
        }
    }
    return false;
}

// =============================================================================
// Communication Status
// =============================================================================

CommStatus C2System::get_comm_status(EntityId entity) const {
    auto it = comm_links_.find(entity);
    if (it != comm_links_.end()) {
        return it->second.status;
    }
    return CommStatus::CONNECTED;  // Default if no link
}

bool C2System::is_autonomous(EntityId entity) const {
    auto it = autonomous_mode_.find(entity);
    if (it != autonomous_mode_.end()) {
        return it->second;
    }
    return false;
}

f64 C2System::get_effectiveness_multiplier(EntityId entity) const {
    // Check autonomous mode
    if (is_autonomous(entity)) {
        return config_.autonomous_effectiveness;
    }
    
    // Check comm status
    auto it = comm_links_.find(entity);
    if (it != comm_links_.end()) {
        switch (it->second.status) {
            case CommStatus::CONNECTED:
                return 1.0;
            case CommStatus::DEGRADED:
                return 0.9;
            case CommStatus::JAMMED:
                return 0.7;
            case CommStatus::OUT_OF_RANGE:
            case CommStatus::DESTROYED:
                return config_.autonomous_effectiveness;
            default:
                return 1.0;
        }
    }
    
    return 1.0;
}

void C2System::set_jamming(f64 x, f64 y, f64 radius, f64 intensity) {
    JammingZone zone;
    zone.x = x;
    zone.y = y;
    zone.radius = radius;
    zone.intensity = std::clamp(intensity, 0.0, 1.0);
    jamming_zones_.push_back(zone);
}

void C2System::clear_jamming() {
    jamming_zones_.clear();
}

// =============================================================================
// Internal Methods
// =============================================================================

f64 C2System::calculate_comm_reliability(
    const EntityStorage& storage,
    EntityId from,
    EntityId to,
    Tick tick) const {
    
    // Find entity indices
    usize from_idx = storage.count;
    usize to_idx = storage.count;
    
    for (usize i = 0; i < storage.count; ++i) {
        if (storage.id[i] == from) from_idx = i;
        if (storage.id[i] == to) to_idx = i;
    }
    
    if (from_idx >= storage.count || to_idx >= storage.count) {
        return 0.0;  // Entity not found
    }
    
    // Check if entities are active
    if (!storage.is_active(from_idx) || !storage.is_active(to_idx)) {
        return 0.0;
    }
    
    // Calculate distance
    f64 dx = storage.pos_x[to_idx] - storage.pos_x[from_idx];
    f64 dy = storage.pos_y[to_idx] - storage.pos_y[from_idx];
    f64 distance_m = std::sqrt(dx*dx + dy*dy);
    f64 distance_km = distance_m / 1000.0;
    
    // Beyond max range = no communication
    if (distance_km > config_.max_comm_range_km) {
        return 0.0;
    }
    
    // Base reliability
    f64 reliability = config_.base_comm_reliability;
    
    // Range falloff
    reliability -= config_.range_reliability_falloff * distance_km;
    
    // Jamming effect
    if (config_.jamming_enabled) {
        // Check jamming at both positions
        f64 jamming_from = get_jamming_at(storage.pos_x[from_idx], storage.pos_y[from_idx]);
        f64 jamming_to = get_jamming_at(storage.pos_x[to_idx], storage.pos_y[to_idx]);
        f64 max_jamming = std::max(jamming_from, jamming_to);
        
        reliability *= (1.0 - max_jamming * config_.jamming_susceptibility);
    }
    
    return std::clamp(reliability, 0.0, 1.0);
}

f64 C2System::get_jamming_at(f64 x, f64 y) const {
    f64 max_jamming = 0.0;
    
    for (const auto& zone : jamming_zones_) {
        f64 dx = x - zone.x;
        f64 dy = y - zone.y;
        f64 distance = std::sqrt(dx*dx + dy*dy);
        
        if (distance < zone.radius) {
            // Linear falloff from center
            f64 falloff = 1.0 - (distance / zone.radius);
            f64 jamming = zone.intensity * falloff;
            max_jamming = std::max(max_jamming, jamming);
        }
    }
    
    return max_jamming;
}

u32 C2System::calculate_delay(EntityId from, EntityId to) const {
    // Base delay
    f64 delay = config_.base_command_delay_ticks;
    
    // Add delay per echelon level difference
    u32 from_level = get_echelon_level(from);
    u32 to_level = get_echelon_level(to);
    
    if (to_level > from_level) {
        delay += (to_level - from_level) * config_.delay_per_echelon;
    }
    
    return static_cast<u32>(std::ceil(delay));
}

void C2System::process_pending_orders(Tick tick) {
    // Process and remove delivered orders
    auto it = pending_orders_.begin();
    while (it != pending_orders_.end()) {
        if (tick >= it->delivery_tick) {
            // Check if communication still works
            auto comm_it = comm_links_.find(it->target);
            bool can_deliver = true;
            
            if (comm_it != comm_links_.end()) {
                CommStatus status = comm_it->second.status;
                if (status == CommStatus::DESTROYED || 
                    status == CommStatus::OUT_OF_RANGE) {
                    can_deliver = false;
                }
            }
            
            if (can_deliver) {
                // Deliver order
                it->order.received_tick = tick;
                it->order.acknowledged = true;
                current_orders_[it->target] = it->order;
                stats_.orders_delivered++;
                
                // Update last contact
                last_contact_[it->target] = tick;
                autonomous_mode_[it->target] = false;
            } else {
                stats_.orders_failed++;
            }
            
            it = pending_orders_.erase(it);
        } else {
            ++it;
        }
    }
}

void C2System::update_comm_status(EntityStorage& storage, Rng& rng, Tick tick) {
    for (auto& [entity, link] : comm_links_) {
        stats_.comm_checks++;
        
        // Check if commander still exists and is active
        usize cmd_idx = storage.count;
        for (usize i = 0; i < storage.count; ++i) {
            if (storage.id[i] == link.superior) {
                cmd_idx = i;
                break;
            }
        }
        
        if (cmd_idx >= storage.count || !storage.is_active(cmd_idx)) {
            link.status = CommStatus::DESTROYED;
            link.current_reliability = 0.0;
            continue;
        }
        
        // Calculate reliability
        f64 reliability = calculate_comm_reliability(storage, link.superior, entity, tick);
        link.current_reliability = reliability;
        
        if (reliability <= 0.0) {
            link.status = CommStatus::OUT_OF_RANGE;
        } else {
            // Roll for communication success
            f64 roll = rng.next_f64();
            
            if (roll < reliability) {
                // Success
                link.failed_attempts = 0;
                link.last_contact_tick = tick;
                last_contact_[entity] = tick;
                
                // Check jamming level
                usize ent_idx = storage.count;
                for (usize i = 0; i < storage.count; ++i) {
                    if (storage.id[i] == entity) {
                        ent_idx = i;
                        break;
                    }
                }
                
                f64 jamming = 0.0;
                if (ent_idx < storage.count) {
                    jamming = get_jamming_at(storage.pos_x[ent_idx], storage.pos_y[ent_idx]);
                }
                
                if (jamming > 0.5) {
                    link.status = CommStatus::JAMMED;
                } else if (reliability < 0.7) {
                    link.status = CommStatus::DEGRADED;
                } else {
                    link.status = CommStatus::CONNECTED;
                }
            } else {
                // Failure
                stats_.comm_failures++;
                link.failed_attempts++;
                
                if (link.failed_attempts >= 3) {
                    link.status = CommStatus::DEGRADED;
                }
            }
        }
    }
}

void C2System::check_autonomy(Tick tick) {
    stats_.units_autonomous = 0;
    
    for (auto& [entity, last_tick] : last_contact_) {
        if (tick - last_tick > config_.autonomy_threshold_ticks) {
            autonomous_mode_[entity] = true;
            stats_.units_autonomous++;
        }
    }
}

}  // namespace systems
}  // namespace athena
