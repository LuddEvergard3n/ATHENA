// ATHENA Core - Integrated GUI (Dear ImGui)
// Full graphical interface combining all features.
// All render_*() methods of UnifiedApp live here.
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/integrated.hpp"
#include "athena/report/pdf_report.hpp"
#include "imgui.h"

#include <cstring>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <dirent.h>
#include <cmath>
#include <chrono>

namespace athena {

// =============================================================================
// ImGui Style Setup
// =============================================================================

static void setup_imgui_style() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding    = 4.0f;
    style.FrameRounding     = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding      = 2.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.WindowPadding     = ImVec2(8, 8);
    style.FramePadding      = ImVec2(6, 4);
    style.ItemSpacing       = ImVec2(8, 6);

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text]                  = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    c[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    c[ImGuiCol_WindowBg]              = ImVec4(0.08f, 0.08f, 0.10f, 0.98f);
    c[ImGuiCol_ChildBg]               = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    c[ImGuiCol_PopupBg]               = ImVec4(0.10f, 0.10f, 0.12f, 0.98f);
    c[ImGuiCol_Border]                = ImVec4(0.25f, 0.28f, 0.30f, 0.80f);
    c[ImGuiCol_FrameBg]               = ImVec4(0.15f, 0.16f, 0.18f, 1.00f);
    c[ImGuiCol_FrameBgHovered]        = ImVec4(0.20f, 0.22f, 0.25f, 1.00f);
    c[ImGuiCol_FrameBgActive]         = ImVec4(0.25f, 0.28f, 0.32f, 1.00f);
    c[ImGuiCol_TitleBg]               = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
    c[ImGuiCol_TitleBgActive]         = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);
    c[ImGuiCol_MenuBarBg]             = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    c[ImGuiCol_ScrollbarBg]           = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]         = ImVec4(0.25f, 0.28f, 0.32f, 1.00f);
    c[ImGuiCol_Button]                = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
    c[ImGuiCol_ButtonHovered]         = ImVec4(0.30f, 0.38f, 0.45f, 1.00f);
    c[ImGuiCol_ButtonActive]          = ImVec4(0.35f, 0.45f, 0.55f, 1.00f);
    c[ImGuiCol_Header]                = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
    c[ImGuiCol_HeaderHovered]         = ImVec4(0.30f, 0.38f, 0.45f, 1.00f);
    c[ImGuiCol_HeaderActive]          = ImVec4(0.35f, 0.45f, 0.55f, 1.00f);
    c[ImGuiCol_Tab]                   = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    c[ImGuiCol_TabHovered]            = ImVec4(0.30f, 0.38f, 0.45f, 1.00f);
    c[ImGuiCol_TabActive]             = ImVec4(0.25f, 0.32f, 0.40f, 1.00f);
    c[ImGuiCol_PlotLines]             = ImVec4(0.50f, 0.70f, 0.90f, 1.00f);
    c[ImGuiCol_PlotHistogram]         = ImVec4(0.35f, 0.65f, 0.35f, 1.00f);
    c[ImGuiCol_TableHeaderBg]         = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    c[ImGuiCol_TableBorderStrong]     = ImVec4(0.25f, 0.28f, 0.32f, 1.00f);
    c[ImGuiCol_TableBorderLight]      = ImVec4(0.20f, 0.22f, 0.25f, 1.00f);
    c[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
}

// =============================================================================
// Menu Bar
// =============================================================================

void UnifiedApp::render_menu_bar() {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scenario", "Ctrl+N"))
            state_.show_new_scenario = true;
        if (ImGui::MenuItem("Open Scenario...", "Ctrl+O"))
            state_.show_load_scenario = true;
        if (ImGui::MenuItem("Save Scenario", "Ctrl+S")) {
            if (state_.scenario.is_new || state_.scenario.scenario_path.empty()) {
                state_.show_save_scenario = true;  // Redirect to Save As
            } else {
                save_scenario(state_.scenario.scenario_path);
            }
        }
        if (ImGui::MenuItem("Save Scenario As...", "Ctrl+Shift+S"))
            state_.show_save_scenario = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Export Results...", nullptr, false,
                            state_.analysis.has_data))
            state_.show_export_results = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4"))
            state_.should_close = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
        ImGui::MenuItem("Redo", "Ctrl+Y", false, false);
        ImGui::Separator();
        if (ImGui::MenuItem("Settings..."))
            state_.show_settings = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Platform Browser", nullptr,
                            state_.active_view == UnifiedAppState::View::BROWSER))
            state_.active_view = UnifiedAppState::View::BROWSER;
        if (ImGui::MenuItem("Scenario Editor", nullptr,
                            state_.active_view == UnifiedAppState::View::SCENARIO))
            state_.active_view = UnifiedAppState::View::SCENARIO;
        if (ImGui::MenuItem("Simulation", nullptr,
                            state_.active_view == UnifiedAppState::View::SIMULATION))
            state_.active_view = UnifiedAppState::View::SIMULATION;
        if (ImGui::MenuItem("Analysis", nullptr,
                            state_.active_view == UnifiedAppState::View::ANALYSIS))
            state_.active_view = UnifiedAppState::View::ANALYSIS;
        ImGui::Separator();
        ImGui::MenuItem("Show Demo Window", nullptr, &state_.show_demo);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Simulation")) {
        if (ImGui::MenuItem("Start", "F5", false,
                            !state_.sim_control.is_running))
            start_simulation();
        if (ImGui::MenuItem("Pause", "F6", false,
                            state_.sim_control.is_running && !state_.sim_control.is_paused))
            pause_simulation();
        if (ImGui::MenuItem("Resume", "F6", false,
                            state_.sim_control.is_running && state_.sim_control.is_paused))
            resume_simulation();
        if (ImGui::MenuItem("Stop", "F7", false,
                            state_.sim_control.is_running))
            stop_simulation();
        ImGui::Separator();
        if (ImGui::MenuItem("Step Forward", "F10", false,
                            state_.sim_control.is_loaded && state_.sim_control.is_paused))
            step_simulation(1);
        ImGui::Separator();
        if (ImGui::MenuItem("Run Monte Carlo...", nullptr, false,
                            !state_.mc_control.is_running))
            state_.show_run_simulation_dialog = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About ATHENA..."))
            state_.show_about = true;
        ImGui::EndMenu();
    }

    // Right-aligned status (only set cursor when we have content to render)
    if (state_.sim_control.is_running || state_.mc_control.is_running) {
        float status_x = ImGui::GetWindowWidth() - 200;
        ImGui::SetCursorPosX(status_x);
        if (state_.sim_control.is_running) {
            if (state_.sim_control.is_paused)
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "PAUSED");
            else
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "RUNNING");
        } else {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "MC: %u/%u",
                               state_.mc_control.completed_iterations,
                               state_.mc_control.num_iterations);
        }
    }

    ImGui::EndMainMenuBar();
}

// =============================================================================
// Browser View
// =============================================================================

