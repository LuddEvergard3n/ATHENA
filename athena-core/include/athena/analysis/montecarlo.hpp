// ATHENA Core - Monte Carlo Batch Executor
// Contract: Deterministic parallel execution of scenario iterations.
//
// RULES:
// - Each iteration has unique, derived seed
// - Iterations are independent (no shared state)
// - Results aggregated in iteration order (deterministic)
// - Parallelism at iteration level only
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_ANALYSIS_MONTECARLO_HPP
#define ATHENA_ANALYSIS_MONTECARLO_HPP

#include "athena/types.hpp"
#include "athena/rng.hpp"
#include "athena/scenario.hpp"
#include "athena/context.hpp"
#include "athena/entities.hpp"
#include "athena/scheduler.hpp"
#include "athena/systems.hpp"
#include "athena/environment.hpp"
#include <vector>
#include <functional>

namespace athena {
namespace analysis {

// =============================================================================
// Termination Reason (v1.1.7 — auditable stop conditions)
// =============================================================================

enum class TerminationReason : u8 {
    MAX_TICKS,            // Reached max tick limit
    DECISIVE_VICTORY,     // One side eliminated (or below threshold)
    NO_OFFENSIVE_CAPACITY,// All remaining units lack firepower
    MUTUAL_BLINDNESS,     // No side can detect any enemy
    UNREACHABLE,          // Remaining units cannot close distance to enemies
    CANCELLED,            // User cancelled
    ERROR,                // Simulation error
};

/// Human-readable label for termination reason
inline const char* termination_reason_label(TerminationReason r) {
    switch (r) {
        case TerminationReason::MAX_TICKS:             return "Time limit reached";
        case TerminationReason::DECISIVE_VICTORY:      return "Decisive victory";
        case TerminationReason::NO_OFFENSIVE_CAPACITY:  return "No offensive capacity remaining";
        case TerminationReason::MUTUAL_BLINDNESS:       return "No side can detect enemies";
        case TerminationReason::UNREACHABLE:            return "Forces cannot reach each other";
        case TerminationReason::CANCELLED:              return "Cancelled by user";
        case TerminationReason::ERROR:                  return "Simulation error";
    }
    return "Unknown";
}

// =============================================================================
// Iteration Result
// =============================================================================

/// State of a single entity at a specific tick
struct EntitySnapshot {
    EntityId entity_id;
    f64 pos_x = 0.0;
    f64 pos_y = 0.0;
    f64 health = 1.0;
    f64 supply = 1.0;
    f64 morale = 1.0;
    bool is_active = true;
};

/// State of all entities at a specific tick
struct TickSnapshot {
    Tick tick = 0;
    std::vector<EntitySnapshot> entities;
    u32 blue_count = 0;
    u32 red_count = 0;
    u32 engagements_this_tick = 0;
};

/// Summary of engagements between two entities over entire simulation
struct EngagementSummary {
    EntityId attacker;
    EntityId defender;
    u32 engagement_count = 0;
    f64 total_damage_dealt = 0.0;
    f64 total_damage_received = 0.0;
    Tick first_engagement = 0;
    Tick last_engagement = 0;
};

/// Maps entity index to original actor ID from scenario
struct EntityMapping {
    EntityId entity_id;
    std::string actor_id;
    Side side;
    f64 initial_x = 0.0;
    f64 initial_y = 0.0;
};

struct IterationResult {
    u32 iteration_id;
    Seed seed;
    
    // Outcome metrics
    Tick ticks_executed;
    bool completed_normally;
    
    // Final state summary
    u32 blue_surviving;
    u32 red_surviving;
    u32 neutral_surviving;
    
    f64 blue_total_health;
    f64 red_total_health;
    
    f64 blue_avg_supply;
    f64 red_avg_supply;
    
    f64 blue_avg_morale;
    f64 red_avg_morale;
    
    // Combat statistics
    u64 total_engagements;
    f64 total_blue_damage;
    f64 total_red_damage;
    
    // Performance timing (v1.1.1)
    f64 wall_time_ms = 0.0;  // Wall clock time for this iteration
    
    // Detailed engagement data (aggregated over simulation)
    std::vector<EngagementSummary> engagement_details;
    
    // Entity to actor mapping (for UI correlation)
    std::vector<EntityMapping> entity_mappings;
    
    // Timeline data: snapshots at each tick (for playback)
    // Note: Only populated when record_timeline=true in config
    std::vector<TickSnapshot> tick_snapshots;
    
    // Initial entity count (for reference)
    u32 blue_initial = 0;
    u32 red_initial = 0;
    
    // Time to outcome (ticks)
    Tick time_to_first_casualty;
    Tick time_to_decisive;  // When one side < 50%
    
    // Custom metrics (user-defined)
    std::vector<std::pair<std::string, f64>> custom_metrics;
    
    // Termination reason (v1.1.7)
    TerminationReason termination_reason = TerminationReason::MAX_TICKS;
};

// =============================================================================
// Batch Configuration
// =============================================================================

struct BatchConfig {
    // Number of iterations
    u32 num_iterations = 100;
    
    // Master seed (iterations derive from this)
    Seed master_seed = 0;
    
    // Max ticks per iteration (0 = scenario default)
    Tick max_ticks_per_iteration = 0;
    
    // Early termination conditions
    bool stop_on_decisive = true;   // Stop when one side fully eliminated
    f64 decisive_threshold = 0.01;  // Proportion remaining (<1% = decisive)
    
    // Timeline recording (for playback UI)
    bool record_timeline = false;      // If true, capture snapshots each tick
    u32 timeline_sample_interval = 1;  // Capture every Nth tick (1 = all)
    
