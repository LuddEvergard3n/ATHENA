// ATHENA Core - Sobol Sensitivity Analysis Implementation
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/analysis/sobol.hpp"
#include "athena/analysis/montecarlo.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace athena {
namespace analysis {

// =============================================================================
// Sobol Sequence Generator
// =============================================================================

// Direction numbers for first 40 dimensions (Joe-Kuo 2008)
// Each row: s, a, m_1, m_2, ..., m_s for polynomial x^s + a_1*x^(s-1) + ... + a_s
// Simplified: using primitive polynomials and their direction numbers
static const u32 SOBOL_MAX_DIM = 40;

// Primitive polynomial coefficients (degree, coefficients as binary)
static const u32 POLY_DEGREE[] = {
    1, 2, 3, 3, 4, 4, 5, 5, 5, 5,
    5, 5, 6, 6, 6, 6, 6, 6, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 8, 8, 8, 8
};

static const u32 POLY_COEFF[] = {
    0, 1, 1, 2, 1, 4, 2, 4, 7, 11,
    13, 14, 1, 13, 16, 19, 22, 25, 1, 4,
    7, 8, 14, 19, 21, 28, 31, 32, 37, 41,
    42, 50, 55, 56, 59, 62, 14, 21, 22, 38
};

// Initial direction numbers (first 8 bits sufficient for most applications)
static const u32 INIT_M[][8] = {
    {1, 0, 0, 0, 0, 0, 0, 0},  // dim 1
    {1, 3, 0, 0, 0, 0, 0, 0},  // dim 2
    {1, 3, 1, 0, 0, 0, 0, 0},  // dim 3
    {1, 1, 1, 0, 0, 0, 0, 0},  // dim 4
    {1, 1, 3, 3, 0, 0, 0, 0},  // dim 5
    {1, 3, 5, 13, 0, 0, 0, 0}, // dim 6
    {1, 1, 5, 5, 17, 0, 0, 0}, // dim 7
    {1, 1, 5, 5, 5, 0, 0, 0},  // dim 8
    {1, 1, 7, 11, 19, 0, 0, 0},// dim 9
    {1, 1, 5, 1, 1, 0, 0, 0},  // dim 10
    {1, 1, 1, 3, 11, 0, 0, 0}, // dim 11
    {1, 3, 5, 5, 31, 0, 0, 0}, // dim 12
    {1, 3, 3, 9, 7, 49, 0, 0}, // dim 13
    {1, 1, 1, 15, 21, 21, 0, 0},// dim 14
    {1, 3, 1, 13, 27, 49, 0, 0},// dim 15
    {1, 1, 1, 15, 7, 5, 0, 0}, // dim 16
    {1, 3, 1, 15, 13, 25, 0, 0},// dim 17
    {1, 1, 5, 5, 19, 61, 0, 0},// dim 18
    {1, 3, 7, 11, 23, 15, 103, 0},// dim 19
    {1, 3, 7, 13, 13, 15, 69, 0},// dim 20
};

SobolSequence::SobolSequence(u32 dimension, Seed seed)
    : dimension_(dimension), index_(0), seed_(seed)
{
    if (dimension == 0) {
        throw std::invalid_argument("SobolSequence dimension must be >= 1");
    }
    if (dimension > SOBOL_MAX_DIM) {
        throw std::invalid_argument("SobolSequence dimension exceeds maximum supported");
    }
    
    x_.resize(dimension, 0);
    init_direction_numbers();
}

void SobolSequence::init_direction_numbers() {
    direction_numbers_.resize(dimension_);
    
    constexpr u32 BITS = 32;
    
    for (u32 d = 0; d < dimension_; ++d) {
        direction_numbers_[d].resize(BITS);
        
        if (d == 0) {
            // First dimension: 1, 2, 4, 8, ...
            for (u32 i = 0; i < BITS; ++i) {
                direction_numbers_[d][i] = 1u << (BITS - 1 - i);
            }
        } else {
            u32 s = POLY_DEGREE[d - 1];
            u32 a = POLY_COEFF[d - 1];
            
            // Initialize first s direction numbers from table
            for (u32 i = 0; i < s && i < BITS; ++i) {
                u32 m = (d - 1 < 20 && i < 8) ? INIT_M[d - 1][i] : (2 * i + 1);
                direction_numbers_[d][i] = m << (BITS - 1 - i);
            }
            
            // Compute remaining direction numbers using recurrence
            for (u32 i = s; i < BITS; ++i) {
                u32 v = direction_numbers_[d][i - s];
                v ^= (v >> s);
                
                for (u32 j = 1; j < s; ++j) {
                    if ((a >> (s - 1 - j)) & 1) {
                        v ^= direction_numbers_[d][i - j];
                    }
                }
                direction_numbers_[d][i] = v;
            }
        }
    }
}

std::vector<f64> SobolSequence::next() {
    std::vector<f64> point(dimension_);
    
    // Find rightmost zero bit of index (Gray code)
    u64 c = 1;
    u64 value = index_;
    while (value & 1) {
        value >>= 1;
        ++c;
    }
    
    if (c > 32) c = 32;  // Clamp to 32 bits
    
    constexpr f64 SCALE = 1.0 / static_cast<f64>(1ull << 32);
    
    for (u32 d = 0; d < dimension_; ++d) {
        x_[d] ^= direction_numbers_[d][c - 1];
        point[d] = static_cast<f64>(x_[d]) * SCALE;
    }
    
    ++index_;
    return point;
}

std::vector<std::vector<f64>> SobolSequence::generate(u32 n) {
    std::vector<std::vector<f64>> points;
    points.reserve(n);
    
    for (u32 i = 0; i < n; ++i) {
        points.push_back(next());
    }
    
    return points;
}

void SobolSequence::skip(u32 n) {
    for (u32 i = 0; i < n; ++i) {
        next();
    }
}

void SobolSequence::reset() {
    index_ = 0;
    std::fill(x_.begin(), x_.end(), 0);
}

// =============================================================================
// Sobol Analyzer
// =============================================================================

SobolAnalyzer::SobolAnalyzer()
    : cancelled_(false)
    , evaluations_completed_(0)
    , evaluations_total_(0)
{
}

SobolAnalyzer::~SobolAnalyzer() = default;

void SobolAnalyzer::add_parameter(const SobolParameter& param) {
    parameters_.push_back(param);
}

void SobolAnalyzer::add_parameter(const std::string& name, f64 min_val, f64 max_val,
                                   SobolParameter::Distribution dist) {
    parameters_.emplace_back(name, min_val, max_val, dist);
}

void SobolAnalyzer::clear_parameters() {
    parameters_.clear();
}

void SobolAnalyzer::set_model(ModelFunction model) {
    model_ = std::move(model);
}

void SobolAnalyzer::configure(const SobolConfig& config) {
    config_ = config;
}

u32 SobolAnalyzer::estimate_evaluations() const {
    // Saltelli's method: N * (2k + 2) evaluations
    u32 k = static_cast<u32>(parameters_.size());
    return config_.base_samples * (2 * k + 2);
}

f64 SobolAnalyzer::progress() const {
    if (evaluations_total_ == 0) return 0.0;
    return static_cast<f64>(evaluations_completed_) / static_cast<f64>(evaluations_total_);
}

void SobolAnalyzer::cancel() {
    cancelled_ = true;
}

f64 SobolAnalyzer::transform_sample(f64 u01, const SobolParameter& param) const {
    // Clamp to [0, 1]
    u01 = std::max(0.0, std::min(1.0, u01));
    
    f64 lo = param.min_value;
    f64 hi = param.max_value;
    
    switch (param.distribution) {
        case SobolParameter::Distribution::Uniform:
            return lo + u01 * (hi - lo);
            
        case SobolParameter::Distribution::LogUniform: {
            if (lo <= 0.0) lo = 1e-10;  // Prevent log(0)
            f64 log_lo = std::log(lo);
            f64 log_hi = std::log(hi);
            return std::exp(log_lo + u01 * (log_hi - log_lo));
        }
        
        case SobolParameter::Distribution::Normal: {
            // Box-Muller approximation using inverse CDF
            // Map u01 to standard normal, then scale to [lo, hi] as ±3σ
            f64 z = 0.0;
            if (u01 > 0.0 && u01 < 1.0) {
                // Rational approximation to inverse normal CDF
                f64 t = std::sqrt(-2.0 * std::log(std::min(u01, 1.0 - u01)));
                f64 c0 = 2.515517, c1 = 0.802853, c2 = 0.010328;
                f64 d1 = 1.432788, d2 = 0.189269, d3 = 0.001308;
                z = t - (c0 + c1*t + c2*t*t) / (1.0 + d1*t + d2*t*t + d3*t*t*t);
                if (u01 > 0.5) z = -z;
            }
            // Scale: z ∈ [-3, 3] maps to [lo, hi]
            f64 mid = (lo + hi) / 2.0;
            f64 sigma = (hi - lo) / 6.0;
            return std::max(lo, std::min(hi, mid + z * sigma));
        }
        
        case SobolParameter::Distribution::Triangular: {
            f64 mode = param.mode_set ? param.mode : (lo + hi) / 2.0;
            f64 fc = (mode - lo) / (hi - lo);
            if (u01 < fc) {
                return lo + std::sqrt(u01 * (hi - lo) * (mode - lo));
            } else {
                return hi - std::sqrt((1.0 - u01) * (hi - lo) * (hi - mode));
            }
        }
        
        default:
            return lo + u01 * (hi - lo);
    }
}

void SobolAnalyzer::generate_samples(std::vector<std::vector<f64>>& A,
                                      std::vector<std::vector<f64>>& B) {
    u32 N = config_.base_samples;
    u32 k = static_cast<u32>(parameters_.size());
    
    A.resize(N);
    B.resize(N);
    
    if (config_.use_sobol_sequence) {
        // Generate 2k-dimensional Sobol sequence, split into A and B
        SobolSequence seq(2 * k, config_.seed);
        
        // Skip first point (all zeros)
        seq.next();
        
        for (u32 i = 0; i < N; ++i) {
            auto point = seq.next();
            A[i].resize(k);
            B[i].resize(k);
            
            for (u32 j = 0; j < k; ++j) {
                A[i][j] = point[j];
                B[i][j] = point[k + j];
            }
        }
    } else {
        // Pseudo-random sampling
        Rng rng(config_.seed);
        
        for (u32 i = 0; i < N; ++i) {
            A[i].resize(k);
            B[i].resize(k);
            
            for (u32 j = 0; j < k; ++j) {
                A[i][j] = rng.next_f64();
                B[i][j] = rng.next_f64();
            }
        }
    }
}

f64 SobolAnalyzer::evaluate(const std::vector<f64>& params) {
    std::map<std::string, f64> param_map;
    for (usize i = 0; i < parameters_.size(); ++i) {
        f64 value = transform_sample(params[i], parameters_[i]);
        param_map[parameters_[i].name] = value;
    }
    
    f64 result = model_(param_map);
    ++evaluations_completed_;
    
    if (config_.progress_callback && evaluations_total_ > 0) {
        config_.progress_callback(evaluations_completed_, evaluations_total_);
    }
    
    return result;
}

void SobolAnalyzer::compute_indices(const std::vector<f64>& yA,
                                     const std::vector<f64>& yB,
                                     const std::vector<std::vector<f64>>& yAB,
                                     SobolResult& result) {
    u32 N = static_cast<u32>(yA.size());
    u32 k = static_cast<u32>(parameters_.size());
    
    // Compute output statistics
    f64 sum_all = 0.0;
    f64 sum_sq_all = 0.0;
    u32 n_all = 2 * N;
    
    for (u32 i = 0; i < N; ++i) {
        sum_all += yA[i] + yB[i];
        sum_sq_all += yA[i] * yA[i] + yB[i] * yB[i];
    }
    
    result.output_mean = sum_all / n_all;
    result.output_variance = sum_sq_all / n_all - result.output_mean * result.output_mean;
    result.output_stddev = std::sqrt(std::max(0.0, result.output_variance));
    
    // Avoid division by zero
    f64 var_inv = (result.output_variance > 1e-15) ? 1.0 / result.output_variance : 0.0;
    
    // Compute indices for each parameter using Saltelli estimator
    result.parameters.resize(k);
    result.sum_S1 = 0.0;
    result.sum_ST = 0.0;
    
    for (u32 j = 0; j < k; ++j) {
        ParameterSensitivity& ps = result.parameters[j];
        ps.name = parameters_[j].name;
        
        // First-order index: S1_j = (1/N) * sum(yB * (yAB_j - yA)) / V(Y)
        f64 sum_s1 = 0.0;
        for (u32 i = 0; i < N; ++i) {
            sum_s1 += yB[i] * (yAB[j][i] - yA[i]);
        }
        ps.S1 = (sum_s1 / N) * var_inv;
        
        // Total-order index: ST_j = (1/(2N)) * sum((yA - yAB_j)^2) / V(Y)
        f64 sum_st = 0.0;
        for (u32 i = 0; i < N; ++i) {
            f64 diff = yA[i] - yAB[j][i];
            sum_st += diff * diff;
        }
        ps.ST = (sum_st / (2.0 * N)) * var_inv;
        
        // Clamp to valid range [0, 1]
        ps.S1 = std::max(0.0, std::min(1.0, ps.S1));
        ps.ST = std::max(0.0, std::min(1.0, ps.ST));
        
        result.sum_S1 += ps.S1;
        result.sum_ST += ps.ST;
    }
}

void SobolAnalyzer::compute_confidence(const std::vector<f64>& yA,
                                        const std::vector<f64>& yB,
                                        const std::vector<std::vector<f64>>& yAB,
                                        SobolResult& result) {
    if (config_.bootstrap_samples == 0) {
        // No bootstrap - set wide confidence intervals
        for (auto& ps : result.parameters) {
            ps.S1_confidence_low = 0.0;
            ps.S1_confidence_high = 1.0;
            ps.ST_confidence_low = 0.0;
            ps.ST_confidence_high = 1.0;
        }
        return;
    }
    
    u32 N = static_cast<u32>(yA.size());
    u32 k = static_cast<u32>(parameters_.size());
    u32 B = config_.bootstrap_samples;
    
    // Storage for bootstrap estimates
    std::vector<std::vector<f64>> s1_boot(k), st_boot(k);
    for (u32 j = 0; j < k; ++j) {
        s1_boot[j].reserve(B);
        st_boot[j].reserve(B);
    }
    
    Rng rng(config_.seed + 12345);
    
    for (u32 b = 0; b < B; ++b) {
        // Resample indices with replacement
        std::vector<u32> indices(N);
        for (u32 i = 0; i < N; ++i) {
            indices[i] = rng.next_u32() % N;
        }
        
        // Compute variance on bootstrap sample
        f64 sum = 0.0, sum_sq = 0.0;
        for (u32 i = 0; i < N; ++i) {
            u32 idx = indices[i];
            sum += yA[idx] + yB[idx];
            sum_sq += yA[idx]*yA[idx] + yB[idx]*yB[idx];
        }
        f64 mean = sum / (2.0 * N);
        f64 var = sum_sq / (2.0 * N) - mean * mean;
        f64 var_inv = (var > 1e-15) ? 1.0 / var : 0.0;
        
        // Compute indices on bootstrap sample
        for (u32 j = 0; j < k; ++j) {
            f64 sum_s1 = 0.0, sum_st = 0.0;
            for (u32 i = 0; i < N; ++i) {
                u32 idx = indices[i];
                sum_s1 += yB[idx] * (yAB[j][idx] - yA[idx]);
                f64 diff = yA[idx] - yAB[j][idx];
                sum_st += diff * diff;
            }
            
            f64 s1 = std::max(0.0, std::min(1.0, (sum_s1 / N) * var_inv));
            f64 st = std::max(0.0, std::min(1.0, (sum_st / (2.0 * N)) * var_inv));
            
            s1_boot[j].push_back(s1);
            st_boot[j].push_back(st);
        }
    }
    
    // Compute percentiles for confidence intervals
    f64 alpha = 1.0 - config_.confidence_level;
    u32 lo_idx = static_cast<u32>(alpha / 2.0 * B);
    u32 hi_idx = static_cast<u32>((1.0 - alpha / 2.0) * B);
    
    for (u32 j = 0; j < k; ++j) {
        std::sort(s1_boot[j].begin(), s1_boot[j].end());
        std::sort(st_boot[j].begin(), st_boot[j].end());
        
        result.parameters[j].S1_confidence_low = s1_boot[j][lo_idx];
        result.parameters[j].S1_confidence_high = s1_boot[j][hi_idx];
        result.parameters[j].ST_confidence_low = st_boot[j][lo_idx];
        result.parameters[j].ST_confidence_high = st_boot[j][hi_idx];
    }
    
    result.bootstrap_samples = B;
}

void SobolAnalyzer::rank_parameters(SobolResult& result) {
    u32 k = static_cast<u32>(result.parameters.size());
    
    // Create index arrays
    std::vector<u32> idx_s1(k), idx_st(k);
    for (u32 i = 0; i < k; ++i) {
        idx_s1[i] = i;
        idx_st[i] = i;
    }
    
    // Sort by S1 descending
    std::sort(idx_s1.begin(), idx_s1.end(), [&](u32 a, u32 b) {
        return result.parameters[a].S1 > result.parameters[b].S1;
    });
    
    // Sort by ST descending
    std::sort(idx_st.begin(), idx_st.end(), [&](u32 a, u32 b) {
        return result.parameters[a].ST > result.parameters[b].ST;
    });
    
    // Store ranked names
    result.ranked_by_S1.clear();
    result.ranked_by_ST.clear();
    
    for (u32 i = 0; i < k; ++i) {
        result.ranked_by_S1.push_back(result.parameters[idx_s1[i]].name);
        result.ranked_by_ST.push_back(result.parameters[idx_st[i]].name);
    }
}

Result<SobolResult> SobolAnalyzer::analyze() {
    // Validate inputs
    if (!model_) {
        return Result<SobolResult>(ErrorCode::INTERNAL_ERROR, "Model function not set");
    }
    
    if (parameters_.empty()) {
        return Result<SobolResult>(ErrorCode::INVALID_ARGUMENT, "No parameters defined");
    }
    
    if (config_.base_samples < 16) {
        return Result<SobolResult>(ErrorCode::INVALID_ARGUMENT, "Base samples too small (minimum 16)");
    }
    
    cancelled_ = false;
    evaluations_completed_ = 0;
    evaluations_total_ = estimate_evaluations();
    
    u32 N = config_.base_samples;
    u32 k = static_cast<u32>(parameters_.size());
    
    // Generate sample matrices A and B
    std::vector<std::vector<f64>> A, B;
    generate_samples(A, B);
    
    // Evaluate model on A
    std::vector<f64> yA(N);
    for (u32 i = 0; i < N && !cancelled_; ++i) {
        yA[i] = evaluate(A[i]);
    }
    
    if (cancelled_) {
        return Result<SobolResult>(ErrorCode::SIM_NOT_RUNNING, "Analysis cancelled");
    }
    
    // Evaluate model on B
    std::vector<f64> yB(N);
    for (u32 i = 0; i < N && !cancelled_; ++i) {
        yB[i] = evaluate(B[i]);
    }
    
    if (cancelled_) {
        return Result<SobolResult>(ErrorCode::SIM_NOT_RUNNING, "Analysis cancelled");
    }
    
    // For each parameter j, create AB_j (A with column j from B) and evaluate
    std::vector<std::vector<f64>> yAB(k);
    
    for (u32 j = 0; j < k && !cancelled_; ++j) {
        yAB[j].resize(N);
        
        for (u32 i = 0; i < N && !cancelled_; ++i) {
            // Create AB_j: copy A[i], replace element j with B[i][j]
            std::vector<f64> ab_sample = A[i];
            ab_sample[j] = B[i][j];
            yAB[j][i] = evaluate(ab_sample);
        }
    }
    
    if (cancelled_) {
        return Result<SobolResult>(ErrorCode::SIM_NOT_RUNNING, "Analysis cancelled");
    }
    
    // Compute indices
    SobolResult result;
    result.num_samples = N;
    result.num_evaluations = evaluations_completed_;
    
    compute_indices(yA, yB, yAB, result);
    compute_confidence(yA, yB, yAB, result);
    rank_parameters(result);
    
    return result;
}

// =============================================================================
// Monte Carlo Bridge (stub - needs full integration)
// =============================================================================

MonteCarloSobolBridge::MonteCarloSobolBridge(const Scenario& scenario, MetricExtractor extractor)
    : scenario_(&scenario)
    , extractor_(std::move(extractor))
    , iterations_(32)
{
}

void MonteCarloSobolBridge::map_parameter(const std::string& scenario_param, f64 min_val, f64 max_val) {
    param_ranges_[scenario_param] = {min_val, max_val};
}

std::vector<SobolParameter> MonteCarloSobolBridge::get_parameters() const {
    std::vector<SobolParameter> params;
    for (const auto& [name, range] : param_ranges_) {
        params.emplace_back(name, range.first, range.second);
    }
    return params;
}

ModelFunction MonteCarloSobolBridge::get_model_function() {
    // Stub: full integration requires Scenario/MonteCarloExecutor refactoring
    // For now, return a placeholder that throws if called
    return [](const std::map<std::string, f64>&) -> f64 {
        throw std::runtime_error("MonteCarloSobolBridge not fully implemented - use SobolAnalyzer directly");
    };
}

// =============================================================================
// Utility Functions
// =============================================================================

std::string sobol_to_json(const SobolResult& result) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    
    ss << "{\n";
    ss << "  \"num_samples\": " << result.num_samples << ",\n";
    ss << "  \"num_evaluations\": " << result.num_evaluations << ",\n";
    ss << "  \"bootstrap_samples\": " << result.bootstrap_samples << ",\n";
    ss << "  \"output_mean\": " << result.output_mean << ",\n";
    ss << "  \"output_variance\": " << result.output_variance << ",\n";
    ss << "  \"output_stddev\": " << result.output_stddev << ",\n";
    ss << "  \"sum_S1\": " << result.sum_S1 << ",\n";
    ss << "  \"sum_ST\": " << result.sum_ST << ",\n";
    ss << "  \"is_valid\": " << (result.is_valid() ? "true" : "false") << ",\n";
    
