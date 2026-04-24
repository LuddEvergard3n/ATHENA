// ATHENA TUI - Terminal Implementation
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/tui/terminal.hpp"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace athena::tui {

// =============================================================================
// Terminal Implementation
// =============================================================================

Terminal::Terminal() {
#ifdef _WIN32
    hStdin_ = GetStdHandle(STD_INPUT_HANDLE);
    hStdout_ = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
}

Terminal::~Terminal() {
    if (initialized_) {
        cleanup();
    }
}

bool Terminal::init() {
    if (initialized_) return true;
    
    enable_ansi();
    enable_raw_mode();
    hide_cursor();
    clear();
    
    initialized_ = true;
    return true;
}

void Terminal::cleanup() {
    if (!initialized_) return;
    
    show_cursor();
    reset_style();
    clear();
    move_cursor(0, 0);
    disable_raw_mode();
    flush();
    
    initialized_ = false;
}

void Terminal::enable_ansi() {
#ifdef _WIN32
    // Enable ANSI escape codes on Windows 10+
    DWORD mode = 0;
    GetConsoleMode(hStdout_, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hStdout_, mode);
    
    // Set UTF-8 output
    SetConsoleOutputCP(CP_UTF8);
#endif
}

void Terminal::enable_raw_mode() {
#ifdef _WIN32
    GetConsoleMode(hStdin_, &originalMode_);
    DWORD mode = originalMode_;
    mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
    mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    SetConsoleMode(hStdin_, mode);
#else
    tcgetattr(STDIN_FILENO, &original_termios_);
    struct termios raw = original_termios_;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;  // 100ms timeout
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
#endif
}

void Terminal::disable_raw_mode() {
#ifdef _WIN32
    SetConsoleMode(hStdin_, originalMode_);
#else
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios_);
#endif
}

TerminalSize Terminal::size() const {
    TerminalSize sz;
    
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hStdout_, &csbi)) {
        sz.width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        sz.height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        sz.width = w.ws_col;
        sz.height = w.ws_row;
    }
#endif
    
    return sz;
}

Key Terminal::read_key() {
#ifdef _WIN32
    if (!_kbhit()) return Key::None;
    
    int ch = _getch();
    
    // Extended key (arrow keys, etc.)
    if (ch == 0 || ch == 224) {
        ch = _getch();
        switch (ch) {
            case 72: return Key::Up;
            case 80: return Key::Down;
            case 75: return Key::Left;
            case 77: return Key::Right;
            case 71: return Key::Home;
            case 79: return Key::End;
            case 73: return Key::PageUp;
            case 81: return Key::PageDown;
            case 82: return Key::Insert;
            case 83: return Key::Delete;
        }
        return Key::None;
    }
    
    if (ch == 13) return Key::Enter;
    if (ch == 27) return Key::Escape;
    if (ch == 8 || ch == 127) return Key::Backspace;
    if (ch == 9) return Key::Tab;
    if (ch == 3) return Key::CtrlC;
    if (ch == 4) return Key::CtrlD;
    if (ch == 17) return Key::CtrlQ;
    
    return static_cast<Key>(ch);
#else
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return Key::None;
    
    // Escape sequence
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return Key::Escape;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return Key::Escape;
        
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return Key::Escape;
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return Key::Home;
                        case '3': return Key::Delete;
                        case '4': return Key::End;
                        case '5': return Key::PageUp;
                        case '6': return Key::PageDown;
                        case '7': return Key::Home;
                        case '8': return Key::End;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return Key::Up;
                    case 'B': return Key::Down;
                    case 'C': return Key::Right;
                    case 'D': return Key::Left;
                    case 'H': return Key::Home;
                    case 'F': return Key::End;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return Key::Home;
                case 'F': return Key::End;
            }
        }
        
        return Key::Escape;
    }
    
    if (c == '\r' || c == '\n') return Key::Enter;
    if (c == 127 || c == 8) return Key::Backspace;
    if (c == '\t') return Key::Tab;
    if (c == 3) return Key::CtrlC;
    if (c == 4) return Key::CtrlD;
    if (c == 17) return Key::CtrlQ;
    
    return static_cast<Key>(c);
#endif
}

char Terminal::read_char() {
    Key k = read_key();
    int code = static_cast<int>(k);
    if (code >= 32 && code < 127) {
        return static_cast<char>(code);
    }
    return '\0';
}

bool Terminal::key_available() {
#ifdef _WIN32
    return _kbhit() != 0;
#else
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = {0, 0};
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
#endif
}

void Terminal::clear() {
    std::cout << "\x1b[2J";  // Clear entire screen
}

void Terminal::clear_line() {
    std::cout << "\x1b[2K";  // Clear entire line
}

void Terminal::move_cursor(int x, int y) {
    std::cout << "\x1b[" << (y + 1) << ";" << (x + 1) << "H";
}

void Terminal::hide_cursor() {
    std::cout << "\x1b[?25l";
}

void Terminal::show_cursor() {
    std::cout << "\x1b[?25h";
}

