// ATHENA Core - Detection System
// Contract: Fog of war, sensor modeling, contact management.
//
// RULES:
// - Detection uses TerrainSemantics for LOS and sensor effectiveness
// - All detection is probabilistic (uses RNG)
// - Contacts decay if not refreshed
// - Identification is separate from detection
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_SYSTEMS_DETECTION_HPP
#define ATHENA_SYSTEMS_DETECTION_HPP

#include "athena/types.hpp"
#include "athena/rng.hpp"
#include "athena/entities.hpp"
#include "athena/terrain_semantics.hpp"
#include <vector>
#include <map>

namespace athena {
namespace systems {

// =============================================================================
// Detection Configuration
// =============================================================================

struct DetectionConfig {
    // Base probabilities (modified by terrain, weather, range)
    f64 base_detection_probability = 0.3;
    f64 base_identification_probability = 0.5;  // Given detection
    
    // Range parameters
    f64 detection_range_km = 10.0;
    f64 identification_range_km = 5.0;
    
    // Decay
    u32 contact_decay_ticks = 6;  // Contacts fade after N ticks without refresh
    
    // Sensor capabilities
    bool visual_enabled = true;
    bool thermal_enabled = false;
    bool radar_enabled = false;
    
    // Terrain integration
    bool terrain_los_enabled = true;
    bool terrain_concealment_enabled = true;
    
    // Detection modifiers
    f64 moving_target_bonus = 0.2;      // Easier to detect moving targets
    f64 firing_target_bonus = 0.4;      // Much easier to detect firing targets
    f64 stationary_penalty = -0.1;      // Harder to detect stationary targets
};

// =============================================================================
// Contact Information
// =============================================================================

enum class ContactType : u8 {
    UNKNOWN = 0,    // Detected but not identified
    FRIENDLY,       // Identified as friendly
    HOSTILE,        // Identified as hostile
    NEUTRAL         // Identified as neutral
};

struct Contact {
    EntityId entity_id;         // Who we detected
    ContactType type;           // What we think they are
    
    // Last known position
    f64 last_x;
    f64 last_y;
    f64 last_z;
    
    // Tracking
    Tick first_detected;        // When first seen
    Tick last_seen;             // Last refresh
    u32 detection_count;        // How many times detected
    
    // Confidence
    f64 position_accuracy;      // How accurate is position (meters error)
    f64 identification_confidence;  // How sure of type (0-1)
    
    // State estimates
    bool is_moving;
    f64 estimated_heading;
    f64 estimated_speed;
};

// =============================================================================
// Detection System
// =============================================================================

class DetectionSystem {
public:
    DetectionSystem();
    
    /// Initialize detection system
    /// @param capacity Maximum number of entities
    /// @param config Detection parameters
    void init(usize capacity, const DetectionConfig& config);
    
    /// Reset to initial state
    void reset();
    
    /// Set terrain semantics for LOS calculations
    /// @param terrain Pointer to terrain system (can be null)
    void set_terrain(const TerrainSemantics* terrain);
    
    /// Main update - run detection for all entities
    /// @param storage Entity storage
    /// @param rng Random number generator
    /// @param tick Current simulation tick
    /// @return Status
    Status update(EntityStorage& storage, Rng& rng, Tick tick);
    
    // =========================================================================
    // Contact Queries
    // =========================================================================
    
    /// Get all contacts for an entity
    const std::vector<Contact>& get_contacts(usize entity_idx) const;
    
    /// Get specific contact
    const Contact* get_contact(usize observer_idx, EntityId target_id) const;
    
    /// Check if entity has detected another
    bool has_detected(usize observer_idx, EntityId target_id) const;
    
    /// Check if entity has identified another
    bool has_identified(usize observer_idx, EntityId target_id) const;
    
    /// Get number of contacts for entity
    usize contact_count(usize entity_idx) const;
    
    // =========================================================================
    // Detection Calculations (exposed for testing)
    // =========================================================================
    
