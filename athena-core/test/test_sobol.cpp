// ATHENA Core - Sobol Sensitivity Analysis Tests
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/analysis/sobol.hpp"
#include <cmath>
#include <iostream>
#include <cassert>

using namespace athena;
using namespace athena::analysis;

// =============================================================================
// Test Utilities
// =============================================================================

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "  Running " #name "... "; \
    test_##name(); \
    std::cout << "PASS\n"; \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        std::cerr << "\nAssertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
        std::exit(1); \
    } \
} while(0)

#define ASSERT_NEAR(a, b, tol) do { \
    f64 _a = (a), _b = (b), _t = (tol); \
    if (std::abs(_a - _b) > _t) { \
        std::cerr << "\nAssertion failed: |" << _a << " - " << _b << "| > " << _t \
                  << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
        std::exit(1); \
    } \
} while(0)

// =============================================================================
// Sobol Sequence Tests
// =============================================================================

TEST(sobol_sequence_basic) {
    SobolSequence seq(2, 0);
    
    // First point should not be all zeros (we skip it)
    auto p1 = seq.next();
    ASSERT_TRUE(p1.size() == 2);
    
    // Generate more points
    auto p2 = seq.next();
    auto p3 = seq.next();
    
    // Points should be in [0, 1]
    ASSERT_TRUE(p1[0] >= 0.0 && p1[0] <= 1.0);
    ASSERT_TRUE(p1[1] >= 0.0 && p1[1] <= 1.0);
    ASSERT_TRUE(p2[0] >= 0.0 && p2[0] <= 1.0);
    ASSERT_TRUE(p2[1] >= 0.0 && p2[1] <= 1.0);
    
    // Points should be different
    ASSERT_TRUE(p1[0] != p2[0] || p1[1] != p2[1]);
}

TEST(sobol_sequence_high_dimension) {
    SobolSequence seq(20, 12345);
    
    auto points = seq.generate(100);
    ASSERT_TRUE(points.size() == 100);
    
    // Check all points have correct dimension
    for (const auto& p : points) {
        ASSERT_TRUE(p.size() == 20);
        for (f64 x : p) {
            ASSERT_TRUE(x >= 0.0 && x <= 1.0);
        }
    }
}

TEST(sobol_sequence_reset) {
    SobolSequence seq(3, 42);
    
    auto p1 = seq.next();
    auto p2 = seq.next();
    
    seq.reset();
    
    auto p1_again = seq.next();
    auto p2_again = seq.next();
    
    // Should get same sequence after reset
    ASSERT_NEAR(p1[0], p1_again[0], 1e-10);
    ASSERT_NEAR(p1[1], p1_again[1], 1e-10);
    ASSERT_NEAR(p2[0], p2_again[0], 1e-10);
}

// =============================================================================
// Sobol Analyzer Tests
// =============================================================================

// Ishigami function: classic test function for sensitivity analysis
// f(x1, x2, x3) = sin(x1) + a*sin(x2)^2 + b*x3^4*sin(x1)
// Analytical Sobol indices known for a=7, b=0.1:
//   S1 = [0.3139, 0.4424, 0.0], ST = [0.5576, 0.4424, 0.2437]
f64 ishigami(const std::map<std::string, f64>& params) {
    constexpr f64 a = 7.0;
    constexpr f64 b = 0.1;
    
    f64 x1 = params.at("x1");
    f64 x2 = params.at("x2");
    f64 x3 = params.at("x3");
    
    return std::sin(x1) + a * std::sin(x2) * std::sin(x2) + b * x3 * x3 * x3 * x3 * std::sin(x1);
}