void Terminal::set_style(const Style& style) {
    std::cout << "\x1b[0m";  // Reset first
    
    if (style.bold) std::cout << "\x1b[1m";
    if (style.dim) std::cout << "\x1b[2m";
    if (style.underline) std::cout << "\x1b[4m";
    if (style.reverse) std::cout << "\x1b[7m";
    
    if (style.fg != Color::Default) {
        std::cout << "\x1b[" << static_cast<int>(style.fg) << "m";
    }
    if (style.bg != Color::Default) {
        std::cout << "\x1b[" << (static_cast<int>(style.bg) + 10) << "m";
    }
}

void Terminal::reset_style() {
    std::cout << "\x1b[0m";
}

void Terminal::write(const std::string& text) {
    std::cout << text;
}

void Terminal::write(const std::string& text, const Style& style) {
    set_style(style);
    std::cout << text;
    reset_style();
}

void Terminal::write_at(int x, int y, const std::string& text) {
    move_cursor(x, y);
    std::cout << text;
}

void Terminal::write_at(int x, int y, const std::string& text, const Style& style) {
    move_cursor(x, y);
    set_style(style);
    std::cout << text;
    reset_style();
}

void Terminal::flush() {
    std::cout.flush();
}

void Terminal::draw_box(int x, int y, int w, int h, const Style& style) {
    set_style(style);
    
    // Top border
    move_cursor(x, y);
    std::cout << box::TL;
    for (int i = 0; i < w - 2; ++i) std::cout << box::H;
    std::cout << box::TR;
    
    // Sides
    for (int i = 1; i < h - 1; ++i) {
        move_cursor(x, y + i);
        std::cout << box::V;
        move_cursor(x + w - 1, y + i);
        std::cout << box::V;
    }
    
    // Bottom border
    move_cursor(x, y + h - 1);
    std::cout << box::BL;
    for (int i = 0; i < w - 2; ++i) std::cout << box::H;
    std::cout << box::BR;
    
    reset_style();
}

void Terminal::draw_hline(int x, int y, int len, char c) {
    move_cursor(x, y);
    for (int i = 0; i < len; ++i) std::cout << c;
}

void Terminal::draw_vline(int x, int y, int len, char c) {
    for (int i = 0; i < len; ++i) {
        move_cursor(x, y + i);
        std::cout << c;
    }
}

void Terminal::fill_rect(int x, int y, int w, int h, char c) {
    std::string line(w, c);
    for (int i = 0; i < h; ++i) {
        move_cursor(x, y + i);
        std::cout << line;
    }
}

// =============================================================================
// ScreenBuffer Implementation
// =============================================================================

ScreenBuffer::ScreenBuffer(int width, int height)
    : width_(width), height_(height), cells_(width * height) {
}

void ScreenBuffer::resize(int width, int height) {
    width_ = width;
    height_ = height;
    cells_.resize(width * height);
    clear();
}

void ScreenBuffer::clear() {
    for (auto& cell : cells_) {
        cell.ch = ' ';
        cell.style = {};
    }
}

void ScreenBuffer::set(int x, int y, char ch, const Style& style) {
    if (in_bounds(x, y)) {
        cells_[index(x, y)] = {ch, style};
    }
}

void ScreenBuffer::write(int x, int y, const std::string& text, const Style& style) {
    for (size_t i = 0; i < text.size(); ++i) {
        set(x + static_cast<int>(i), y, text[i], style);
    }
}

Cell& ScreenBuffer::at(int x, int y) {
    static Cell empty;
    if (in_bounds(x, y)) return cells_[index(x, y)];
    return empty;
}

const Cell& ScreenBuffer::at(int x, int y) const {
    static const Cell empty;
    if (in_bounds(x, y)) return cells_[index(x, y)];
    return empty;
}

void ScreenBuffer::render(Terminal& term, ScreenBuffer* previous) {
    Style current_style;
    
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const Cell& cell = at(x, y);
            
            // Skip if unchanged
            if (previous && 
                cell.ch == previous->at(x, y).ch &&
                std::memcmp(&cell.style, &previous->at(x, y).style, sizeof(Style)) == 0) {
                continue;
            }
            
            term.move_cursor(x, y);
            
            // Update style if needed
            if (std::memcmp(&cell.style, &current_style, sizeof(Style)) != 0) {
                term.set_style(cell.style);
                current_style = cell.style;
            }
            
            term.write(std::string(1, cell.ch));
        }
    }
    
    term.reset_style();
    term.flush();
}

// =============================================================================
// Utility Functions
// =============================================================================

std::string fit_width(const std::string& text, int width, char pad, bool right_align) {
    int len = static_cast<int>(text.size());
    
    if (len >= width) {
        return text.substr(0, width);
    }
    
    int padding = width - len;
    if (right_align) {
        return std::string(padding, pad) + text;
    } else {
        return text + std::string(padding, pad);
    }
}

int display_width(const std::string& text) {
    // Simple implementation - counts bytes
    // For full Unicode support, would need proper handling
    int width = 0;
    for (unsigned char c : text) {
        // Skip UTF-8 continuation bytes
        if ((c & 0xC0) != 0x80) {
            ++width;
        }
    }
    return width;
}

}  // namespace athena::tui