void UnifiedApp::render_browser_view() {
    // Left panel — Categories
    ImGui::BeginChild("Categories", ImVec2(200, 0), true);
    ImGui::Text("Categories");
    ImGui::Separator();

    // "All" entry
    {
        char label[128];
        snprintf(label, sizeof(label), "All (%zu)", state_.browser.db.count());
        bool sel = (state_.browser.selected_category == PlatformCategory::UNKNOWN);
        if (ImGui::Selectable(label, sel)) {
            state_.browser.selected_category = PlatformCategory::UNKNOWN;
            apply_browser_filters();
        }
    }

    for (const auto& [cat, count] : state_.browser.category_counts) {
        char label[128];
        snprintf(label, sizeof(label), "%s (%d)",
                 category_to_string(cat), count);
        bool sel = (state_.browser.selected_category == cat);
        if (ImGui::Selectable(label, sel)) {
            state_.browser.selected_category = cat;
            apply_browser_filters();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Main content
    ImGui::BeginChild("BrowserContent", ImVec2(0, 0));

    // Search bar
    ImGui::Text("Search:");
    ImGui::SameLine();
    if (ImGui::InputText("##Search", state_.browser.search_buffer,
                         sizeof(state_.browser.search_buffer)))
        apply_browser_filters();

    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        state_.browser.search_buffer[0] = '\0';
        apply_browser_filters();
    }

    // Country filter
    ImGui::SameLine(0, 20);
    ImGui::Text("Country:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::BeginCombo("##Country",
                           state_.browser.selected_country.empty()
                               ? "All"
                               : state_.browser.selected_country.c_str())) {
        if (ImGui::Selectable("All", state_.browser.selected_country.empty())) {
            state_.browser.selected_country.clear();
            apply_browser_filters();
        }
        for (const auto& country : state_.browser.countries) {
            if (ImGui::Selectable(country.c_str(),
                                  state_.browser.selected_country == country)) {
                state_.browser.selected_country = country;
                apply_browser_filters();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();
    ImGui::Text("Showing %zu platforms", state_.browser.filtered.size());

    // Platform table
    if (ImGui::BeginTable("Platforms", 4,
                          ImGuiTableFlags_Borders |
                          ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_Resizable |
                          ImGuiTableFlags_ScrollY,
                          ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 8))) {
        ImGui::TableSetupColumn("Name",    ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type",    ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Country", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Weight",  ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(state_.browser.filtered.size()); ++i) {
            const auto* spec = state_.browser.filtered[i];

            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool selected = (state_.browser.selected_idx == i);
            if (ImGui::Selectable(spec->name.c_str(), selected,
                                  ImGuiSelectableFlags_SpanAllColumns)) {
                state_.browser.selected_idx = i;
                state_.browser.selected     = spec;
            }

            ImGui::TableNextColumn();
            ImGui::Text("%s", category_to_string(spec->category));

            ImGui::TableNextColumn();
            ImGui::Text("%s", spec->country_of_origin.c_str());

            ImGui::TableNextColumn();
            if (spec->weight_kg > 0) {
                ImGui::Text("%.1f t", spec->weight_kg / 1000.0);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    // Detail panel
    if (state_.browser.selected) {
        render_platform_detail();
    }

    ImGui::EndChild();
}

// =============================================================================
// Platform Detail
// =============================================================================

void UnifiedApp::render_platform_detail() {
    const auto* p = state_.browser.selected;
    if (!p) return;

    ImGui::BeginChild("PlatformDetail", ImVec2(0, 0), true);

    ImGui::Text("%s", p->name.c_str());
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", p->id.c_str());
    ImGui::Separator();

    if (ImGui::BeginTable("Specs", 2, ImGuiTableFlags_Borders)) {
        auto row = [](const char* label, const char* value) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%s", label);
            ImGui::TableNextColumn(); ImGui::Text("%s", value);
        };

        row("Country",      p->country_of_origin.c_str());
        row("Manufacturer", p->manufacturer.c_str());
        row("Category",     category_to_string(p->category));

        if (p->crew > 0) {
            row("Crew", std::to_string(p->crew).c_str());
        }
        if (p->weight_kg > 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1f t", p->weight_kg / 1000.0);
            row("Weight", buf);
        }
        if (p->mobility.max_speed_kmh > 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.0f km/h", p->mobility.max_speed_kmh);
            row("Max Speed", buf);
        }
        if (p->mobility.range_km > 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.0f km", p->mobility.range_km);
            row("Range", buf);
        }
        if (p->protection.front_mm_rha > 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.0f mm RHA", p->protection.front_mm_rha);
            row("Armor (front)", buf);
        }

        ImGui::EndTable();
    }

    // Ratings
    if (p->firepower_rating > 0 || p->protection_rating > 0 || p->mobility_rating > 0) {
        ImGui::Separator();
        ImGui::Text("Ratings");
        ImGui::ProgressBar(static_cast<float>(p->firepower_rating / 500.0),
                           ImVec2(-1, 0), "Firepower");
        ImGui::ProgressBar(static_cast<float>(p->protection_rating / 500.0),
                           ImVec2(-1, 0), "Protection");
        ImGui::ProgressBar(static_cast<float>(p->mobility_rating / 200.0),
                           ImVec2(-1, 0), "Mobility");
    }

    if (!p->notes.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", p->notes.c_str());
    }

    ImGui::EndChild();
}

// =============================================================================
// Scenario View
// =============================================================================

void UnifiedApp::render_scenario_view() {
    // ---- Top toolbar --------------------------------------------------------
    render_scenario_toolbar();

    // ---- Three-panel layout: actor list | map | properties ------------------
    float avail_h = ImGui::GetContentRegionAvail().y;

    // Left panel — Actor list + Terrain config (200px)
    ImGui::BeginChild("ActorList", ImVec2(200, avail_h), true);

    // Terrain config section
    if (ImGui::CollapsingHeader("Terrain", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto& tc = state_.scenario.scenario.environment.terrain;

        // Terrain presets — each preset sets base_type + overlays in one click.
        // User can then fine-tune sliders below.
        // Preset index: 0=Custom (manual), 1+ = named presets
        struct TerrainPreset {
            const char* name;
            const char* base;
            float roughness;
            float forest;
            float urban;
            bool rivers;
        };
        static constexpr TerrainPreset presets[] = {
            { "Custom",          "flat",      0.3f, 0.0f,  0.0f,  false },
            { "Plains",          "flat",      0.1f, 0.0f,  0.0f,  false },
            { "Hills",           "rolling",   0.4f, 0.1f,  0.0f,  false },
            { "Mountains",       "mountains", 0.8f, 0.1f,  0.0f,  false },
            { "Forest",          "rolling",   0.3f, 0.6f,  0.0f,  false },
            { "Dense Forest",    "rolling",   0.4f, 0.8f,  0.0f,  true  },
            { "Jungle",          "rolling",   0.5f, 0.75f, 0.0f,  true  },
            { "Rainforest",      "rolling",   0.6f, 0.8f,  0.0f,  true  },
            { "Desert",          "flat",      0.2f, 0.0f,  0.0f,  false },
            { "Desert Mountains","mountains", 0.7f, 0.0f,  0.0f,  false },
            { "Coastal",         "coastal",   0.3f, 0.05f, 0.0f,  true  },
            { "River Valley",    "coastal",   0.3f, 0.2f,  0.05f, true  },
            { "Village",         "flat",      0.2f, 0.1f,  0.08f, false },
            { "Small City",      "flat",      0.1f, 0.05f, 0.15f, false },
            { "City",            "flat",      0.1f, 0.02f, 0.3f,  false },
            { "Urban",           "flat",      0.05f,0.0f,  0.4f,  false },
            { "Metropolis",      "flat",      0.0f, 0.0f,  0.5f,  false },
            { "Urban + Forest",  "rolling",   0.3f, 0.3f,  0.2f,  false },
            { "Urban + Mountain","mountains", 0.6f, 0.05f, 0.2f,  false },
            { "City + River",    "flat",      0.1f, 0.05f, 0.25f, true  },
            { "Mountain Forest", "mountains", 0.7f, 0.5f,  0.0f,  true  },
            { "Steppe",          "flat",      0.15f,0.02f, 0.0f,  false },
            { "Tundra",          "flat",      0.2f, 0.0f,  0.0f,  false },
            { "Swampland",       "coastal",   0.2f, 0.3f,  0.0f,  true  },
            { "Bocage",          "rolling",   0.3f, 0.4f,  0.1f,  false },
        };
        static constexpr int NUM_PRESETS = sizeof(presets) / sizeof(presets[0]);

        // Detect current preset match (0 = Custom if nothing matches)
        static int preset_idx = 0;
        ImGui::SetNextItemWidth(130);
        if (ImGui::Combo("##TerrainPreset", &preset_idx, [](void* data, int idx, const char** out) -> bool {
            *out = static_cast<const TerrainPreset*>(data)[idx].name;
            return true;
        }, (void*)presets, NUM_PRESETS)) {
            if (preset_idx > 0) {
                const auto& p = presets[preset_idx];
                tc.base_type = p.base;
                tc.roughness = p.roughness;
                tc.forest_density = p.forest;
                tc.urban_density = p.urban;
                tc.rivers_enabled = p.rivers;
                state_.scenario.is_modified = true;
            }
        }

        // Fine-tune sliders (always visible, editing sets preset to Custom)
        const char* base_types[] = {"flat", "rolling", "mountains", "coastal"};
        int base_idx = 0;
        if (tc.base_type == "rolling") base_idx = 1;
        else if (tc.base_type == "mountains") base_idx = 2;
        else if (tc.base_type == "coastal") base_idx = 3;

        ImGui::SetNextItemWidth(130);
        if (ImGui::Combo("##TerrainBase", &base_idx, base_types, 4)) {
            tc.base_type = base_types[base_idx];
            preset_idx = 0;  // Switch to Custom
            state_.scenario.is_modified = true;
        }

        float rough = static_cast<float>(tc.roughness);
        ImGui::SetNextItemWidth(130);
        if (ImGui::SliderFloat("Rough##tr", &rough, 0.0f, 1.0f, "%.1f")) {
            tc.roughness = rough;
            preset_idx = 0;
            state_.scenario.is_modified = true;
        }

        float forest = static_cast<float>(tc.forest_density);
        ImGui::SetNextItemWidth(130);
        if (ImGui::SliderFloat("Forest##tf", &forest, 0.0f, 0.8f, "%.2f")) {
            tc.forest_density = forest;
            preset_idx = 0;
            state_.scenario.is_modified = true;
        }

        float urban = static_cast<float>(tc.urban_density);
        ImGui::SetNextItemWidth(130);
        if (ImGui::SliderFloat("Urban##tu", &urban, 0.0f, 0.5f, "%.2f")) {
            tc.urban_density = urban;
            preset_idx = 0;
            state_.scenario.is_modified = true;
        }

        bool rivers = tc.rivers_enabled;
        if (ImGui::Checkbox("Rivers##trv", &rivers)) {
            tc.rivers_enabled = rivers;
            preset_idx = 0;
            state_.scenario.is_modified = true;
        }

        // Terrain impact summary
        ImGui::Spacing();
        ImGui::TextDisabled("Impact:");
        float fd = static_cast<float>(tc.forest_density);
        float ud = static_cast<float>(tc.urban_density);
        float rg = static_cast<float>(tc.roughness);
        if (fd > 0.1f)
            ImGui::TextDisabled("  Forest: speed -%.0f%%, detect -%.0f%%, cover +%.0f%%",
                                fd * 60, fd * 70, fd * 50);
        if (ud > 0.05f)
            ImGui::TextDisabled("  Urban: speed -%.0f%%, cover +%.0f%%",
                                ud * 50, ud * 60);
        if (rg > 0.3f)
            ImGui::TextDisabled("  Terrain: vehicle speed -%.0f%%",
                                rg * 40);
        if (tc.rivers_enabled)
            ImGui::TextDisabled("  Rivers: block ground, slow amphibious");
    }

    // v1.2.0: Weather & Environment
    if (ImGui::CollapsingHeader("Weather & Environment")) {
        auto& ss = state_.scenario;
        static const char* weather_names[] = {
            "Clear", "Overcast", "Rain", "Heavy Rain",
            "Snow", "Blizzard", "Fog", "Dust", "Sandstorm"
        };
        static const char* climate_names[] = {
            "Temperate", "Arctic", "Tropical", "Arid", "Continental"
        };

        ImGui::SetNextItemWidth(130);
        ImGui::Combo("Weather##wx", &ss.weather_idx, weather_names, 9);
        ImGui::SetNextItemWidth(130);
        ImGui::Combo("Climate##cl", &ss.climate_idx, climate_names, 5);

        ImGui::SliderFloat("Visibility (km)##vis", &ss.visibility_km, 0.1f, 20.0f, "%.1f");
        ImGui::SliderFloat("Temperature (C)##tmp", &ss.temperature_c, -40.0f, 55.0f, "%.0f");
        ImGui::SliderFloat("Wind (km/h)##wnd", &ss.wind_speed_kmh, 0.0f, 120.0f, "%.0f");
        ImGui::Checkbox("Day/Night Cycle##dn", &ss.day_night);

        ImGui::Spacing();
        ImGui::TextDisabled("Impact:");
        if (ss.weather_idx == 2 || ss.weather_idx == 3)
            ImGui::TextDisabled("  Rain: visibility -30-60%%, movement -20-40%%");
        if (ss.weather_idx == 4 || ss.weather_idx == 5)
            ImGui::TextDisabled("  Snow: visibility -40-80%%, movement -30-60%%");
        if (ss.weather_idx == 6)
            ImGui::TextDisabled("  Fog: visibility -80%%, sensors degraded");
        if (ss.weather_idx == 7 || ss.weather_idx == 8)
            ImGui::TextDisabled("  Dust/Sand: visibility -50-90%%, equipment wear");
        if (ss.visibility_km < 2.0f)
            ImGui::TextDisabled("  Low vis: air ops severely limited");
    }

    // Side rename section
    if (ImGui::CollapsingHeader("Sides")) {
        ImGui::TextDisabled("Rename sides:");
        for (int si = 0; si < 6; ++si) {
            ImGui::PushID(100 + si);
            char buf[64];
            strncpy(buf, state_.side_display_names[si].c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##sn", buf, sizeof(buf))) {
                state_.side_display_names[si] = buf;
            }
            ImGui::PopID();
        }
    }

    ImGui::Separator();
    ImGui::Text("Units (%zu)", state_.scenario.scenario.actors.size());
    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(state_.scenario.scenario.actors.size()); ++i) {
        const auto& actor = state_.scenario.scenario.actors[i];
        ImVec4 color = (actor.side == "blue")
                            ? ImVec4(0.3f, 0.5f, 1.0f, 1.0f)
                      : (actor.side == "red")
                            ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f)
                      : (actor.side == "green")
                            ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f)
                      : (actor.side == "orange")
                            ? ImVec4(1.0f, 0.6f, 0.1f, 1.0f)
                      : (actor.side == "yellow")
                            ? ImVec4(1.0f, 0.9f, 0.1f, 1.0f)
                            : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        bool selected = (state_.scenario.selected_actor_idx == i);
        // Show name + unit type abbreviation
        UnitType ut = unit_type_from_string(actor.type);
        char label[256];
        if (ut != UnitType::GENERIC) {
            snprintf(label, sizeof(label), "%s [%s]",
                     actor.name.c_str(), unit_type_to_string(ut));
        } else {
            snprintf(label, sizeof(label), "%s", actor.name.c_str());
        }
        if (ImGui::Selectable(label, selected))
            state_.scenario.selected_actor_idx = i;
        ImGui::PopStyleColor();
        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::SameLine();

    // Center — Map (flex)
    float props_w = 280.0f;
    float map_w = ImGui::GetContentRegionAvail().x - props_w - 8.0f;
    if (map_w < 200.0f) map_w = ImGui::GetContentRegionAvail().x;

    ImGui::BeginChild("ScenarioMapPanel", ImVec2(map_w, avail_h), true);
    render_scenario_map();
    ImGui::EndChild();

    // Right panel — Actor properties (only if map has room)
    if (map_w < ImGui::GetContentRegionAvail().x) {
        // already consumed, skip
    } else {
        ImGui::SameLine();
    }
    if (ImGui::GetContentRegionAvail().x > 50) {
        ImGui::SameLine();
        ImGui::BeginChild("ActorProps", ImVec2(0, avail_h), true);
        render_actor_properties();
        ImGui::EndChild();
    }
}

// =============================================================================
// Scenario Toolbar
// =============================================================================

void UnifiedApp::render_scenario_toolbar() {
    auto& sc = state_.scenario;
    auto& scenario = sc.scenario;

    // Scenario name (editable)
    static char name_buf[128] = {0};
    if (name_buf[0] == '\0' && !scenario.name.empty()) {
        strncpy(name_buf, scenario.name.c_str(), sizeof(name_buf) - 1);
    }
    ImGui::Text("Scenario:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##ScenarioName", name_buf, sizeof(name_buf))) {
        scenario.name = name_buf;
        sc.is_modified = true;
    }

    ImGui::SameLine(0, 20);

    // Duration in hours (user-facing). Internally: max_ticks * tick_duration_seconds / 3600
    f64 total_hours = scenario.temporal.tick_duration_seconds
                    * scenario.temporal.max_ticks / 3600.0;
    float hours_f = static_cast<float>(total_hours);
    ImGui::Text("Duration:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    if (ImGui::InputFloat("##DurationHrs", &hours_f, 0, 0, "%.0fh")) {
        if (hours_f >= 1.0f) {
            // Keep tick_duration fixed at 1 hour, adjust max_ticks
            scenario.temporal.tick_duration_seconds = 3600.0;
            scenario.temporal.max_ticks = static_cast<u32>(hours_f);
            sc.is_modified = true;
        }
    }

    ImGui::SameLine(0, 30);

    // Add unit buttons — one per side
    static int s_actor_counter = 0;  // Monotonic, never resets
    auto add_unit_button = [&](const char* label, const char* side_str,
                               ImVec4 btn_color) {
        ImGui::PushStyleColor(ImGuiCol_Button, btn_color);
        if (ImGui::Button(label)) {
            ActorDefinition actor;
            actor.id = std::string(side_str) + "-" +
                       std::to_string(++s_actor_counter);
            actor.name = std::string("New ") + label + " Unit";
            // Strip the "+ " prefix from label for name
            if (actor.name.size() > 6) {
                actor.name = "New " + std::string(label + 2) + " Unit";
            }
            actor.type = "unit";
            actor.side = side_str;
            actor.initial_position = {sc.map_center_lat, sc.map_center_lon, 0.0};
            actor.initial_health = 1.0;
            actor.initial_supply = 1.0;
            actor.initial_morale = 0.9;
            actor.initial_readiness = 1.0;
            scenario.actors.push_back(actor);
            sc.selected_actor_idx = static_cast<int>(scenario.actors.size()) - 1;
            sc.is_modified = true;
        }
        ImGui::PopStyleColor();
    };

    // Add unit buttons use display names
    char btn_buf[64];
    snprintf(btn_buf, sizeof(btn_buf), "+ %s", state_.side_display_names[0].c_str());
    add_unit_button(btn_buf, "blue",   ImVec4(0.15f, 0.3f, 0.7f, 1.0f));
    ImGui::SameLine();
    snprintf(btn_buf, sizeof(btn_buf), "+ %s", state_.side_display_names[1].c_str());
    add_unit_button(btn_buf, "red",    ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
    ImGui::SameLine();
    snprintf(btn_buf, sizeof(btn_buf), "+ %s", state_.side_display_names[2].c_str());
    add_unit_button(btn_buf, "green",  ImVec4(0.15f, 0.55f, 0.15f, 1.0f));
    ImGui::SameLine();
    snprintf(btn_buf, sizeof(btn_buf), "+ %s", state_.side_display_names[3].c_str());
    add_unit_button(btn_buf, "orange", ImVec4(0.7f, 0.4f, 0.05f, 1.0f));
    ImGui::SameLine();
    snprintf(btn_buf, sizeof(btn_buf), "+ %s", state_.side_display_names[4].c_str());
    add_unit_button(btn_buf, "yellow", ImVec4(0.65f, 0.6f, 0.05f, 1.0f));

    ImGui::SameLine(0, 10);
    if (sc.selected_actor_idx >= 0 &&
        sc.selected_actor_idx < static_cast<int>(scenario.actors.size())) {
        if (ImGui::Button("Delete Selected")) {
            scenario.actors.erase(scenario.actors.begin() + sc.selected_actor_idx);
            sc.selected_actor_idx = -1;
            sc.is_modified = true;
        }
    }

    ImGui::SameLine(0, 10);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.5f, 1.0f));
    if (ImGui::Button("Deploy Formation")) {
        state_.show_force_builder = true;
    }
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 30);
    if (ImGui::Button("Center Map")) {
        auto_center_scenario_map();
    }

    ImGui::Separator();
}

// =============================================================================
// Scenario Map — Interactive lat/lon with drag-to-move
// =============================================================================

