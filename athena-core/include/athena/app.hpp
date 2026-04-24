// ATHENA Core - Integrated Application
// Unified application combining simulation, browsers, and analysis
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_APP_HPP
#define ATHENA_APP_HPP

#include "athena/types.hpp"
#include "athena/scenario.hpp"
#include "athena/platform_loader.hpp"
#include "athena/systems.hpp"
#include "athena/analysis.hpp"

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace athena {

// =============================================================================
// Application Mode
// =============================================================================

enum class AppMode {
    CLI,            // Command-line batch simulation
    TUI_BROWSER,    // Terminal platform browser
    GUI_BROWSER,    // Graphical platform browser  
    GUI_FULL,       // Full GUI with simulation + browser
    SERVER          // Headless server mode (future)
};

// =============================================================================
// Simulation State (for real-time visualization)
// =============================================================================

struct SimulationState {
    // Current tick
    u32 current_tick = 0;
    u32 max_ticks = 168;
    
    // Running state
    bool is_running = false;
    bool is_paused = false;
    bool is_complete = false;
    
    // Speed control
    f64 time_scale = 1.0;      // 1.0 = real-time (1 tick = 1 hour)
    u32 ticks_per_frame = 1;   // For fast-forward
    
    // Statistics (live)
    u32 blue_alive = 0;
    u32 blue_total = 0;
    u32 red_alive = 0;
    u32 red_total = 0;
    
    // Events log
    struct Event {
        u32 tick;
        std::string message;
        std::string type;  // "combat", "detection", "movement", etc.
    };
    std::vector<Event> events;
    
    // Entity positions for visualization
    struct EntityVis {
        EntityId id;
        std::string name;
        std::string side;
        f64 x, y;
        f64 heading;
        bool alive;
        f64 health;
    };
    std::vector<EntityVis> entities;
};

// =============================================================================
// Scenario Builder State
// =============================================================================

struct ScenarioBuilderState {
    // Scenario being edited
    Scenario scenario;
    
    // Selection
    int selected_actor_idx = -1;
    
    // Map bounds
    f64 map_min_x = 0;
    f64 map_max_x = 100000;
    f64 map_min_y = 0;
    f64 map_max_y = 100000;
    
    // Drag state
    bool dragging = false;
    int drag_actor_idx = -1;
    
    // Platform selection (from database)
    const PlatformSpec* selected_platform = nullptr;
    std::string placement_side = "blue";
    
    // UI state
    bool show_add_actor_dialog = false;
    bool show_scenario_settings = false;
    bool modified = false;
};

// =============================================================================
// Analysis Results State
// =============================================================================

struct AnalysisState {
    // Monte Carlo results
    analysis::BatchStatistics stats;
    std::vector<analysis::IterationResult> results;
    
    // Convergence
    analysis::ConvergenceMetrics convergence;
    
    // Selected for detail view
    int selected_iteration = -1;
    
    // Charts data
    std::vector<f64> blue_survival_histogram;
    std::vector<f64> red_survival_histogram;
    std::vector<f64> duration_histogram;
    
    // Export state
    std::string export_path;
    bool export_binary = false;
};

// =============================================================================
// Integrated Application State
// =============================================================================

struct IntegratedAppState {
    // Mode
    AppMode mode = AppMode::GUI_FULL;
    
    // Window
    int window_width = 1600;
    int window_height = 1000;
    bool should_close = false;
    bool fullscreen = false;
    
    // Platform database (shared)
    PlatformDatabase platform_db;
    bool db_loaded = false;
    std::string db_path = "data/platforms";
    
    // Browser state (filtering, selection)
    struct BrowserState {
        char search_buffer[256] = {0};
        PlatformCategory selected_category = PlatformCategory::UNKNOWN;
        std::string selected_country;
        std::vector<const PlatformSpec*> filtered;
        int selected_idx = -1;
        const PlatformSpec* selected = nullptr;
        std::vector<const PlatformSpec*> compare_list;
        bool show_detail = false;
        bool show_compare = false;
    } browser;
    
    // Scenario state
    ScenarioBuilderState scenario_builder;
    std::string loaded_scenario_path;
    
    // Simulation state
    SimulationState simulation;
    systems::SystemsBundle* systems = nullptr;
    
    // Analysis state
    AnalysisState analysis;
    
    // Monte Carlo config
    analysis::BatchConfig mc_config;
    
    // Active view
    enum class View {
        BROWSER,
        SCENARIO_EDITOR,
        SIMULATION,
        ANALYSIS,
        SETTINGS
    };
    View active_view = View::BROWSER;
    
    // Dialogs
    bool show_new_scenario_dialog = false;
    bool show_load_scenario_dialog = false;
    bool show_save_scenario_dialog = false;
    bool show_run_simulation_dialog = false;
    bool show_about_dialog = false;
    bool show_settings_dialog = false;
    
    // Status
    std::string status_message;
    f64 status_time = 0;
};

// =============================================================================
// Integrated Application Class
// =============================================================================

class IntegratedApp {
public:
    IntegratedApp();
    ~IntegratedApp();
    
    // Initialize application
    bool init(AppMode mode, const std::string& data_path = "data/platforms");
    
    // Main loop (for CLI mode, returns exit code)
    int run_cli(int argc, char* argv[]);
    
    // Frame update (for GUI modes, call from main loop)
    void update(f64 delta_time);
    void render();
    
    // State access
    IntegratedAppState& state() { return state_; }
    const IntegratedAppState& state() const { return state_; }
    bool should_close() const { return state_.should_close; }
    
    // Operations
    bool load_platform_database(const std::string& path);
    bool load_scenario(const std::string& path);
    bool save_scenario(const std::string& path);
    bool new_scenario(const std::string& name);
    
    // Simulation control
    bool start_simulation();
    void pause_simulation();
    void resume_simulation();
    void stop_simulation();
    void step_simulation(u32 ticks = 1);
    
    // Monte Carlo
    bool run_monte_carlo(u32 iterations, Seed seed = 0);
    void cancel_monte_carlo();
    
    // Browser operations
    void apply_browser_filters();
    void select_platform(int index);
    void add_to_compare(const PlatformSpec* spec);
    void clear_compare();
    
    // Scenario builder operations
    void add_actor_from_platform(const PlatformSpec* spec, 
                                  const std::string& side,
                                  f64 x, f64 y);
    void remove_actor(int index);
    void duplicate_actor(int index);
    
    // Callbacks
    using SimulationCallback = std::function<void(const SimulationState&)>;
    void set_simulation_callback(SimulationCallback cb) { sim_callback_ = cb; }
    
    using MonteCarloProgressCallback = std::function<void(u32 current, u32 total)>;
    void set_monte_carlo_callback(MonteCarloProgressCallback cb) { mc_callback_ = cb; }

private:
    IntegratedAppState state_;
    
    // Callbacks
    SimulationCallback sim_callback_;
    MonteCarloProgressCallback mc_callback_;
    
    // Internal simulation state
    std::unique_ptr<systems::SystemsBundle> systems_;
    SimulationContext* sim_context_ = nullptr;
    
    // Monte Carlo executor
    std::unique_ptr<analysis::MonteCarloExecutor> mc_executor_;
    bool mc_running_ = false;
    
    // Helpers
    void build_browser_data();
    void update_simulation_visualization();
    void process_simulation_events();
    std::string get_category_name(PlatformCategory cat) const;
};

// =============================================================================
// Entry Points
// =============================================================================

// CLI entry point
int run_cli_app(int argc, char* argv[]);

// TUI Browser entry point
int run_tui_browser(const std::string& data_path);

// GUI Browser entry point (requires ImGui context)
int run_gui_browser(const std::string& data_path);

// Full GUI entry point (requires ImGui context)
int run_gui_full(const std::string& data_path);

}  // namespace athena

#endif  // ATHENA_APP_HPP