    ss << "  \"parameters\": [\n";
    for (usize i = 0; i < result.parameters.size(); ++i) {
        const auto& p = result.parameters[i];
        ss << "    {\n";
        ss << "      \"name\": \"" << p.name << "\",\n";
        ss << "      \"S1\": " << p.S1 << ",\n";
        ss << "      \"S1_confidence\": [" << p.S1_confidence_low << ", " << p.S1_confidence_high << "],\n";
        ss << "      \"ST\": " << p.ST << ",\n";
        ss << "      \"ST_confidence\": [" << p.ST_confidence_low << ", " << p.ST_confidence_high << "],\n";
        ss << "      \"interaction_effect\": " << p.interaction_effect() << ",\n";
        ss << "      \"is_significant\": " << (p.is_significant() ? "true" : "false") << "\n";
        ss << "    }";
        if (i < result.parameters.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    
    ss << "  \"ranked_by_S1\": [";
    for (usize i = 0; i < result.ranked_by_S1.size(); ++i) {
        ss << "\"" << result.ranked_by_S1[i] << "\"";
        if (i < result.ranked_by_S1.size() - 1) ss << ", ";
    }
    ss << "],\n";
    
    ss << "  \"ranked_by_ST\": [";
    for (usize i = 0; i < result.ranked_by_ST.size(); ++i) {
        ss << "\"" << result.ranked_by_ST[i] << "\"";
        if (i < result.ranked_by_ST.size() - 1) ss << ", ";
    }
    ss << "]\n";
    
    ss << "}\n";
    
    return ss.str();
}

std::string sobol_summary(const SobolResult& result) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4);
    
