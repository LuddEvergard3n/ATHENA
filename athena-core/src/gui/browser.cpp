// ATHENA Platform Browser - Dear ImGui Implementation
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/gui/browser.hpp"
#include "imgui.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <set>

namespace athena::gui {

// =============================================================================
// Category Names
// =============================================================================

static const std::map<PlatformCategory, const char*> CATEGORY_NAMES = {
    {PlatformCategory::UNKNOWN, "All Platforms"},
    {PlatformCategory::TANK, "Tanks"},
    {PlatformCategory::IFV, "IFVs"},
    {PlatformCategory::APC, "APCs"},
    {PlatformCategory::ARTILLERY, "Artillery"},
    {PlatformCategory::MLRS, "MLRS"},
    {PlatformCategory::SAM, "SAM Systems"},
    {PlatformCategory::ATGM, "ATGM"},
    {PlatformCategory::MANPADS, "MANPADS"},
    {PlatformCategory::COUNTER_UAS, "Counter-UAS"},
    {PlatformCategory::AIRCRAFT, "Aircraft"},
    {PlatformCategory::HELICOPTER, "Helicopters"},
    {PlatformCategory::BOMBER, "Bombers"},
    {PlatformCategory::UAV, "UAVs"},
    {PlatformCategory::SHIP, "Ships"},
    {PlatformCategory::SUBMARINE, "Submarines"},
    {PlatformCategory::SMALL_ARMS, "Small Arms"},
    {PlatformCategory::BODY_ARMOR, "Body Armor"},
    {PlatformCategory::OPTICS, "Optics/NV"},
    {PlatformCategory::RADAR, "Radars"},
    {PlatformCategory::EW_SYSTEM, "EW Systems"},
    {PlatformCategory::COMMS, "Communications"},
    {PlatformCategory::ENGINEERING, "Engineering"},
    {PlatformCategory::REGIONAL, "Regional"},
    {PlatformCategory::MISSILE, "Missiles"},
    {PlatformCategory::MUNITION, "Munitions"},
};

// =============================================================================
// BrowserApp Implementation
// =============================================================================

BrowserApp::BrowserApp() = default;
BrowserApp::~BrowserApp() = default;

bool BrowserApp::init(const std::string& data_path) {
    auto result = state_.db.load_all(data_path);
    if (!result.ok()) {
        return false;
    }
    
    build_category_list();
    build_country_list();
    apply_filters();
    setup_style();
    
    return true;
}

void BrowserApp::build_category_list() {
    state_.categories.clear();
    
    // Add "All" first
    state_.categories[PlatformCategory::UNKNOWN] = {"All Platforms", static_cast<int>(state_.db.count())};
    
    // Add categories with platforms
    for (int i = 1; i < static_cast<int>(PlatformCategory::MAX_CATEGORIES); ++i) {
        auto cat = static_cast<PlatformCategory>(i);
        int count = static_cast<int>(state_.db.count_by_category(cat));
        if (count == 0) continue;
        
        auto it = CATEGORY_NAMES.find(cat);
        if (it != CATEGORY_NAMES.end()) {
            state_.categories[cat] = {it->second, count};
        }
    }
}

void BrowserApp::build_country_list() {
    state_.countries.clear();
    std::set<std::string> country_set;
    
    for (const auto& [id, spec] : state_.db.all()) {
        if (!spec.country_of_origin.empty()) {
            country_set.insert(spec.country_of_origin);
        }
    }
    
    state_.countries.assign(country_set.begin(), country_set.end());
    std::sort(state_.countries.begin(), state_.countries.end());
}

void BrowserApp::apply_filters() {
    state_.filtered.clear();
    
    std::string search_lower;
    if (state_.search_buffer[0] != '\0') {
        search_lower = state_.search_buffer;
        std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
    }
    
    for (const auto& [id, spec] : state_.db.all()) {
        // Category filter
        if (state_.selected_category != PlatformCategory::UNKNOWN &&
            spec.category != state_.selected_category) {
            continue;
        }
        
        // Country filter
        if (!state_.selected_country.empty() &&
            spec.country_of_origin != state_.selected_country) {
            continue;
        }
        
        // Search filter
        if (!search_lower.empty()) {
            std::string search_str = spec.id + " " + spec.name + " " + 
                                     spec.type + " " + spec.manufacturer;
            std::transform(search_str.begin(), search_str.end(), search_str.begin(), ::tolower);
            
            if (search_str.find(search_lower) == std::string::npos) {
                continue;
            }
        }
        
        state_.filtered.push_back(&spec);
    }
    
    // Sort by name
    std::sort(state_.filtered.begin(), state_.filtered.end(),
        [](const PlatformSpec* a, const PlatformSpec* b) {
            return a->name < b->name;
        });
    
    // Reset selection if out of bounds
    if (state_.selected_platform_idx >= static_cast<int>(state_.filtered.size())) {
        state_.selected_platform_idx = -1;
        state_.selected_platform = nullptr;
    }
}

void BrowserApp::setup_style() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Dark military theme
    ImVec4* colors = style.Colors;
    
    colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
    
    colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.22f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.28f, 0.35f, 1.00f);
    
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.10f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.16f, 0.22f, 1.00f);
    
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.35f, 0.55f, 0.80f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.45f, 0.65f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.50f, 0.70f, 1.00f);
    
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.35f, 0.50f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.45f, 0.60f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.50f, 0.70f, 1.00f);
    
    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.18f, 0.25f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.40f, 0.55f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.35f, 0.50f, 1.00f);
    
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.12f, 0.16f, 0.22f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.20f, 0.25f, 0.32f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.15f, 0.18f, 0.24f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.10f, 0.12f, 0.15f, 0.50f);
    
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.92f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.52f, 0.55f, 1.00f);
    
    // Styling
    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;
    
    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);
    
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 12.0f;
}

void BrowserApp::render() {
    // Main menu bar
    render_menu_bar();
    
    // Calculate layout
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_pos = viewport->WorkPos;
    ImVec2 work_size = viewport->WorkSize;
    
    float sidebar_width = 280.0f;
    float status_height = 28.0f;
    
    // Sidebar
    ImGui::SetNextWindowPos(ImVec2(work_pos.x, work_pos.y));
    ImGui::SetNextWindowSize(ImVec2(sidebar_width, work_size.y - status_height));
    render_sidebar();
    
    // Main content
    ImGui::SetNextWindowPos(ImVec2(work_pos.x + sidebar_width, work_pos.y));
    ImGui::SetNextWindowSize(ImVec2(work_size.x - sidebar_width, work_size.y - status_height));
    render_main_content();
    
    // Status bar
    ImGui::SetNextWindowPos(ImVec2(work_pos.x, work_pos.y + work_size.y - status_height));
    ImGui::SetNextWindowSize(ImVec2(work_size.x, status_height));
    render_status_bar();
    
    // Floating windows
    if (state_.show_detail_window && state_.selected_platform) {
        render_detail_window();
    }
    
    if (state_.show_compare_window && state_.compare_list.size() >= 2) {
        render_compare_window();
    }
    
    if (state_.show_about_window) {
        render_about_window();
    }
    
    if (state_.show_demo_window) {
        ImGui::ShowDemoWindow(&state_.show_demo_window);
    }
    
    state_.first_frame = false;
}

void BrowserApp::render_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Reload Database", "Ctrl+R")) {
                // TODO: Reload
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                state_.should_close = true;
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Detail Panel", "F3", &state_.show_detail_window);
            ImGui::MenuItem("Compare Panel", "F4", &state_.show_compare_window);
            ImGui::Separator();
            ImGui::MenuItem("ImGui Demo", nullptr, &state_.show_demo_window);
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About ATHENA")) {
                state_.show_about_window = true;
            }
            ImGui::EndMenu();
        }
        
        // Stats on the right
        ImGui::SameLine(ImGui::GetWindowWidth() - 350);
        ImGui::TextDisabled("Platforms: %zu | Filtered: %zu | Categories: %zu",
            state_.db.count(), state_.filtered.size(), state_.categories.size() - 1);
        
        ImGui::EndMainMenuBar();
    }
}