void UnifiedApp::render_scenario_map() {
    auto& sc = state_.scenario;
    auto& actors = sc.scenario.actors;

    ImVec2 canvas_pos  = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    ImDrawList* dl     = ImGui::GetWindowDrawList();

    // Background
    dl->AddRectFilled(canvas_pos,
                      ImVec2(canvas_pos.x + canvas_size.x,
                             canvas_pos.y + canvas_size.y),
                      IM_COL32(15, 22, 30, 255));

    // ---- Procedural terrain visualization ----
    // Draws colored cells based on scenario terrain config to give the operator
    // visual feedback on terrain type. Uses deterministic hash for cell placement.
    {
        const auto& tc = sc.scenario.environment.terrain;
        float forest_d = static_cast<float>(tc.forest_density);
        float urban_d  = static_cast<float>(tc.urban_density);
        float rough    = static_cast<float>(tc.roughness);
        bool  rivers   = tc.rivers_enabled;

        // Only draw if there's something to show
        if (forest_d > 0.01f || urban_d > 0.01f || rough > 0.15f || rivers) {
            constexpr int TCELLS = 32;  // grid resolution
            float cell_w = canvas_size.x / TCELLS;
            float cell_h = canvas_size.y / TCELLS;

            for (int cy = 0; cy < TCELLS; ++cy) {
                for (int cx = 0; cx < TCELLS; ++cx) {
                    // Deterministic hash for this cell
                    u32 h = static_cast<u32>(cx * 73856093u ^ cy * 19349669u);
                    h ^= h >> 13; h *= 0x165667B1u; h ^= h >> 16;
                    float r = (h & 0xFFFF) / 65535.0f;  // 0-1

                    ImU32 cell_col = 0;

                    // Priority: river > urban > forest > rough terrain > open
                    if (rivers && r < 0.04f) {
                        // River cells (blue)
                        cell_col = IM_COL32(30, 55, 90, 120);
                    } else if (r < urban_d) {
                        // Urban (gray)
                        u8 g = static_cast<u8>(50 + (h >> 8 & 0x1F));
                        cell_col = IM_COL32(g, g - 5, g - 10, 100);
                    } else if (r < urban_d + forest_d) {
                        // Forest (green, varied)
                        u8 g = static_cast<u8>(35 + (h >> 16 & 0x1F));
                        cell_col = IM_COL32(15, g, 12, 110);
                    } else if (rough > 0.4f && r < rough * 0.4f) {
                        // Rough/elevated terrain (brown tint)
                        cell_col = IM_COL32(40, 32, 20, 70);
                    }

                    if (cell_col != 0) {
                        float x0 = canvas_pos.x + cx * cell_w;
                        float y0 = canvas_pos.y + cy * cell_h;
                        dl->AddRectFilled(ImVec2(x0, y0),
                                          ImVec2(x0 + cell_w, y0 + cell_h),
                                          cell_col);
                    }
                }
            }
        }
    }

    // Viewport: lat/lon -> screen
    f64 view_min_lat = sc.map_center_lat - sc.map_span_lat * 0.5;
    f64 view_max_lat = sc.map_center_lat + sc.map_span_lat * 0.5;
    f64 view_min_lon = sc.map_center_lon - sc.map_span_lon * 0.5;
    f64 view_max_lon = sc.map_center_lon + sc.map_span_lon * 0.5;

    auto latlon_to_screen = [&](f64 lat, f64 lon) -> ImVec2 {
        float sx = canvas_pos.x + static_cast<float>(
            (lon - view_min_lon) / (view_max_lon - view_min_lon) * canvas_size.x);
        float sy = canvas_pos.y + canvas_size.y - static_cast<float>(
            (lat - view_min_lat) / (view_max_lat - view_min_lat) * canvas_size.y);
        return ImVec2(sx, sy);
    };

    auto screen_to_latlon = [&](ImVec2 sp, f64& lat, f64& lon) {
        lon = view_min_lon + (sp.x - canvas_pos.x) / canvas_size.x *
              (view_max_lon - view_min_lon);
        lat = view_max_lat - (sp.y - canvas_pos.y) / canvas_size.y *
              (view_max_lat - view_min_lat);
    };

    // Grid (latitude/longitude lines)
    if (sc.show_grid) {
        // Auto grid spacing: ~8 lines across
        f64 grid_step_lon = sc.map_span_lon / 8.0;
        f64 grid_step_lat = sc.map_span_lat / 8.0;
        // Round to nice values
        auto nice_step = [](f64 rough) -> f64 {
            f64 steps[] = {0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0};
            for (f64 s : steps) { if (s >= rough * 0.7) return s; }
            return rough;
        };
        grid_step_lon = nice_step(grid_step_lon);
        grid_step_lat = nice_step(grid_step_lat);

        ImU32 gc = IM_COL32(35, 42, 52, 255);
        ImU32 tc = IM_COL32(55, 65, 80, 200);

        f64 start_lon = std::floor(view_min_lon / grid_step_lon) * grid_step_lon;
        for (f64 lon = start_lon; lon <= view_max_lon; lon += grid_step_lon) {
            ImVec2 top = latlon_to_screen(view_max_lat, lon);
            ImVec2 bot = latlon_to_screen(view_min_lat, lon);
            dl->AddLine(top, bot, gc);
            char buf[16]; snprintf(buf, sizeof(buf), "%.2f", lon);
            dl->AddText(ImVec2(top.x + 2, canvas_pos.y + 2), tc, buf);
        }
        f64 start_lat = std::floor(view_min_lat / grid_step_lat) * grid_step_lat;
        for (f64 lat = start_lat; lat <= view_max_lat; lat += grid_step_lat) {
            ImVec2 left  = latlon_to_screen(lat, view_min_lon);
            ImVec2 right = latlon_to_screen(lat, view_max_lon);
            dl->AddLine(left, right, gc);
            char buf[16]; snprintf(buf, sizeof(buf), "%.2f", lat);
            dl->AddText(ImVec2(canvas_pos.x + 2, left.y - 14), tc, buf);
        }
    }

    // Draw engagement ranges (subtle circles)
    for (int i = 0; i < static_cast<int>(actors.size()); ++i) {
        const auto& actor = actors[i];
        if (actor.firepower.has_value()) {
            ImVec2 center = latlon_to_screen(actor.initial_position.lat,
                                              actor.initial_position.lon);
            // range_km -> degrees (approximate)
            f64 range_deg = actor.firepower->range_km / 111.0;
            f64 range_px = range_deg / sc.map_span_lat * canvas_size.y;
            if (range_px > 3.0) {
                ImU32 range_col = (actor.side == "blue")
                    ? IM_COL32(60, 100, 200, 40)
                    : (actor.side == "green")
                    ? IM_COL32(60, 200, 60, 40)
                    : (actor.side == "orange")
                    ? IM_COL32(200, 140, 20, 40)
                    : (actor.side == "yellow")
                    ? IM_COL32(200, 200, 20, 40) : IM_COL32(200, 60, 60, 40);
                dl->AddCircleFilled(center, static_cast<float>(range_px), range_col);
            }
        }
    }

    // Draw actors
    for (int i = 0; i < static_cast<int>(actors.size()); ++i) {
        const auto& actor = actors[i];
        ImVec2 sp = latlon_to_screen(actor.initial_position.lat,
                                      actor.initial_position.lon);

        ImU32 color = (actor.side == "blue")   ? IM_COL32(80, 140, 255, 255)
                    : (actor.side == "red")    ? IM_COL32(255, 80, 80, 255)
                    : (actor.side == "green")  ? IM_COL32(80, 220, 80, 255)
                    : (actor.side == "orange") ? IM_COL32(255, 160, 30, 255)
                    : (actor.side == "yellow") ? IM_COL32(240, 220, 30, 255)
                                              : IM_COL32(120, 120, 120, 255);

        bool selected = (sc.selected_actor_idx == i);
        float radius = selected ? 10.0f : 7.0f;

        // NATO-style filled diamond for units
        float r = radius;
        dl->AddQuadFilled(ImVec2(sp.x, sp.y - r), ImVec2(sp.x + r, sp.y),
                          ImVec2(sp.x, sp.y + r), ImVec2(sp.x - r, sp.y), color);

        if (selected) {
            dl->AddQuad(ImVec2(sp.x, sp.y - r - 3), ImVec2(sp.x + r + 3, sp.y),
                        ImVec2(sp.x, sp.y + r + 3), ImVec2(sp.x - r - 3, sp.y),
                        IM_COL32(255, 255, 100, 220), 2.0f);
        }

        // Name label
        dl->AddText(ImVec2(sp.x + r + 4, sp.y - 7),
                    IM_COL32(210, 210, 210, 220), actor.name.c_str());
    }

    // Scale bar (bottom-left)
    {
        // approx 1 degree lat = 111km
        f64 visible_km = sc.map_span_lat * 111.0;
        f64 bar_km = visible_km * 0.2;
        f64 nice_bars[] = {1, 2, 5, 10, 20, 50, 100, 200, 500};
        f64 chosen = nice_bars[0];
        for (f64 n : nice_bars) { if (n <= bar_km) chosen = n; }
        float bar_px = static_cast<float>(chosen / visible_km * canvas_size.y);
        float bx = canvas_pos.x + 10;
        float by = canvas_pos.y + canvas_size.y - 15;
        dl->AddLine(ImVec2(bx, by), ImVec2(bx + bar_px, by),
                    IM_COL32(180, 180, 180, 200), 2.0f);
        char buf[32]; snprintf(buf, sizeof(buf), "%.0f km", chosen);
        dl->AddText(ImVec2(bx, by - 14), IM_COL32(180, 180, 180, 200), buf);
    }

    // Terrain legend (top-right corner, compact)
    {
        const auto& tc = sc.scenario.environment.terrain;
        float lx = canvas_pos.x + canvas_size.x - 110;
        float ly = canvas_pos.y + 5;
        float lh = 13.0f;
        int items = 0;

        if (tc.forest_density > 0.01) {
            dl->AddRectFilled(ImVec2(lx, ly), ImVec2(lx + 10, ly + 10),
                              IM_COL32(15, 50, 12, 200));
            dl->AddText(ImVec2(lx + 14, ly - 1), IM_COL32(150, 150, 150, 200), "Forest");
            ly += lh; items++;
        }
        if (tc.urban_density > 0.01) {
            dl->AddRectFilled(ImVec2(lx, ly), ImVec2(lx + 10, ly + 10),
                              IM_COL32(55, 50, 45, 200));
            dl->AddText(ImVec2(lx + 14, ly - 1), IM_COL32(150, 150, 150, 200), "Urban");
            ly += lh; items++;
        }
        if (tc.rivers_enabled) {
            dl->AddRectFilled(ImVec2(lx, ly), ImVec2(lx + 10, ly + 10),
                              IM_COL32(30, 55, 90, 200));
            dl->AddText(ImVec2(lx + 14, ly - 1), IM_COL32(150, 150, 150, 200), "River");
            ly += lh; items++;
        }
        if (tc.roughness > 0.4) {
            dl->AddRectFilled(ImVec2(lx, ly), ImVec2(lx + 10, ly + 10),
                              IM_COL32(40, 32, 20, 200));
            dl->AddText(ImVec2(lx + 14, ly - 1), IM_COL32(150, 150, 150, 200), "Rough");
            items++;
        }
        (void)items;
    }

    // ---- Mouse interaction ---------------------------------------------------
    ImGui::InvisibleButton("scenariocanvas", canvas_size);
    bool hovered = ImGui::IsItemHovered();

    // Click to select actor
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !sc.dragging_actor) {
        ImVec2 mouse = ImGui::GetMousePos();
        int closest = -1;
        float closest_dist = 20.0f;  // max pick distance in pixels
        for (int i = 0; i < static_cast<int>(actors.size()); ++i) {
            ImVec2 sp = latlon_to_screen(actors[i].initial_position.lat,
                                          actors[i].initial_position.lon);
            float dx = mouse.x - sp.x;
            float dy = mouse.y - sp.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < closest_dist) {
                closest_dist = dist;
                closest = i;
            }
        }
        if (closest >= 0) {
            sc.selected_actor_idx = closest;
            sc.dragging_actor = true;
            sc.drag_actor_idx = closest;
        }
    }

    // Drag selected actor
    if (sc.dragging_actor && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (sc.drag_actor_idx >= 0 && sc.drag_actor_idx < static_cast<int>(actors.size())) {
            ImVec2 mouse = ImGui::GetMousePos();
            f64 lat, lon;
            screen_to_latlon(mouse, lat, lon);
            actors[sc.drag_actor_idx].initial_position.lat = lat;
            actors[sc.drag_actor_idx].initial_position.lon = lon;
            sc.is_modified = true;
        }
    }
    if (sc.dragging_actor && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        sc.dragging_actor = false;
        sc.drag_actor_idx = -1;
    }

    // Middle-drag to pan
    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        f64 lon_per_px = sc.map_span_lon / canvas_size.x;
        f64 lat_per_px = sc.map_span_lat / canvas_size.y;
        sc.map_center_lon -= delta.x * lon_per_px;
        sc.map_center_lat += delta.y * lat_per_px;
    }

    // Scroll to zoom
    if (hovered) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0) {
            f64 factor = (wheel > 0) ? 0.85 : 1.18;
            sc.map_span_lat *= factor;
            sc.map_span_lon *= factor;
            sc.map_span_lat = std::clamp(sc.map_span_lat, 0.01, 50.0);
            sc.map_span_lon = std::clamp(sc.map_span_lon, 0.01, 60.0);
        }
    }
}

// =============================================================================
// Actor Properties Panel
// =============================================================================

void UnifiedApp::render_actor_properties() {
    auto& sc = state_.scenario;
    auto& actors = sc.scenario.actors;

    if (sc.selected_actor_idx < 0 ||
        sc.selected_actor_idx >= static_cast<int>(actors.size())) {
        ImGui::TextDisabled("Select a unit to edit.");
        return;
    }

    auto& actor = actors[sc.selected_actor_idx];
    ImGui::Text("Unit Properties");
    ImGui::Separator();

    // Name
    static char name_buf[128] = {0};
    if (ImGui::IsWindowAppearing() ||
        strncmp(name_buf, actor.name.c_str(), sizeof(name_buf)) != 0) {
        strncpy(name_buf, actor.name.c_str(), sizeof(name_buf) - 1);
    }
    ImGui::Text("Name:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##ActorName", name_buf, sizeof(name_buf))) {
        actor.name = name_buf;
        sc.is_modified = true;
    }

    // Side
    ImGui::Text("Side:");
    int side_idx = (actor.side == "blue")   ? 0 :
                   (actor.side == "red")    ? 1 :
                   (actor.side == "green")  ? 2 :
                   (actor.side == "orange") ? 3 :
                   (actor.side == "yellow") ? 4 : 5;
    const char* side_vals[] = {"blue", "red", "green", "orange", "yellow", "neutral"};
    // Build display names from user-customizable labels
    const char* side_display[6];
    for (int si = 0; si < 6; ++si)
        side_display[si] = state_.side_display_names[si].c_str();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##Side", &side_idx, side_display, 6)) {
        actor.side = side_vals[side_idx];
        sc.is_modified = true;
    }

    // Unit Type
    ImGui::Text("Type:");
    UnitType current_ut = unit_type_from_string(actor.type);
    int ut_idx = static_cast<int>(current_ut);
    const char* ut_names[] = {
        "Generic", "Light Infantry", "Heavy Infantry", "Mechanized",
        "Armor", "Artillery", "Air Defense", "Logistics", "Medical",
        "Recon", "Special Forces", "Engineer", "Fighter", "Attack Helo", "Naval"
    };
    const char* ut_vals[] = {
        "unit", "light_infantry", "heavy_infantry", "mechanized",
        "armor", "artillery", "air_defense", "logistics", "medical",
        "recon", "special_forces", "engineer", "fighter", "attack_helo", "naval"
    };
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##UnitType", &ut_idx, ut_names, 15)) {
        actor.type = ut_vals[ut_idx];
        sc.is_modified = true;
    }
    // Show modifier summary
    {
        auto ut = static_cast<UnitType>(ut_idx);
        const auto& m = get_unit_modifiers(ut);
        ImGui::TextDisabled("FP:%.1fx Def:%.1fx Mob:%.1fx",
                           m.firepower_factor, m.defense_factor, m.mobility_factor);
    }

    ImGui::Separator();

    // Position
    ImGui::Text("Position:");
    float lat = static_cast<float>(actor.initial_position.lat);
    float lon = static_cast<float>(actor.initial_position.lon);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat("Lat##pos", &lat, 0.001f, -90.0f, 90.0f, "%.4f")) {
        actor.initial_position.lat = lat;
        sc.is_modified = true;
    }
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat("Lon##pos", &lon, 0.001f, -180.0f, 180.0f, "%.4f")) {
        actor.initial_position.lon = lon;
        sc.is_modified = true;
    }

    ImGui::Separator();

    // =========================================================================
    // Platform Picker — select from loaded database, auto-populates stats
    // =========================================================================
    ImGui::Text("Platform:");
    if (!actor.platform_id.empty()) {
        // Show currently selected platform
        const PlatformSpec* current_spec = state_.browser.db.get(actor.platform_id);
        if (current_spec) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "%s",
                               current_spec->name.c_str());
            ImGui::TextDisabled("(%s)", actor.platform_id.c_str());

            // Show key stats from platform
            if (current_spec->firepower_rating > 0.0)
                ImGui::TextDisabled("FP: %.1f", current_spec->firepower_rating);
            if (current_spec->mobility.max_speed_kmh > 0.0)
                ImGui::TextDisabled("Speed: %.0f km/h",
                                   current_spec->mobility.max_speed_kmh);
            if (current_spec->detection_range_m > 0.0)
                ImGui::TextDisabled("Detect: %.0f m",
                                   current_spec->detection_range_m);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                               "Not found: %s", actor.platform_id.c_str());
        }
        if (ImGui::SmallButton("Clear Platform")) {
            actor.platform_id.clear();
            sc.is_modified = true;
        }
    } else {
        ImGui::TextDisabled("No platform assigned");
    }

    // Search box
    ImGui::SetNextItemWidth(-1);
    bool search_changed = ImGui::InputText("##PlatSearch",
        sc.platform_search_buf, sizeof(sc.platform_search_buf));

    // Update search results when text changes
    if (search_changed && sc.platform_search_buf[0] != '\0') {
        sc.platform_search_results.clear();
        std::string query = sc.platform_search_buf;
        std::transform(query.begin(), query.end(), query.begin(), ::tolower);

        for (const auto& [id, spec] : state_.browser.db.all()) {
            std::string haystack = spec.id + " " + spec.name + " " +
                                   spec.type + " " + spec.manufacturer +
                                   " " + spec.country_of_origin;
            std::transform(haystack.begin(), haystack.end(),
                           haystack.begin(), ::tolower);
            if (haystack.find(query) != std::string::npos) {
                sc.platform_search_results.push_back(&spec);
                if (sc.platform_search_results.size() >= 15) break;
            }
        }
    }
    if (sc.platform_search_buf[0] == '\0') {
        sc.platform_search_results.clear();
    }

    // Show search results as selectable list
    if (!sc.platform_search_results.empty()) {
        ImGui::BeginChild("##PlatResults", ImVec2(-1, 150), true);
        for (const auto* spec : sc.platform_search_results) {
            char label[256];
            snprintf(label, sizeof(label), "%s [%s] (%s)",
                     spec->name.c_str(), spec->id.c_str(),
                     spec->country_of_origin.c_str());
            if (ImGui::Selectable(label)) {
                actor.platform_id = spec->id;
                // Auto-set name from platform if still default
                if (actor.name.find("New ") == 0) {
                    actor.name = spec->name;
                    strncpy(name_buf, actor.name.c_str(), sizeof(name_buf) - 1);
                }
                // Auto-infer unit type from platform category
                const char* inferred = infer_unit_type_str(spec->category, spec->type);
                actor.type = inferred;

                sc.platform_search_buf[0] = '\0';
                sc.platform_search_results.clear();
                sc.is_modified = true;
            }
        }
        ImGui::EndChild();
    }

    ImGui::Separator();
}

