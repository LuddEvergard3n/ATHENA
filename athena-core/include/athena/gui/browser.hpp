// ATHENA Platform Browser - Dear ImGui GUI
// Cross-platform graphical browser for the platform database
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_GUI_BROWSER_HPP
#define ATHENA_GUI_BROWSER_HPP

#include "athena/platform_loader.hpp"
#include <string>
#include <vector>
#include <map>

namespace athena::gui {

// =============================================================================
// Application State
// =============================================================================

struct AppState {
    // Window
    int window_width = 1400;
    int window_height = 900;
    bool should_close = false;
    
    // Platform data
    PlatformDatabase db;
    std::vector<const PlatformSpec*> filtered;
    
    // Filters
    char search_buffer[256] = {0};
    PlatformCategory selected_category = PlatformCategory::UNKNOWN;
    std::string selected_country;
    int year_from = 0;
    int year_to = 2030;
    
    // Selection
    int selected_platform_idx = -1;
    const PlatformSpec* selected_platform = nullptr;
    bool show_detail_window = false;
    
    // Compare
    std::vector<const PlatformSpec*> compare_list;
    bool show_compare_window = false;
    
    // UI state
    bool show_demo_window = false;
    bool show_about_window = false;
    bool first_frame = true;
    
    // Category info cache
    struct CategoryInfo {
        std::string name;
        int count = 0;
    };
    std::map<PlatformCategory, CategoryInfo> categories;
    std::vector<std::string> countries;
};

// =============================================================================
// Browser Application
// =============================================================================

class BrowserApp {
public:
    BrowserApp();
    ~BrowserApp();
    
    // Initialize with data path
    bool init(const std::string& data_path);
    
    // Render one frame (call from main loop)
    void render();
    
    // Check if should close
    bool should_close() const { return state_.should_close; }
    
    // Get state for external access
    AppState& state() { return state_; }
    
private:
    AppState state_;
    
    // Data operations
    void build_category_list();
    void build_country_list();
    void apply_filters();
    
    // UI Panels
    void render_menu_bar();
    void render_sidebar();
    void render_search_panel();
    void render_category_panel();
    void render_filter_panel();
    void render_main_content();
    void render_platform_table();
    void render_platform_cards();
    void render_detail_window();
    void render_compare_window();
    void render_about_window();
    void render_status_bar();
    
    // Helpers
    void select_platform(int index);
    void add_to_compare(const PlatformSpec* spec);
    void remove_from_compare(const PlatformSpec* spec);
    std::string get_category_name(PlatformCategory cat) const;
    const char* get_country_flag(const std::string& code) const;
    
    // Style
    void setup_style();
};

// =============================================================================
// Utility
// =============================================================================

// Format large numbers with separators
std::string format_number(double value, int decimals = 0);

// Format weight (kg to appropriate unit)
std::string format_weight(double kg);

// Format distance (m or km)
std::string format_distance(double meters);

// Format speed
std::string format_speed(double kmh);

}  // namespace athena::gui

#endif  // ATHENA_GUI_BROWSER_HPP
