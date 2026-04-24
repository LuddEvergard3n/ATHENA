// ATHENA Platform Browser - Main Entry Point
// Terminal-based platform database browser
//
// Usage: athena-browser [data_path]
//   data_path: Path to platform database (default: data/platforms)
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/tui/browser.hpp"
#include <iostream>
#include <string>

void print_usage(const char* program) {
    std::cerr << "ATHENA Platform Browser v0.8.4\n\n";
    std::cerr << "Usage: " << program << " [data_path]\n\n";
    std::cerr << "Arguments:\n";
    std::cerr << "  data_path    Path to platform database directory\n";
    std::cerr << "               Default: data/platforms\n\n";
    std::cerr << "Controls:\n";
    std::cerr << "  Tab          Switch focus between panels\n";
    std::cerr << "  /            Focus search box\n";
    std::cerr << "  Arrow keys   Navigate\n";
    std::cerr << "  Enter        Select/Open detail\n";
    std::cerr << "  Escape       Close detail/Clear search\n";
    std::cerr << "  Ctrl+Q       Quit\n";
}

int main(int argc, char* argv[]) {
    std::string data_path = "data/platforms";
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        
        if (arg[0] != '-') {
            data_path = arg;
        }
    }
    
    // Create and initialize browser
    athena::tui::Browser browser;
    
    if (!browser.init(data_path)) {
        std::cerr << "Error: Failed to initialize browser.\n";
        std::cerr << "Make sure the platform database exists at: " << data_path << "\n";
        return 1;
    }
    
    // Run main loop
    browser.run();
    
    return 0;
}
