// ATHENA Core - Analysis Framework
// Contract: Deterministic analysis tools for simulation results.
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_ANALYSIS_HPP
#define ATHENA_ANALYSIS_HPP

#include "athena/analysis/montecarlo.hpp"
#include "athena/analysis/sobol.hpp"

namespace athena {
namespace analysis {

// =============================================================================
// Sensitivity Analysis (legacy interface - use SobolAnalyzer for full features)
// =============================================================================

struct SobolIndices {
    std::string parameter_name;
    f64 first_order;     // Direct effect (S1)
    f64 total_order;     // Including interactions (ST)
    f64 confidence;      // Bootstrap confidence width
};

// Legacy function - wraps SobolAnalyzer for simple use cases
std::vector<SobolIndices> compute_sobol_indices(
    const std::vector<IterationResult>& results,
    const std::vector<std::map<std::string, f64>>& parameter_samples,
    const std::string& output_metric);

// =============================================================================
// Outcome Analysis
// =============================================================================

struct OutcomeDistribution {
    // Discrete outcomes
    f64 blue_win_probability;
    f64 red_win_probability;
    f64 draw_probability;
    
    // Continuous metrics
    f64 expected_blue_casualties;
    f64 expected_red_casualties;
    f64 expected_duration;
    
    // Confidence intervals (95%)
    f64 blue_win_ci_low;
    f64 blue_win_ci_high;
};

OutcomeDistribution compute_outcome_distribution(
    const std::vector<IterationResult>& results);

// =============================================================================
// Convergence Analysis
// =============================================================================

struct ConvergenceMetrics {
    u32 iterations_checked;
    f64 mean_estimate;
    f64 variance_estimate;
    f64 standard_error;
    f64 coefficient_of_variation;
    bool converged;
    f64 required_iterations_95;  // Estimated for 5% precision
};

ConvergenceMetrics check_convergence(
    const std::vector<IterationResult>& results,
    const std::string& metric_name,
    f64 target_precision = 0.05);

}  // namespace analysis
}  // namespace athena

#endif  // ATHENA_ANALYSIS_HPP