// =============================================================================
// Simulation View
// =============================================================================

void UnifiedApp::render_simulation_view() {
    // Controls
    ImGui::BeginChild("SimControls", ImVec2(0, 100), true);

    // Top row: scenario info + controls
    if (!state_.sim_control.is_loaded) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                           "No scenario loaded. Use File > Open Scenario first.");
        ImGui::EndChild();
        return;
    }

    // Scenario name
    ImGui::Text("Scenario: %s", state_.scenario.scenario.name.c_str());
    ImGui::SameLine(0, 20);

    // Seed: auto-generated by default, manual via Advanced section
    static bool sim_use_manual_seed = false;
    static int sim_manual_seed = 42;

    auto get_sim_seed = [&]() -> Seed {
        if (sim_use_manual_seed)
            return static_cast<Seed>(sim_manual_seed);
        return static_cast<Seed>(
            std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFFFFFF);
    };

    // Start / Resume / Pause / Stop buttons
    if (!state_.sim_control.is_running && !state_.sim_control.is_complete) {
        if (ImGui::Button("Start", ImVec2(80, 30)))
            start_simulation(get_sim_seed());
    } else if (state_.sim_control.is_complete) {
        if (ImGui::Button("Restart", ImVec2(80, 30)))
            start_simulation(get_sim_seed());
    } else {
        if (state_.sim_control.is_paused) {
            if (ImGui::Button("Resume", ImVec2(80, 30)))
                resume_simulation();
        } else {
            if (ImGui::Button("Pause", ImVec2(80, 30)))
                pause_simulation();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Stop", ImVec2(80, 30)))
        stop_simulation();
    ImGui::SameLine();
    if (ImGui::Button("Step", ImVec2(80, 30))) {
        if (!state_.sim_control.is_running && state_.sim_control.is_loaded)
            start_simulation(get_sim_seed());
        step_simulation(1);
    }

    // v1.2.3: Reorganize — reset BROKEN units of a chosen side
    ImGui::SameLine(0, 10);
    if (ImGui::Button("Reorg", ImVec2(60, 30)))
        ImGui::OpenPopup("ReorgPopup");
    if (ImGui::BeginPopup("ReorgPopup")) {
        ImGui::TextDisabled("Reform broken units:");
        if (state_.sim_control.blue_total > 0 && ImGui::Selectable("Blue"))
            reorganize_side(Side::BLUE);
        if (state_.sim_control.red_total > 0 && ImGui::Selectable("Red"))
            reorganize_side(Side::RED);
        if (state_.sim_control.green_total > 0 && ImGui::Selectable("Green"))
            reorganize_side(Side::GREEN);
        if (state_.sim_control.orange_total > 0 && ImGui::Selectable("Orange"))
            reorganize_side(Side::ORANGE);
        if (state_.sim_control.yellow_total > 0 && ImGui::Selectable("Yellow"))
            reorganize_side(Side::YELLOW);
        ImGui::EndPopup();
    }

    // Speed slider
    ImGui::SameLine(0, 30);
    ImGui::Text("Speed:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    float speed = static_cast<float>(state_.sim_control.time_scale);
    if (ImGui::SliderFloat("##Speed", &speed, 0.1f, 10.0f, "%.1fx"))
        state_.sim_control.time_scale = static_cast<f64>(speed);

    // Progress: show elapsed time instead of ticks
    ImGui::SameLine(0, 30);
    {
        f64 tick_sec = state_.scenario.scenario.temporal.tick_duration_seconds;
        f64 elapsed_h = state_.sim_control.current_tick * tick_sec / 3600.0;
        f64 total_h   = state_.sim_control.max_ticks * tick_sec / 3600.0;
        ImGui::Text("%.0fh / %.0fh", elapsed_h, total_h);
    }
    ImGui::SameLine();
    ImGui::ProgressBar(static_cast<float>(state_.sim_control.progress_percent / 100.0),
                       ImVec2(200, 0));

    // Stats — show all active sides dynamically
    {
        const auto& s = state_.sim_control;
        std::string stats;
        if (s.blue_total > 0)
            stats += "Blue: " + std::to_string(s.blue_alive) + "/" +
                     std::to_string(s.blue_total) + "  ";
        if (s.red_total > 0)
            stats += "Red: " + std::to_string(s.red_alive) + "/" +
                     std::to_string(s.red_total) + "  ";
        if (s.green_total > 0)
            stats += "Green: " + std::to_string(s.green_alive) + "/" +
                     std::to_string(s.green_total) + "  ";
        if (s.orange_total > 0)
            stats += "Orange: " + std::to_string(s.orange_alive) + "/" +
                     std::to_string(s.orange_total) + "  ";
        if (s.yellow_total > 0)
            stats += "Yellow: " + std::to_string(s.yellow_alive) + "/" +
                     std::to_string(s.yellow_total) + "  ";
        stats += "| Engagements: " + std::to_string(s.total_engagements);

        // v1.2.0: Weather indicator
        static const char* wx_short[] = {
            "", "Overcast", "Rain", "Heavy Rain",
            "Snow", "Blizzard", "Fog", "Dust", "Sandstorm"
        };
        int wi = state_.scenario.weather_idx;
        if (wi > 0 && wi < 9)
            stats += std::string(" | ") + wx_short[wi];

        ImGui::Text("%s", stats.c_str());
    }

    // Advanced: Reproducibility (collapsed by default)
    if (!state_.sim_control.is_running) {
        ImGui::SameLine(0, 20);
        if (ImGui::TreeNode("Advanced##SimSeed")) {
            ImGui::Checkbox("Fixed seed", &sim_use_manual_seed);
            if (sim_use_manual_seed) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100);
                ImGui::InputInt("##SimSeed", &sim_manual_seed);
            }
            ImGui::TreePop();
        }
    }

    ImGui::EndChild();

    // Map + event log side by side
    ImGui::BeginChild("SimContent");

    ImGui::BeginChild("SimMap",
                       ImVec2(ImGui::GetContentRegionAvail().x * 0.7f, 0), true);
    render_simulation_map();
    ImGui::EndChild();

    ImGui::SameLine();

    // Event log
    ImGui::BeginChild("EventLog", ImVec2(0, 0), true);
    ImGui::Text("Event Log");
    ImGui::Separator();

    ImGui::BeginChild("EventScroll");
    for (const auto& msg : state_.event_log) {
        ImGui::TextWrapped("%s", msg.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    ImGui::EndChild();  // EventLog
    ImGui::EndChild();  // SimContent
}

// =============================================================================
// Simulation Map
// =============================================================================

void UnifiedApp::render_simulation_map() {
    ImVec2 canvas_pos  = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    ImDrawList* dl     = ImGui::GetWindowDrawList();

    dl->AddRectFilled(canvas_pos,
                      ImVec2(canvas_pos.x + canvas_size.x,
                             canvas_pos.y + canvas_size.y),
                      IM_COL32(15, 18, 22, 255));

    // World-to-screen coordinate mapping using dynamic bounds
    const auto& sc = state_.sim_control;
    f64 world_w = sc.bounds_max_x - sc.bounds_min_x;
    f64 world_h = sc.bounds_max_y - sc.bounds_min_y;
    if (world_w < 1.0) world_w = 100000.0;
    if (world_h < 1.0) world_h = 100000.0;

    // Uniform scale (preserve aspect ratio)
    f64 scale = std::min(
        static_cast<f64>(canvas_size.x) / world_w,
        static_cast<f64>(canvas_size.y) / world_h);

    // Center offset
    f64 offset_x = (canvas_size.x - world_w * scale) * 0.5;
    f64 offset_y = (canvas_size.y - world_h * scale) * 0.5;

    auto world_to_screen = [&](f64 wx, f64 wy) -> ImVec2 {
        float sx = canvas_pos.x + static_cast<float>(
            offset_x + (wx - sc.bounds_min_x) * scale);
        float sy = canvas_pos.y + canvas_size.y - static_cast<float>(
            offset_y + (wy - sc.bounds_min_y) * scale);
        return ImVec2(sx, sy);
    };

    // Terrain overlay (sample grid and color by overlay type)
    if (sim_terrain_ && sim_terrain_->is_initialized()) {
        // Sample at ~40x40 grid resolution for performance
        constexpr int GRID = 40;
        f64 cell_w = world_w / GRID;
        f64 cell_h = world_h / GRID;

        for (int gy = 0; gy < GRID; ++gy) {
            for (int gx = 0; gx < GRID; ++gx) {
                f64 wx = sc.bounds_min_x + (gx + 0.5) * cell_w;
                f64 wy = sc.bounds_min_y + (gy + 0.5) * cell_h;

                OverlayType ot = sim_terrain_->overlay_at(wx, wy);
                if (ot == OverlayType::NONE) continue;

                ImU32 fill;
                switch (ot) {
                    case OverlayType::FOREST: fill = IM_COL32(30, 80, 30, 60);  break;
                    case OverlayType::URBAN:  fill = IM_COL32(90, 90, 90, 50);  break;
                    case OverlayType::SWAMP:  fill = IM_COL32(50, 70, 40, 50);  break;
                    case OverlayType::RIVER:  fill = IM_COL32(30, 50, 120, 80); break;
                    case OverlayType::ROAD:   fill = IM_COL32(100, 90, 70, 40); break;
                    default: continue;
                }

                ImVec2 tl = world_to_screen(sc.bounds_min_x + gx * cell_w,
                                            sc.bounds_min_y + (gy + 1) * cell_h);
                ImVec2 br = world_to_screen(sc.bounds_min_x + (gx + 1) * cell_w,
                                            sc.bounds_min_y + gy * cell_h);
                dl->AddRectFilled(tl, br, fill);
            }
        }
    }

    // Draw entities
    for (const auto& ent : state_.entity_vis) {
        ImU32 color;
        if (!ent.alive) {
            color = IM_COL32(80, 80, 80, 128);
        } else if (ent.broken) {
            // v1.1.8: Broken units — dimmed with alpha, operationally dead
            color = IM_COL32(100, 100, 100, 160);
        } else if (ent.side == "blue") {
            color = IM_COL32(80, 140, 255, 255);
        } else if (ent.side == "green") {
            color = IM_COL32(80, 220, 80, 255);
        } else if (ent.side == "red") {
            color = IM_COL32(255, 80, 80, 255);
        } else if (ent.side == "orange") {
            color = IM_COL32(255, 160, 30, 255);
        } else if (ent.side == "yellow") {
            color = IM_COL32(240, 220, 30, 255);
        } else {
            color = IM_COL32(120, 120, 120, 255);
        }

        ImVec2 sp = world_to_screen(ent.x, ent.y);
        float radius = ent.alive ? 6.0f : 4.0f;

        if (ent.alive && ent.broken) {
            // Broken: hollow circle (ring only) + diagonal line through it
            dl->AddCircle(sp, radius, color, 0, 2.0f);
            dl->AddLine(ImVec2(sp.x - 4, sp.y + 4), ImVec2(sp.x + 4, sp.y - 4),
                        IM_COL32(200, 200, 0, 180), 1.5f);
        } else {
            dl->AddCircleFilled(sp, radius, color);
        }

        // v1.1.8: Fatigue indicator — orange ring around fatigued units
        if (ent.alive && ent.fatigued && !ent.broken) {
            dl->AddCircle(sp, radius + 2.0f, IM_COL32(255, 160, 0, 180), 0, 1.5f);
        }

        // v1.2.0: Suppression indicator — red dashed ring
        if (ent.alive && ent.suppressed && !ent.broken) {
            // Intensity proportional to suppression level
            u8 alpha = static_cast<u8>(120 + 135 * ent.suppression);
            dl->AddCircle(sp, radius + 4.0f, IM_COL32(255, 40, 40, alpha), 8, 1.5f);
        }

        // v1.2.0: Heading indicator — small line showing facing direction
        if (ent.alive && !ent.broken) {
            float hx = sp.x + std::cos(static_cast<float>(ent.heading)) * (radius + 3.0f);
            float hy = sp.y - std::sin(static_cast<float>(ent.heading)) * (radius + 3.0f);
            dl->AddLine(sp, ImVec2(hx, hy), IM_COL32(255, 255, 255, 140), 1.5f);
        }

        // v1.2.0: Artillery marker — small diamond above indirect fire units
        if (ent.alive && ent.is_indirect) {
            float dx = sp.x, dy = sp.y - radius - 6.0f;
            dl->AddQuadFilled(
                ImVec2(dx, dy - 3), ImVec2(dx + 3, dy),
                ImVec2(dx, dy + 3), ImVec2(dx - 3, dy),
                IM_COL32(255, 220, 80, 200));
        }

        // Name label (only for alive entities)
        if (ent.alive && !ent.name.empty()) {
            dl->AddText(ImVec2(sp.x + 8, sp.y - 6), IM_COL32(200, 200, 200, 200),
                        ent.name.c_str());
        }

        // Health bar (only for alive entities with damage)
        if (ent.alive && ent.health < 1.0) {
            float bar_w = 20.0f;
            float bar_h = 3.0f;
            float bx = sp.x - bar_w * 0.5f;
            float by = sp.y + radius + 2.0f;
            dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bar_w, by + bar_h),
                              IM_COL32(60, 0, 0, 200));
            dl->AddRectFilled(ImVec2(bx, by),
                              ImVec2(bx + bar_w * static_cast<float>(ent.health),
                                     by + bar_h),
                              IM_COL32(0, 200, 0, 220));

            // v1.1.8: Cohesion bar (below health bar) — only if degraded
            if (ent.cohesion < 0.95) {
                float cy = by + bar_h + 1.0f;
                dl->AddRectFilled(ImVec2(bx, cy), ImVec2(bx + bar_w, cy + bar_h),
                                  IM_COL32(40, 0, 40, 200));
                ImU32 coh_color = (ent.cohesion > 0.5)
                    ? IM_COL32(80, 80, 255, 220)     // Blue: ok
                    : (ent.cohesion > 0.25)
                        ? IM_COL32(255, 160, 0, 220)  // Orange: degraded
                        : IM_COL32(255, 0, 0, 220);   // Red: near collapse
                dl->AddRectFilled(ImVec2(bx, cy),
                    ImVec2(bx + bar_w * static_cast<float>(ent.cohesion), cy + bar_h),
                    coh_color);
            }
        }

        // Death marker
        if (!ent.alive) {
            dl->AddLine(ImVec2(sp.x - 4, sp.y - 4), ImVec2(sp.x + 4, sp.y + 4),
                        IM_COL32(255, 50, 50, 180), 2.0f);
            dl->AddLine(ImVec2(sp.x + 4, sp.y - 4), ImVec2(sp.x - 4, sp.y + 4),
                        IM_COL32(255, 50, 50, 180), 2.0f);
        }
    }

    // Scale indicator (bottom-left)
    {
        f64 scale_m = world_w * 0.2;  // ~20% of visible width
        // Round to nice number
        f64 nice[] = {1000, 2000, 5000, 10000, 20000, 50000, 100000};
        f64 chosen = nice[0];
        for (f64 n : nice) {
            if (n <= scale_m) chosen = n;
        }
        float px_len = static_cast<float>(chosen * scale);
        float sx = canvas_pos.x + 10;
        float sy = canvas_pos.y + canvas_size.y - 15;
        dl->AddLine(ImVec2(sx, sy), ImVec2(sx + px_len, sy),
                    IM_COL32(180, 180, 180, 200), 2.0f);
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f km", chosen / 1000.0);
        dl->AddText(ImVec2(sx, sy - 14), IM_COL32(180, 180, 180, 200), buf);
    }

    ImGui::InvisibleButton("simcanvas", canvas_size);

    // v1.2.3: Entity tooltip on hover
    if (ImGui::IsItemHovered()) {
        ImVec2 mouse = ImGui::GetMousePos();
        const UnifiedAppState::EntityVis* closest = nullptr;
        float closest_dist_sq = 15.0f * 15.0f;  // 15px pick radius

        for (const auto& ent : state_.entity_vis) {
            if (!ent.alive) continue;
            ImVec2 sp = world_to_screen(ent.x, ent.y);
            float dx = mouse.x - sp.x;
            float dy = mouse.y - sp.y;
            float dsq = dx * dx + dy * dy;
            if (dsq < closest_dist_sq) {
                closest_dist_sq = dsq;
                closest = &ent;
            }
        }

        if (closest) {
            ImGui::BeginTooltip();
            ImGui::Text("%s  [%s]", closest->name.c_str(), closest->side.c_str());
            ImGui::Separator();
            ImGui::Text("Health:    %.0f%%", closest->health * 100.0);
            ImGui::Text("Cohesion:  %.0f%%", closest->cohesion * 100.0);
            ImGui::Text("Fatigue:   %.0f%%", closest->fatigue * 100.0);
            ImGui::Text("Supply:    %.0f%%", closest->supply * 100.0);
            ImGui::Text("Morale:    %.0f%%", closest->morale * 100.0);
            if (closest->suppression > 0.01)
                ImGui::Text("Suppress:  %.0f%%", closest->suppression * 100.0);
            ImGui::Separator();
            ImGui::Text("Move:      %s", closest->movement_type);
            ImGui::Text("Range:     %.0f m", closest->engagement_range);
            ImGui::Text("Heading:   %.0f deg",
                closest->heading * (180.0 / 3.14159265358979323846));
            if (closest->is_indirect)
                ImGui::Text("Type:      Indirect Fire");
            if (closest->broken)
                ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "** BROKEN **");
            ImGui::EndTooltip();
        }

        // Middle-drag to pan
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            f64 inv_scale = 1.0 / scale;
            state_.sim_control.bounds_min_x -= delta.x * inv_scale;
            state_.sim_control.bounds_max_x -= delta.x * inv_scale;
            state_.sim_control.bounds_min_y += delta.y * inv_scale;
            state_.sim_control.bounds_max_y += delta.y * inv_scale;
        }

        // Scroll to zoom
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            f64 factor = (wheel > 0) ? 0.85 : 1.18;
            f64 cx = (state_.sim_control.bounds_min_x + state_.sim_control.bounds_max_x) * 0.5;
            f64 cy = (state_.sim_control.bounds_min_y + state_.sim_control.bounds_max_y) * 0.5;
            f64 hw = (state_.sim_control.bounds_max_x - state_.sim_control.bounds_min_x) * 0.5 * factor;
            f64 hh = (state_.sim_control.bounds_max_y - state_.sim_control.bounds_min_y) * 0.5 * factor;
            state_.sim_control.bounds_min_x = cx - hw;
            state_.sim_control.bounds_max_x = cx + hw;
            state_.sim_control.bounds_min_y = cy - hh;
            state_.sim_control.bounds_max_y = cy + hh;
        }
    }
}

