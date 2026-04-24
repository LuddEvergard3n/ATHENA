// ATHENA Core - PDF Report Generator Implementation
// Minimal valid PDF 1.4 with Helvetica text.
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/report/pdf_report.hpp"
#include <cstdio>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <vector>

namespace athena {
namespace report {

// Internal: tracks byte offsets for xref table
struct PdfWriter {
    FILE* f;
    std::vector<long> obj_offsets;

    void begin_obj(int id) {
        while (static_cast<int>(obj_offsets.size()) < id)
            obj_offsets.push_back(0);
        obj_offsets[id - 1] = ftell(f);
        fprintf(f, "%d 0 obj\n", id);
    }

    void end_obj() {
        fprintf(f, "endobj\n");
    }

    static std::string escape(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == '(' || c == ')' || c == '\\') out += '\\';
            out += c;
        }
        return out;
    }
};

// Build the content stream text for the report
static std::string build_content(const ReportConfig& cfg) {
    std::ostringstream ss;

    // A4 = 595 x 842 points. Origin bottom-left.
    float y = 800.0f;
    float left = 50.0f;
    float line_h = 16.0f;
    float section_gap = 24.0f;

    auto line = [&](const char* font_size, const std::string& text) {
        ss << "BT\n"
           << "/F1 " << font_size << " Tf\n"
           << left << " " << y << " Td\n"
           << "(" << PdfWriter::escape(text) << ") Tj\n"
           << "ET\n";
        y -= line_h;
    };

    auto line_bold = [&](const std::string& text) {
        ss << "BT\n"
           << "/F2 12 Tf\n"
           << left << " " << y << " Td\n"
           << "(" << PdfWriter::escape(text) << ") Tj\n"
           << "ET\n";
        y -= line_h;
    };

    auto gap = [&]() { y -= section_gap; };

    // Title
    ss << "BT\n/F2 18 Tf\n" << left << " " << y << " Td\n"
       << "(ATHENA Monte Carlo Analysis Report) Tj\nET\n";
    y -= 28;

    char buf[256];
    snprintf(buf, sizeof(buf), "Scenario: %s", cfg.scenario_name.c_str());
    line("10", buf);
    snprintf(buf, sizeof(buf), "Generated: %s  |  ATHENA %s",
             cfg.timestamp.c_str(), cfg.version.c_str());
    line("9", buf);
    gap();

    // Horizontal rule
    ss << left << " " << y + 8 << " m " << 545 << " " << y + 8 << " l S\n";
    gap();

    // Win probabilities
    line_bold("Outcome Probabilities");
    y -= 4;

    const auto& st = cfg.stats;
    f64 total = std::max(st.completed_iterations, 1u);
    f64 blue_pct = 100.0 * st.blue_wins / total;
    f64 red_pct  = 100.0 * st.red_wins  / total;
    f64 draw_pct = 100.0 * st.draws     / total;

    snprintf(buf, sizeof(buf), "Blue Win: %.1f%%    Red Win: %.1f%%    Draw: %.1f%%",
             blue_pct, red_pct, draw_pct);
    line("11", buf);

    snprintf(buf, sizeof(buf), "Iterations: %u%s",
             st.completed_iterations,
             st.converged_early ? " (converged)" : "");
    line("10", buf);
    gap();

    // Survival rates
    line_bold("Survival Analysis");
    y -= 4;

    snprintf(buf, sizeof(buf), "Blue survival: %.0f%% mean [%.0f%% - %.0f%% CI95]",
             st.blue_survival_rate.mean * 100,
             st.blue_survival_rate.p5 * 100,
             st.blue_survival_rate.p95 * 100);
    line("10", buf);

    snprintf(buf, sizeof(buf), "Red survival:  %.0f%% mean [%.0f%% - %.0f%% CI95]",
             st.red_survival_rate.mean * 100,
             st.red_survival_rate.p5 * 100,
             st.red_survival_rate.p95 * 100);
    line("10", buf);

    f64 tick_sec = cfg.tick_duration_seconds > 0 ? cfg.tick_duration_seconds : 3600.0;
    f64 mean_h = st.ticks_to_completion.mean * tick_sec / 3600.0;
    f64 med_h  = st.ticks_to_completion.median * tick_sec / 3600.0;
    snprintf(buf, sizeof(buf), "Time to resolution: %.0fh mean (%.0fh median)", mean_h, med_h);
    line("10", buf);
    gap();

    // Performance
    if (st.iteration_time_ms.mean > 0.0) {
        line_bold("Performance");
        y -= 4;
        snprintf(buf, sizeof(buf),
                 "%.1f ms/iteration (min: %.1f, max: %.1f, total: %.1fs)",
                 st.iteration_time_ms.mean,
                 st.iteration_time_ms.min,
                 st.iteration_time_ms.max,
                 st.total_batch_time_ms / 1000.0);
        line("10", buf);

        if (st.iteration_time_ms.mean > 0.0) {
            f64 rate = 1000.0 / st.iteration_time_ms.mean;
            snprintf(buf, sizeof(buf), "Throughput: %.0f iterations/sec", rate);
            line("10", buf);
        }
        gap();
    }

    // Variant comparison
    if (!cfg.variants.empty()) {
        line_bold("Variant Comparison");
        y -= 4;

        snprintf(buf, sizeof(buf),
                 "%-16s %6s %8s %8s %8s %8s",
                 "Name", "Iters", "Blue%", "Red%", "BlueSrv", "AvgHrs");
        line("9", buf);

        ss << left << " " << y + 8 << " m " << 500 << " " << y + 8 << " l S\n";
        y -= 4;

        for (const auto& v : cfg.variants) {
            snprintf(buf, sizeof(buf),
                     "%-16s %6u %7.1f%% %7.1f%% %7.0f%% %7.0fh",
                     v.name.c_str(), v.iterations,
                     v.blue_win_pct, v.red_win_pct,
                     v.blue_survival_mean * 100,
                     v.avg_duration_hours);
            line("9", buf);
        }
    }

    // Footer
    y = 30;
    ss << "BT\n/F1 8 Tf\n" << left << " " << y << " Td\n"
       << "(Generated by ATHENA - Advanced Tactical & Heuristic "
          "Engagement & Network Analyzer) Tj\nET\n";

    return ss.str();
}

