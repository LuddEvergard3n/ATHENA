// ATHENA Core - Simulation Systems
// Contract: All simulation systems in one include.
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_SYSTEMS_HPP
#define ATHENA_SYSTEMS_HPP

#include "athena/systems/movement.hpp"
#include "athena/systems/combat.hpp"
#include "athena/systems/logistics.hpp"
#include "athena/systems/detection.hpp"
#include "athena/systems/c2.hpp"
#include "athena/systems/operational.hpp"
#include "athena/scheduler.hpp"

namespace athena {
namespace systems {

// =============================================================================
// System Registration Helper
// =============================================================================

// Initialize all systems with default configs
struct SystemsBundle {
    MovementSystem movement;
    CombatSystem combat;
    LogisticsSystem logistics;
    DetectionSystem detection;
    C2System c2;
    
    void init(usize entity_capacity) {
        movement.init(entity_capacity, MovementConfig{});
        combat.init(entity_capacity, CombatConfig{});
        logistics.init(entity_capacity, LogisticsConfig{});
        detection.init(entity_capacity, DetectionConfig{});
        c2.init(entity_capacity, C2Config{});
        
        // Set global pointers
        g_movement_system = &movement;
        g_combat_system = &combat;
        g_logistics_system = &logistics;
        g_detection_system = &detection;
        g_c2_system = &c2;
    }
    
    void init(usize entity_capacity,
              const MovementConfig& move_cfg,
              const CombatConfig& combat_cfg,
              const LogisticsConfig& logistics_cfg) {
        movement.init(entity_capacity, move_cfg);
        combat.init(entity_capacity, combat_cfg);
        logistics.init(entity_capacity, logistics_cfg);
        detection.init(entity_capacity, DetectionConfig{});
        c2.init(entity_capacity, C2Config{});
        
        g_movement_system = &movement;
        g_combat_system = &combat;
        g_logistics_system = &logistics;
        g_detection_system = &detection;
        g_c2_system = &c2;
    }
    
    void init(usize entity_capacity,
              const MovementConfig& move_cfg,
              const CombatConfig& combat_cfg,
              const LogisticsConfig& logistics_cfg,
              const DetectionConfig& detection_cfg) {
        movement.init(entity_capacity, move_cfg);
        combat.init(entity_capacity, combat_cfg);
        logistics.init(entity_capacity, logistics_cfg);
        detection.init(entity_capacity, detection_cfg);
        c2.init(entity_capacity, C2Config{});
        
        g_movement_system = &movement;
        g_combat_system = &combat;
        g_logistics_system = &logistics;
        g_detection_system = &detection;
        g_c2_system = &c2;
    }
    
    void init(usize entity_capacity,
              const MovementConfig& move_cfg,
              const CombatConfig& combat_cfg,
              const LogisticsConfig& logistics_cfg,
              const DetectionConfig& detection_cfg,
              const C2Config& c2_cfg) {
        movement.init(entity_capacity, move_cfg);
        combat.init(entity_capacity, combat_cfg);
        logistics.init(entity_capacity, logistics_cfg);
        detection.init(entity_capacity, detection_cfg);
        c2.init(entity_capacity, c2_cfg);
        
        g_movement_system = &movement;
        g_combat_system = &combat;
        g_logistics_system = &logistics;
        g_detection_system = &detection;
        g_c2_system = &c2;
    }
    
    void reset() {
        movement.reset();
        combat.reset();
        logistics.reset();
        detection.reset();
        c2.reset();
    }
    
    ~SystemsBundle() {
        // Clear global pointers
        if (g_movement_system == &movement) g_movement_system = nullptr;
        if (g_combat_system == &combat) g_combat_system = nullptr;
        if (g_logistics_system == &logistics) g_logistics_system = nullptr;
        if (g_detection_system == &detection) g_detection_system = nullptr;
        if (g_c2_system == &c2) g_c2_system = nullptr;
    }
};

// Register all system updates with scheduler
inline Status register_all_systems(Scheduler& scheduler) {
    Status s;
    
    s = scheduler.register_system(
        Scheduler::Phase::MOVEMENT, 
        movement_system_update, 
        "movement");
    if (!s.ok()) return s;
    
    s = scheduler.register_system(
        Scheduler::Phase::DETECTION,
        detection_system_update,
        "detection");
    if (!s.ok()) return s;
    
    s = scheduler.register_system(
        Scheduler::Phase::COMBAT,
        combat_system_update,
        "combat");
    if (!s.ok()) return s;
    
    s = scheduler.register_system(
        Scheduler::Phase::LOGISTICS,
        logistics_system_update,
        "logistics");
    if (!s.ok()) return s;
    
    return Status();
}

}  // namespace systems
}  // namespace athena

#endif  // ATHENA_SYSTEMS_HPP
