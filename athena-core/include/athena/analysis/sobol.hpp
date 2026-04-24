// ATHENA Core - Sobol Sensitivity Analysis
// Contract: Variance-based global sensitivity analysis for Monte Carlo results.
//
// THEORY:
// Sobol indices decompose output variance into contributions from input parameters:
//   S_i  = V[E(Y|X_i)] / V(Y)       -- First-order (direct effect)
//   S_Ti = E[V(Y|X_~i)] / V(Y)      -- Total-order (includes interactions)
//
// Where:
//   S_i  ∈ [0,1] measures parameter i's direct contribution to variance
//   S_Ti ∈ [0,1] measures parameter i's total contribution (direct + interactions)
//   Sum(S_i) ≤ 1, Sum(S_Ti) ≥ 1 (equality only if no interactions)
//
// METHOD:
// Saltelli's estimator (2010) - efficient sampling scheme requiring N(2k+2) evaluations
// where N = base sample size, k = number of parameters.
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_ANALYSIS_SOBOL_HPP
#define ATHENA_ANALYSIS_SOBOL_HPP

#include "athena/types.hpp"
#include "athena/rng.hpp"
#include <vector>
#include <map>
#include <string>
#include <functional>

namespace athena {

// Forward declaration
struct Scenario;

namespace analysis {

// =============================================================================
// Parameter Definition
// =============================================================================

/// Defines a parameter's sampling range for sensitivity analysis
struct SobolParameter {
    std::string name;
    f64 min_value;
    f64 max_value;
    
    // Distribution type for sampling
    enum class Distribution {
        Uniform,      // Uniform in [min, max]
        LogUniform,   // Uniform in log-space (for parameters spanning orders of magnitude)
        Normal,       // Truncated normal, min/max as 3-sigma bounds
        Triangular    // Triangular with mode at (min+max)/2
    };
    Distribution distribution = Distribution::Uniform;
    
    // Optional: custom mode for triangular (default: midpoint)
    f64 mode = 0.0;
    bool mode_set = false;
    
    SobolParameter() = default;
    SobolParameter(const std::string& n, f64 lo, f64 hi, Distribution d = Distribution::Uniform)
        : name(n), min_value(lo), max_value(hi), distribution(d) {}
};

// =============================================================================
// Sobol Indices Result
// =============================================================================

/// Sensitivity indices for a single parameter
struct ParameterSensitivity {
    std::string name;
    
    // First-order index: direct effect only
    f64 S1;
    f64 S1_confidence_low;   // 95% CI lower bound
    f64 S1_confidence_high;  // 95% CI upper bound
    
    // Total-order index: direct + all interactions
    f64 ST;
    f64 ST_confidence_low;
    f64 ST_confidence_high;
    
    // Derived metrics
    f64 interaction_effect() const { return ST - S1; }  // Contribution from interactions
    bool is_significant() const { return S1_confidence_low > 0.01; }  // Meaningful effect
};

/// Complete sensitivity analysis results
struct SobolResult {
    // Per-parameter indices
    std::vector<ParameterSensitivity> parameters;
    
    // Model statistics
    f64 output_mean;
    f64 output_variance;
    f64 output_stddev;
    
    // Convergence metrics
    u32 num_samples;
    u32 num_evaluations;
    f64 sum_S1;   // Should be ≤ 1
    f64 sum_ST;   // Should be ≥ 1
    
    // Bootstrap confidence
    u32 bootstrap_samples;
    
    // Ranking by importance
    std::vector<std::string> ranked_by_S1;   // Most to least important (first-order)
    std::vector<std::string> ranked_by_ST;   // Most to least important (total)
    
    // Check validity
    bool is_valid() const {
        return sum_S1 <= 1.1 && sum_ST >= 0.9;  // Allow 10% tolerance for estimation error
    }
    
    // Get parameter by name
    const ParameterSensitivity* get(const std::string& name) const {
        for (const auto& p : parameters) {
            if (p.name == name) return &p;
        }
        return nullptr;
    }
};

// Helper for Result<SobolResult>
template<typename T>
inline bool is_ok(const Result<T>& r) { return r.ok(); }

template<typename T>
inline bool is_error(const Result<T>& r) { return !r.ok(); }

template<typename T>
inline const T& value(const Result<T>& r) { return r.get(); }

// =============================================================================
// Sobol Sequence Generator
// =============================================================================

/// Generates quasi-random Sobol sequences for low-discrepancy sampling
class SobolSequence {
public:
    /// Initialize for given dimension
    explicit SobolSequence(u32 dimension, Seed seed = 0);
    
    /// Generate next point in [0,1]^d
    std::vector<f64> next();
    
    /// Generate N points
    std::vector<std::vector<f64>> generate(u32 n);
    
    /// Skip ahead in sequence
    void skip(u32 n);
    
    /// Reset to beginning
    void reset();
    
    u32 dimension() const { return dimension_; }
    u64 index() const { return index_; }

private:
    u32 dimension_;
    u64 index_;
    Seed seed_;
    
    // Direction numbers for Sobol sequence (up to 21201 dimensions supported)
    std::vector<std::vector<u32>> direction_numbers_;
    std::vector<u32> x_;  // Current state
    
