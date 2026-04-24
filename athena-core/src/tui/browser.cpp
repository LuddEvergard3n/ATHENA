// ATHENA Platform Browser - Implementation
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/tui/browser.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace athena::tui {

// Country flags (ASCII approximation for terminal)
static const std::map<std::string, std::string> COUNTRY_FLAGS = {
    {"US", "[US]"}, {"RU", "[RU]"}, {"CN", "[CN]"}, {"GB", "[GB]"}, {"FR", "[FR]"},
    {"DE", "[DE]"}, {"IL", "[IL]"}, {"IN", "[IN]"}, {"JP", "[JP]"}, {"KR", "[KR]"},
    {"TR", "[TR]"}, {"IT", "[IT]"}, {"UA", "[UA]"}, {"AU", "[AU]"}, {"SE", "[SE]"},
    {"NO", "[NO]"}, {"PL", "[PL]"}, {"IR", "[IR]"}, {"KP", "[KP]"}, {"PK", "[PK]"},
    {"EG", "[EG]"}, {"SA", "[SA]"}, {"BR", "[BR]"}, {"TW", "[TW]"}, {"SG", "[SG]"},
};

// Category info
static const std::map<PlatformCategory, std::pair<std::string, std::string>> CATEGORY_INFO = {
    {PlatformCategory::UNKNOWN, {"All", "*"}},
    {PlatformCategory::TANK, {"Tanks", "T"}},
    {PlatformCategory::IFV, {"IFVs", "I"}},
    {PlatformCategory::APC, {"APCs", "A"}},
    {PlatformCategory::ARTILLERY, {"Artillery", "H"}},
    {PlatformCategory::MLRS, {"MLRS", "M"}},
    {PlatformCategory::SAM, {"SAM", "S"}},
    {PlatformCategory::ATGM, {"ATGM", "G"}},
    {PlatformCategory::MANPADS, {"MANPADS", "P"}},
    {PlatformCategory::COUNTER_UAS, {"C-UAS", "U"}},
    {PlatformCategory::AIRCRAFT, {"Aircraft", "F"}},
    {PlatformCategory::HELICOPTER, {"Helicopters", "H"}},
    {PlatformCategory::BOMBER, {"Bombers", "B"}},
    {PlatformCategory::UAV, {"UAVs", "D"}},
    {PlatformCategory::SHIP, {"Ships", "N"}},
    {PlatformCategory::SUBMARINE, {"Submarines", "V"}},
    {PlatformCategory::SMALL_ARMS, {"Small Arms", "W"}},
    {PlatformCategory::BODY_ARMOR, {"Body Armor", "K"}},
    {PlatformCategory::OPTICS, {"Optics", "O"}},
    {PlatformCategory::RADAR, {"Radars", "R"}},
    {PlatformCategory::EW_SYSTEM, {"EW Systems", "E"}},
    {PlatformCategory::COMMS, {"Comms", "C"}},
    {PlatformCategory::ENGINEERING, {"Engineering", "X"}},
    {PlatformCategory::REGIONAL, {"Regional", "L"}},
    {PlatformCategory::MISSILE, {"Missiles", "M"}},
    {PlatformCategory::MUNITION, {"Munitions", "Z"}},
};

Browser::Browser() = default;
Browser::~Browser() = default;

bool Browser::init(const std::string& data_path) {
    // Load platform database
    auto result = db_.load_all(data_path);
    if (!result.ok()) {
        return false;
    }
    
    // Initialize terminal
    if (!term_.init()) {
        return false;
    }
    
    // Build category list
    build_category_list();
    
    // Initial filter (show all)
    apply_filters();
    
    return true;
}

void Browser::build_category_list() {
    categories_.clear();
    category_order_.clear();
    
    // Add "All" option first
    CategoryInfo all_info;
    all_info.name = "All Platforms";
    all_info.icon = "*";
    all_info.count = static_cast<int>(db_.count());
    categories_[PlatformCategory::UNKNOWN] = all_info;
    category_order_.push_back(PlatformCategory::UNKNOWN);
    
    // Add categories with platforms
    for (int i = 1; i < static_cast<int>(PlatformCategory::MAX_CATEGORIES); ++i) {
        auto cat = static_cast<PlatformCategory>(i);
        int count = static_cast<int>(db_.count_by_category(cat));
        if (count == 0) continue;
        
        auto it = CATEGORY_INFO.find(cat);
        if (it != CATEGORY_INFO.end()) {
            CategoryInfo info;
            info.name = it->second.first;
            info.icon = it->second.second;
            info.count = count;
            categories_[cat] = info;
            category_order_.push_back(cat);
        }
    }
}

void Browser::apply_filters() {
    filtered_.clear();
    
    for (const auto& [id, spec] : db_.all()) {
        // Category filter
        if (selected_category_ != PlatformCategory::UNKNOWN &&
            spec.category != selected_category_) {
            continue;
        }
        
        // Search filter
        if (!search_query_.empty()) {
            std::string search_str = spec.id + " " + spec.name + " " + 
                                     spec.type + " " + spec.manufacturer +
                                     " " + spec.country_of_origin;
            
            // Case-insensitive search
            std::transform(search_str.begin(), search_str.end(), search_str.begin(), ::tolower);
            std::string query = search_query_;
            std::transform(query.begin(), query.end(), query.begin(), ::tolower);
            
            if (search_str.find(query) == std::string::npos) {
                continue;
            }
        }
        
        filtered_.push_back(&spec);
    }
    
    // Sort by name
    std::sort(filtered_.begin(), filtered_.end(),
        [](const PlatformSpec* a, const PlatformSpec* b) {
            return a->name < b->name;
        });
    
    // Reset selection
    platform_selected_ = 0;
    platform_scroll_ = 0;
}

void Browser::calculate_layout() {
    size_ = term_.size();
    
    // Header: row 0
    // Status: last row
    
    int sidebar_width = 24;
    int detail_width = show_detail_ ? 40 : 0;
    
    // Sidebar
    sidebar_rect_ = {0, 1, sidebar_width, size_.height - 2};
    
    // Search box in sidebar
    search_rect_ = {1, 2, sidebar_width - 2, 3};
    
    // Categories in sidebar
    categories_rect_ = {1, 6, sidebar_width - 2, size_.height - 9};
    
    // Main content area
    main_rect_ = {sidebar_width, 1, size_.width - sidebar_width - detail_width, size_.height - 2};
    
    // Detail panel
    if (show_detail_) {
        detail_rect_ = {size_.width - detail_width, 1, detail_width, size_.height - 2};
    }
    
    // Status bar
    status_rect_ = {0, size_.height - 1, size_.width, 1};
}

void Browser::run() {
    bool running = true;
    
    while (running) {
        calculate_layout();
        draw();
        term_.flush();
        
        // Wait for input
        Key key = term_.read_key();
        
        if (key == Key::None) continue;
        
        // Global keys
        if (key == Key::CtrlQ || key == Key::CtrlC) {
            running = false;
            continue;
        }
        
        // Tab to switch focus
        if (key == Key::Tab) {
            switch (focus_) {
                case Focus::Search: focus_ = Focus::Categories; break;
                case Focus::Categories: focus_ = Focus::Platforms; break;
                case Focus::Platforms: 
                    focus_ = show_detail_ ? Focus::Detail : Focus::Search; 
                    break;
                case Focus::Detail: focus_ = Focus::Search; break;
            }
            continue;
        }
        
        // Escape to close detail or clear
        if (key == Key::Escape) {
            if (show_detail_) {
                show_detail_ = false;
            } else if (!search_query_.empty()) {
                search_query_.clear();
                apply_filters();
            }
            continue;
        }
        
        // '/' to focus search
        if (static_cast<int>(key) == '/') {
            focus_ = Focus::Search;
            continue;
        }
        
        handle_input(key);
    }
    
    term_.cleanup();
}

void Browser::handle_input(Key key) {
    switch (focus_) {
        case Focus::Search:
            handle_search_input(key);
            break;
        case Focus::Categories:
            handle_category_input(key);
            break;
        case Focus::Platforms:
            handle_platform_input(key);
            break;
        case Focus::Detail:
            handle_detail_input(key);
            break;
    }
}

void Browser::handle_search_input(Key key) {
    int code = static_cast<int>(key);
    
    // Printable characters
    if (code >= 32 && code < 127) {
        search_query_ += static_cast<char>(code);
        apply_filters();
        return;
    }
    
    switch (key) {
        case Key::Backspace:
            if (!search_query_.empty()) {
                search_query_.pop_back();
                apply_filters();
            }
            break;
            
        case Key::Enter:
            focus_ = Focus::Platforms;
            break;
            
        case Key::Down:
            focus_ = Focus::Categories;
            break;
            
        default:
            break;
    }
}

void Browser::handle_category_input(Key key) {
    switch (key) {
        case Key::Up:
            if (category_selected_ > 0) category_selected_--;
            break;
            
        case Key::Down:
            if (category_selected_ < static_cast<int>(category_order_.size()) - 1)
                category_selected_++;
            break;
            
        case Key::Enter:
        case Key::Right:
            selected_category_ = category_order_[category_selected_];
            apply_filters();
            focus_ = Focus::Platforms;
            break;
            
        default:
            break;
    }
}

void Browser::handle_platform_input(Key key) {
    switch (key) {
        case Key::Up:
            if (platform_selected_ > 0) {
                platform_selected_--;
                ensure_platform_visible();
            }
            break;
            
        case Key::Down:
            if (platform_selected_ < static_cast<int>(filtered_.size()) - 1) {
                platform_selected_++;
                ensure_platform_visible();
            }
            break;
            
        case Key::PageUp:
            platform_selected_ = std::max(0, platform_selected_ - (main_rect_.h - 3));
            ensure_platform_visible();
            break;
            
        case Key::PageDown:
            platform_selected_ = std::min(static_cast<int>(filtered_.size()) - 1,
                                         platform_selected_ + (main_rect_.h - 3));
            ensure_platform_visible();
            break;
            
        case Key::Home:
            platform_selected_ = 0;
            platform_scroll_ = 0;
            break;
            
        case Key::End:
            platform_selected_ = static_cast<int>(filtered_.size()) - 1;
            ensure_platform_visible();
            break;
            
        case Key::Enter:
        case Key::Right:
            if (!filtered_.empty()) {
                show_detail_ = true;
                detail_scroll_ = 0;
                focus_ = Focus::Detail;
            }
            break;
            
        case Key::Left:
            focus_ = Focus::Categories;
            break;
            
        default:
            break;
    }
}

void Browser::handle_detail_input(Key key) {
    switch (key) {
        case Key::Up:
            if (detail_scroll_ > 0) detail_scroll_--;
            break;
            
        case Key::Down:
            detail_scroll_++;
            break;
            
        case Key::Left:
        case Key::Escape:
            focus_ = Focus::Platforms;
            break;
            
        default:
            break;
    }
}

void Browser::ensure_platform_visible() {
    int visible = main_rect_.h - 3;  // Header + border
    
    if (platform_selected_ < platform_scroll_) {
        platform_scroll_ = platform_selected_;
    } else if (platform_selected_ >= platform_scroll_ + visible) {
        platform_scroll_ = platform_selected_ - visible + 1;
    }
}

void Browser::draw() {
    // Clear screen
    term_.clear();
    
    draw_header();
    draw_sidebar();
    draw_platforms();
    
    if (show_detail_) {
        draw_detail();
    }
    
    draw_status();
}

void Browser::draw_header() {
    Style title_style = Style{Color::Cyan, Color::Default, true};
    Style subtitle_style = Style{Color::BrightBlack};
    Style stat_style = Style{Color::White};
    Style stat_value_style = Style{Color::Cyan, Color::Default, true};
    
    // Title
    term_.move_cursor(1, 0);
    term_.write("[A] ATHENA", title_style);
    term_.write(" Platform Browser", subtitle_style);
    
    // Stats
    std::ostringstream stats;
    stats << " | Platforms: ";
    term_.move_cursor(30, 0);
    term_.write(" | Platforms: ", stat_style);
    term_.write(std::to_string(db_.count()), stat_value_style);
    term_.write(" | Categories: ", stat_style);
    term_.write(std::to_string(category_order_.size() - 1), stat_value_style);
    term_.write(" | Filtered: ", stat_style);
    term_.write(std::to_string(filtered_.size()), stat_value_style);
}

void Browser::draw_sidebar() {
    Style border_style = Style{Color::BrightBlack};
    
    // Vertical separator
    for (int y = 1; y < size_.height - 1; ++y) {
        term_.move_cursor(sidebar_rect_.w, y);
        term_.write(box::V, border_style);
    }
    
    draw_search();
    draw_categories();
}

void Browser::draw_search() {
    Style label_style = Style{Color::BrightBlack};
    Style box_style = (focus_ == Focus::Search) ? Style{Color::Cyan} : Style{Color::White};
    Style text_style = Style{Color::White};
    Style placeholder_style = Style{Color::BrightBlack};
    
    // Label
    term_.move_cursor(1, 1);
    term_.write("SEARCH", label_style);
    
    // Box
    term_.draw_box(search_rect_.x, search_rect_.y, search_rect_.w, search_rect_.h, box_style);
    
    // Content
    int inner_w = search_rect_.w - 2;
    term_.move_cursor(search_rect_.x + 1, search_rect_.y + 1);
    
    if (search_query_.empty()) {
        term_.write(fit_width("Type to search...", inner_w), placeholder_style);
    } else {
        term_.write(fit_width(search_query_, inner_w), text_style);
    }
    
    // Cursor
    if (focus_ == Focus::Search) {
        int cursor_x = std::min(static_cast<int>(search_query_.size()), inner_w - 1);
        term_.move_cursor(search_rect_.x + 1 + cursor_x, search_rect_.y + 1);
        term_.write("_", Style{Color::Cyan, Color::Default, true});
    }
}

void Browser::draw_categories() {
    Style label_style = Style{Color::BrightBlack};
    Style normal_style = Style{Color::White};
    Style selected_style = Style{Color::Black, Color::Cyan};
    Style count_style = Style{Color::BrightBlack};
    Style active_style = Style{Color::Cyan, Color::Default, true};
    
    // Label
    term_.move_cursor(1, 5);
    term_.write("CATEGORIES", label_style);
    
    int visible = categories_rect_.h;
    
    for (int i = 0; i < visible && i < static_cast<int>(category_order_.size()); ++i) {
        auto cat = category_order_[i];
        const auto& info = categories_[cat];
        
        int y = categories_rect_.y + i;
        bool is_selected = (i == category_selected_) && (focus_ == Focus::Categories);
        bool is_active = (cat == selected_category_);
        
        Style style = is_selected ? selected_style : 
                     (is_active ? active_style : normal_style);
        
        // Clear line
        term_.move_cursor(categories_rect_.x, y);
        term_.write(std::string(categories_rect_.w, ' '), style);
        
        // Icon and name
        term_.move_cursor(categories_rect_.x, y);
        std::string text = "[" + info.icon + "] " + info.name;
        text = fit_width(text, categories_rect_.w - 5);
        term_.write(text, style);
        
        // Count
        std::string count_str = std::to_string(info.count);
        term_.move_cursor(categories_rect_.x + categories_rect_.w - static_cast<int>(count_str.size()) - 1, y);
        term_.write(count_str, is_selected ? selected_style : count_style);
    }
}

void Browser::draw_platforms() {
    Style border_style = (focus_ == Focus::Platforms) ? Style{Color::Cyan} : Style{Color::White};
    Style title_style = Style{Color::Cyan, Color::Default, true};
    Style normal_style = Style{Color::White};
    Style selected_style = Style{Color::Black, Color::Cyan};
    Style id_style = Style{Color::BrightBlack};
    Style type_style = Style{Color::Yellow};
    
    // Border
    term_.draw_box(main_rect_.x, main_rect_.y, main_rect_.w, main_rect_.h, border_style);
    
    // Title
    std::string title = " Platforms (" + std::to_string(filtered_.size()) + ") ";
    term_.move_cursor(main_rect_.x + 2, main_rect_.y);
    term_.write(title, title_style);
    
    // Column headers
    int y = main_rect_.y + 1;
    int inner_w = main_rect_.w - 2;
    term_.move_cursor(main_rect_.x + 1, y);
    
    int name_w = inner_w - 25;  // Reserve space for type and country
    term_.write(fit_width("NAME", name_w), Style{Color::BrightBlack});
    term_.write(fit_width("TYPE", 12), Style{Color::BrightBlack});
    term_.write(fit_width("CTRY", 6), Style{Color::BrightBlack});
    
    // Separator
    term_.move_cursor(main_rect_.x + 1, y + 1);
    term_.write(std::string(inner_w, '-'), Style{Color::BrightBlack});
    
    // Platforms
    int visible = main_rect_.h - 4;  // Header, separator, borders
    
    for (int i = 0; i < visible; ++i) {
        int idx = platform_scroll_ + i;
        int row_y = main_rect_.y + 3 + i;
        
        // Clear line
        term_.move_cursor(main_rect_.x + 1, row_y);
        term_.write(std::string(inner_w, ' '));
        
        if (idx >= static_cast<int>(filtered_.size())) continue;
        
        const auto* p = filtered_[idx];
        bool is_selected = (idx == platform_selected_) && (focus_ == Focus::Platforms);
        Style style = is_selected ? selected_style : normal_style;
        
        term_.move_cursor(main_rect_.x + 1, row_y);
        
        // Name
        std::string name = p->name;
        if (name.size() > static_cast<size_t>(name_w - 1)) {
            name = name.substr(0, name_w - 4) + "...";
        }
        term_.write(fit_width(name, name_w), style);
        
        // Type
        std::string type = p->type;
        if (type.size() > 11) type = type.substr(0, 10) + ".";
        term_.write(fit_width(type, 12), is_selected ? selected_style : type_style);
        
        // Country
        term_.write(fit_width(p->country_of_origin, 6), style);
    }
    
    // Scrollbar hint
    if (filtered_.size() > static_cast<size_t>(visible)) {
        int scroll_pos = main_rect_.y + 3 + 
            (platform_scroll_ * (visible - 1)) / 
            std::max(1, static_cast<int>(filtered_.size()) - visible);
        term_.move_cursor(main_rect_.x + main_rect_.w - 1, scroll_pos);
        term_.write("#", Style{Color::Cyan});
    }
}

void Browser::draw_detail() {
    if (filtered_.empty() || platform_selected_ >= static_cast<int>(filtered_.size())) {
        return;
    }
    
    const auto* p = filtered_[platform_selected_];
    
    Style border_style = (focus_ == Focus::Detail) ? Style{Color::Cyan} : Style{Color::White};
    Style title_style = Style{Color::Cyan, Color::Default, true};
    Style header_style = Style{Color::Yellow, Color::Default, true};
    Style label_style = Style{Color::BrightBlack};
    Style value_style = Style{Color::White};
    
    // Border
    term_.draw_box(detail_rect_.x, detail_rect_.y, detail_rect_.w, detail_rect_.h, border_style);
    
    // Title
    std::string title = " " + p->name + " ";
    if (title.size() > static_cast<size_t>(detail_rect_.w - 4)) {
        title = title.substr(0, detail_rect_.w - 7) + "... ";
    }
    term_.move_cursor(detail_rect_.x + 2, detail_rect_.y);
    term_.write(title, title_style);
    
    int y = detail_rect_.y + 1;
    int inner_w = detail_rect_.w - 2;
    int label_w = 12;
    int value_w = inner_w - label_w - 1;
    
    auto draw_field = [&](const std::string& label, const std::string& value) {
        if (y >= detail_rect_.y + detail_rect_.h - 1) return;
        term_.move_cursor(detail_rect_.x + 1, y);
        term_.write(fit_width(label + ":", label_w), label_style);
        term_.write(fit_width(value, value_w), value_style);
        y++;
    };
    
    auto draw_header_line = [&](const std::string& text) {
        if (y >= detail_rect_.y + detail_rect_.h - 1) return;
        term_.move_cursor(detail_rect_.x + 1, y);
        term_.write(fit_width(text, inner_w), header_style);
        y++;
    };
    
    // Scroll offset
    y -= detail_scroll_;
    
    // Identity
    draw_header_line("=== IDENTITY ===");
    draw_field("ID", p->id);
    draw_field("Type", p->type);
    draw_field("Country", p->country_of_origin);
    draw_field("Manufacturer", p->manufacturer);
    if (!p->nato_name.empty()) {
        draw_field("NATO Name", p->nato_name);
    }
    
    // Physical
    y++;
    draw_header_line("=== PHYSICAL ===");
    if (p->crew > 0) draw_field("Crew", std::to_string(p->crew));
    if (p->weight_kg > 0) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << (p->weight_kg / 1000.0) << " t";
        draw_field("Weight", ss.str());
    }
    if (p->length_m > 0) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << p->length_m << " m";
        draw_field("Length", ss.str());
    }
    
    // Mobility
    if (p->mobility.max_speed_kmh > 0 || p->mobility.range_km > 0) {
        y++;
        draw_header_line("=== MOBILITY ===");
        if (p->mobility.max_speed_kmh > 0) {
            draw_field("Max Speed", std::to_string(static_cast<int>(p->mobility.max_speed_kmh)) + " km/h");
        }
        if (p->mobility.range_km > 0) {
            draw_field("Range", std::to_string(static_cast<int>(p->mobility.range_km)) + " km");
        }
        if (!p->mobility.engine_model.empty()) {
            draw_field("Engine", p->mobility.engine_model);
        }
    }
    
    // Armament
    if (p->armament.main_gun.has_value()) {
        y++;
        draw_header_line("=== ARMAMENT ===");
        const auto& gun = p->armament.main_gun.value();
        draw_field("Main Gun", gun.designation);
        if (gun.caliber_mm > 0) {
            draw_field("Caliber", std::to_string(static_cast<int>(gun.caliber_mm)) + " mm");
        }
        if (gun.ammunition_carried > 0) {
            draw_field("Ammo", std::to_string(gun.ammunition_carried) + " rds");
        }
    }
    
    // Protection
    if (p->protection.front_mm_rha > 0) {
        y++;
        draw_header_line("=== PROTECTION ===");
        draw_field("Front", std::to_string(static_cast<int>(p->protection.front_mm_rha)) + " mm RHA");
        if (p->protection.aps_active) {
            draw_field("APS", p->protection.aps_type);
        }
    }
    
    // Sensors
    if (!p->sensors.radar_name.empty() || p->sensors.thermal_sight) {
        y++;
        draw_header_line("=== SENSORS ===");
        if (!p->sensors.radar_name.empty()) {
            draw_field("Radar", p->sensors.radar_name);
        }
        if (p->sensors.thermal_sight) {
            draw_field("Thermal", "Gen " + std::to_string(p->sensors.thermal_generation));
        }
    }
    
    // Ratings
    y++;
    draw_header_line("=== RATINGS ===");
    draw_field("Firepower", std::to_string(static_cast<int>(p->firepower_rating)));
    draw_field("Protection", std::to_string(static_cast<int>(p->protection_rating)));
    draw_field("Mobility", std::to_string(static_cast<int>(p->mobility_rating)));
    
    // Notes
    if (!p->notes.empty()) {
        y++;
        draw_header_line("=== NOTES ===");
        // Word wrap notes
        std::string notes = p->notes;
        while (!notes.empty() && y < detail_rect_.y + detail_rect_.h - 1) {
            std::string line = notes.substr(0, inner_w);
            if (notes.size() > static_cast<size_t>(inner_w)) {
                // Find last space
                size_t last_space = line.rfind(' ');
                if (last_space != std::string::npos && last_space > 10) {
                    line = notes.substr(0, last_space);
                }
            }
            term_.move_cursor(detail_rect_.x + 1, y);
            term_.write(fit_width(line, inner_w), value_style);
            notes = notes.substr(line.size());
            if (!notes.empty() && notes[0] == ' ') notes = notes.substr(1);
            y++;
        }
    }
}

