// ATHENA Core - Unified Application Entry Point
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/integrated.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstring>
#include <dirent.h>

using namespace athena;

// =============================================================================
// GLFW Error Callback
// =============================================================================

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// =============================================================================
// Print Usage
// =============================================================================

static void print_usage(const char* program) {
    fprintf(stderr, "ATHENA %s - Unified Military Simulation Platform\n\n", 
            IntegratedVersion::STRING);
    fprintf(stderr, "Usage: %s [mode] [options]\n\n", program);
    fprintf(stderr, "Modes:\n");
    fprintf(stderr, "  gui [data_path]      Launch full GUI (default)\n");
    fprintf(stderr, "  browser [data_path]  Launch platform browser only\n");
    fprintf(stderr, "  run <scenario>       Run single simulation (CLI)\n");
    fprintf(stderr, "  batch <scenario>     Run Monte Carlo batch (CLI)\n");
    fprintf(stderr, "  tui [data_path]      Terminal browser (no GUI)\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h, --help           Show this help\n");
    fprintf(stderr, "  -v, --version        Show version\n");
    fprintf(stderr, "  -s, --seed <value>   Master seed for simulation\n");
    fprintf(stderr, "  -n, --iterations N   Monte Carlo iterations (batch mode)\n");
    fprintf(stderr, "  -o, --output <file>  Output file for results\n\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s gui                          # Full GUI\n", program);
    fprintf(stderr, "  %s browser data/platforms       # Browser only\n", program);
    fprintf(stderr, "  %s run scenario.json            # Single run\n", program);
    fprintf(stderr, "  %s batch -n 1000 scenario.json  # 1000 iterations\n", program);
}

// =============================================================================
// Run GUI Application
// =============================================================================