// =============================================================================
// Analysis View
// =============================================================================

void UnifiedApp::render_analysis_view() {
    // Two sections: current MC results + comparative table

    // ---- Current MC Results -------------------------------------------------
    if (!state_.analysis.has_data && state_.variant_results.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));
        ImGui::Text("DECISION SUPPORT WORKFLOW");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::TextWrapped(
            "ATHENA evaluates Courses of Action (COAs) through Monte Carlo analysis "
            "and recommends the option with the highest probability of mission success.\n");
        ImGui::Spacing();
        ImGui::TextDisabled("Step 1: Design COA 'A' in the Scenario Editor (place forces, assign platforms)");
        ImGui::TextDisabled("Step 2: Run Monte Carlo analysis (100-1000 iterations)");
        ImGui::TextDisabled("Step 3: Save result as variant 'Plan A'");
        ImGui::TextDisabled("Step 4: Modify scenario for COA 'B' (reposition, add/remove forces)");
        ImGui::TextDisabled("Step 5: Run Monte Carlo again, save as 'Plan B'");
        ImGui::TextDisabled("Step 6: ATHENA compares variants and recommends the best COA");
        ImGui::Spacing();
        bool has_scenario = !state_.scenario.scenario.actors.empty();
        if (has_scenario) {
            if (ImGui::Button("Run Monte Carlo Now", ImVec2(200, 35))) {
                state_.show_run_simulation_dialog = true;
            }
        } else {
            ImGui::TextDisabled("Load or create a scenario first.");
        }
        return;
    }

    // ---- MC Running Progress ------------------------------------------------
    if (state_.mc_control.is_running) {
        ImGui::BeginChild("MCProgress", ImVec2(0, 100), true);
        ImGui::Text("Monte Carlo running...");
        u32 done = state_.mc_control.completed_iterations;
        u32 total_iter = state_.mc_control.num_iterations;
        f64 progress = (total_iter > 0)
            ? static_cast<f64>(done) / total_iter : 0.0;
        ImGui::ProgressBar(static_cast<float>(progress), ImVec2(-1, 25));

        // ETA computation
        f64 now_ms = std::chrono::duration<f64, std::milli>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        f64 elapsed_ms = now_ms - state_.mc_control.start_time_ms;
        if (done > 10 && progress > 0.01) {
            f64 total_est_ms = elapsed_ms / progress;
            f64 remaining_ms = total_est_ms - elapsed_ms;
            f64 remaining_s = remaining_ms / 1000.0;
            if (remaining_s < 60.0) {
                ImGui::Text("%u / %u simulations  |  ~%.0fs remaining",
                            done, total_iter, remaining_s);
            } else {
                ImGui::Text("%u / %u simulations  |  ~%.1f min remaining",
                            done, total_iter, remaining_s / 60.0);
            }
        } else {
            ImGui::Text("%u / %u simulations  |  estimating...", done, total_iter);
        }

        // Cancel button
        ImGui::Spacing();
        if (ImGui::Button("Cancel Analysis", ImVec2(140, 0))) {
            cancel_monte_carlo();
        }

        ImGui::EndChild();
        ImGui::Spacing();
    }

    if (state_.analysis.has_data) {
        const auto& stats = state_.analysis.stats;
        f64 total = std::max(stats.completed_iterations, 1u);
        f64 blue_pct = 100.0 * stats.blue_wins / total;
        f64 red_pct  = 100.0 * stats.red_wins  / total;
        f64 draw_pct = 100.0 * stats.draws      / total;
        f64 tick_sec = state_.scenario.scenario.temporal.tick_duration_seconds;
        if (tick_sec <= 0) tick_sec = 3600.0;
        f64 max_hours = state_.scenario.scenario.temporal.max_ticks * tick_sec / 3600.0;
        f64 mean_hours = stats.ticks_to_completion.mean * tick_sec / 3600.0;

        // Count actors per side in current scenario
        int n_blue = 0, n_red = 0, n_other = 0;
        for (const auto& a : state_.scenario.scenario.actors) {
            if (a.side == "blue") n_blue++;
            else if (a.side == "red") n_red++;
            else n_other++;
        }

        // =================================================================
        // ASSESSMENT BOX (colored background)
        // =================================================================
        const char* assessment_title;
        const char* assessment_detail;
        ImVec4 box_bg;

        if (blue_pct >= 70.0) {
            assessment_title = "FAVORABLE";
            assessment_detail = "High probability of success. Current force structure adequate.";
            box_bg = ImVec4(0.05f, 0.18f, 0.05f, 1.0f);
        } else if (blue_pct >= 50.0) {
            assessment_title = "CONTESTED";
            assessment_detail = "Marginal advantage. Consider reinforcement or repositioning.";
            box_bg = ImVec4(0.15f, 0.14f, 0.02f, 1.0f);
        } else if (blue_pct >= 30.0) {
            assessment_title = "UNFAVORABLE";
            assessment_detail = "Significant risk of failure. Alternative COA recommended.";
            box_bg = ImVec4(0.18f, 0.08f, 0.02f, 1.0f);
        } else {
            assessment_title = "CRITICAL";
            assessment_detail = "Likely defeat. Requires force restructuring or withdrawal.";
            box_bg = ImVec4(0.18f, 0.03f, 0.03f, 1.0f);
        }

        ImGui::PushStyleColor(ImGuiCol_ChildBg, box_bg);
        ImGui::BeginChild("AssessmentBox", ImVec2(0, 90), true);

        // Title with colored badge
        ImVec4 badge_color = (blue_pct >= 70.0) ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f)
                           : (blue_pct >= 50.0) ? ImVec4(1.0f, 0.85f, 0.3f, 1.0f)
                           : (blue_pct >= 30.0) ? ImVec4(1.0f, 0.5f, 0.2f, 1.0f)
                           :                      ImVec4(1.0f, 0.25f, 0.25f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, badge_color);
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("%s", assessment_title);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::SameLine(0, 15);
        ImGui::TextDisabled("(%u simulations%s)", stats.completed_iterations,
                            stats.converged_early ? ", converged" : "");

        ImGui::Text("%s", assessment_detail);
        ImGui::Spacing();

        // Plain language one-liner
        if (blue_pct > red_pct && blue_pct > draw_pct) {
            ImGui::Text("Blue wins in %.0f of %u simulations. Red wins in %.0f. Inconclusive in %.0f.",
                        stats.blue_wins * 1.0, stats.completed_iterations,
                        stats.red_wins * 1.0, stats.draws * 1.0);
        } else if (red_pct > blue_pct && red_pct > draw_pct) {
            ImGui::Text("Red wins in %.0f of %u simulations. Blue wins in %.0f. Inconclusive in %.0f.",
                        stats.red_wins * 1.0, stats.completed_iterations,
                        stats.blue_wins * 1.0, stats.draws * 1.0);
        } else {
            ImGui::Text("No decisive outcome in %u simulations. Blue wins %.0f, Red wins %.0f, Draw %.0f.",
                        stats.completed_iterations, stats.blue_wins * 1.0,
                        stats.red_wins * 1.0, stats.draws * 1.0);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        // =================================================================
        // CONFIDENCE INDICATOR (v1.1.7)
        // =================================================================
        {
            const auto& an = state_.analysis;
            const char* conf_label;
            ImVec4 conf_color;
            switch (an.confidence) {
                case UnifiedAppState::AnalysisState::Confidence::HIGH:
                    conf_label = "HIGH";
                    conf_color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
                    break;
                case UnifiedAppState::AnalysisState::Confidence::MEDIUM:
                    conf_label = "MEDIUM";
                    conf_color = ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
                    break;
                default:
                    conf_label = "LOW";
                    conf_color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                    break;
            }
            ImGui::Text("Result Confidence: ");
            ImGui::SameLine(0, 0);
            ImGui::PushStyleColor(ImGuiCol_Text, conf_color);
            ImGui::Text("%s", conf_label);
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 15);
            ImGui::TextDisabled("%s", an.confidence_text.c_str());
        }

        ImGui::Spacing();

        // =================================================================
        // OUTCOME BAR (stacked horizontal bar: blue | draw | red)
        // =================================================================
        {
            ImVec2 bar_start = ImGui::GetCursorScreenPos();
            float bar_w = ImGui::GetContentRegionAvail().x;
            float bar_h = 30.0f;
            ImDrawList* dl = ImGui::GetWindowDrawList();

            float bw = bar_w * static_cast<float>(blue_pct / 100.0);
            float dw = bar_w * static_cast<float>(draw_pct / 100.0);
            float rw = bar_w - bw - dw;

            // Blue segment
            if (bw > 1) {
                dl->AddRectFilled(
                    bar_start,
                    ImVec2(bar_start.x + bw, bar_start.y + bar_h),
                    IM_COL32(50, 100, 200, 255));
            }
            // Draw segment
            if (dw > 1) {
                dl->AddRectFilled(
                    ImVec2(bar_start.x + bw, bar_start.y),
                    ImVec2(bar_start.x + bw + dw, bar_start.y + bar_h),
                    IM_COL32(100, 100, 100, 255));
            }
            // Red segment
            if (rw > 1) {
                dl->AddRectFilled(
                    ImVec2(bar_start.x + bw + dw, bar_start.y),
                    ImVec2(bar_start.x + bar_w, bar_start.y + bar_h),
                    IM_COL32(200, 50, 50, 255));
            }

            // Labels on bar
            char buf[32];
            if (blue_pct >= 8.0) {
                snprintf(buf, sizeof(buf), "%.0f%%", blue_pct);
                ImVec2 ts = ImGui::CalcTextSize(buf);
                dl->AddText(ImVec2(bar_start.x + bw * 0.5f - ts.x * 0.5f,
                                   bar_start.y + bar_h * 0.5f - ts.y * 0.5f),
                            IM_COL32(255, 255, 255, 255), buf);
            }
            if (draw_pct >= 8.0) {
                snprintf(buf, sizeof(buf), "%.0f%%", draw_pct);
                ImVec2 ts = ImGui::CalcTextSize(buf);
                dl->AddText(ImVec2(bar_start.x + bw + dw * 0.5f - ts.x * 0.5f,
                                   bar_start.y + bar_h * 0.5f - ts.y * 0.5f),
                            IM_COL32(255, 255, 255, 255), buf);
            }
            if (red_pct >= 8.0) {
                snprintf(buf, sizeof(buf), "%.0f%%", red_pct);
                ImVec2 ts = ImGui::CalcTextSize(buf);
                dl->AddText(ImVec2(bar_start.x + bw + dw + rw * 0.5f - ts.x * 0.5f,
                                   bar_start.y + bar_h * 0.5f - ts.y * 0.5f),
                            IM_COL32(255, 255, 255, 255), buf);
            }

            // Border
            dl->AddRect(bar_start, ImVec2(bar_start.x + bar_w, bar_start.y + bar_h),
                        IM_COL32(80, 80, 80, 255));

            ImGui::Dummy(ImVec2(bar_w, bar_h));
        }

        // Legend under bar
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.55f, 1.0f, 1.0f));
        ImGui::Text("Blue Victory");
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 20);
        ImGui::TextDisabled("Inconclusive");
        ImGui::SameLine(0, 20);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::Text("Red Victory");
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // =================================================================
        // FORCE STATUS TABLE
        // =================================================================
        if (ImGui::BeginTable("ForceStatus", 4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Force",     ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Units",     ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("Avg Surviving", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Worst / Best Case", ImGuiTableColumnFlags_WidthFixed, 140);
            ImGui::TableHeadersRow();

            // Blue row
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.55f, 1.0f, 1.0f));
            ImGui::Text("BLUE");
            ImGui::PopStyleColor();
            ImGui::TableNextColumn();
            ImGui::Text("%d", n_blue);
            ImGui::TableNextColumn();
            {
                float frac = static_cast<float>(stats.blue_survival_rate.mean);
                char overlay[32];
                snprintf(overlay, sizeof(overlay), "%.0f%%", frac * 100);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
                ImGui::ProgressBar(frac, ImVec2(-1, 18), overlay);
                ImGui::PopStyleColor();
            }
            ImGui::TableNextColumn();
            ImGui::Text("%.0f%% / %.0f%%",
                        stats.blue_survival_rate.p5 * 100,
                        stats.blue_survival_rate.p95 * 100);

            // Red row
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ImGui::Text("RED");
            ImGui::PopStyleColor();
            ImGui::TableNextColumn();
            ImGui::Text("%d", n_red);
            ImGui::TableNextColumn();
            {
                float frac = static_cast<float>(stats.red_survival_rate.mean);
                char overlay[32];
                snprintf(overlay, sizeof(overlay), "%.0f%%", frac * 100);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::ProgressBar(frac, ImVec2(-1, 18), overlay);
                ImGui::PopStyleColor();
            }
            ImGui::TableNextColumn();
            ImGui::Text("%.0f%% / %.0f%%",
                        stats.red_survival_rate.p5 * 100,
                        stats.red_survival_rate.p95 * 100);

            ImGui::EndTable();
        }

        ImGui::Spacing();

        // =================================================================
        // RESOLUTION TIME
        // =================================================================
        bool resolved_early = (mean_hours < max_hours * 0.95);
        if (resolved_early) {
            ImGui::Text("Avg resolution: %.0fh of %.0fh simulated",
                        mean_hours, max_hours);
        } else {
            ImGui::TextDisabled("No decisive resolution within %.0fh simulation window. "
                                "Consider increasing duration.", max_hours);
        }

        ImGui::Spacing();
        ImGui::Separator();

        // =================================================================
        // SAVE / EXPORT ROW
        // =================================================================
        static char variant_name[128] = "Plan A";
        ImGui::SetNextItemWidth(150);
        ImGui::InputText("##VarName", variant_name, sizeof(variant_name));
        ImGui::SameLine();
        if (ImGui::Button("Save as Variant")) {
            store_variant_result(variant_name, state_.mc_control.master_seed,
                                 stats.completed_iterations, stats);
            int len = static_cast<int>(strlen(variant_name));
            if (len > 0 && variant_name[len - 1] >= 'A' && variant_name[len - 1] < 'Z') {
                variant_name[len - 1]++;
            }
            set_status("Variant saved: " + std::string(variant_name));
        }
        ImGui::SameLine(0, 15);
        if (ImGui::Button("Export JSON"))
            export_results_json("results.json");
        ImGui::SameLine(0, 8);
        if (ImGui::Button("Export PDF")) {
            report::ReportConfig rc;
            rc.scenario_name = state_.scenario.scenario.name;
            rc.version = IntegratedVersion::STRING;
            rc.stats = stats;
            rc.tick_duration_seconds = state_.scenario.scenario.temporal.tick_duration_seconds;
            {
                auto now = std::chrono::system_clock::now();
                auto t = std::chrono::system_clock::to_time_t(now);
                char ts[64];
                std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M", std::localtime(&t));
                rc.timestamp = ts;
            }
            for (const auto& vr : state_.variant_results) {
                report::VariantRow row;
                row.name = vr.name;
                row.iterations = vr.stats.completed_iterations;
                f64 vt = std::max(vr.stats.completed_iterations, 1u);
                row.blue_win_pct = 100.0 * vr.stats.blue_wins / vt;
                row.red_win_pct  = 100.0 * vr.stats.red_wins  / vt;
                row.draw_pct     = 100.0 * vr.stats.draws     / vt;
                row.blue_survival_mean = vr.stats.blue_survival_rate.mean;
                row.red_survival_mean  = vr.stats.red_survival_rate.mean;
                f64 vts = state_.scenario.scenario.temporal.tick_duration_seconds;
                row.avg_duration_hours = vr.stats.ticks_to_completion.mean * vts / 3600.0;
                rc.variants.push_back(row);
            }
            std::string pdf_path = "athena_report.pdf";
            if (report::generate_pdf_report(pdf_path, rc)) {
                set_status("PDF report saved: " + pdf_path);
            } else {
                set_status("PDF export failed", UnifiedAppState::StatusLevel::ERROR);
            }
        }
        ImGui::SameLine(0, 30);
        ImGui::TextDisabled("%.1f ms/iter | %u runs",
                            stats.iteration_time_ms.mean, stats.completed_iterations);
    }

    // ---- Histogram ----------------------------------------------------------
    if (state_.analysis.has_data) {
        render_analysis_charts();
    }

    // ---- Outcome Analysis (v1.1.7) ------------------------------------------
    if (state_.analysis.has_data) {
        render_outcome_analysis();
    }

    // ---- Comparative Table --------------------------------------------------
    render_comparative_view();
}

