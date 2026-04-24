// ATHENA TUI - Widget Components
// Reusable UI components for the browser
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_TUI_WIDGETS_HPP
#define ATHENA_TUI_WIDGETS_HPP

#include "terminal.hpp"
#include <vector>
#include <string>
#include <functional>

namespace athena::tui {

// =============================================================================
// Widget Base
// =============================================================================

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    
    bool contains(int px, int py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

class Widget {
public:
    virtual ~Widget() = default;
    
    virtual void draw(Terminal& term) = 0;
    virtual bool handle_key(Key key) { return false; }
    
    void set_bounds(const Rect& r) { bounds_ = r; }
    const Rect& bounds() const { return bounds_; }
    
    void set_focused(bool f) { focused_ = f; }
    bool focused() const { return focused_; }
    
protected:
    Rect bounds_;
    bool focused_ = false;
};

// =============================================================================
// Text Input
// =============================================================================

class TextInput : public Widget {
public:
    using OnChange = std::function<void(const std::string&)>;
    
    void draw(Terminal& term) override;
    bool handle_key(Key key) override;
    
    void set_placeholder(const std::string& ph) { placeholder_ = ph; }
    void set_value(const std::string& v) { value_ = v; cursor_ = v.size(); }
    const std::string& value() const { return value_; }
    
    void on_change(OnChange cb) { on_change_ = cb; }
    
private:
    std::string value_;
    std::string placeholder_ = "Search...";
    size_t cursor_ = 0;
    OnChange on_change_;
};

// =============================================================================
// List View
// =============================================================================

class ListView : public Widget {
public:
    using OnSelect = std::function<void(int index)>;
    
    void draw(Terminal& term) override;
    bool handle_key(Key key) override;
    
    void set_items(const std::vector<std::string>& items);
    void clear();
    
    int selected() const { return selected_; }
    void set_selected(int idx);
    
    void on_select(OnSelect cb) { on_select_ = cb; }
    
    // For custom rendering (with counts, icons, etc.)
    struct Item {
        std::string text;
        std::string suffix;  // e.g., count
        Style style;
    };
    void set_items_ex(const std::vector<Item>& items);
    
private:
    std::vector<Item> items_;
    int selected_ = 0;
    int scroll_offset_ = 0;
    OnSelect on_select_;
    
    void ensure_visible();
};

// =============================================================================
// Table View
// =============================================================================

class TableView : public Widget {
public:
    using OnSelect = std::function<void(int row)>;
    
    struct Column {
        std::string header;
        int width;
        bool right_align = false;
    };
    
    void draw(Terminal& term) override;
    bool handle_key(Key key) override;
    
    void set_columns(const std::vector<Column>& cols) { columns_ = cols; }
    void set_rows(const std::vector<std::vector<std::string>>& rows);
    void clear();
    
    int selected() const { return selected_; }
    void set_selected(int idx);
    
    void on_select(OnSelect cb) { on_select_ = cb; }
    
private:
    std::vector<Column> columns_;
    std::vector<std::vector<std::string>> rows_;
    int selected_ = 0;
    int scroll_offset_ = 0;
    OnSelect on_select_;
    
    void ensure_visible();
};

// =============================================================================
// Panel (Container with title)
// =============================================================================

class Panel : public Widget {
public:
    void draw(Terminal& term) override;
    
    void set_title(const std::string& t) { title_ = t; }
    void set_content(const std::vector<std::string>& lines) { content_ = lines; }
    
    // Inner bounds for child widgets
    Rect inner_bounds() const {
        return {bounds_.x + 1, bounds_.y + 1, bounds_.w - 2, bounds_.h - 2};
    }
    
private:
    std::string title_;
    std::vector<std::string> content_;
};

// =============================================================================
// Detail View (Key-Value pairs)
// =============================================================================

class DetailView : public Widget {
public:
    struct Entry {
        std::string label;
        std::string value;
        bool header = false;  // Section header
    };
    
    void draw(Terminal& term) override;
    bool handle_key(Key key) override;
    
    void set_entries(const std::vector<Entry>& entries) { entries_ = entries; }
    void clear() { entries_.clear(); scroll_offset_ = 0; }
    
private:
    std::vector<Entry> entries_;
    int scroll_offset_ = 0;
};

// =============================================================================
// Status Bar
// =============================================================================

class StatusBar : public Widget {
public:
    void draw(Terminal& term) override;
    
    void set_left(const std::string& text) { left_ = text; }
    void set_center(const std::string& text) { center_ = text; }
    void set_right(const std::string& text) { right_ = text; }
    
private:
    std::string left_;
    std::string center_;
    std::string right_;
};

// =============================================================================
// Menu Bar
// =============================================================================

class MenuBar : public Widget {
public:
    struct MenuItem {
        std::string label;
        char shortcut = '\0';
    };
    
    void draw(Terminal& term) override;
    
    void set_items(const std::vector<MenuItem>& items) { items_ = items; }
    
private:
    std::vector<MenuItem> items_;
};

}  // namespace athena::tui

#endif  // ATHENA_TUI_WIDGETS_HPP
