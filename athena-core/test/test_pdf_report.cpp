// ATHENA Core - PDF Report Generator Tests
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/pdf_report.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <cstdio>

using namespace athena;
using namespace athena::report;

// =============================================================================
// Test Helpers
// =============================================================================

static bool file_exists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

static size_t file_size(const std::string& path) {
    std::ifstream f(path, std::ios::ate | std::ios::binary);
    return f.tellg();
}

static bool file_starts_with(const std::string& path, const std::string& prefix) {
    std::ifstream f(path);
    std::string line;
    std::getline(f, line);
    return line.substr(0, prefix.size()) == prefix;
}

// =============================================================================
// PDF Writer Tests
// =============================================================================

void test_pdf_writer_basic() {
    std::cout << "  test_pdf_writer_basic... ";
    
    PDFWriter pdf;
    bool opened = pdf.open("/tmp/athena_test_basic.pdf");
    assert(opened);
    
    pdf.new_page();
    pdf.set_font("Helvetica", 24);
    pdf.draw_text(100, 700, "Test PDF Document");
    
    pdf.set_font("Helvetica", 12);
    pdf.draw_text(100, 650, "This is a test line.");
    
    pdf.close();
    
    assert(file_exists("/tmp/athena_test_basic.pdf"));
    assert(file_size("/tmp/athena_test_basic.pdf") > 100);
    assert(file_starts_with("/tmp/athena_test_basic.pdf", "%PDF-1.4"));
    
    std::remove("/tmp/athena_test_basic.pdf");
    
    std::cout << "OK\n";
}

void test_pdf_writer_multipage() {
    std::cout << "  test_pdf_writer_multipage... ";
    
    PDFWriter pdf;
    pdf.open("/tmp/athena_test_multipage.pdf");
    
    // Page 1
    pdf.new_page();
    pdf.set_font("Helvetica-Bold", 18);
    pdf.draw_text_centered(700, "Page 1");
    
    // Page 2
    pdf.new_page();
    pdf.set_font("Helvetica", 14);
    pdf.draw_text(100, 700, "Page 2 content");
    
    // Page 3
    pdf.new_page();
    pdf.draw_text(100, 700, "Page 3 content");
    
    pdf.close();
    
    assert(file_exists("/tmp/athena_test_multipage.pdf"));
    assert(file_size("/tmp/athena_test_multipage.pdf") > 500);
    
    std::remove("/tmp/athena_test_multipage.pdf");
    
    std::cout << "OK\n";
}

void test_pdf_writer_graphics() {
    std::cout << "  test_pdf_writer_graphics... ";
    
    PDFWriter pdf;
    pdf.open("/tmp/athena_test_graphics.pdf");
    
    pdf.new_page();
    
    // Lines
    pdf.draw_line(100, 700, 500, 700, 2.0);
    pdf.draw_line(100, 650, 500, 650, 0.5);
    
    // Rectangles
    pdf.draw_rect(100, 500, 200, 100, 1.0);
    
    pdf.set_fill_color(0.8, 0.2, 0.2);
    pdf.fill_rect(350, 500, 100, 100);
    
    // Bars
    pdf.draw_bar(100, 300, 50, 100, 0.2, 0.4, 0.8);
    pdf.draw_bar(160, 300, 50, 150, 0.4, 0.6, 0.8);
    pdf.draw_bar(220, 300, 50, 80, 0.6, 0.8, 0.8);
    
    pdf.close();
    
    assert(file_exists("/tmp/athena_test_graphics.pdf"));
    
    std::remove("/tmp/athena_test_graphics.pdf");
    
    std::cout << "OK\n";
}

// =============================================================================
// Report Generator Tests
// =============================================================================

void test_report_metadata() {
    std::cout << "  test_report_metadata... ";
    
    auto meta = create_default_metadata("Test Report");
    
    assert(meta.title == "Test Report");
    assert(!meta.date.empty());
    assert(meta.classification == "UNCLASSIFIED");
    assert(!meta.author.empty());
    
    std::cout << "OK\n";
}

