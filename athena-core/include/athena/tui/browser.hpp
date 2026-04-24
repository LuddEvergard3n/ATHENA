// ATHENA Platform Browser - Terminal UI Application
// Main browser interface for the platform database
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_TUI_BROWSER_HPP
#define ATHENA_TUI_BROWSER_HPP

#include "athena/tui/terminal.hpp"
#include "athena/tui/widgets.hpp"
#include "athena/platform_loader.hpp"
#include <memory>
#include <map>

namespace athena::tui {

// =============================================================================
// Browser Application
// =============================================================================

class Browser {
public:
    Browser();
    ~Browser();
    
    // Initialize with platform database
    bool init(const std::string& data_path);
    
    // Run main loop
    void run();
    
private:
    // Terminal and rendering
    Terminal term_;
    TerminalSize size_;
    
    // Platform data
    PlatformDatabase db_;
    std::vector<const PlatformSpec*> filtered_;
    
    // Filters
    std::string search_query_;
    PlatformCategory selected_category_ = PlatformCategory::UNKNOWN;
    
    // UI state
    enum class Focus { Categories, Search, Platforms, Detail };
    Focus focus_ = Focus::Categories;
    int platform_scroll_ = 0;
    int platform_selected_ = 0;
    int detail_scroll_ = 0;
    bool show_detail_ = false;
    
    // Category info
    struct CategoryInfo {
        std::string name;
        std::string icon;
        int count = 0;
    };
    std::map<PlatformCategory, CategoryInfo> categories_;
    std::vector<PlatformCategory> category_order_;
    int category_selected_ = 0;
    
    // Layout
    void calculate_layout();
    Rect sidebar_rect_;
    Rect search_rect_;
    Rect categories_rect_;
    Rect main_rect_;
    Rect detail_rect_;
    Rect status_rect_;
    
    // Drawing
    void draw();
    void draw_header();
    void draw_sidebar();
    void draw_search();
    void draw_categories();
    void draw_platforms();
    void draw_detail();
    void draw_status();
    
    // Input handling
    void handle_input(Key key);
    void handle_search_input(Key key);
    void handle_category_input(Key key);
    void handle_platform_input(Key key);
    void handle_detail_input(Key key);
    
    // Data operations
    void apply_filters();
    void build_category_list();
    std::string format_spec_value(const std::string& key, double value);
    
    // Helpers
    void ensure_platform_visible();
    std::string get_country_flag(const std::string& code);
    std::string get_category_icon(PlatformCategory cat);
};

}  // namespace athena::tui

#endif  // ATHENA_TUI_BROWSER_HPP