// =============================================================================
// Analysis Charts
// =============================================================================

void UnifiedApp::render_analysis_charts() {
    ImGui::BeginChild("Charts", ImVec2(0, 210), true);

    // Build histograms (10 bins, easier to read)
    constexpr int NBINS = 10;
    float blue_hist[NBINS] = {};
    float red_hist[NBINS]  = {};

    for (const auto& result : state_.analysis.results) {
        f64 b_surv = (result.blue_initial > 0)
                     ? static_cast<f64>(result.blue_surviving) / result.blue_initial
                     : 0.0;
        f64 r_surv = (result.red_initial > 0)
                     ? static_cast<f64>(result.red_surviving) / result.red_initial
                     : 0.0;
        int b_bin = std::clamp(static_cast<int>(b_surv * (NBINS - 0.01)), 0, NBINS - 1);
        int r_bin = std::clamp(static_cast<int>(r_surv * (NBINS - 0.01)), 0, NBINS - 1);
        blue_hist[b_bin] += 1.0f;
        red_hist[r_bin]  += 1.0f;
    }

    float b_max = *std::max_element(blue_hist, blue_hist + NBINS);
    float r_max = *std::max_element(red_hist, red_hist + NBINS);
    if (b_max > 0) { for (auto& v : blue_hist) v /= b_max; }
    if (r_max > 0) { for (auto& v : red_hist) v /= r_max; }

    // Side by side
    float half_w = ImGui::GetContentRegionAvail().x * 0.5f - 8;

    // Blue histogram
    ImGui::Text("Blue Force Survival Distribution");
    ImGui::SameLine(half_w + 16);
    ImGui::Text("Red Force Survival Distribution");

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
    ImGui::PlotHistogram("##BlueHist", blue_hist, NBINS, 0, nullptr,
                         0.0f, 1.0f, ImVec2(half_w, 100));
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 16);

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PlotHistogram("##RedHist", red_hist, NBINS, 0, nullptr,
                         0.0f, 1.0f, ImVec2(half_w, 100));
    ImGui::PopStyleColor();

    // Axis labels
    ImGui::TextDisabled("0%%    Total loss                 All survive    100%%");
    ImGui::SameLine(half_w + 16);
    ImGui::TextDisabled("0%%    Total loss                 All survive    100%%");

    ImGui::EndChild();
}

// =============================================================================
// Outcome Analysis Panel (v1.1.7)
// =============================================================================

void UnifiedApp::render_outcome_analysis() {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));
    ImGui::Text("OUTCOME ANALYSIS");
    ImGui::PopStyleColor();
    ImGui::Separator();

    const auto& a = state_.analysis;
    const auto& s = a.stats;
    f64 total = std::max(s.completed_iterations, 1u);
    f64 tick_sec = state_.scenario.scenario.temporal.tick_duration_seconds;
    if (tick_sec <= 0.0) tick_sec = 3600.0;

    // Average time to decisive outcome and first contact
    {
        f64 sum_decisive = 0.0, sum_contact = 0.0;
        u32 n_decisive = 0, n_contact = 0;
        for (const auto& r : a.results) {
            if (!r.completed_normally) continue;
            if (r.time_to_decisive > 0) {
                sum_decisive += r.time_to_decisive * tick_sec / 3600.0;
                n_decisive++;
            }
            if (r.time_to_first_casualty > 0) {
                sum_contact += r.time_to_first_casualty * tick_sec / 3600.0;
                n_contact++;
            }
        }
        if (n_contact > 0) {
            ImGui::Text("Average time to first casualty: %.1f hours", sum_contact / n_contact);
        }
        if (n_decisive > 0) {
            ImGui::Text("Average time to decisive outcome: %.1f hours", sum_decisive / n_decisive);
        }
        f64 mean_hours = s.ticks_to_completion.mean * tick_sec / 3600.0;
        ImGui::Text("Average simulation duration: %.1f hours", mean_hours);
    }

    // Sensitivity summary (plain language)
    if (!a.sensitivity_summary.empty()) {
        ImGui::Spacing();
        for (const auto& line : a.sensitivity_summary) {
            ImGui::TextWrapped("%s", line.c_str());
        }
    }

    // Sensitivity factors bar chart
    if (!s.sensitivity_factors.empty()) {
        ImGui::Spacing();
        ImGui::Text("Key Factors (impact on outcome):");
        for (const auto& [name, impact] : s.sensitivity_factors) {
            char overlay[64];
            snprintf(overlay, sizeof(overlay), "%s (%.0f%%)", name.c_str(), impact * 100);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.6f, 0.5f, 0.2f, 1.0f));
            ImGui::ProgressBar(static_cast<float>(impact), ImVec2(-1, 18), overlay);
            ImGui::PopStyleColor();
        }
    }

    // Termination reason breakdown (if any non-decisive stops occurred)
    if (s.term_no_offensive > 0 || s.term_mutual_blind > 0 || 
        s.term_unreachable > 0 || s.term_max_ticks > 0) {
        ImGui::Spacing();
        ImGui::Text("How simulations ended:");
        if (ImGui::BeginTable("TermReasons", 2, ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Reason", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 80);

            auto row = [&](const char* label, u32 count) {
                if (count == 0) return;
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%s", label);
                ImGui::TableNextColumn(); ImGui::Text("%u (%.0f%%)", count, 100.0 * count / total);
            };

            row("Decisive victory", s.term_decisive);
            row("Time limit reached", s.term_max_ticks);
            row("No offensive capacity", s.term_no_offensive);
            row("Mutual detection failure", s.term_mutual_blind);
            row("Forces unreachable", s.term_unreachable);

            ImGui::EndTable();
        }
    }

    ImGui::Spacing();
}

// =============================================================================
// Comparative Decision View — THE CORE VALUE
// =============================================================================