void BrowserApp::render_sidebar() {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    
    ImGui::Begin("##Sidebar", nullptr, flags);
    
    // Logo/Title
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
    ImGui::SetWindowFontScale(1.3f);
    ImGui::Text("ATHENA");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::TextDisabled("Platform Database");
    ImGui::Separator();
    
    // Search
    render_search_panel();
    
    ImGui::Spacing();
    
    // Categories
    render_category_panel();
    
    ImGui::Spacing();
    
    // Filters
    render_filter_panel();
    
    ImGui::End();
}

void BrowserApp::render_search_panel() {
    ImGui::Text("Search");
    
    ImGui::PushItemWidth(-1);
    bool search_changed = ImGui::InputTextWithHint("##Search", "Type to search...", 
        state_.search_buffer, sizeof(state_.search_buffer));
    ImGui::PopItemWidth();
    
    if (search_changed) {
        apply_filters();
    }
    
    if (state_.search_buffer[0] != '\0') {
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            state_.search_buffer[0] = '\0';
            apply_filters();
        }
    }
}

void BrowserApp::render_category_panel() {
    ImGui::Text("Categories");
    
    ImGui::BeginChild("##Categories", ImVec2(0, 300), true);
    
    for (const auto& [cat, info] : state_.categories) {
        bool selected = (cat == state_.selected_category);
        
        char label[128];
        snprintf(label, sizeof(label), "%s (%d)", info.name.c_str(), info.count);
        
        if (ImGui::Selectable(label, selected)) {
            state_.selected_category = cat;
            apply_filters();
        }
    }
    
    ImGui::EndChild();
}

void BrowserApp::render_filter_panel() {
    ImGui::Text("Filters");
    
    // Country filter
    ImGui::Text("Country:");
    ImGui::PushItemWidth(-1);
    
    if (ImGui::BeginCombo("##Country", state_.selected_country.empty() ? "All Countries" : state_.selected_country.c_str())) {
        if (ImGui::Selectable("All Countries", state_.selected_country.empty())) {
            state_.selected_country.clear();
            apply_filters();
        }
        
        for (const auto& country : state_.countries) {
            bool selected = (country == state_.selected_country);
            if (ImGui::Selectable(country.c_str(), selected)) {
                state_.selected_country = country;
                apply_filters();
            }
        }
        ImGui::EndCombo();
    }
    
    ImGui::PopItemWidth();
    
    // Clear filters button
    ImGui::Spacing();
    if (ImGui::Button("Clear All Filters", ImVec2(-1, 0))) {
        state_.search_buffer[0] = '\0';
        state_.selected_category = PlatformCategory::UNKNOWN;
        state_.selected_country.clear();
        apply_filters();
    }
}

void BrowserApp::render_main_content() {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    
    ImGui::Begin("##MainContent", nullptr, flags);
    
    // Header
    ImGui::Text("Platforms (%zu)", state_.filtered.size());
    ImGui::SameLine();
    
    // View toggle (future: cards vs table)
    ImGui::SameLine(ImGui::GetWindowWidth() - 200);
    if (ImGui::SmallButton("Compare Selected")) {
        state_.show_compare_window = true;
    }
    
    ImGui::Separator();
    
    // Platform table
    render_platform_table();
    
    ImGui::End();
}

void BrowserApp::render_platform_table() {
    ImGuiTableFlags table_flags = 
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
    
    if (ImGui::BeginTable("##PlatformTable", 6, table_flags)) {
        // Setup columns
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch, 0.35f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.15f);
        ImGui::TableSetupColumn("Country", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Year", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthStretch, 0.15f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        
        // Clipper for performance
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(state_.filtered.size()));
        
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const PlatformSpec* spec = state_.filtered[row];
                
                ImGui::TableNextRow();
                ImGui::PushID(row);
                
                // Name column (selectable)
                ImGui::TableNextColumn();
                bool selected = (row == state_.selected_platform_idx);
                if (ImGui::Selectable(spec->name.c_str(), selected, 
                    ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    select_platform(row);
                    
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        state_.show_detail_window = true;
                    }
                }
                
                // Type
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", spec->type.c_str());
                
                // Country
                ImGui::TableNextColumn();
                ImGui::Text("%s", spec->country_of_origin.c_str());
                
                // Year
                ImGui::TableNextColumn();
                // Try to get year from various fields
                ImGui::TextDisabled("-");
                
                // Category
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", get_category_name(spec->category).c_str());
                
                // Actions
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("View")) {
                    select_platform(row);
                    state_.show_detail_window = true;
                }
                ImGui::SameLine();
                
                // Check if in compare list
                bool in_compare = std::find(state_.compare_list.begin(), 
                    state_.compare_list.end(), spec) != state_.compare_list.end();
                
                if (in_compare) {
                    if (ImGui::SmallButton("-")) {
                        remove_from_compare(spec);
                    }
                } else {
                    if (ImGui::SmallButton("+")) {
                        add_to_compare(spec);
                    }
                }
                
                ImGui::PopID();
            }
        }
        
        ImGui::EndTable();
    }
}