TEST(sobol_analyzer_ishigami) {
    SobolAnalyzer analyzer;
    
    // Ishigami parameters: x1, x2, x3 ∈ [-π, π]
    constexpr f64 PI = 3.14159265358979323846;
    analyzer.add_parameter("x1", -PI, PI);
    analyzer.add_parameter("x2", -PI, PI);
    analyzer.add_parameter("x3", -PI, PI);
    
    analyzer.set_model(ishigami);
    
    SobolConfig config;
    config.base_samples = 1024;  // Reasonable for convergence
    config.bootstrap_samples = 50;
    config.seed = 42;
    analyzer.configure(config);
    
    auto result = analyzer.analyze();
    ASSERT_TRUE(result.ok());
    
    SobolResult sr = result.get();
    
    // Check result validity
    ASSERT_TRUE(sr.is_valid());
    ASSERT_TRUE(sr.parameters.size() == 3);
    
    // Analytical values (with tolerance for estimation error)
    // S1: x1=0.314, x2=0.442, x3=0
    // ST: x1=0.558, x2=0.442, x3=0.244
    
    const auto* x1 = sr.get("x1");
    const auto* x2 = sr.get("x2");
    const auto* x3 = sr.get("x3");
    
    ASSERT_TRUE(x1 != nullptr);
    ASSERT_TRUE(x2 != nullptr);
    ASSERT_TRUE(x3 != nullptr);
    
    // Check S1 indices (allow ~0.1 tolerance for N=1024)
    ASSERT_NEAR(x1->S1, 0.314, 0.15);
    ASSERT_NEAR(x2->S1, 0.442, 0.15);
    ASSERT_NEAR(x3->S1, 0.0, 0.10);
    
    // Check ST indices
    ASSERT_NEAR(x1->ST, 0.558, 0.15);
    ASSERT_NEAR(x2->ST, 0.442, 0.15);
    ASSERT_NEAR(x3->ST, 0.244, 0.15);
    
    // x3 should have significant interaction effect (ST > S1)
    ASSERT_TRUE(x3->interaction_effect() > 0.1);
    
    // x2 should have no interaction (ST ≈ S1)
    ASSERT_NEAR(x2->interaction_effect(), 0.0, 0.05);
    
    std::cout << "\n" << sobol_summary(sr);
}

// Linear model: y = a*x1 + b*x2 + c*x3
// No interactions, S1 = ST
f64 linear_model(const std::map<std::string, f64>& params) {
    return 2.0 * params.at("x1") + 1.0 * params.at("x2") + 0.5 * params.at("x3");
}

TEST(sobol_analyzer_linear) {
    SobolAnalyzer analyzer;
    
    analyzer.add_parameter("x1", 0.0, 1.0);
    analyzer.add_parameter("x2", 0.0, 1.0);
    analyzer.add_parameter("x3", 0.0, 1.0);
    
    analyzer.set_model(linear_model);
    
    SobolConfig config;
    config.base_samples = 512;
    config.bootstrap_samples = 0;  // Skip bootstrap for speed
    config.seed = 123;
    analyzer.configure(config);
    
    auto result = analyzer.analyze();
    ASSERT_TRUE(result.ok());
    
    SobolResult sr = result.get();
    
    // For linear model: variance contributions proportional to coefficient squared
    // coeffs: 2, 1, 0.5 -> squares: 4, 1, 0.25 -> sum: 5.25
    // Expected S1: 4/5.25=0.762, 1/5.25=0.190, 0.25/5.25=0.048
    
    const auto* x1 = sr.get("x1");
    const auto* x2 = sr.get("x2");
    const auto* x3 = sr.get("x3");
    
    ASSERT_NEAR(x1->S1, 0.762, 0.10);
    ASSERT_NEAR(x2->S1, 0.190, 0.10);
    ASSERT_NEAR(x3->S1, 0.048, 0.05);
    
    // Linear model: S1 ≈ ST (no interactions)
    ASSERT_NEAR(x1->S1, x1->ST, 0.05);
    ASSERT_NEAR(x2->S1, x2->ST, 0.05);
    ASSERT_NEAR(x3->S1, x3->ST, 0.05);
    
    // Sum of S1 should be close to 1
    ASSERT_NEAR(sr.sum_S1, 1.0, 0.10);
    
    // Ranking should be x1 > x2 > x3
    ASSERT_TRUE(sr.ranked_by_S1[0] == "x1");
    ASSERT_TRUE(sr.ranked_by_S1[1] == "x2");
    ASSERT_TRUE(sr.ranked_by_S1[2] == "x3");
}

// Single parameter model
TEST(sobol_analyzer_single_param) {
    SobolAnalyzer analyzer;
    
    analyzer.add_parameter("x", 0.0, 10.0);
    analyzer.set_model([](const std::map<std::string, f64>& p) {
        return p.at("x") * p.at("x");
    });
    
    SobolConfig config;
    config.base_samples = 256;
    config.bootstrap_samples = 0;
    analyzer.configure(config);
    
    auto result = analyzer.analyze();
    ASSERT_TRUE(result.ok());
    
    SobolResult sr = result.get();
    
    // Single parameter: S1 = ST = 1.0
    ASSERT_NEAR(sr.parameters[0].S1, 1.0, 0.05);
    ASSERT_NEAR(sr.parameters[0].ST, 1.0, 0.05);
}