void test_mission_report() {
    std::cout << "  test_mission_report... ";
    
    ReportGenerator gen;
    
    auto meta = create_default_metadata("Mission Test Report");
    gen.set_metadata(meta);
    
    // Create test scenario
    Scenario scenario;
    scenario.name = "Test Battle";
    scenario.description = "A test scenario for report generation";
    
    ScenarioActor blue1;
    blue1.id = "blue_tank_1";
    blue1.name = "M1A2 Abrams";
    blue1.side = "blue";
    scenario.actors.push_back(blue1);
    
    ScenarioActor red1;
    red1.id = "red_tank_1";
    red1.name = "T-90M";
    red1.side = "red";
    scenario.actors.push_back(red1);
    
    // Create test summaries
    ForceSummary blue_summary;
    blue_summary.side = "blue";
    blue_summary.initial_count = 10;
    blue_summary.final_count = 7;
    blue_summary.losses = 3;
    blue_summary.survival_rate = 0.7;
    
    ForceSummary red_summary;
    red_summary.side = "red";
    red_summary.initial_count = 15;
    red_summary.final_count = 5;
    red_summary.losses = 10;
    red_summary.survival_rate = 0.333;
    
    EngagementSummary engagement;
    engagement.total_engagements = 45;
    engagement.blue_shots_fired = 120;
    engagement.red_shots_fired = 180;
    
    std::vector<std::string> events = {
        "[001] Contact: Blue Tank spotted Red Tank at 3500m",
        "[002] Engagement: Blue Tank fires at Red Tank",
        "[003] Hit: Red Tank damaged (75% health)",
        "[010] Kill: Red Tank destroyed"
    };
    
    bool success = gen.generate_mission_report(
        "/tmp/athena_mission_report.pdf",
        scenario,
        blue_summary,
        red_summary,
        engagement,
        events);
    
    assert(success);
    assert(file_exists("/tmp/athena_mission_report.pdf"));
    assert(file_size("/tmp/athena_mission_report.pdf") > 1000);
    
    std::remove("/tmp/athena_mission_report.pdf");
    
    std::cout << "OK\n";
}

void test_montecarlo_report() {
    std::cout << "  test_montecarlo_report... ";
    
    ReportGenerator gen;
    
    Scenario scenario;
    scenario.name = "Monte Carlo Test";
    scenario.description = "Testing MC report generation";
    
    MonteCarloSummary mc;
    mc.iterations = 1000;
    mc.master_seed = 12345;
    mc.elapsed_seconds = 45.7;
    mc.blue_wins = 650;
    mc.red_wins = 300;
    mc.draws = 50;
    mc.blue_win_pct = 65.0;
    mc.red_win_pct = 30.0;
    mc.draw_pct = 5.0;
    mc.blue_survival_mean = 0.72;
    mc.blue_survival_stddev = 0.15;
    mc.blue_survival_p5 = 0.45;
    mc.blue_survival_p95 = 0.95;
    mc.red_survival_mean = 0.35;
    mc.red_survival_stddev = 0.20;
    mc.red_survival_p5 = 0.05;
    mc.red_survival_p95 = 0.70;
    
    // Simple histogram
    std::vector<f64> blue_hist = {0.1, 0.2, 0.3, 0.5, 0.7, 0.9, 1.0, 0.8, 0.6, 0.4,
                                   0.3, 0.2, 0.15, 0.1, 0.08, 0.05, 0.03, 0.02, 0.01, 0.01};
    std::vector<f64> red_hist = {0.5, 0.7, 0.9, 1.0, 0.8, 0.6, 0.4, 0.3, 0.2, 0.15,
                                  0.1, 0.08, 0.05, 0.03, 0.02, 0.01, 0.01, 0.01, 0.0, 0.0};
    
    bool success = gen.generate_montecarlo_report(
        "/tmp/athena_mc_report.pdf",
        scenario,
        mc,
        blue_hist,
        red_hist);
    
    assert(success);
    assert(file_exists("/tmp/athena_mc_report.pdf"));
    assert(file_size("/tmp/athena_mc_report.pdf") > 2000);
    
    std::remove("/tmp/athena_mc_report.pdf");
    
    std::cout << "OK\n";
}

void test_comparison_report() {
    std::cout << "  test_comparison_report... ";
    
    ReportGenerator gen;
    
    std::vector<std::pair<std::string, std::map<std::string, f64>>> platforms;
    
    platforms.push_back({"M1A2 Abrams", {
        {"Weight (tons)", 62.0},
        {"Max Speed (km/h)", 67.0},
        {"Main Gun (mm)", 120.0},
        {"Armor (mm RHAe)", 800.0}
    }});
    
    platforms.push_back({"T-90M", {
        {"Weight (tons)", 48.0},
        {"Max Speed (km/h)", 60.0},
        {"Main Gun (mm)", 125.0},
        {"Armor (mm RHAe)", 650.0}
    }});
    
    platforms.push_back({"Leopard 2A7", {
        {"Weight (tons)", 67.0},
        {"Max Speed (km/h)", 68.0},
        {"Main Gun (mm)", 120.0},
        {"Armor (mm RHAe)", 850.0}
    }});
    
    bool success = gen.generate_comparison_report(
        "/tmp/athena_comparison.pdf",
        "Main Battle Tank Comparison",
        platforms);
    
    assert(success);
    assert(file_exists("/tmp/athena_comparison.pdf"));
    
    std::remove("/tmp/athena_comparison.pdf");
    
    std::cout << "OK\n";
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "=== ATHENA PDF Report Tests ===\n\n";
    
    std::cout << "PDF Writer Tests:\n";
    test_pdf_writer_basic();
    test_pdf_writer_multipage();
    test_pdf_writer_graphics();
    
    std::cout << "\nReport Generator Tests:\n";
    test_report_metadata();
    test_mission_report();
    test_montecarlo_report();
    test_comparison_report();
    
    std::cout << "\n=== All PDF report tests passed! ===\n";
    return 0;
}