    // Parallel execution (future use)
    u32 thread_count = 1;  // 1 = sequential (deterministic)
    
    // Progress callback (called after each iteration)
    std::function<void(u32 completed, u32 total)> progress_callback;
    
    // Entity capacity per iteration
    usize entity_capacity = 256;

    // v1.2.1: Environment config (weather, climate) for MC iterations
    TerrainEnvironmentConfig env_config;

    // Statistical convergence (v1.1)
    // Stop early when win probability CI half-width < threshold.
    bool convergence_enabled = true;
    f64 convergence_threshold = 0.02;   // ±2% CI half-width
    u32 convergence_min_iterations = 100; // Minimum before checking
    u32 convergence_check_interval = 50;  // Check every N iterations
};

// =============================================================================
// Batch Statistics
// =============================================================================

struct BatchStatistics {
    u32 total_iterations;
    u32 completed_iterations;
    u32 failed_iterations;
    
    // Outcome distribution
    u32 blue_wins;      // Blue survivors > Red
    u32 red_wins;       // Red survivors > Blue
    u32 draws;          // Equal or both zero
    bool converged_early = false;  // True if MC stopped by convergence criterion
    
    // Aggregate metrics (mean, stddev, min, max)
    struct MetricStats {
        f64 mean;
        f64 stddev;
        f64 min;
        f64 max;
        f64 median;
        f64 p5;   // 5th percentile
        f64 p95;  // 95th percentile
    };
    
    MetricStats ticks_to_completion;
    MetricStats blue_survival_rate;
    MetricStats red_survival_rate;
    MetricStats blue_final_health;
    MetricStats red_final_health;
    
    // Correlation data (for sensitivity)
    std::vector<std::pair<std::string, f64>> parameter_correlations;

    // Performance profiling (v1.1.1)
    MetricStats iteration_time_ms;   // Wall clock ms per iteration
    f64 total_batch_time_ms = 0.0;   // Total batch wall clock time

    // Termination reason distribution (v1.1.7)
    u32 term_decisive = 0;
    u32 term_max_ticks = 0;
    u32 term_no_offensive = 0;
    u32 term_mutual_blind = 0;
    u32 term_unreachable = 0;
    u32 term_cancelled = 0;

    // Sensitivity factors (v1.1.7)
    // Each pair: factor name, variance contribution (0-1, higher = more impact)
    // Sorted descending by impact. Empty if < 30 completed iterations.
    std::vector<std::pair<std::string, f64>> sensitivity_factors;
};

// =============================================================================
// Monte Carlo Executor
// =============================================================================

class MonteCarloExecutor {
public:
    MonteCarloExecutor();
    
    // Configure batch run
    void configure(const BatchConfig& config);
    
    // Set scenario to run
    void set_scenario(const Scenario& scenario);
    
    // Set platform database for spec-driven combat
    void set_platform_database(const class PlatformDatabase* db) { platform_db_ = db; }
    
    // Set parameter overrides (for sensitivity analysis)
    void set_parameter(const std::string& name, f64 value);
    void clear_parameters();
    
    // Execute batch
    // Returns error if scenario not set or invalid config
    Status execute();
    
    // Get results
    const std::vector<IterationResult>& results() const { return results_; }
    
    // Compute statistics from results
    BatchStatistics compute_statistics() const;
    
    // Export results to JSON
    std::string export_json() const;
    
    // Get current progress (0.0 to 1.0)
    f64 progress() const;
    
    // Cancel execution (for async use)
    void cancel();
    bool is_cancelled() const { return cancelled_; }

private:
    BatchConfig config_;
    const Scenario* scenario_;
    const class PlatformDatabase* platform_db_ = nullptr;
    std::map<std::string, f64> parameter_overrides_;
    
    std::vector<IterationResult> results_;
    
    u32 current_iteration_;
    bool cancelled_;
    bool running_;
    f64 batch_total_time_ms_ = 0.0;
    
    // Run single iteration
    Result<IterationResult> run_iteration(u32 iteration_id, Seed seed);
    
    // Derive iteration seed deterministically
    Seed derive_seed(u32 iteration_id) const;
    
    // Collect metrics from final state
    void collect_metrics(IterationResult& result, 
                        const EntityStorage& storage,
                        const class systems::CombatSystem* combat);
    
    // Check decisive outcome
    bool check_decisive(const EntityStorage& storage) const;
    
    // Stalemate detection (v1.1.7)
    // Returns true if no side has offensive capability
    bool check_no_offensive(const EntityStorage& storage) const;
    // Returns true if no side can detect any enemy
    bool check_mutual_blindness(const EntityStorage& storage) const;
    // Returns true if remaining units cannot close distance to any enemy
    // within the remaining ticks at their max speed
    bool check_unreachable(const EntityStorage& storage, Tick current_tick,
                           Tick max_ticks, f64 tick_duration_s) const;
};

// =============================================================================
// Distribution Sampler
// =============================================================================

// Sample from distribution specification
class DistributionSampler {
public:
    // Sample single value from distribution
    static f64 sample(const Distribution& dist, Rng& rng);
    
    // Sample parameter with uncertainty
    static f64 sample_parameter(const UncertainParameter& param, Rng& rng);
    
    // Generate Latin Hypercube samples for multiple parameters
    static std::vector<std::map<std::string, f64>> 
    latin_hypercube(const std::map<std::string, UncertainParameter>& params,
                    u32 num_samples, Rng& rng);
};

}  // namespace analysis
}  // namespace athena

#endif  // ATHENA_ANALYSIS_MONTECARLO_HPP