    void init_direction_numbers();
    f64 generate_coordinate(u32 dim);
};

// =============================================================================
// Sobol Analyzer
// =============================================================================

/// Model function type: takes parameter values, returns scalar output
using ModelFunction = std::function<f64(const std::map<std::string, f64>&)>;

/// Progress callback: (completed_evaluations, total_evaluations)
using ProgressCallback = std::function<void(u32, u32)>;

/// Configuration for Sobol analysis
struct SobolConfig {
    // Base sample size N (total evaluations = N * (2k + 2) where k = num_params)
    u32 base_samples = 1024;
    
    // Bootstrap resamples for confidence intervals
    u32 bootstrap_samples = 100;
    
    // Confidence level (0.95 = 95% CI)
    f64 confidence_level = 0.95;
    
    // Master seed for reproducibility
    Seed seed = 0;
    
    // Use Sobol sequence (true) or pseudo-random (false)
    bool use_sobol_sequence = true;
    
    // Second-order indices (expensive: requires N * k * (k-1) / 2 additional evaluations)
    bool compute_second_order = false;
    
    // Progress reporting
    ProgressCallback progress_callback;
    
    // Parallel evaluation (if model is thread-safe)
    u32 num_threads = 1;
};

/// Main analyzer class
class SobolAnalyzer {
public:
    SobolAnalyzer();
    ~SobolAnalyzer();
    
    /// Add parameter to analyze
    void add_parameter(const SobolParameter& param);
    void add_parameter(const std::string& name, f64 min_val, f64 max_val,
                       SobolParameter::Distribution dist = SobolParameter::Distribution::Uniform);
    
    /// Clear all parameters
    void clear_parameters();
    
    /// Get parameter count
    u32 num_parameters() const { return static_cast<u32>(parameters_.size()); }
    
    /// Set model function
    void set_model(ModelFunction model);
    
    /// Configure analysis
    void configure(const SobolConfig& config);
    
    /// Run analysis
    /// Returns error status if model not set or no parameters defined
    Result<SobolResult> analyze();
    
    /// Cancel running analysis
    void cancel();
    bool is_cancelled() const { return cancelled_; }
    
    /// Get progress (0.0 to 1.0)
    f64 progress() const;
    
    /// Estimate number of model evaluations required
    u32 estimate_evaluations() const;

private:
    std::vector<SobolParameter> parameters_;
    ModelFunction model_;
    SobolConfig config_;
    
    bool cancelled_;
    u32 evaluations_completed_;
    u32 evaluations_total_;
    
    // Generate sample matrices A, B
    void generate_samples(std::vector<std::vector<f64>>& A,
                          std::vector<std::vector<f64>>& B);
    
    // Transform [0,1] to parameter space
    f64 transform_sample(f64 u01, const SobolParameter& param) const;
    
    // Evaluate model with parameter vector
    f64 evaluate(const std::vector<f64>& params);
    
    // Compute indices using Saltelli estimator
    void compute_indices(const std::vector<f64>& yA,
                         const std::vector<f64>& yB,
                         const std::vector<std::vector<f64>>& yAB,
                         SobolResult& result);
    
    // Bootstrap confidence intervals
    void compute_confidence(const std::vector<f64>& yA,
                            const std::vector<f64>& yB,
                            const std::vector<std::vector<f64>>& yAB,
                            SobolResult& result);
    
    // Rank parameters
    void rank_parameters(SobolResult& result);
};

// =============================================================================
// Integration with Monte Carlo (forward declarations)
// =============================================================================

/// Wrapper to use MonteCarloExecutor as Sobol model function
/// Allows sensitivity analysis of any scenario output metric
class MonteCarloSobolBridge {
public:
    /// Output metric extractor: takes batch statistics, returns scalar
    using MetricExtractor = std::function<f64(const struct BatchStatistics&)>;
    
    /// Construct bridge with scenario and metric
    MonteCarloSobolBridge(const Scenario& scenario, MetricExtractor extractor);
    
    /// Set iterations per evaluation (trade-off: accuracy vs speed)
    void set_iterations(u32 n) { iterations_ = n; }
    
    /// Add parameter mapping: scenario_param -> [min, max]
    void map_parameter(const std::string& scenario_param, f64 min_val, f64 max_val);
    
    /// Get model function for SobolAnalyzer
    ModelFunction get_model_function();
    
    /// Get parameters for SobolAnalyzer
    std::vector<SobolParameter> get_parameters() const;

private:
    const Scenario* scenario_;
    MetricExtractor extractor_;
    u32 iterations_;
    std::map<std::string, std::pair<f64, f64>> param_ranges_;
};

// =============================================================================
// Utility Functions
// =============================================================================

/// Export Sobol results to JSON string
std::string sobol_to_json(const SobolResult& result);

/// Print human-readable summary
std::string sobol_summary(const SobolResult& result);

/// Common metric extractors for MonteCarloSobolBridge
namespace metrics {
    f64 blue_win_rate(const struct BatchStatistics& stats);
    f64 red_win_rate(const struct BatchStatistics& stats);
    f64 avg_blue_survival(const struct BatchStatistics& stats);
    f64 avg_red_survival(const struct BatchStatistics& stats);
    f64 avg_duration(const struct BatchStatistics& stats);
}

}  // namespace analysis
}  // namespace athena

#endif  // ATHENA_ANALYSIS_SOBOL_HPP