void UnifiedApp::render_comparative_view() {
    if (state_.variant_results.empty()) return;

    const auto& variants = state_.variant_results;
    const int nv = static_cast<int>(variants.size());

    ImGui::BeginChild("Comparative", ImVec2(0, 0), true);

    // =========================================================================
    // HEADER: Decision Support Title
    // =========================================================================
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));
    ImGui::Text("COURSE OF ACTION ANALYSIS");
    ImGui::PopStyleColor();
    ImGui::Text("%d variant%s evaluated", nv, nv > 1 ? "s" : "");
    ImGui::Separator();
    ImGui::Spacing();

    // =========================================================================
    // Compute composite scores for each variant
    // Weighted: Win% (50%) + Force Preservation (30%) + Speed (20%)
    // =========================================================================
    struct COAScore {
        int    idx;
        f64    win_pct;
        f64    loss_pct;
        f64    draw_pct;
        f64    blue_surv_mean;
        f64    red_surv_mean;     // lower = better for blue
        f64    avg_hours;
        f64    blue_surv_worst;   // p5
        f64    blue_surv_best;    // p95
        f64    composite;         // weighted score 0-100
        const char* risk_level;
    };

    std::vector<COAScore> scores(nv);
    f64 max_hours = 1.0;
    for (int i = 0; i < nv; ++i) {
        // ticks_to_completion is in ticks, tick=3600s => hours = ticks
        f64 h = variants[i].stats.ticks_to_completion.mean;
        if (h > max_hours) max_hours = h;
    }

    int best_idx = 0;
    f64 best_composite = -1.0;

    for (int i = 0; i < nv; ++i) {
        auto& s = scores[i];
        const auto& st = variants[i].stats;
        f64 total = std::max(st.completed_iterations, 1u);

        s.idx           = i;
        s.win_pct       = 100.0 * st.blue_wins / total;
        s.loss_pct      = 100.0 * st.red_wins  / total;
        s.draw_pct      = 100.0 * st.draws     / total;
        s.blue_surv_mean = st.blue_survival_rate.mean * 100.0;
        s.red_surv_mean  = st.red_survival_rate.mean * 100.0;
        s.avg_hours      = st.ticks_to_completion.mean;  // ticks = hours at 3600s
        s.blue_surv_worst = st.blue_survival_rate.p5  * 100.0;
        s.blue_surv_best  = st.blue_survival_rate.p95 * 100.0;

        // Composite: Win (50%) + Preservation (30%) + Speed (20%)
        f64 win_score   = s.win_pct;                               // 0-100
        f64 pres_score  = s.blue_surv_mean;                        // 0-100
        f64 speed_score = (max_hours > 0) ? (1.0 - s.avg_hours / max_hours) * 100.0 : 50.0;
        if (speed_score < 0) speed_score = 0;

        s.composite = win_score * 0.50 + pres_score * 0.30 + speed_score * 0.20;

        // Risk classification
        if (s.win_pct >= 70.0 && s.blue_surv_worst >= 30.0)
            s.risk_level = "LOW";
        else if (s.win_pct >= 50.0 && s.blue_surv_worst >= 10.0)
            s.risk_level = "MODERATE";
        else if (s.win_pct >= 30.0)
            s.risk_level = "HIGH";
        else
            s.risk_level = "CRITICAL";

        if (s.composite > best_composite) {
            best_composite = s.composite;
            best_idx = i;
        }
    }

    // =========================================================================
    // RECOMMENDATION BOX (only if 2+ variants)
    // =========================================================================
    if (nv >= 2) {
        const auto& best = scores[best_idx];
        const auto& bv = variants[best_idx];

        // Green box for recommendation
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.15f, 0.05f, 1.0f));
        ImGui::BeginChild("RecommendBox", ImVec2(0, 120), true);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
        ImGui::Text(">> RECOMMENDED COA: %s", bv.name.c_str());
        ImGui::PopStyleColor();

        ImGui::Text("Win probability: %.0f%% | Force preservation: %.0f%% | Risk: %s",
                    best.win_pct, best.blue_surv_mean, best.risk_level);

        // Find second best for comparison
        int second_idx = -1;
        f64 second_best = -1.0;
        for (int i = 0; i < nv; ++i) {
            if (i != best_idx && scores[i].composite > second_best) {
                second_best = scores[i].composite;
                second_idx = i;
            }
        }

        if (second_idx >= 0) {
            const auto& alt = scores[second_idx];
            f64 advantage = best.win_pct - alt.win_pct;
            ImGui::Spacing();
            if (advantage > 0) {
                ImGui::Text("Advantage over %s: +%.0f%% win rate, %.0f%% vs %.0f%% force preservation",
                            variants[second_idx].name.c_str(),
                            advantage, best.blue_surv_mean, alt.blue_surv_mean);
            } else {
                ImGui::Text("vs %s: similar win rate (%.0f%% vs %.0f%%), but better composite (%.0f vs %.0f)",
                            variants[second_idx].name.c_str(),
                            best.win_pct, alt.win_pct,
                            best.composite, alt.composite);
            }

            // Tradeoff warning
            if (alt.blue_surv_mean > best.blue_surv_mean + 10.0) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.3f, 1.0f));
                ImGui::Text("NOTE: %s has better force preservation (%.0f%% vs %.0f%%) - "
                            "consider if casualty minimization is the priority.",
                            variants[second_idx].name.c_str(),
                            alt.blue_surv_mean, best.blue_surv_mean);
                ImGui::PopStyleColor();
            }
            if (alt.avg_hours < best.avg_hours * 0.7) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.3f, 1.0f));
                ImGui::Text("NOTE: %s resolves %.0f%% faster (%.0fh vs %.0fh) - "
                            "consider if speed of resolution is critical.",
                            variants[second_idx].name.c_str(),
                            (1.0 - alt.avg_hours / best.avg_hours) * 100.0,
                            alt.avg_hours, best.avg_hours);
                ImGui::PopStyleColor();
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
    // Single variant: just show assessment
    else {
        const auto& s = scores[0];
        ImVec4 box_color;
        if (s.win_pct >= 60.0)
            box_color = ImVec4(0.05f, 0.15f, 0.05f, 1.0f);
        else if (s.win_pct >= 40.0)
            box_color = ImVec4(0.15f, 0.12f, 0.02f, 1.0f);
        else
            box_color = ImVec4(0.15f, 0.05f, 0.05f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, box_color);
        ImGui::BeginChild("AssessBox", ImVec2(0, 80), true);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));
        ImGui::Text("ASSESSMENT: %s", variants[0].name.c_str());
        ImGui::PopStyleColor();

        ImGui::Text("Win probability: %.0f%% | Force preservation: %.0f%% | Risk: %s | Score: %.0f/100",
                    s.win_pct, s.blue_surv_mean, s.risk_level, s.composite);
        ImGui::Text("Save as variant, modify scenario, run again, and compare to evaluate alternatives.");

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // =========================================================================
    // COMPARISON TABLE
    // =========================================================================
    ImGui::Text("Variant Comparison:");
    if (ImGui::BeginTable("VariantTable", 9,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("COA",        ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Score",      ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Win%",       ImGuiTableColumnFlags_WidthFixed, 55);
        ImGui::TableSetupColumn("Loss%",      ImGuiTableColumnFlags_WidthFixed, 55);
        ImGui::TableSetupColumn("Draw%",      ImGuiTableColumnFlags_WidthFixed, 55);
        ImGui::TableSetupColumn("Blue Surv.", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Red Surv.",  ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Avg Time",   ImGuiTableColumnFlags_WidthFixed, 65);
        ImGui::TableSetupColumn("Risk",       ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableHeadersRow();

        for (int i = 0; i < nv; ++i) {
            const auto& s = scores[i];
            ImGui::TableNextRow();

            // Highlight recommended variant
            if (i == best_idx && nv > 1) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
                                       IM_COL32(30, 60, 30, 255));
            }

            ImGui::TableNextColumn();
            ImGui::Text("%s%s", variants[i].name.c_str(),
                        (i == best_idx && nv > 1) ? " *" : "");

            ImGui::TableNextColumn();
            ImGui::Text("%.0f", s.composite);

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
            ImGui::Text("%.1f", s.win_pct);
            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ImGui::Text("%.1f", s.loss_pct);
            ImGui::PopStyleColor();

            ImGui::TableNextColumn();
            ImGui::Text("%.1f", s.draw_pct);

            ImGui::TableNextColumn();
            ImGui::Text("%.0f%% [%.0f-%.0f]", s.blue_surv_mean,
                        s.blue_surv_worst, s.blue_surv_best);

            ImGui::TableNextColumn();
            ImGui::Text("%.0f%%", s.red_surv_mean);

            ImGui::TableNextColumn();
            ImGui::Text("%.0fh", s.avg_hours);

            ImGui::TableNextColumn();
            // Color-code risk
            ImVec4 risk_color;
            if (s.risk_level[0] == 'L')      risk_color = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
            else if (s.risk_level[0] == 'M')  risk_color = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
            else if (s.risk_level[0] == 'H')  risk_color = ImVec4(1.0f, 0.5f, 0.2f, 1.0f);
            else                               risk_color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, risk_color);
            ImGui::Text("%s", s.risk_level);
            ImGui::PopStyleColor();
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Score = Win%% x 0.5 + Force Preservation x 0.3 + Speed x 0.2");
    ImGui::TextDisabled("Risk: LOW (>70%% win, >30%% worst-case surv.) | MODERATE | HIGH | CRITICAL (<30%% win)");

    // =========================================================================
    // WORKFLOW GUIDANCE
    // =========================================================================
    if (nv < 2) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("To compare COAs:");
        ImGui::TextDisabled("  1. Save current result as variant");
        ImGui::TextDisabled("  2. Modify scenario (add/remove units, change positions, swap platforms)");
        ImGui::TextDisabled("  3. Run Monte Carlo again");
        ImGui::TextDisabled("  4. Save as another variant");
        ImGui::TextDisabled("  ATHENA will recommend the best course of action.");
    }

    // Clear button
    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Clear All Variants")) {
        state_.variant_results.clear();
    }

    ImGui::EndChild();
}

// =============================================================================
// Dialogs
// =============================================================================

