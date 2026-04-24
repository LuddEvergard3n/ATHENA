// ATHENA TUI - Widget Implementation
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/tui/widgets.hpp"
#include <algorithm>

namespace athena::tui {

// =============================================================================
// TextInput
// =============================================================================

void TextInput::draw(Terminal& term) {
    Style box_style = focused_ ? Style{Color::Cyan} : Style{Color::White};
    Style text_style = Style{Color::White};
    Style placeholder_style = Style{Color::BrightBlack};
    
    // Draw box
    term.draw_box(bounds_.x, bounds_.y, bounds_.w, bounds_.h, box_style);
    
    // Draw content
    int inner_w = bounds_.w - 2;
    term.move_cursor(bounds_.x + 1, bounds_.y + 1);
    
    if (value_.empty() && !focused_) {
        std::string ph = fit_width(placeholder_, inner_w);
        term.write(ph, placeholder_style);
    } else {
        std::string display = value_;
        if (display.size() > static_cast<size_t>(inner_w)) {
            display = display.substr(display.size() - inner_w);
        }
        display = fit_width(display, inner_w);
        term.write(display, text_style);
        
        // Draw cursor if focused
        if (focused_) {
            int cursor_pos = std::min(cursor_, static_cast<size_t>(inner_w - 1));
            term.move_cursor(bounds_.x + 1 + cursor_pos, bounds_.y + 1);
            term.write("_", Style{Color::Cyan, Color::Default, true});
        }
    }
}

bool TextInput::handle_key(Key key) {
    int code = static_cast<int>(key);
    
    // Printable characters
    if (code >= 32 && code < 127) {
        value_.insert(cursor_, 1, static_cast<char>(code));
        cursor_++;
        if (on_change_) on_change_(value_);
        return true;
    }
    
    switch (key) {
        case Key::Backspace:
            if (cursor_ > 0) {
                value_.erase(cursor_ - 1, 1);
                cursor_--;
                if (on_change_) on_change_(value_);
            }
            return true;
            
        case Key::Delete:
            if (cursor_ < value_.size()) {
                value_.erase(cursor_, 1);
                if (on_change_) on_change_(value_);
            }
            return true;
            
        case Key::Left:
            if (cursor_ > 0) cursor_--;
            return true;
            
        case Key::Right:
            if (cursor_ < value_.size()) cursor_++;
            return true;
            
        case Key::Home:
            cursor_ = 0;
            return true;
            
        case Key::End:
            cursor_ = value_.size();
            return true;
            
        default:
            break;
    }
    
    return false;
}

// =============================================================================
// ListView
// =============================================================================

void ListView::set_items(const std::vector<std::string>& items) {
    items_.clear();
    for (const auto& s : items) {
        items_.push_back({s, "", {}});
    }
    selected_ = 0;
    scroll_offset_ = 0;
}

void ListView::set_items_ex(const std::vector<Item>& items) {
    items_ = items;
    selected_ = 0;
    scroll_offset_ = 0;
}

void ListView::clear() {
    items_.clear();
    selected_ = 0;
    scroll_offset_ = 0;
}

void ListView::set_selected(int idx) {
    if (idx >= 0 && idx < static_cast<int>(items_.size())) {
        selected_ = idx;
        ensure_visible();
    }
}

void ListView::ensure_visible() {
    int visible_rows = bounds_.h;
    
    if (selected_ < scroll_offset_) {
        scroll_offset_ = selected_;
    } else if (selected_ >= scroll_offset_ + visible_rows) {
        scroll_offset_ = selected_ - visible_rows + 1;
    }
}

void ListView::draw(Terminal& term) {
    int visible_rows = bounds_.h;
    
    Style normal_style = Style{Color::White};
    Style selected_style = Style{Color::Black, Color::Cyan, true};
    Style suffix_style = Style{Color::BrightBlack};
    
    for (int i = 0; i < visible_rows; ++i) {
        int idx = scroll_offset_ + i;
        int y = bounds_.y + i;
        
        // Clear line
        term.move_cursor(bounds_.x, y);
        term.write(std::string(bounds_.w, ' '));
        
        if (idx >= static_cast<int>(items_.size())) continue;
        
        const auto& item = items_[idx];
        bool is_selected = (idx == selected_) && focused_;
        
        Style style = is_selected ? selected_style : 
                      (item.style.fg != Color::Default ? item.style : normal_style);
        
        // Calculate available width
        int suffix_w = item.suffix.empty() ? 0 : static_cast<int>(item.suffix.size()) + 1;
        int text_w = bounds_.w - suffix_w;
        
        std::string text = fit_width(item.text, text_w);
        
        term.move_cursor(bounds_.x, y);
        term.write(text, style);
        
        if (!item.suffix.empty()) {
            term.write(" ", is_selected ? selected_style : normal_style);
            term.write(item.suffix, is_selected ? selected_style : suffix_style);
        }
    }
}

bool ListView::handle_key(Key key) {
    if (items_.empty()) return false;
    
    int old_selected = selected_;
    
    switch (key) {
        case Key::Up:
            if (selected_ > 0) selected_--;
            break;
            
        case Key::Down:
            if (selected_ < static_cast<int>(items_.size()) - 1) selected_++;
            break;
            
        case Key::PageUp:
            selected_ = std::max(0, selected_ - bounds_.h);
            break;
            
        case Key::PageDown:
            selected_ = std::min(static_cast<int>(items_.size()) - 1, selected_ + bounds_.h);
            break;
            
        case Key::Home:
            selected_ = 0;
            break;
            
        case Key::End:
            selected_ = static_cast<int>(items_.size()) - 1;
            break;
            
        case Key::Enter:
            if (on_select_) on_select_(selected_);
            return true;
            
        default:
            return false;
    }
    
    if (selected_ != old_selected) {
        ensure_visible();
        return true;
    }
    
    return false;
}

// =============================================================================
// TableView
// =============================================================================

void TableView::set_rows(const std::vector<std::vector<std::string>>& rows) {
    rows_ = rows;
    selected_ = 0;
    scroll_offset_ = 0;
}

void TableView::clear() {
    rows_.clear();
    selected_ = 0;
    scroll_offset_ = 0;
}

void TableView::set_selected(int idx) {
    if (idx >= 0 && idx < static_cast<int>(rows_.size())) {
        selected_ = idx;
        ensure_visible();
    }
}

void TableView::ensure_visible() {
    int visible_rows = bounds_.h - 1;  // -1 for header
    
    if (selected_ < scroll_offset_) {
        scroll_offset_ = selected_;
    } else if (selected_ >= scroll_offset_ + visible_rows) {
        scroll_offset_ = selected_ - visible_rows + 1;
    }
}

void TableView::draw(Terminal& term) {
    if (columns_.empty()) return;
    
    Style header_style = Style{Color::Cyan, Color::Default, true};
    Style normal_style = Style{Color::White};
    Style selected_style = Style{Color::Black, Color::Cyan, true};
    Style dim_style = Style{Color::BrightBlack};
    
    // Draw header
    int x = bounds_.x;
    term.move_cursor(x, bounds_.y);
    
    for (const auto& col : columns_) {
        std::string text = fit_width(col.header, col.width, ' ', col.right_align);
        term.write(text, header_style);
        term.write(" ", header_style);
        x += col.width + 1;
    }
    
    // Draw separator
    term.move_cursor(bounds_.x, bounds_.y + 1);
    term.write(std::string(bounds_.w, '-'), dim_style);
    
    // Draw rows
    int visible_rows = bounds_.h - 2;  // -2 for header + separator
    
    for (int i = 0; i < visible_rows; ++i) {
        int idx = scroll_offset_ + i;
        int y = bounds_.y + 2 + i;
        
        // Clear line
        term.move_cursor(bounds_.x, y);
        term.write(std::string(bounds_.w, ' '));
        
        if (idx >= static_cast<int>(rows_.size())) continue;
        
        const auto& row = rows_[idx];
        bool is_selected = (idx == selected_) && focused_;
        Style style = is_selected ? selected_style : normal_style;
        
        x = bounds_.x;
        term.move_cursor(x, y);
        
        for (size_t c = 0; c < columns_.size(); ++c) {
            const auto& col = columns_[c];
            std::string text = (c < row.size()) ? row[c] : "";
            text = fit_width(text, col.width, ' ', col.right_align);
            term.write(text, style);
            term.write(" ", style);
        }
    }
}

bool TableView::handle_key(Key key) {
    if (rows_.empty()) return false;
    
    int old_selected = selected_;
    
    switch (key) {
        case Key::Up:
            if (selected_ > 0) selected_--;
            break;
            
        case Key::Down:
            if (selected_ < static_cast<int>(rows_.size()) - 1) selected_++;
            break;
            
        case Key::PageUp:
            selected_ = std::max(0, selected_ - (bounds_.h - 2));
            break;
            
        case Key::PageDown:
            selected_ = std::min(static_cast<int>(rows_.size()) - 1, selected_ + (bounds_.h - 2));
            break;
            
        case Key::Home:
            selected_ = 0;
            break;
            
        case Key::End:
            selected_ = static_cast<int>(rows_.size()) - 1;
            break;
            
        case Key::Enter:
            if (on_select_) on_select_(selected_);
            return true;
            
        default:
            return false;
    }
    
    if (selected_ != old_selected) {
        ensure_visible();
        return true;
    }
    
    return false;
}

// =============================================================================
// Panel
// =============================================================================

void Panel::draw(Terminal& term) {
    Style border_style = focused_ ? Style{Color::Cyan} : Style{Color::White};
    Style title_style = Style{Color::Cyan, Color::Default, true};
    Style content_style = Style{Color::White};
    
    // Draw border
    term.draw_box(bounds_.x, bounds_.y, bounds_.w, bounds_.h, border_style);
    
    // Draw title
    if (!title_.empty()) {
        std::string title = " " + title_ + " ";
        term.move_cursor(bounds_.x + 2, bounds_.y);
        term.write(title, title_style);
    }
    
    // Draw content
    int inner_w = bounds_.w - 2;
    int inner_h = bounds_.h - 2;
    
    for (int i = 0; i < inner_h && i < static_cast<int>(content_.size()); ++i) {
        term.move_cursor(bounds_.x + 1, bounds_.y + 1 + i);
        std::string line = fit_width(content_[i], inner_w);
        term.write(line, content_style);
    }
}

// =============================================================================
// DetailView
// =============================================================================

void DetailView::draw(Terminal& term) {
    Style header_style = Style{Color::Yellow, Color::Default, true};
    Style label_style = Style{Color::BrightBlack};
    Style value_style = Style{Color::White};
    
    int visible = bounds_.h;
    int y = bounds_.y;
    
    for (int i = 0; i < visible; ++i) {
        int idx = scroll_offset_ + i;
        
        // Clear line
        term.move_cursor(bounds_.x, y + i);
        term.write(std::string(bounds_.w, ' '));
        
        if (idx >= static_cast<int>(entries_.size())) continue;
        
        const auto& entry = entries_[idx];
        term.move_cursor(bounds_.x, y + i);
        
        if (entry.header) {
            // Section header
            term.write(entry.label, header_style);
        } else {
            // Label: Value
            int label_w = std::min(15, bounds_.w / 3);
            std::string label = fit_width(entry.label + ":", label_w);
            std::string value = fit_width(entry.value, bounds_.w - label_w - 1);
            
            term.write(label, label_style);
            term.write(" ", label_style);
            term.write(value, value_style);
        }
    }
}

bool DetailView::handle_key(Key key) {
    int visible = bounds_.h;
    int total = static_cast<int>(entries_.size());
    
    switch (key) {
        case Key::Up:
            if (scroll_offset_ > 0) scroll_offset_--;
            return true;
            
        case Key::Down:
            if (scroll_offset_ < total - visible) scroll_offset_++;
            return true;
            
        case Key::PageUp:
            scroll_offset_ = std::max(0, scroll_offset_ - visible);
            return true;
            
        case Key::PageDown:
            scroll_offset_ = std::min(std::max(0, total - visible), scroll_offset_ + visible);
            return true;
            
        default:
            break;
    }
    
    return false;
}

// =============================================================================
// StatusBar
// =============================================================================

void StatusBar::draw(Terminal& term) {
    Style style = Style{Color::Black, Color::White};
    
    // Fill background
    term.move_cursor(bounds_.x, bounds_.y);
    term.write(std::string(bounds_.w, ' '), style);
    
    // Left text
    if (!left_.empty()) {
        term.move_cursor(bounds_.x + 1, bounds_.y);
        term.write(left_, style);
    }
    
    // Center text
    if (!center_.empty()) {
        int x = bounds_.x + (bounds_.w - static_cast<int>(center_.size())) / 2;
        term.move_cursor(x, bounds_.y);
        term.write(center_, style);
    }
    
    // Right text
    if (!right_.empty()) {
        int x = bounds_.x + bounds_.w - static_cast<int>(right_.size()) - 1;
        term.move_cursor(x, bounds_.y);
        term.write(right_, style);
    }
}

// =============================================================================
// MenuBar
// =============================================================================

void MenuBar::draw(Terminal& term) {
    Style bg_style = Style{Color::White, Color::Blue};
    Style key_style = Style{Color::Yellow, Color::Blue, true};
    
    // Fill background
    term.move_cursor(bounds_.x, bounds_.y);
    term.write(std::string(bounds_.w, ' '), bg_style);
    
    int x = bounds_.x + 1;
    term.move_cursor(x, bounds_.y);
    
    for (const auto& item : items_) {
        // Highlight shortcut key
        for (char c : item.label) {
            if (std::tolower(c) == std::tolower(item.shortcut)) {
                term.write(std::string(1, c), key_style);
            } else {
                term.write(std::string(1, c), bg_style);
            }
        }
        term.write("  ", bg_style);
    }
}

}  // namespace athena::tui
