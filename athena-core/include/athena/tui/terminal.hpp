// ATHENA TUI - Terminal Abstraction Layer
// Cross-platform terminal handling (Windows/macOS/Linux)
// Uses ANSI escape codes for rendering
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_TUI_TERMINAL_HPP
#define ATHENA_TUI_TERMINAL_HPP

#include <string>
#include <functional>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <conio.h>
#else
    #include <sys/ioctl.h>
    #include <termios.h>
    #include <unistd.h>
#endif

namespace athena::tui {

// =============================================================================
// Terminal Size
// =============================================================================

struct TerminalSize {
    int width = 80;
    int height = 24;
};

// =============================================================================
// Key Codes
// =============================================================================

enum class Key : int {
    None = 0,
    Enter = 13,
    Escape = 27,
    Backspace = 127,
    Tab = 9,
    
    // Arrow keys (after escape sequence)
    Up = 1000,
    Down,
    Left,
    Right,
    
    // Function keys
    Home,
    End,
    PageUp,
    PageDown,
    Delete,
    Insert,
    
    // Special
    CtrlC = 3,
    CtrlD = 4,
    CtrlQ = 17,
};

// =============================================================================
// Colors
// =============================================================================

enum class Color : int {
    Default = 0,
    Black = 30,
    Red = 31,
    Green = 32,
    Yellow = 33,
    Blue = 34,
    Magenta = 35,
    Cyan = 36,
    White = 37,
    
    // Bright variants
    BrightBlack = 90,
    BrightRed = 91,
    BrightGreen = 92,
    BrightYellow = 93,
    BrightBlue = 94,
    BrightMagenta = 95,
    BrightCyan = 96,
    BrightWhite = 97,
};

// =============================================================================
// Text Style
// =============================================================================

struct Style {
    Color fg = Color::Default;
    Color bg = Color::Default;
    bool bold = false;
    bool dim = false;
    bool underline = false;
    bool reverse = false;
    
    static Style Default() { return {}; }
    static Style Bold(Color c) { return {c, Color::Default, true}; }
    static Style Reverse() { return {Color::Default, Color::Default, false, false, false, true}; }
};

// =============================================================================
// Terminal Class
// =============================================================================

class Terminal {
public:
    Terminal();
    ~Terminal();
    
    // Disable copy
    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    
    // Initialize/cleanup raw mode
    bool init();
    void cleanup();
    
    // Get terminal size
    TerminalSize size() const;
    
    // Input
    Key read_key();
    char read_char();
    bool key_available();
    
    // Output - ANSI sequences
    void clear();
    void clear_line();
    void move_cursor(int x, int y);
    void hide_cursor();
    void show_cursor();
    
    // Styled output
    void set_style(const Style& style);
    void reset_style();
    void write(const std::string& text);
    void write(const std::string& text, const Style& style);
    void write_at(int x, int y, const std::string& text);
    void write_at(int x, int y, const std::string& text, const Style& style);
    
    // Flush output
    void flush();
    
    // Drawing helpers
    void draw_box(int x, int y, int w, int h, const Style& style = {});
    void draw_hline(int x, int y, int len, char c = '-');
    void draw_vline(int x, int y, int len, char c = '|');
    void fill_rect(int x, int y, int w, int h, char c = ' ');
    
private:
    bool initialized_ = false;
    
#ifdef _WIN32
    HANDLE hStdin_;
    HANDLE hStdout_;
    DWORD originalMode_;
#else
    struct termios original_termios_;
#endif
    
    void enable_raw_mode();
    void disable_raw_mode();
    void enable_ansi();
};

// =============================================================================
// Screen Buffer (double buffering)
// =============================================================================

struct Cell {
    char ch = ' ';
    Style style;
};

class ScreenBuffer {
public:
    ScreenBuffer(int width, int height);
    
    void resize(int width, int height);
    void clear();
    
    void set(int x, int y, char ch, const Style& style = {});
    void write(int x, int y, const std::string& text, const Style& style = {});
    
    Cell& at(int x, int y);
    const Cell& at(int x, int y) const;
    
    int width() const { return width_; }
    int height() const { return height_; }
    
    // Render to terminal (only changed cells)
    void render(Terminal& term, ScreenBuffer* previous = nullptr);
    
private:
    int width_;
    int height_;
    std::vector<Cell> cells_;
    
    int index(int x, int y) const { return y * width_ + x; }
    bool in_bounds(int x, int y) const {
        return x >= 0 && x < width_ && y >= 0 && y < height_;
    }
};

// =============================================================================
// Utility Functions
// =============================================================================

// Truncate or pad string to exact width
std::string fit_width(const std::string& text, int width, char pad = ' ', bool right_align = false);

// Unicode-aware string length (approximate)
int display_width(const std::string& text);

// Box drawing characters (UTF-8)
namespace box {
    constexpr const char* TL = "┌";  // Top-left
    constexpr const char* TR = "┐";  // Top-right
    constexpr const char* BL = "└";  // Bottom-left
    constexpr const char* BR = "┘";  // Bottom-right
    constexpr const char* H  = "─";  // Horizontal
    constexpr const char* V  = "│";  // Vertical
    constexpr const char* LT = "├";  // Left-T
    constexpr const char* RT = "┤";  // Right-T
    constexpr const char* TT = "┬";  // Top-T
    constexpr const char* BT = "┴";  // Bottom-T
    constexpr const char* X  = "┼";  // Cross
    
    // Double line
    constexpr const char* DTL = "╔";
    constexpr const char* DTR = "╗";
    constexpr const char* DBL = "╚";
    constexpr const char* DBR = "╝";
    constexpr const char* DH  = "═";
    constexpr const char* DV  = "║";
}

}  // namespace athena::tui

#endif  // ATHENA_TUI_TERMINAL_HPP