void BrowserApp::render_detail_window() {
    if (!state_.selected_platform) return;
    
    const auto* p = state_.selected_platform;
    
    ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);
    
    char title[256];
    snprintf(title, sizeof(title), "Platform: %s###DetailWindow", p->name.c_str());
    
    if (ImGui::Begin(title, &state_.show_detail_window)) {
        // Header
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
        ImGui::Text("%s", p->name.c_str());
        ImGui::PopStyleColor();
        
        ImGui::TextDisabled("%s", p->id.c_str());
        ImGui::Separator();
        
        // Identity section
        if (ImGui::CollapsingHeader("Identity", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 120);
            
            ImGui::Text("Type:"); ImGui::NextColumn();
            ImGui::Text("%s", p->type.c_str()); ImGui::NextColumn();
            
            ImGui::Text("Country:"); ImGui::NextColumn();
            ImGui::Text("%s", p->country_of_origin.c_str()); ImGui::NextColumn();
            
            ImGui::Text("Manufacturer:"); ImGui::NextColumn();
            ImGui::Text("%s", p->manufacturer.c_str()); ImGui::NextColumn();
            
            if (!p->nato_name.empty()) {
                ImGui::Text("NATO Name:"); ImGui::NextColumn();
                ImGui::Text("%s", p->nato_name.c_str()); ImGui::NextColumn();
            }
            
            ImGui::Columns(1);
        }
        
        // Physical section
        if (ImGui::CollapsingHeader("Physical", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 120);
            
            if (p->crew > 0) {
                ImGui::Text("Crew:"); ImGui::NextColumn();
                ImGui::Text("%d", p->crew); ImGui::NextColumn();
            }
            
            if (p->weight_kg > 0) {
                ImGui::Text("Weight:"); ImGui::NextColumn();
                ImGui::Text("%s", format_weight(p->weight_kg).c_str()); ImGui::NextColumn();
            }
            
            if (p->length_m > 0) {
                ImGui::Text("Length:"); ImGui::NextColumn();
                ImGui::Text("%.1f m", p->length_m); ImGui::NextColumn();
            }
            
            if (p->width_m > 0) {
                ImGui::Text("Width:"); ImGui::NextColumn();
                ImGui::Text("%.1f m", p->width_m); ImGui::NextColumn();
            }
            
            ImGui::Columns(1);
        }
        
        // Mobility section
        if (p->mobility.max_speed_kmh > 0 || p->mobility.range_km > 0) {
            if (ImGui::CollapsingHeader("Mobility")) {
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 120);
                
                if (p->mobility.max_speed_kmh > 0) {
                    ImGui::Text("Max Speed:"); ImGui::NextColumn();
                    ImGui::Text("%.0f km/h", p->mobility.max_speed_kmh); ImGui::NextColumn();
                }
                
                if (p->mobility.range_km > 0) {
                    ImGui::Text("Range:"); ImGui::NextColumn();
                    ImGui::Text("%.0f km", p->mobility.range_km); ImGui::NextColumn();
                }
                
                if (!p->mobility.engine_model.empty()) {
                    ImGui::Text("Engine:"); ImGui::NextColumn();
                    ImGui::Text("%s", p->mobility.engine_model.c_str()); ImGui::NextColumn();
                }
                
                if (p->mobility.power_hp > 0) {
                    ImGui::Text("Power:"); ImGui::NextColumn();
                    ImGui::Text("%.0f hp", p->mobility.power_hp); ImGui::NextColumn();
                }
                
                ImGui::Columns(1);
            }
        }
        
        // Armament section
        if (p->armament.main_gun.has_value()) {
            if (ImGui::CollapsingHeader("Armament")) {
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 120);
                
                const auto& gun = p->armament.main_gun.value();
                
                ImGui::Text("Main Gun:"); ImGui::NextColumn();
                ImGui::Text("%s", gun.designation.c_str()); ImGui::NextColumn();
                
                if (gun.caliber_mm > 0) {
                    ImGui::Text("Caliber:"); ImGui::NextColumn();
                    ImGui::Text("%.0f mm", gun.caliber_mm); ImGui::NextColumn();
                }
                
                if (gun.ammunition_carried > 0) {
                    ImGui::Text("Ammo:"); ImGui::NextColumn();
                    ImGui::Text("%d rounds", gun.ammunition_carried); ImGui::NextColumn();
                }
                
                ImGui::Columns(1);
            }
        }
        
        // Protection section
        if (p->protection.front_mm_rha > 0) {
            if (ImGui::CollapsingHeader("Protection")) {
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 120);
                
                ImGui::Text("Front:"); ImGui::NextColumn();
                ImGui::Text("%.0f mm RHA", p->protection.front_mm_rha); ImGui::NextColumn();
                
                if (p->protection.side_mm_rha > 0) {
                    ImGui::Text("Side:"); ImGui::NextColumn();
                    ImGui::Text("%.0f mm RHA", p->protection.side_mm_rha); ImGui::NextColumn();
                }
                
                if (p->protection.aps_active) {
                    ImGui::Text("APS:"); ImGui::NextColumn();
                    ImGui::Text("%s", p->protection.aps_type.c_str()); ImGui::NextColumn();
                }
                
                ImGui::Columns(1);
            }
        }
        
        // Ratings section
        if (ImGui::CollapsingHeader("Ratings")) {
            ImGui::ProgressBar(p->firepower_rating / 500.0f, ImVec2(-1, 0), "Firepower");
            ImGui::ProgressBar(p->protection_rating / 500.0f, ImVec2(-1, 0), "Protection");
            ImGui::ProgressBar(p->mobility_rating / 200.0f, ImVec2(-1, 0), "Mobility");
        }
        
        // Notes
        if (!p->notes.empty()) {
            if (ImGui::CollapsingHeader("Notes")) {
                ImGui::TextWrapped("%s", p->notes.c_str());
            }
        }
        
        // Compare button
        ImGui::Separator();
        bool in_compare = std::find(state_.compare_list.begin(), 
            state_.compare_list.end(), p) != state_.compare_list.end();
        
        if (in_compare) {
            if (ImGui::Button("Remove from Compare")) {
                remove_from_compare(p);
            }
        } else {
            if (ImGui::Button("Add to Compare")) {
                add_to_compare(p);
            }
        }
    }
    ImGui::End();
}