// Test parameter distributions
TEST(sobol_parameter_distributions) {
    SobolAnalyzer analyzer;
    
    // Test different distributions
    analyzer.add_parameter("uniform", 0.0, 1.0, SobolParameter::Distribution::Uniform);
    analyzer.add_parameter("loguniform", 0.01, 100.0, SobolParameter::Distribution::LogUniform);
    analyzer.add_parameter("normal", -3.0, 3.0, SobolParameter::Distribution::Normal);
    analyzer.add_parameter("triangular", 0.0, 1.0, SobolParameter::Distribution::Triangular);
    
    // Model that just returns sum
    analyzer.set_model([](const std::map<std::string, f64>& p) {
        return p.at("uniform") + p.at("loguniform") + p.at("normal") + p.at("triangular");
    });
    
    SobolConfig config;
    config.base_samples = 128;
    config.bootstrap_samples = 0;
    analyzer.configure(config);
    
    auto result = analyzer.analyze();
    ASSERT_TRUE(result.ok());
}

// Test error handling
TEST(sobol_error_handling) {
    SobolAnalyzer analyzer;
    
    // No model set
    auto r1 = analyzer.analyze();
    ASSERT_TRUE(!r1.ok());
    
    // No parameters
    analyzer.set_model([](const std::map<std::string, f64>&) { return 1.0; });
    auto r2 = analyzer.analyze();
    ASSERT_TRUE(!r2.ok());
    
    // Valid now
    analyzer.add_parameter("x", 0.0, 1.0);
    SobolConfig config;
    config.base_samples = 64;
    config.bootstrap_samples = 0;
    analyzer.configure(config);
    
    auto r3 = analyzer.analyze();
    ASSERT_TRUE(r3.ok());
}

// Test JSON export
TEST(sobol_json_export) {
    SobolAnalyzer analyzer;
    
    analyzer.add_parameter("alpha", 0.0, 1.0);
    analyzer.add_parameter("beta", 0.0, 1.0);
    analyzer.set_model([](const std::map<std::string, f64>& p) {
        return p.at("alpha") + 2.0 * p.at("beta");
    });
    
    SobolConfig config;
    config.base_samples = 64;
    config.bootstrap_samples = 10;
    analyzer.configure(config);
    
    auto result = analyzer.analyze();
    ASSERT_TRUE(result.ok());
    
    std::string json = sobol_to_json(result.get());
    
    // Check JSON contains expected fields
    ASSERT_TRUE(json.find("\"num_samples\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"parameters\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"alpha\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"beta\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"S1\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"ST\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"ranked_by_S1\"") != std::string::npos);
}

// Test estimation count
TEST(sobol_estimation_count) {
    SobolAnalyzer analyzer;
    
    // Add 5 parameters
    for (int i = 0; i < 5; ++i) {
        analyzer.add_parameter("x" + std::to_string(i), 0.0, 1.0);
    }
    
    SobolConfig config;
    config.base_samples = 100;
    analyzer.configure(config);
    
    // Expected: N * (2k + 2) = 100 * (2*5 + 2) = 1200
    ASSERT_TRUE(analyzer.estimate_evaluations() == 1200);
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "ATHENA Sobol Sensitivity Analysis Tests\n";
    std::cout << "========================================\n\n";
    
    std::cout << "Sobol Sequence:\n";
    RUN_TEST(sobol_sequence_basic);
    RUN_TEST(sobol_sequence_high_dimension);
    RUN_TEST(sobol_sequence_reset);
    
    std::cout << "\nSobol Analyzer:\n";
    RUN_TEST(sobol_analyzer_single_param);
    RUN_TEST(sobol_analyzer_linear);
    RUN_TEST(sobol_analyzer_ishigami);  // Main validation test
    RUN_TEST(sobol_parameter_distributions);
    RUN_TEST(sobol_error_handling);
    RUN_TEST(sobol_json_export);
    RUN_TEST(sobol_estimation_count);
    
    std::cout << "\n========================================\n";
    std::cout << "All Sobol tests passed!\n";
    
    return 0;
}