bool generate_pdf_report(const std::string& output_path, const ReportConfig& config) {
    FILE* f = fopen(output_path.c_str(), "wb");
    if (!f) return false;

    PdfWriter pw;
    pw.f = f;
    pw.obj_offsets.resize(6, 0);

    // Header
    fprintf(f, "%%PDF-1.4\n");
    fprintf(f, "%%%c%c%c%c\n", (char)0xC0, (char)0xC1, (char)0xC2, (char)0xC3);

    // Object 1: Catalog
    pw.begin_obj(1);
    fprintf(f, "<< /Type /Catalog /Pages 2 0 R >>\n");
    pw.end_obj();

    // Object 2: Pages
    pw.begin_obj(2);
    fprintf(f, "<< /Type /Pages /Kids [3 0 R] /Count 1 >>\n");
    pw.end_obj();

    // Object 3: Page
    pw.begin_obj(3);
    fprintf(f, "<< /Type /Page /Parent 2 0 R\n"
               "   /MediaBox [0 0 595 842]\n"
               "   /Contents 6 0 R\n"
               "   /Resources << /Font << /F1 4 0 R /F2 5 0 R >> >>\n"
               ">>\n");
    pw.end_obj();

    // Object 4: Font (Helvetica)
    pw.begin_obj(4);
    fprintf(f, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\n");
    pw.end_obj();

    // Object 5: Font (Helvetica-Bold)
    pw.begin_obj(5);
    fprintf(f, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >>\n");
    pw.end_obj();

    // Object 6: Content stream
    std::string content = build_content(config);

    pw.begin_obj(6);
    fprintf(f, "<< /Length %zu >>\nstream\n", content.size());
    fwrite(content.c_str(), 1, content.size(), f);
    fprintf(f, "\nendstream\n");
    pw.end_obj();

    // Cross-reference table
    long xref_offset = ftell(f);
    int num_objects = 7;  // 0 + 6 objects
    fprintf(f, "xref\n0 %d\n", num_objects);
    fprintf(f, "0000000000 65535 f \n");
    for (int i = 0; i < 6; ++i) {
        fprintf(f, "%010ld 00000 n \n", pw.obj_offsets[i]);
    }

    // Trailer
    fprintf(f, "trailer\n<< /Size %d /Root 1 0 R >>\n", num_objects);
    fprintf(f, "startxref\n%ld\n", xref_offset);
    fprintf(f, "%%%%EOF\n");

    fclose(f);
    return true;
}

}  // namespace report
}  // namespace athena