    /// Calculate detection probability between two entities
    f64 calculate_detection_probability(
        const EntityStorage& storage,
        usize observer_idx,
        usize target_idx,
        Tick tick) const;
    
    /// Calculate identification probability (given detection)
    f64 calculate_identification_probability(
        const EntityStorage& storage,
        usize observer_idx,
        usize target_idx,
        Tick tick) const;
    
    /// Calculate line of sight fraction
    f64 calculate_los(
        f64 obs_x, f64 obs_y, f64 obs_z,
        f64 tgt_x, f64 tgt_y, f64 tgt_z,
        Tick tick) const;
    
    // =========================================================================
    // Statistics
    // =========================================================================
    
    struct Stats {
        u64 detection_attempts;
        u64 successful_detections;
        u64 successful_identifications;
        u64 contacts_created;
        u64 contacts_decayed;
    };
    
    const Stats& stats() const { return stats_; }
    void reset_stats();

    // =========================================================================
    // Recon Propagation (v1.1.8) — side-level shared detection
    // =========================================================================
    // When any unit on a side detects an enemy, all friendly units on the
    // same side receive that contact (degraded accuracy, delayed by 1 tick).
    // This models the reconnaissance network / C2 sharing.

    /// Propagate contacts within each side.
    /// Call AFTER the main detection update each tick.
    void propagate_contacts_by_side(const EntityStorage& storage, Tick tick);

    /// Query: get ALL contacts known to a side (union of all units)
    /// Returns a merged list with best accuracy per target.
    struct SideContact {
        EntityId target_id;
        f64 best_x, best_y, best_z;   // Best known position
        f64 accuracy;                   // Best position accuracy (meters)
        Tick last_seen;                 // Most recent sighting
        ContactType type;
        bool direct;                    // true if at least one unit sees directly
    };
    std::vector<SideContact> get_side_contacts(Side side, const EntityStorage& storage) const;

private:
    DetectionConfig config_;
    const TerrainSemantics* terrain_;
    
    // Per-entity contact lists
    // contacts_[observer_idx] = list of contacts
    std::vector<std::vector<Contact>> contacts_;
    
    // For quick lookup: has observer detected target?
    // detected_[observer_idx][target_id] = true/false
    std::vector<std::map<EntityId, bool>> detected_;
    
    Stats stats_;
    bool initialized_;
    
    // Internal helpers
    void attempt_detection(
        EntityStorage& storage,
        usize observer_idx,
        usize target_idx,
        Rng& rng,
        Tick tick);
    
    void update_contact(
        usize observer_idx,
        usize target_idx,
        const EntityStorage& storage,
        Tick tick,
        bool identified);
    
    void decay_contacts(Tick tick);
    
    f64 get_sensor_modifier(
        const EntityStorage& storage,
        usize observer_idx,
        usize target_idx,
        Tick tick) const;
    
    f64 get_target_signature(
        const EntityStorage& storage,
        usize target_idx) const;
    
    ContactType determine_contact_type(
        const EntityStorage& storage,
        usize observer_idx,
        usize target_idx) const;
};

// =============================================================================
// String Conversions
// =============================================================================

inline const char* contact_type_name(ContactType type) {
    switch (type) {
        case ContactType::UNKNOWN:  return "unknown";
        case ContactType::FRIENDLY: return "friendly";
        case ContactType::HOSTILE:  return "hostile";
        case ContactType::NEUTRAL:  return "neutral";
        default:                    return "unknown";
    }
}

// =============================================================================
// Global System Pointer (for scheduler integration)
// =============================================================================

inline DetectionSystem* g_detection_system = nullptr;

/// System update function for scheduler
inline Status detection_system_update(EntityStorage& storage, Rng& rng, Tick tick) {
    if (!g_detection_system) {
        return Error(ErrorCode::INTERNAL_ERROR, "Detection system not initialized");
    }
    return g_detection_system->update(storage, rng, tick);
}

}  // namespace systems
}  // namespace athena

#endif  // ATHENA_SYSTEMS_DETECTION_HPP