void BrowserApp::render_compare_window() {
    if (state_.compare_list.size() < 2) return;
    
    ImGui::SetNextWindowSize(ImVec2(800, 500), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Compare Platforms", &state_.show_compare_window)) {
        int cols = static_cast<int>(state_.compare_list.size()) + 1;
        
        if (ImGui::BeginTable("##CompareTable", cols, 
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX)) {
            
            // Header row
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            for (const auto* p : state_.compare_list) {
                ImGui::TableSetupColumn(p->name.c_str());
            }
            ImGui::TableHeadersRow();
            
            // Data rows
            auto add_row = [&](const char* label, auto getter) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", label);
                for (const auto* p : state_.compare_list) {
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", getter(p).c_str());
                }
            };
            
            add_row("Type", [](const PlatformSpec* p) { return p->type; });
            add_row("Country", [](const PlatformSpec* p) { return p->country_of_origin; });
            add_row("Weight", [](const PlatformSpec* p) { return format_weight(p->weight_kg); });
            add_row("Crew", [](const PlatformSpec* p) { return p->crew > 0 ? std::to_string(p->crew) : "-"; });
            add_row("Max Speed", [](const PlatformSpec* p) { 
                return p->mobility.max_speed_kmh > 0 ? 
                    std::to_string(static_cast<int>(p->mobility.max_speed_kmh)) + " km/h" : "-"; 
            });
            add_row("Range", [](const PlatformSpec* p) { 
                return p->mobility.range_km > 0 ? 
                    std::to_string(static_cast<int>(p->mobility.range_km)) + " km" : "-"; 
            });
            add_row("Firepower", [](const PlatformSpec* p) { 
                return std::to_string(static_cast<int>(p->firepower_rating)); 
            });
            add_row("Protection", [](const PlatformSpec* p) { 
                return std::to_string(static_cast<int>(p->protection_rating)); 
            });
            
            ImGui::EndTable();
        }
        
        // Clear button
        ImGui::Separator();
        if (ImGui::Button("Clear Compare List")) {
            state_.compare_list.clear();
        }
    }
    ImGui::End();
}