void Browser::draw_status() {
    Style bg_style = Style{Color::Black, Color::White};
    Style key_style = Style{Color::Blue, Color::White, true};
    
    // Fill background
    term_.move_cursor(0, status_rect_.y);
    term_.write(std::string(status_rect_.w, ' '), bg_style);
    
    // Help text
    term_.move_cursor(1, status_rect_.y);
    term_.write("[", bg_style);
    term_.write("Tab", key_style);
    term_.write("] Focus  [", bg_style);
    term_.write("/", key_style);
    term_.write("] Search  [", bg_style);
    term_.write("Enter", key_style);
    term_.write("] Select  [", bg_style);
    term_.write("Esc", key_style);
    term_.write("] Back  [", bg_style);
    term_.write("Ctrl+Q", key_style);
    term_.write("] Quit", bg_style);
    
    // Current focus indicator
    std::string focus_str;
    switch (focus_) {
        case Focus::Search: focus_str = "Search"; break;
        case Focus::Categories: focus_str = "Categories"; break;
        case Focus::Platforms: focus_str = "Platforms"; break;
        case Focus::Detail: focus_str = "Detail"; break;
    }
    term_.move_cursor(status_rect_.w - static_cast<int>(focus_str.size()) - 2, status_rect_.y);
    term_.write(focus_str, key_style);
}

std::string Browser::get_country_flag(const std::string& code) {
    auto it = COUNTRY_FLAGS.find(code);
    if (it != COUNTRY_FLAGS.end()) {
        return it->second;
    }
    return "[" + code + "]";
}

std::string Browser::get_category_icon(PlatformCategory cat) {
    auto it = CATEGORY_INFO.find(cat);
    if (it != CATEGORY_INFO.end()) {
        return it->second.second;
    }
    return "?";
}

}  // namespace athena::tui
