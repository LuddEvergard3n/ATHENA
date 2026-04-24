// ATHENA Core - PDF Report Generator
// Generates binary PDF files from Monte Carlo analysis results.
// Pure C++ implementation — zero external dependencies.
//
// PDF structure: minimal valid PDF 1.4 with text content.
// Produces a single-page (or multi-page) report with:
//   - Scenario name and timestamp
//   - Win probabilities with CI95
//   - Survival rates
//   - Performance profiling
//   - Variant comparison table (if multiple variants)
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_PDF_REPORT_HPP
#define ATHENA_PDF_REPORT_HPP

#include "athena/types.hpp"
#include "athena/analysis/montecarlo.hpp"
#include <string>
#include <vector>

namespace athena {
namespace report {

// Variant row for comparison table
struct VariantRow {
    std::string name;
    u32 iterations;
    f64 blue_win_pct;
    f64 red_win_pct;
    f64 draw_pct;
    f64 blue_survival_mean;
    f64 red_survival_mean;
    f64 avg_duration_hours;
};

// Report configuration
struct ReportConfig {
    std::string scenario_name;
    std::string timestamp;       // ISO 8601
    std::string version;         // ATHENA version string

    // Latest MC result (required)
    analysis::BatchStatistics stats;
    f64 tick_duration_seconds = 3600.0;

    // Optional variant comparison
    std::vector<VariantRow> variants;
};

// Generate a PDF report file.
// Returns true on success.
bool generate_pdf_report(const std::string& output_path, const ReportConfig& config);

}  // namespace report
}  // namespace athena

#endif  // ATHENA_PDF_REPORT_HPP