void UnifiedApp::render_dialogs() {
    // =========================================================================
    // About dialog
    // =========================================================================
    if (state_.show_about)
        ImGui::OpenPopup("About ATHENA");

    if (ImGui::BeginPopupModal("About ATHENA", &state_.show_about,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("ATHENA - Advanced Tactical & Heuristic\n"
                    "Engagement & Network Analyzer");
        ImGui::Separator();
        ImGui::Text("Version: %s (%s)",
                    IntegratedVersion::STRING, IntegratedVersion::CODENAME);
        ImGui::Text("Database: %zu platforms", state_.browser.db.count());
        ImGui::Separator();
        ImGui::Text("Copyright (c) 2026 ATHENA Project");

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            state_.show_about = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // =========================================================================
    // Load Scenario dialog
    // =========================================================================
    if (state_.show_load_scenario)
        ImGui::OpenPopup("Open Scenario");

    if (ImGui::BeginPopupModal("Open Scenario", &state_.show_load_scenario,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter scenario file path:");
        ImGui::SetNextItemWidth(400);
        ImGui::InputText("##ScenarioPath", state_.dialog_path_buf,
                         sizeof(state_.dialog_path_buf));

        ImGui::Spacing();

        // Scan scenarios directory and list available files
        if (!scenarios_dir_.empty()) {
            ImGui::TextDisabled("Scenarios (%s):", scenarios_dir_.c_str());
            DIR* d = opendir(scenarios_dir_.c_str());
            if (d) {
                struct dirent* ent;
                int found = 0;
                while ((ent = readdir(d)) != nullptr) {
                    std::string fname = ent->d_name;
                    if (fname.size() > 5 &&
                        fname.substr(fname.size() - 5) == ".json") {
                        if (ImGui::SmallButton(fname.c_str())) {
                            std::string full = scenarios_dir_ + "/" + fname;
                            snprintf(state_.dialog_path_buf,
                                     sizeof(state_.dialog_path_buf),
                                     "%s", full.c_str());
                        }
                        if (++found % 3 != 0) ImGui::SameLine();
                    }
                }
                closedir(d);
                if (found > 0) ImGui::NewLine();
            }
        }

        // Quick-access buttons for bundled example scenarios
        ImGui::TextDisabled("Built-in examples:");
        if (ImGui::SmallButton("test-scenario.json")) {
            snprintf(state_.dialog_path_buf, sizeof(state_.dialog_path_buf),
                     "examples/test-scenario.json");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("baltics-2025.json")) {
            snprintf(state_.dialog_path_buf, sizeof(state_.dialog_path_buf),
                     "examples/baltics-2025.json");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Load", ImVec2(120, 0))) {
            if (load_scenario(state_.dialog_path_buf)) {
                state_.status_message = "Scenario loaded: " +
                    state_.scenario.scenario.name;
                state_.status_level = UnifiedAppState::StatusLevel::INFO;
                state_.active_view = UnifiedAppState::View::SCENARIO;
            } else {
                state_.status_message = "Failed to load: " +
                    std::string(state_.dialog_path_buf);
                state_.status_level = UnifiedAppState::StatusLevel::ERROR;
            }
            state_.show_load_scenario = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            state_.show_load_scenario = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // =========================================================================
    // Save Scenario As dialog
    // =========================================================================
    if (state_.show_save_scenario) {
        // Pre-fill path suggestion if empty
        if (state_.dialog_path_buf[0] == '\0') {
            if (!state_.scenario.scenario_path.empty()) {
                snprintf(state_.dialog_path_buf, sizeof(state_.dialog_path_buf),
                         "%s", state_.scenario.scenario_path.c_str());
            } else {
                // Default: save to scenarios directory
                std::string default_name = state_.scenario.scenario.name;
                for (auto& c : default_name) {
                    if (c == ' ') c = '-';
                    else c = static_cast<char>(::tolower(c));
                }
                if (default_name.empty()) default_name = "my-scenario";
                snprintf(state_.dialog_path_buf, sizeof(state_.dialog_path_buf),
                         "%s/%s.json", scenarios_dir_.c_str(),
                         default_name.c_str());
            }
        }
        ImGui::OpenPopup("Save Scenario As");
    }

    if (ImGui::BeginPopupModal("Save Scenario As", &state_.show_save_scenario,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Save scenario file to:");
        ImGui::SetNextItemWidth(400);
        ImGui::InputText("##SavePath", state_.dialog_path_buf,
                         sizeof(state_.dialog_path_buf));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            if (save_scenario(state_.dialog_path_buf)) {
                state_.status_message = "Scenario saved: " +
                    std::string(state_.dialog_path_buf);
                state_.status_level = UnifiedAppState::StatusLevel::INFO;
            } else {
                state_.status_message = "Failed to save: " +
                    std::string(state_.dialog_path_buf);
                state_.status_level = UnifiedAppState::StatusLevel::ERROR;
            }
            state_.show_save_scenario = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            state_.show_save_scenario = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // =========================================================================
    // New Scenario confirmation
    // =========================================================================
    if (state_.show_new_scenario)
        ImGui::OpenPopup("New Scenario");

    if (ImGui::BeginPopupModal("New Scenario", &state_.show_new_scenario,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create a new empty scenario?");
        if (state_.scenario.is_modified)
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                               "Current scenario has unsaved changes.");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            state_.scenario.scenario = Scenario();
            state_.scenario.scenario.name = "New Scenario";
            state_.scenario.scenario.id = "custom-scenario";
            state_.scenario.scenario.version = "1.0.0";
            state_.scenario.scenario.scale = ScenarioScale::MESO;
            state_.scenario.scenario.temporal.tick_duration_seconds = 3600.0;
            state_.scenario.scenario.temporal.max_ticks = 72;
            state_.scenario.scenario.spatial_bounds = {54.0, 56.0, 20.0, 22.0};
            state_.scenario.scenario.id   = "custom-scenario";
            state_.scenario.scenario.version = "1.0.0";
            state_.scenario.scenario.temporal.tick_duration_seconds = 3600;
            state_.scenario.scenario.temporal.max_ticks = 72;
            state_.scenario.scenario.spatial_bounds = {54.0, 56.0, 20.0, 22.0};
            state_.scenario.scenario_path.clear();
            state_.scenario.is_new = true;
            state_.scenario.is_modified = false;
            state_.scenario.selected_actor_idx = -1;
            state_.sim_control.is_loaded = true;
            auto_center_scenario_map();
            state_.status_message = "New scenario created - add units in Scenario Editor";
            state_.status_level = UnifiedAppState::StatusLevel::INFO;
            state_.active_view = UnifiedAppState::View::SCENARIO;
            state_.show_new_scenario = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            state_.show_new_scenario = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // =========================================================================
    // Run Monte Carlo dialog
    // =========================================================================
    if (state_.show_run_simulation_dialog) {
        validate_scenario();  // v1.1.7: Check scenario before showing dialog
        ImGui::OpenPopup("Run Monte Carlo");
    }

    if (ImGui::BeginPopupModal("Run Monte Carlo", &state_.show_run_simulation_dialog,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        bool has_scenario = !state_.scenario.scenario.actors.empty();

        if (!has_scenario) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                               "No scenario loaded. Load a scenario first.");
            ImGui::Spacing();
        } else {
            ImGui::Text("Scenario: %s", state_.scenario.scenario.name.c_str());
            ImGui::Text("Units: %zu", state_.scenario.scenario.actors.size());

            f64 total_hours = state_.scenario.scenario.temporal.tick_duration_seconds
                            * state_.scenario.scenario.temporal.max_ticks / 3600.0;
            ImGui::Text("Duration: %.0f hours (%.1f days)", total_hours, total_hours / 24.0);
            
            // v1.1.7: Scenario validation warnings
            if (!state_.validation_warnings.empty()) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.12f, 0.02f, 1.0f));
                float warn_h = 40.0f + 20.0f * state_.validation_warnings.size();
                ImGui::BeginChild("Warnings", ImVec2(0, warn_h), true);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));
                ImGui::Text("Scenario Warnings:");
                ImGui::PopStyleColor();
                for (const auto& w : state_.validation_warnings) {
                    ImVec4 col = w.is_critical 
                        ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) 
                        : ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, col);
                    ImGui::BulletText("%s", w.message.c_str());
                    ImGui::PopStyleColor();
                }
                ImGui::TextDisabled("Results may be unreliable. You can still run.");
                ImGui::EndChild();
                ImGui::PopStyleColor();
            }
            
            ImGui::Separator();
            ImGui::Spacing();

            // ---- Mode selection ----
            ImGui::Text("Analysis Mode:");
            ImGui::Spacing();

            static int mc_mode = 1;  // 0=Quick, 1=Standard, 2=Custom

            ImGui::PushStyleColor(ImGuiCol_Button,
                mc_mode == 0 ? ImVec4(0.2f, 0.5f, 0.2f, 1.0f) : ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
            if (ImGui::Button("Quick\n~10k", ImVec2(100, 45))) {
                mc_mode = 0;
                state_.dialog_mc_iterations = 10000;
            }
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button,
                mc_mode == 1 ? ImVec4(0.2f, 0.5f, 0.2f, 1.0f) : ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
            if (ImGui::Button("Standard\n~100k", ImVec2(100, 45))) {
                mc_mode = 1;
                state_.dialog_mc_iterations = 100000;
            }
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button,
                mc_mode == 2 ? ImVec4(0.2f, 0.5f, 0.2f, 1.0f) : ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
            if (ImGui::Button("Custom\n...", ImVec2(100, 45))) {
                mc_mode = 2;
            }
            ImGui::PopStyleColor();

            if (mc_mode == 2) {
                ImGui::Spacing();
                ImGui::Text("Simulations:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(150);
                ImGui::InputInt("##MCIter", &state_.dialog_mc_iterations);
                state_.dialog_mc_iterations = std::clamp(state_.dialog_mc_iterations, 100, 1000000);
            }

            ImGui::Spacing();

            // ---- Advanced / Reproducibility (collapsed by default) ----
            static bool use_manual_seed = false;
            static int manual_seed = 42;
            if (ImGui::TreeNode("Advanced: Reproducibility")) {
                ImGui::Checkbox("Use fixed seed", &use_manual_seed);
                if (use_manual_seed) {
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(120);
                    ImGui::InputInt("##ManualSeed", &manual_seed);
                }
                ImGui::TextDisabled("Fixed seed guarantees identical results for same scenario + build.");
                ImGui::TreePop();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Run Analysis", ImVec2(140, 35))) {
                Seed run_seed;
                if (use_manual_seed) {
                    run_seed = static_cast<Seed>(manual_seed);
                } else {
                    run_seed = static_cast<Seed>(
                        std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFFFFFF);
                }

                configure_monte_carlo(
                    static_cast<u32>(state_.dialog_mc_iterations),
                    run_seed,
                    false);
                run_monte_carlo();
                state_.active_view = UnifiedAppState::View::ANALYSIS;
                state_.show_run_simulation_dialog = false;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 35))) {
            state_.show_run_simulation_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // =========================================================================
    // Export Results dialog
    // =========================================================================
    if (state_.show_export_results)
        ImGui::OpenPopup("Export Results");

    if (ImGui::BeginPopupModal("Export Results", &state_.show_export_results,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Export analysis results to file:");
        ImGui::SetNextItemWidth(400);
        ImGui::InputText("##ExportPath", state_.dialog_path_buf,
                         sizeof(state_.dialog_path_buf));

        ImGui::Text("Format:");
        ImGui::RadioButton("JSON", &state_.dialog_export_format, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Binary", &state_.dialog_export_format, 1);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Export", ImVec2(120, 0))) {
            bool ok;
            if (state_.dialog_export_format == 0)
                ok = export_results_json(state_.dialog_path_buf);
            else
                ok = export_results_binary(state_.dialog_path_buf);

            if (ok) {
                state_.status_message = "Exported to: " +
                    std::string(state_.dialog_path_buf);
                state_.status_level = UnifiedAppState::StatusLevel::INFO;
            } else {
                state_.status_message = "Export failed: " +
                    std::string(state_.dialog_path_buf);
                state_.status_level = UnifiedAppState::StatusLevel::ERROR;
            }
            state_.show_export_results = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            state_.show_export_results = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // =========================================================================
    // Settings dialog
    // =========================================================================
    if (state_.show_settings)
        ImGui::OpenPopup("Settings");

    if (ImGui::BeginPopupModal("Settings", &state_.show_settings,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Simulation Defaults");
        ImGui::Separator();

        // Duration in hours (1 tick = 1 hour by default)
        int duration_h = static_cast<int>(state_.sim_control.max_ticks);
        ImGui::Text("Default Duration:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputInt("##DefDuration", &duration_h))
            state_.sim_control.max_ticks = static_cast<u32>(
                std::clamp(duration_h, 1, 10000));
        ImGui::SameLine();
        ImGui::TextDisabled("hours");

        ImGui::Spacing();
        ImGui::Text("Map View");
        ImGui::Separator();
        ImGui::Checkbox("Show grid", &state_.scenario.show_grid);

        float span_lat = static_cast<float>(state_.scenario.map_span_lat);
        ImGui::Text("Map span (degrees lat):");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        if (ImGui::SliderFloat("##SpanLat", &span_lat, 0.01f, 20.0f, "%.2f"))
            state_.scenario.map_span_lat = static_cast<f64>(span_lat);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            state_.show_settings = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // =========================================================================
    // Force Builder dialog — deploy N units of the same platform in formation
    // =========================================================================
    if (state_.show_force_builder)
        ImGui::OpenPopup("Deploy Formation");

    if (ImGui::BeginPopupModal("Deploy Formation", &state_.show_force_builder,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        auto& fb = state_.force_builder;
        auto& sc = state_.scenario;
        auto& scenario = sc.scenario;

        ImGui::Text("Deploy multiple units of the same platform in formation.");
        ImGui::Separator();

        // Side selection
        ImGui::Text("Side:");
        ImGui::SameLine(100);
        ImGui::SetNextItemWidth(200);
        const char* side_display[5];
        for (int si = 0; si < 5; ++si)
            side_display[si] = state_.side_display_names[si].c_str();
        ImGui::Combo("##FBSide", &fb.side_idx, side_display, 5);

        // Platform search
        ImGui::Text("Platform:");
        ImGui::SameLine(100);
        ImGui::SetNextItemWidth(300);
        bool fb_search_changed = ImGui::InputText("##FBPlatSearch",
            fb.platform_search, sizeof(fb.platform_search));

        if (fb_search_changed && fb.platform_search[0] != '\0') {
            fb.search_results.clear();
            std::string query = fb.platform_search;
            std::transform(query.begin(), query.end(), query.begin(), ::tolower);
            for (const auto& [id, spec] : state_.browser.db.all()) {
                std::string haystack = spec.id + " " + spec.name + " " +
                                       spec.type + " " + spec.country_of_origin;
                std::transform(haystack.begin(), haystack.end(),
                               haystack.begin(), ::tolower);
                if (haystack.find(query) != std::string::npos) {
                    fb.search_results.push_back(&spec);
                    if (fb.search_results.size() >= 15) break;
                }
            }
        }
        if (fb.platform_search[0] == '\0') fb.search_results.clear();

        if (!fb.search_results.empty()) {
            ImGui::BeginChild("##FBPlatResults", ImVec2(400, 120), true);
            for (const auto* spec : fb.search_results) {
                char label[256];
                snprintf(label, sizeof(label), "%s [%s] (%s)",
                         spec->name.c_str(), spec->id.c_str(),
                         spec->country_of_origin.c_str());
                if (ImGui::Selectable(label, fb.selected_platform == spec)) {
                    fb.selected_platform = spec;
                    // Auto-calculate spacing from platform type
                    if (fb.spacing_auto) {
                        const char* inferred = infer_unit_type_str(
                            spec->category, spec->type);
                        fb.spacing_m = static_cast<float>(
                            default_formation_spacing(inferred));
                    }
                }
            }
            ImGui::EndChild();
        }

        if (fb.selected_platform) {
            const char* inferred_type = infer_unit_type_str(
                fb.selected_platform->category, fb.selected_platform->type);
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f),
                "Selected: %s", fb.selected_platform->name.c_str());
            ImGui::TextDisabled("Auto-type: %s | FP: %.1f | Speed: %.0f km/h",
                inferred_type,
                fb.selected_platform->firepower_rating,
                fb.selected_platform->mobility.max_speed_kmh);
        }

        ImGui::Separator();

        // Count
        ImGui::Text("Count:");
        ImGui::SameLine(100);
        ImGui::SetNextItemWidth(120);
        ImGui::SliderInt("##FBCount", &fb.count, 1, 20);

        // Formation
        ImGui::Text("Formation:");
        ImGui::SameLine(100);
        ImGui::SetNextItemWidth(200);
        const char* formations[] = {"Line (E-W)", "Column (N-S)", "Wedge (V)",
                                     "Spread (grid)"};
        ImGui::Combo("##FBFormation", &fb.formation, formations, 4);

        // Spacing
        ImGui::Text("Spacing:");
        ImGui::SameLine(100);
        ImGui::Checkbox("Auto", &fb.spacing_auto);
        if (!fb.spacing_auto) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            ImGui::SliderFloat("##FBSpacing", &fb.spacing_m, 10.0f, 10000.0f,
                               "%.0f m", ImGuiSliderFlags_Logarithmic);
        } else {
            ImGui::SameLine();
            ImGui::TextDisabled("%.0f m", fb.spacing_m);
        }

        // Deploy position info
        ImGui::Separator();
        ImGui::TextDisabled("Deploys at map center: %.4f, %.4f",
                           sc.map_center_lat, sc.map_center_lon);

        // Preview formation extents
        f64 total_extent_m = fb.spacing_m * (fb.count - 1);
        if (fb.formation == 3) {  // Grid
            int cols = static_cast<int>(std::ceil(std::sqrt(fb.count)));
            total_extent_m = fb.spacing_m * (cols - 1);
        }
        ImGui::TextDisabled("Formation extent: ~%.0f m", total_extent_m);

        ImGui::Separator();

        // Deploy button
        bool can_deploy = fb.selected_platform != nullptr && fb.count > 0;
        if (!can_deploy) ImGui::BeginDisabled();

        if (ImGui::Button("Deploy", ImVec2(120, 0)) && can_deploy) {
            const char* side_vals[] = {"blue", "red", "green", "orange", "yellow"};
            const char* side_str = side_vals[fb.side_idx];
            const char* inferred_type = infer_unit_type_str(
                fb.selected_platform->category, fb.selected_platform->type);

            // Spacing in degrees (approximate at map center latitude)
            constexpr f64 DEG_PER_METER_LAT = 1.0 / 111320.0;
            f64 cos_lat = std::cos(sc.map_center_lat * 3.14159265358979 / 180.0);
            f64 deg_per_meter_lon = (cos_lat > 0.001)
                ? (1.0 / (111320.0 * cos_lat)) : DEG_PER_METER_LAT;
            f64 spacing_lat = fb.spacing_m * DEG_PER_METER_LAT;
            f64 spacing_lon = fb.spacing_m * deg_per_meter_lon;

            // Monotonic counter for unique IDs
            static int s_fb_counter = 1000;

            for (int i = 0; i < fb.count; ++i) {
                f64 offset_lat = 0.0, offset_lon = 0.0;
                f64 half = (fb.count - 1) * 0.5;

                switch (fb.formation) {
                    case 0: // Line (E-W): spread along longitude
                        offset_lon = (i - half) * spacing_lon;
                        break;
                    case 1: // Column (N-S): spread along latitude
                        offset_lat = (half - i) * spacing_lat;
                        break;
                    case 2: { // Wedge (V shape)
                        if (fb.count == 1) break;
                        // Leader at front (north), wings spread back
                        if (i == 0) {
                            offset_lat = half * spacing_lat * 0.5;
                        } else {
                            int wing = ((i - 1) % 2 == 0) ? -1 : 1;  // alternate
                            int rank = (i + 1) / 2;
                            offset_lat = (half * 0.5 - rank) * spacing_lat;
                            offset_lon = wing * rank * spacing_lon;
                        }
                        break;
                    }
                    case 3: { // Spread (grid)
                        int cols = static_cast<int>(
                            std::ceil(std::sqrt(static_cast<f64>(fb.count))));
                        int row = i / cols;
                        int col = i % cols;
                        f64 half_c = (cols - 1) * 0.5;
                        int rows_total = (fb.count + cols - 1) / cols;
                        f64 half_r = (rows_total - 1) * 0.5;
                        offset_lon = (col - half_c) * spacing_lon;
                        offset_lat = (half_r - row) * spacing_lat;
                        break;
                    }
                }

                ActorDefinition actor;
                actor.id = std::string(side_str) + "-fb-" +
                           std::to_string(++s_fb_counter);
                // Name: "PlatformName #1" etc.
                actor.name = fb.selected_platform->name + " #" +
                             std::to_string(i + 1);
                actor.type = inferred_type;
                actor.side = side_str;
                actor.platform_id = fb.selected_platform->id;
                actor.initial_position = {
                    sc.map_center_lat + offset_lat,
                    sc.map_center_lon + offset_lon,
                    0.0
                };
                actor.initial_health = 1.0;
                actor.initial_supply = 1.0;
                actor.initial_morale = 0.9;
                actor.initial_readiness = 1.0;

                scenario.actors.push_back(actor);
            }

            sc.selected_actor_idx = static_cast<int>(scenario.actors.size()) - 1;
            sc.is_modified = true;

            state_.status_message = "Deployed " + std::to_string(fb.count) +
                "x " + fb.selected_platform->name + " (" + side_str + ")";
            state_.status_level = UnifiedAppState::StatusLevel::INFO;

            state_.show_force_builder = false;
            ImGui::CloseCurrentPopup();
        }

        if (!can_deploy) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            state_.show_force_builder = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // =========================================================================
    // Demo window (ImGui built-in)
    // =========================================================================
    if (state_.show_demo)
        ImGui::ShowDemoWindow(&state_.show_demo);
}

// =============================================================================
// Status Bar
// =============================================================================

void UnifiedApp::render_status_bar() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x,
                                    vp->Pos.y + vp->Size.y - 25));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, 25));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollWithMouse |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));

    if (ImGui::Begin("##StatusBar", nullptr, flags)) {
        if (!state_.status_message.empty()) {
            ImVec4 color;
            switch (state_.status_level) {
                case UnifiedAppState::StatusLevel::WARNING:
                    color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); break;
                case UnifiedAppState::StatusLevel::ERROR:
                    color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); break;
                default:
                    color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
            }
            ImGui::TextColored(color, "%s", state_.status_message.c_str());
        }

        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 100);
        ImGui::Text("%.1f FPS", state_.fps);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

// =============================================================================
// Main Render Entry Point
// =============================================================================

void UnifiedApp::render() {
    setup_imgui_style();

    // v1.2.3: Global keyboard shortcuts (processed before any ImGui window)
    // Only active when no text input is focused
    if (!ImGui::GetIO().WantTextInput) {
        // F5 = Start simulation
        if (ImGui::IsKeyPressed(ImGuiKey_F5) && !state_.sim_control.is_running)
            start_simulation();
        // F6 = Toggle pause/resume
        if (ImGui::IsKeyPressed(ImGuiKey_F6) && state_.sim_control.is_running) {
            if (state_.sim_control.is_paused) resume_simulation();
            else                               pause_simulation();
        }
        // F7 = Stop
        if (ImGui::IsKeyPressed(ImGuiKey_F7) && state_.sim_control.is_running)
            stop_simulation();
        // F10 = Step forward
        if (ImGui::IsKeyPressed(ImGuiKey_F10) && state_.sim_control.is_loaded &&
            state_.sim_control.is_paused)
            step_simulation(1);
        // Space = Toggle pause/resume (same as F6)
        if (ImGui::IsKeyPressed(ImGuiKey_Space) && state_.sim_control.is_running) {
            if (state_.sim_control.is_paused) resume_simulation();
            else                               pause_simulation();
        }
        // +/= = Speed up, -/_ = Speed down
        if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
            state_.sim_control.time_scale = std::min(10.0, state_.sim_control.time_scale + 0.5);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
            state_.sim_control.time_scale = std::max(0.1, state_.sim_control.time_scale - 0.5);
        }
    }

    render_menu_bar();

    // Main content area
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + 20));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, vp->Size.y - 45));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("##MainContent", nullptr, flags)) {
        if (ImGui::BeginTabBar("ViewTabs")) {
            if (ImGui::BeginTabItem("Platform Browser")) {
                state_.active_view = UnifiedAppState::View::BROWSER;
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Scenario Editor")) {
                state_.active_view = UnifiedAppState::View::SCENARIO;
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Simulation")) {
                state_.active_view = UnifiedAppState::View::SIMULATION;
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Analysis")) {
                state_.active_view = UnifiedAppState::View::ANALYSIS;
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::Separator();

        switch (state_.active_view) {
            case UnifiedAppState::View::BROWSER:    render_browser_view();    break;
            case UnifiedAppState::View::SCENARIO:   render_scenario_view();   break;
            case UnifiedAppState::View::SIMULATION: render_simulation_view(); break;
            case UnifiedAppState::View::ANALYSIS:   render_analysis_view();   break;
        }
    }
    ImGui::End();

    render_dialogs();
    render_status_bar();
}

}  // namespace athena