    ss << "╔══════════════════════════════════════════════════════════════════╗\n";
    ss << "║              SOBOL SENSITIVITY ANALYSIS RESULTS                   ║\n";
    ss << "╠══════════════════════════════════════════════════════════════════╣\n";
    ss << "║ Samples: " << std::setw(6) << result.num_samples 
       << "    Evaluations: " << std::setw(8) << result.num_evaluations 
       << "    Bootstrap: " << std::setw(4) << result.bootstrap_samples << " ║\n";
    ss << "║ Output Mean: " << std::setw(10) << result.output_mean
       << "    Std Dev: " << std::setw(10) << result.output_stddev << "           ║\n";
    ss << "║ Sum(S1): " << std::setw(6) << result.sum_S1 
       << "  Sum(ST): " << std::setw(6) << result.sum_ST 
       << "  Valid: " << (result.is_valid() ? "YES" : "NO ") << "                  ║\n";
    ss << "╠══════════════════════════════════════════════════════════════════╣\n";
    ss << "║ Parameter               │    S1 [95% CI]      │    ST [95% CI]    ║\n";
    ss << "╠═════════════════════════╪═════════════════════╪═══════════════════╣\n";
    
    for (const auto& p : result.parameters) {
        ss << "║ " << std::left << std::setw(23) << p.name.substr(0, 23) << " │ "
           << std::right << std::setw(5) << p.S1 
           << " [" << std::setw(4) << p.S1_confidence_low << "-" << std::setw(4) << p.S1_confidence_high << "] │ "
           << std::setw(5) << p.ST
           << " [" << std::setw(4) << p.ST_confidence_low << "-" << std::setw(4) << p.ST_confidence_high << "] ║\n";
    }
    