void BrowserApp::render_about_window() {
    ImGui::SetNextWindowSize(ImVec2(400, 250), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("About ATHENA", &state_.show_about_window, 
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
        ImGui::SetWindowFontScale(1.5f);
        ImGui::Text("ATHENA");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        
        ImGui::Text("Advanced Tactical & Heuristic");
        ImGui::Text("Engagement & Network Analyzer");
        
        ImGui::Separator();
        
        ImGui::Text("Version: 0.9.3");
        ImGui::Text("Platforms: %zu", state_.db.count());
        ImGui::Text("Categories: %zu", state_.categories.size() - 1);
        ImGui::Text("Countries: %zu", state_.countries.size());
        
        ImGui::Separator();
        
        ImGui::TextDisabled("Military tactical analysis and");
        ImGui::TextDisabled("simulation system.");
        
        ImGui::Spacing();
        if (ImGui::Button("Close")) {
            state_.show_about_window = false;
        }
    }
    ImGui::End();
}

void BrowserApp::render_status_bar() {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.14f, 0.18f, 1.0f));
    ImGui::Begin("##StatusBar", nullptr, flags);
    
    // Left side
    if (state_.selected_platform) {
        ImGui::Text("Selected: %s", state_.selected_platform->name.c_str());
    } else {
        ImGui::TextDisabled("No platform selected");
    }
    
    // Right side
    ImGui::SameLine(ImGui::GetWindowWidth() - 200);
    ImGui::Text("Compare: %zu", state_.compare_list.size());
    
    ImGui::SameLine(ImGui::GetWindowWidth() - 100);
    ImGui::TextDisabled("ATHENA v0.9.3");
    
    ImGui::End();
    ImGui::PopStyleColor();
}

void BrowserApp::select_platform(int index) {
    if (index >= 0 && index < static_cast<int>(state_.filtered.size())) {
        state_.selected_platform_idx = index;
        state_.selected_platform = state_.filtered[index];
    }
}

void BrowserApp::add_to_compare(const PlatformSpec* spec) {
    if (state_.compare_list.size() < 5) {  // Max 5 for compare
        if (std::find(state_.compare_list.begin(), state_.compare_list.end(), spec) == state_.compare_list.end()) {
            state_.compare_list.push_back(spec);
        }
    }
}

void BrowserApp::remove_from_compare(const PlatformSpec* spec) {
    auto it = std::find(state_.compare_list.begin(), state_.compare_list.end(), spec);
    if (it != state_.compare_list.end()) {
        state_.compare_list.erase(it);
    }
}

std::string BrowserApp::get_category_name(PlatformCategory cat) const {
    auto it = CATEGORY_NAMES.find(cat);
    if (it != CATEGORY_NAMES.end()) {
        return it->second;
    }
    return "Unknown";
}

// =============================================================================
// Utility Functions
// =============================================================================

std::string format_number(double value, int decimals) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(decimals) << value;
    return ss.str();
}

std::string format_weight(double kg) {
    if (kg <= 0) return "-";
    if (kg >= 1000) {
        return format_number(kg / 1000.0, 1) + " t";
    }
    return format_number(kg, 0) + " kg";
}

std::string format_distance(double meters) {
    if (meters <= 0) return "-";
    if (meters >= 1000) {
        return format_number(meters / 1000.0, 1) + " km";
    }
    return format_number(meters, 0) + " m";
}

std::string format_speed(double kmh) {
    if (kmh <= 0) return "-";
    return format_number(kmh, 0) + " km/h";
}

}  // namespace athena::gui