static int run_gui_app(AppMode mode, const std::string& data_path) {
    // Initialize GLFW
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }
    
    // GL 3.3 + GLSL 330
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    
    // Window title based on mode
    const char* title = (mode == AppMode::GUI_FULL) ? 
                        "ATHENA - Military Simulation Platform" :
                        "ATHENA - Platform Browser";
    
    // Create window
    GLFWwindow* window = glfwCreateWindow(1600, 1000, title, nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // Enable vsync
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    // Initialize application
    UnifiedApp app;
    if (!app.init(mode, data_path)) {
        fprintf(stderr, "Failed to initialize application\n");
        fprintf(stderr, "Make sure platform database exists at: %s\n", data_path.c_str());
        
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    
    printf("ATHENA %s initialized\n", IntegratedVersion::STRING);
    printf("Loaded %zu platforms from %s\n", 
           app.database().count(), data_path.c_str());
    
    // Main loop
    double last_time = glfwGetTime();
    
    while (!glfwWindowShouldClose(window) && !app.should_close()) {
        // Calculate delta time
        double current_time = glfwGetTime();
        double delta_time = current_time - last_time;
        last_time = current_time;
        
        // Poll events
        glfwPollEvents();
        
        // Update application
        app.update(delta_time);
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render application UI
        app.render();
        
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.06f, 0.06f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }
    
    // Cleanup
    app.shutdown();
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}

// =============================================================================
// Main Entry Point
// =============================================================================

int main(int argc, char* argv[]) {
    // Parse arguments
    std::string mode_str;
    std::string data_path = "data/platforms";
    std::string scenario_path;
    std::string output_path;
    Seed seed = 0;
    u32 iterations = 100;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        
        if (arg == "-v" || arg == "--version") {
            printf("ATHENA %s (%s)\n", IntegratedVersion::STRING, IntegratedVersion::CODENAME);
            printf("Platform Database: 1,226 platforms\n");
            return 0;
        }
        
        if ((arg == "-s" || arg == "--seed") && i + 1 < argc) {
            seed = std::stoull(argv[++i]);
            continue;
        }
        
        if ((arg == "-n" || arg == "--iterations") && i + 1 < argc) {
            iterations = std::stoul(argv[++i]);
            continue;
        }
        
        if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_path = argv[++i];
            continue;
        }
        
        if (arg[0] != '-') {
            if (mode_str.empty()) {
                mode_str = arg;
            } else if (mode_str == "run" || mode_str == "batch") {
                scenario_path = arg;
            } else {
                data_path = arg;
            }
        }
    }
    
    // Default to GUI mode
    if (mode_str.empty()) {
        mode_str = "gui";
    }
    
    // Resolve data path: try several common locations.
    // Handles running from ATHENA/ root, athena-core/, or build/ directory.
    {
        auto dir_exists = [](const char* path) -> bool {
            DIR* d = opendir(path);
            if (d) { closedir(d); return true; }
            return false;
        };
        
        if (!dir_exists(data_path.c_str())) {
            const char* fallbacks[] = {
                "athena-core/data/platforms",
                "../data/platforms",
                "../athena-core/data/platforms",
                nullptr
            };
            for (int i = 0; fallbacks[i]; ++i) {
                if (dir_exists(fallbacks[i])) {
                    data_path = fallbacks[i];
                    break;
                }
            }
        }
    }
    
    // Route to appropriate handler
    if (mode_str == "gui") {
        return run_gui_app(AppMode::GUI_FULL, data_path);
    }
    
    if (mode_str == "browser") {
        return run_gui_app(AppMode::GUI_BROWSER, data_path);
    }
    
    if (mode_str == "tui") {
        return run_tui_browser(data_path);
    }
    
    if (mode_str == "run") {
        if (scenario_path.empty()) {
            fprintf(stderr, "Error: No scenario file specified\n");
            return 1;
        }
        
        UnifiedApp app;
        if (!app.init(AppMode::CLI_SIMULATION, data_path)) {
            return 1;
        }
        
        if (!app.load_scenario(scenario_path)) {
            return 1;
        }
        
        if (!app.start_simulation(seed)) {
            return 1;
        }
        
        // Run to completion
        while (!app.sim_control().is_complete) {
            app.step_simulation(1);
        }
        
        printf("\nSimulation complete\n");
        printf("  Ticks: %u\n", app.sim_control().current_tick);
        printf("  Blue: %u/%u\n", app.sim_control().blue_alive, app.sim_control().blue_total);
        printf("  Red: %u/%u\n", app.sim_control().red_alive, app.sim_control().red_total);
        
        return 0;
    }
    
    if (mode_str == "batch") {
        if (scenario_path.empty()) {
            fprintf(stderr, "Error: No scenario file specified\n");
            return 1;
        }
        
        UnifiedApp app;
        if (!app.init(AppMode::CLI_BATCH, data_path)) {
            return 1;
        }
        
        if (!app.load_scenario(scenario_path)) {
            return 1;
        }
        
        printf("Running Monte Carlo: %u iterations\n", iterations);
        
        app.configure_monte_carlo(iterations, seed, false);
        
        app.set_mc_progress_callback([iterations](u32 current, u32 total) {
            printf("\r  Progress: %u/%u (%.1f%%)", current, total, 
                   100.0 * current / total);
            fflush(stdout);
        });
        
        if (!app.run_monte_carlo()) {
            return 1;
        }
        
        printf("\n\n");
        
        const auto& stats = app.mc_control().stats;
        printf("Results:\n");
        printf("  Completed: %u iterations\n", stats.completed_iterations);
        printf("  Blue wins: %u (%.1f%%)\n", stats.blue_wins, 
               100.0 * stats.blue_wins / stats.completed_iterations);
        printf("  Red wins: %u (%.1f%%)\n", stats.red_wins,
               100.0 * stats.red_wins / stats.completed_iterations);
        printf("  Draws: %u (%.1f%%)\n", stats.draws,
               100.0 * stats.draws / stats.completed_iterations);
        printf("\n");
        printf("  Blue survival: %.1f%% +/- %.1f%%\n",
               stats.blue_survival_rate.mean * 100,
               stats.blue_survival_rate.stddev * 100);
        printf("  Red survival: %.1f%% +/- %.1f%%\n",
               stats.red_survival_rate.mean * 100,
               stats.red_survival_rate.stddev * 100);
        
        if (!output_path.empty()) {
            if (app.export_results_json(output_path)) {
                printf("\nResults exported to: %s\n", output_path.c_str());
            }
        }
        
        return 0;
    }
    
    fprintf(stderr, "Unknown mode: %s\n", mode_str.c_str());
    print_usage(argv[0]);
    return 1;
}