    ss << "╠══════════════════════════════════════════════════════════════════╣\n";
    ss << "║ RANKING BY IMPORTANCE (Total Effect):                            ║\n";
    
    for (usize i = 0; i < std::min(result.ranked_by_ST.size(), usize(5)); ++i) {
        const auto* p = result.get(result.ranked_by_ST[i]);
        if (p) {
            ss << "║  " << (i + 1) << ". " << std::left << std::setw(20) << p->name.substr(0, 20)
               << "  ST=" << std::right << std::setw(6) << p->ST
               << "  (interaction: " << std::setw(6) << p->interaction_effect() << ")      ║\n";
        }
    }
    
    ss << "╚══════════════════════════════════════════════════════════════════╝\n";
    
    return ss.str();
}

namespace metrics {

f64 blue_win_rate(const BatchStatistics& stats) {
    if (stats.total_iterations == 0) return 0.0;
    return static_cast<f64>(stats.blue_wins) / stats.total_iterations;
}

f64 red_win_rate(const BatchStatistics& stats) {
    if (stats.total_iterations == 0) return 0.0;
    return static_cast<f64>(stats.red_wins) / stats.total_iterations;
}

f64 avg_blue_survival(const BatchStatistics& stats) {
    return stats.blue_survival_rate.mean;
}

f64 avg_red_survival(const BatchStatistics& stats) {
    return stats.red_survival_rate.mean;
}

f64 avg_duration(const BatchStatistics& stats) {
    return stats.ticks_to_completion.mean;
}

}  // namespace metrics

}  // namespace analysis
}  // namespace athena
