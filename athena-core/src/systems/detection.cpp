// ATHENA Core - Detection System Implementation
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/systems/detection.hpp"
#include <cmath>
#include <algorithm>

namespace athena {
namespace systems {

// =============================================================================
// DetectionSystem Implementation
// =============================================================================

DetectionSystem::DetectionSystem()
    : config_()
    , terrain_(nullptr)
    , contacts_()
    , detected_()
    , stats_()
    , initialized_(false)
{
}

void DetectionSystem::init(usize capacity, const DetectionConfig& config) {
    config_ = config;
    contacts_.resize(capacity);
    detected_.resize(capacity);
    
    for (usize i = 0; i < capacity; ++i) {
        contacts_[i].clear();
        detected_[i].clear();
    }
    
    reset_stats();
    initialized_ = true;
}

void DetectionSystem::reset() {
    for (auto& c : contacts_) {
        c.clear();
    }
    for (auto& d : detected_) {
        d.clear();
    }
    reset_stats();
}

void DetectionSystem::set_terrain(const TerrainSemantics* terrain) {
    terrain_ = terrain;
}

void DetectionSystem::reset_stats() {
    stats_ = Stats{};
}

Status DetectionSystem::update(EntityStorage& storage, Rng& rng, Tick tick) {
    if (!initialized_) {
        return Error(ErrorCode::INTERNAL_ERROR, "DetectionSystem not initialized");
    }
    
    // First, decay old contacts
    decay_contacts(tick);
    
    // For each active entity, attempt detection of all other entities
    for (usize observer = 0; observer < storage.count; ++observer) {
        if (!storage.is_active(observer)) continue;
        
        for (usize target = 0; target < storage.count; ++target) {
            if (!storage.is_active(target)) continue;
            if (observer == target) continue;
            
            // Don't detect friendlies (same side)
            // Actually, let's detect everyone but mark friendlies appropriately
            
            attempt_detection(storage, observer, target, rng, tick);
        }
    }

    // v1.1.8: Propagate contacts within each side (recon network)
    propagate_contacts_by_side(storage, tick);
    
    return Status();
}

void DetectionSystem::attempt_detection(
    EntityStorage& storage,
    usize observer_idx,
    usize target_idx,
    Rng& rng,
    Tick tick) {
    
    stats_.detection_attempts++;
    
    // Calculate detection probability
    f64 det_prob = calculate_detection_probability(storage, observer_idx, target_idx, tick);
    
    if (det_prob <= 0.0) return;
    
    // Roll for detection
    f64 roll = rng.next_f64();
    
    if (roll < det_prob) {
        stats_.successful_detections++;
        
        // Check if we also identify the target
        f64 id_prob = calculate_identification_probability(storage, observer_idx, target_idx, tick);
        f64 id_roll = rng.next_f64();
        bool identified = (id_roll < id_prob);
        
        if (identified) {
            stats_.successful_identifications++;
        }
        
        // Update or create contact
        update_contact(observer_idx, target_idx, storage, tick, identified);
    }
}

f64 DetectionSystem::calculate_detection_probability(
    const EntityStorage& storage,
    usize observer_idx,
    usize target_idx,
    Tick tick) const {
    
    // Get positions
    f64 obs_x = storage.pos_x[observer_idx];
    f64 obs_y = storage.pos_y[observer_idx];
    f64 obs_z = storage.pos_z[observer_idx];
    
    f64 tgt_x = storage.pos_x[target_idx];
    f64 tgt_y = storage.pos_y[target_idx];
    f64 tgt_z = storage.pos_z[target_idx];
    
    // Calculate range
    f64 dx = tgt_x - obs_x;
    f64 dy = tgt_y - obs_y;
    f64 dz = tgt_z - obs_z;
    f64 range_m = std::sqrt(dx*dx + dy*dy + dz*dz);
    f64 range_km = range_m / 1000.0;
    
    // Beyond max range = no detection
    if (range_km > config_.detection_range_km) {
        return 0.0;
    }
    
    // Base probability
    f64 prob = config_.base_detection_probability;
    
    // Range modifier: linear falloff (v1.1.7: safe division)
    f64 range_factor = 1.0 - safe::div(range_km, config_.detection_range_km, 1.0);
    prob *= range_factor;
    
    // Line of sight check
    if (config_.terrain_los_enabled) {
        f64 los = calculate_los(obs_x, obs_y, obs_z, tgt_x, tgt_y, tgt_z, tick);
        prob *= los;
    }
    
    // Target signature (movement, firing, etc.)
    f64 signature = get_target_signature(storage, target_idx);
    prob *= signature;
    
    // Sensor effectiveness
    f64 sensor_mod = get_sensor_modifier(storage, observer_idx, target_idx, tick);
    prob *= sensor_mod;
    
    // Terrain concealment
    if (config_.terrain_concealment_enabled && terrain_) {
        f64 concealment = terrain_->concealment(tgt_x, tgt_y, tick);
        prob *= (1.0 - concealment * 0.7);  // Up to 70% reduction
    }
    
    // Observer's detection capability (sensor range + unit type modifier)
    f64 det_range = storage.detection_range[observer_idx];
    if (det_range > 0.0) {
        f64 det_factor = std::min(1.5, det_range / 5000.0);  // Scale by observer's range
        prob *= det_factor;
    }
    // UnitType detection modifier (e.g., Recon 2.5x, Logistics 0.6x)
    f64 ut_det = get_unit_modifiers(storage.unit_type[observer_idx]).detection_factor;
    prob *= ut_det;
    
    return safe::clamp(prob, 0.0, 0.95);  // Cap at 95%, NaN-safe (v1.1.7)
}

f64 DetectionSystem::calculate_identification_probability(
    const EntityStorage& storage,
    usize observer_idx,
    usize target_idx,
    Tick tick) const {
    
    // Get positions
    f64 obs_x = storage.pos_x[observer_idx];
    f64 obs_y = storage.pos_y[observer_idx];
    f64 tgt_x = storage.pos_x[target_idx];
    f64 tgt_y = storage.pos_y[target_idx];
    
    f64 dx = tgt_x - obs_x;
    f64 dy = tgt_y - obs_y;
    f64 range_m = std::sqrt(dx*dx + dy*dy);
    f64 range_km = range_m / 1000.0;
    
    // Beyond ID range = low probability
    if (range_km > config_.identification_range_km) {
        return config_.base_identification_probability * 0.2;
    }
    
    f64 prob = config_.base_identification_probability;
    
    // Closer = better ID
    f64 range_factor = 1.0 - (range_km / config_.identification_range_km) * 0.5;
    prob *= range_factor;
    
    // Visibility helps
    if (terrain_) {
        // v1.2.1: Use terrain sensor_effectiveness (includes env visibility)
        SensorType sensor = SensorType::VISUAL;
        auto eff = terrain_->sensor_effectiveness(tgt_x, tgt_y, 0, sensor);
        prob *= eff.identification_modifier;
    }
    
    return safe::clamp(prob, 0.0, 0.95);
}

f64 DetectionSystem::calculate_los(
    f64 obs_x, f64 obs_y, f64 obs_z,
    f64 tgt_x, f64 tgt_y, f64 tgt_z,
    Tick tick) const {
    
    if (!terrain_) {
        return 1.0;  // No terrain = full LOS
    }
    
    // Use terrain semantics for LOS
    return terrain_->line_of_sight(obs_x, obs_y, tgt_x, tgt_y, tick);
}

f64 DetectionSystem::get_sensor_modifier(
    const EntityStorage& storage,
    usize observer_idx,
    usize target_idx,
    Tick tick) const {
    
    f64 modifier = 1.0;
    
    // Visual sensor
    if (config_.visual_enabled) {
        // Would be affected by visibility/weather
        modifier = std::max(modifier, 1.0);
    }
    
    // Thermal sensor
    if (config_.thermal_enabled) {
        // Less affected by weather, better at night
        modifier = std::max(modifier, 1.2);
    }
    
    // Radar
    if (config_.radar_enabled) {
        // Good range but affected by terrain clutter
        modifier = std::max(modifier, 1.3);
    }
    
    // Apply terrain sensor effects
    if (terrain_) {
        f64 tgt_x = storage.pos_x[target_idx];
        f64 tgt_y = storage.pos_y[target_idx];
        
        SensorType sensor = SensorType::VISUAL;
        if (config_.thermal_enabled) sensor = SensorType::THERMAL;
        if (config_.radar_enabled) sensor = SensorType::RADAR;
        
        auto eff = terrain_->sensor_effectiveness(tgt_x, tgt_y, tick, sensor);
        modifier *= eff.detection_modifier;
    }
    
    return modifier;
}

f64 DetectionSystem::get_target_signature(
    const EntityStorage& storage,
    usize target_idx) const {
    
    f64 signature = 1.0;
    
    // Moving targets are easier to detect
    f64 speed = std::sqrt(
        storage.vel_x[target_idx] * storage.vel_x[target_idx] +
        storage.vel_y[target_idx] * storage.vel_y[target_idx]
    );
    
    if (speed > 1.0) {  // Moving
        signature += config_.moving_target_bonus;
    } else {
        signature += config_.stationary_penalty;
    }
    
    // Engaged targets are much easier to detect (muzzle flash, noise)
    if (storage.flags[target_idx] & entity_flags::ENGAGED) {
        signature += config_.firing_target_bonus;
    }
    
    return std::max(0.1, signature);
}

void DetectionSystem::update_contact(
    usize observer_idx,
    usize target_idx,
    const EntityStorage& storage,
    Tick tick,
    bool identified) {
    
    EntityId target_id = storage.id[target_idx];
    
    // Check if we already have this contact
    auto it = detected_[observer_idx].find(target_id);
    
    if (it != detected_[observer_idx].end() && it->second) {
        // Update existing contact
        for (auto& contact : contacts_[observer_idx]) {
            if (contact.entity_id == target_id) {
                contact.last_x = storage.pos_x[target_idx];
                contact.last_y = storage.pos_y[target_idx];
                contact.last_z = storage.pos_z[target_idx];
                contact.last_seen = tick;
                contact.detection_count++;
                
                // Update movement estimate
                f64 speed = std::sqrt(
                    storage.vel_x[target_idx] * storage.vel_x[target_idx] +
                    storage.vel_y[target_idx] * storage.vel_y[target_idx]
                );
                contact.is_moving = (speed > 1.0);
                contact.estimated_speed = speed;
                
                if (speed > 0.1) {
                    contact.estimated_heading = std::atan2(
                        storage.vel_y[target_idx],
                        storage.vel_x[target_idx]
                    ) * (180.0 / 3.14159265358979323846);
                }
                
                // Improve accuracy with more detections
                contact.position_accuracy = std::max(50.0, 
                    contact.position_accuracy * 0.9);
                
                // Update identification if we got it
                if (identified && contact.type == ContactType::UNKNOWN) {
                    contact.type = determine_contact_type(storage, observer_idx, target_idx);
                    contact.identification_confidence = 0.7;
                }
                
                // Increase ID confidence with repeated detections
                if (contact.type != ContactType::UNKNOWN) {
                    contact.identification_confidence = std::min(0.99,
                        contact.identification_confidence + 0.05);
                }
                
                return;
            }
        }
    }
    
    // Create new contact
    stats_.contacts_created++;
    
    Contact contact;
    contact.entity_id = target_id;
    contact.type = identified ? 
        determine_contact_type(storage, observer_idx, target_idx) : 
        ContactType::UNKNOWN;
    
    contact.last_x = storage.pos_x[target_idx];
    contact.last_y = storage.pos_y[target_idx];
    contact.last_z = storage.pos_z[target_idx];
    
    contact.first_detected = tick;
    contact.last_seen = tick;
    contact.detection_count = 1;
    
    contact.position_accuracy = 500.0;  // Initial 500m error
    contact.identification_confidence = identified ? 0.6 : 0.0;
    
    f64 speed = std::sqrt(
        storage.vel_x[target_idx] * storage.vel_x[target_idx] +
        storage.vel_y[target_idx] * storage.vel_y[target_idx]
    );
    contact.is_moving = (speed > 1.0);
    contact.estimated_heading = 0.0;
    contact.estimated_speed = speed;
    
    contacts_[observer_idx].push_back(contact);
    detected_[observer_idx][target_id] = true;
}

ContactType DetectionSystem::determine_contact_type(
    const EntityStorage& storage,
    usize observer_idx,
    usize target_idx) const {
    
    Side obs_side = storage.side[observer_idx];
    Side tgt_side = storage.side[target_idx];
    
    if (obs_side == tgt_side) {
        return ContactType::FRIENDLY;
    }
    
    // Different side = hostile (simplified)
    // In real implementation, would check alliance tables
    if (tgt_side == Side::NEUTRAL) {
        return ContactType::NEUTRAL;
    }
    
    return ContactType::HOSTILE;
}

void DetectionSystem::decay_contacts(Tick tick) {
    for (usize i = 0; i < contacts_.size(); ++i) {
        auto& contact_list = contacts_[i];
        
        contact_list.erase(
            std::remove_if(contact_list.begin(), contact_list.end(),
                [this, tick, i](const Contact& c) {
                    if (tick - c.last_seen > config_.contact_decay_ticks) {
                        stats_.contacts_decayed++;
                        detected_[i][c.entity_id] = false;
                        return true;
                    }
                    return false;
                }),
            contact_list.end()
        );
    }
}

// =============================================================================
// Contact Queries
// =============================================================================

const std::vector<Contact>& DetectionSystem::get_contacts(usize entity_idx) const {
    static const std::vector<Contact> empty;
    if (entity_idx >= contacts_.size()) return empty;
    return contacts_[entity_idx];
}

const Contact* DetectionSystem::get_contact(usize observer_idx, EntityId target_id) const {
    if (observer_idx >= contacts_.size()) return nullptr;
    
    for (const auto& contact : contacts_[observer_idx]) {
        if (contact.entity_id == target_id) {
            return &contact;
        }
    }
    
    return nullptr;
}

bool DetectionSystem::has_detected(usize observer_idx, EntityId target_id) const {
    if (observer_idx >= detected_.size()) return false;
    
    auto it = detected_[observer_idx].find(target_id);
    return it != detected_[observer_idx].end() && it->second;
}

bool DetectionSystem::has_identified(usize observer_idx, EntityId target_id) const {
    const Contact* c = get_contact(observer_idx, target_id);
    return c != nullptr && c->type != ContactType::UNKNOWN;
}

usize DetectionSystem::contact_count(usize entity_idx) const {
    if (entity_idx >= contacts_.size()) return 0;
    return contacts_[entity_idx].size();
}

// =============================================================================
// Recon Propagation (v1.1.8)
// =============================================================================
// If unit A (blue) sees enemy X, all blue units get a shared contact for X.
// Shared contacts have degraded accuracy (position error * 2.0) to model
// communication latency and reporting inaccuracy in the reconnaissance network.

void DetectionSystem::propagate_contacts_by_side(const EntityStorage& storage, Tick tick) {
    if (!initialized_) return;
    (void)tick;  // Reserved for future: latency-based propagation delay

    // For each side, collect the best contact per target from any unit on that side
    // Side::MAX_SIDES = 16
    constexpr usize MAX_S = static_cast<usize>(Side::MAX_SIDES);

    // side_contacts[side][target_id] = best Contact
    std::map<EntityId, Contact> side_best[MAX_S];

    // Phase 1: collect best contacts per side
    for (usize i = 0; i < storage.count; ++i) {
        if (!storage.is_active(i)) continue;
        usize s = static_cast<usize>(storage.side[i]);
        if (s >= MAX_S) continue;

        for (const auto& c : contacts_[i]) {
            auto it = side_best[s].find(c.entity_id);
            if (it == side_best[s].end()) {
                side_best[s][c.entity_id] = c;
            } else {
                // Keep the one with better accuracy (lower = better)
                if (c.position_accuracy < it->second.position_accuracy) {
                    it->second = c;
                }
                // Always use most recent timestamp
                if (c.last_seen > it->second.last_seen) {
                    it->second.last_x = c.last_x;
                    it->second.last_y = c.last_y;
                    it->second.last_z = c.last_z;
                    it->second.last_seen = c.last_seen;
                }
            }
        }
    }

    // Phase 2: propagate to all units on each side
    for (usize i = 0; i < storage.count; ++i) {
        if (!storage.is_active(i)) continue;
        usize s = static_cast<usize>(storage.side[i]);
        if (s >= MAX_S) continue;

        for (const auto& [target_id, best] : side_best[s]) {
            // Skip if this unit already has a direct contact (theirs is better)
            if (has_detected(i, target_id)) continue;

            // Create shared contact with degraded accuracy
            Contact shared = best;
            shared.position_accuracy *= 2.0;   // Shared intel is less precise
            shared.identification_confidence *= 0.7;  // Less certain through relay

            contacts_[i].push_back(shared);
            detected_[i][target_id] = true;
        }
    }
}

std::vector<DetectionSystem::SideContact>
DetectionSystem::get_side_contacts(Side side, const EntityStorage& storage) const {
    std::map<EntityId, SideContact> merged;

    for (usize i = 0; i < storage.count; ++i) {
        if (!storage.is_active(i)) continue;
        if (storage.side[i] != side) continue;

        for (const auto& c : contacts_[i]) {
            // Only share hostile/unknown contacts
            if (c.type == ContactType::FRIENDLY) continue;

            auto it = merged.find(c.entity_id);
            if (it == merged.end()) {
                SideContact sc;
                sc.target_id = c.entity_id;
                sc.best_x = c.last_x;
                sc.best_y = c.last_y;
                sc.best_z = c.last_z;
                sc.accuracy = c.position_accuracy;
                sc.last_seen = c.last_seen;
                sc.type = c.type;
                sc.direct = true;  // At least one unit sees directly
                merged[c.entity_id] = sc;
            } else {
                // Update with better data
                if (c.position_accuracy < it->second.accuracy) {
                    it->second.best_x = c.last_x;
                    it->second.best_y = c.last_y;
                    it->second.best_z = c.last_z;
                    it->second.accuracy = c.position_accuracy;
                }
                if (c.last_seen > it->second.last_seen) {
                    it->second.last_seen = c.last_seen;
                }
                if (c.type == ContactType::HOSTILE) {
                    it->second.type = ContactType::HOSTILE;
                }
            }
        }
    }

    std::vector<SideContact> result;
    result.reserve(merged.size());
    for (const auto& [id, sc] : merged) {
        result.push_back(sc);
    }
    return result;
}

}  // namespace systems
}  // namespace athena
