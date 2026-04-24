// ATHENA Core - Integrated Application
// Unified application combining simulation, browsers, and analysis.
// Declares UnifiedApp and all state structs consumed by:
//   - src/gui/integrated_gui.cpp  (render methods)
//   - src/unified_main.cpp        (lifecycle + CLI)
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_INTEGRATED_HPP
#define ATHENA_INTEGRATED_HPP

#include "athena/types.hpp"
#include "athena/platform_loader.hpp"
#include "athena/scenario.hpp"
#include "athena/context.hpp"
#include "athena/systems.hpp"
#include "athena/analysis/montecarlo.hpp"
#include "athena/terrain.hpp"
#include "athena/terrain_semantics.hpp"
#include "athena/environment.hpp"
#include "athena/pathfinding.hpp"

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

namespace athena {

// =============================================================================
// Version
// =============================================================================

struct IntegratedVersion {
    static constexpr const char* STRING   = "1.2.5";
    static constexpr const char* CODENAME = "Tactical";
};

// =============================================================================
// Application Mode
// =============================================================================

enum class AppMode {
    GUI_FULL,
    GUI_BROWSER,
    CLI_SIMULATION,
    CLI_BATCH,
    TUI_BROWSER,
};

// =============================================================================
// Unified Application State
// =============================================================================

struct UnifiedAppState {

    // -- View selection -------------------------------------------------------
    enum class View { BROWSER, SCENARIO, SIMULATION, ANALYSIS };
    View active_view = View::BROWSER;

    // -- Top-level flags ------------------------------------------------------
    bool should_close = false;
    f64  fps          = 0.0;

    // -- Dialog visibility ----------------------------------------------------
    bool show_about                 = false;
    bool show_demo                  = false;
    bool show_settings              = false;
    bool show_new_scenario          = false;
    bool show_load_scenario         = false;
    bool show_save_scenario         = false;
    bool show_run_simulation_dialog = false;
    bool show_export_results        = false;

    // -- Dialog input buffers -------------------------------------------------
    char dialog_path_buf[512]   = {0};    // for load/save/export file paths
    int  dialog_mc_iterations   = 100;
    int  dialog_mc_seed         = 42;
    bool dialog_mc_record_tl    = false;
    int  dialog_export_format   = 0;      // 0=JSON, 1=Binary

    // -- Force Builder dialog --------------------------------------------------
    bool show_force_builder = false;
    struct ForceBuilderState {
        char platform_search[128] = {0};
        std::vector<const PlatformSpec*> search_results;
        const PlatformSpec* selected_platform = nullptr;
        int side_idx = 0;          // 0=blue, 1=red, 2=green, 3=orange, 4=yellow
        int count = 4;             // Number of units to deploy (1-20)
        int formation = 0;         // 0=Line, 1=Column, 2=Wedge, 3=Spread
        float spacing_m = 200.0f;  // Meters between units
        bool spacing_auto = true;  // Auto-calculate from platform type
    } force_builder;

    // -- Status bar -----------------------------------------------------------
    enum class StatusLevel { INFO, WARNING, ERROR };
    std::string status_message;
    StatusLevel status_level = StatusLevel::INFO;

    // -- Browser sub-state ----------------------------------------------------
    struct BrowserState {
        PlatformDatabase db;
        char search_buffer[256] = {0};
        PlatformCategory selected_category = PlatformCategory::UNKNOWN;
        std::string selected_country;
        std::vector<const PlatformSpec*> filtered;
        int selected_idx = -1;
        const PlatformSpec* selected = nullptr;

        // category -> count  (built once after DB load)
        std::map<PlatformCategory, int> category_counts;
        std::vector<std::string> countries;
    } browser;

    // -- Scenario sub-state ---------------------------------------------------
    struct ScenarioState {
        Scenario scenario;            // the actual scenario data
        std::string scenario_path;
        bool is_new      = true;
        bool is_modified = false;

        int selected_actor_idx = -1;

        // Platform picker (for actor editor)
        char platform_search_buf[128] = {0};
        std::vector<const PlatformSpec*> platform_search_results;
        bool platform_picker_open = false;

        // Map viewport (lat/lon degrees)
        f64 map_center_lat = 55.0;
        f64 map_center_lon = 21.0;
        f64 map_span_lat   = 2.5;    // Visible range in degrees
        f64 map_span_lon   = 3.0;
        bool show_grid     = true;

        // Drag state
        bool dragging_actor = false;
        int  drag_actor_idx = -1;

        // v1.2.0: Weather/environment controls
        int weather_idx = 0;       // Index into Weather enum (0=CLEAR)
        int climate_idx = 0;       // Index into Climate enum (0=TEMPERATE)
        bool day_night = false;    // Day/night cycle enabled
        float visibility_km = 10.0f;
        float temperature_c = 15.0f;
        float wind_speed_kmh = 10.0f;
    } scenario;

    // -- Comparative analysis (multiple MC runs) ------------------------------
    struct VariantResult {
        std::string name;
        u32  iterations;
        Seed seed;
        analysis::BatchStatistics stats;
    };
    std::vector<VariantResult> variant_results;

    // -- Simulation control ---------------------------------------------------
    struct SimControl {
        bool is_loaded   = false;
        bool is_running  = false;
        bool is_paused   = false;
        bool is_complete = false;

        u32 current_tick = 0;
        u32 max_ticks    = 168;
        f64 time_scale   = 1.0;
        f64 progress_percent = 0.0;

        u32 blue_alive = 0;
        u32 blue_total = 0;
        u32 red_alive  = 0;
        u32 red_total  = 0;
        u32 green_alive = 0;
        u32 green_total = 0;
        u32 orange_alive = 0;
        u32 orange_total = 0;
        u32 yellow_alive = 0;
        u32 yellow_total = 0;
        u32 total_engagements = 0;

        // World bounds (meters, auto-computed from entities)
        f64 bounds_min_x = 0;
        f64 bounds_max_x = 100000;
        f64 bounds_min_y = 0;
        f64 bounds_max_y = 100000;
    } sim_control;

    // -- Monte-Carlo control --------------------------------------------------
    struct MCControl {
        bool is_running = false;
        u32  num_iterations       = 100;
        u32  completed_iterations = 0;
        Seed master_seed          = 0;
        f64  start_time_ms        = 0.0;  // For ETA computation

        analysis::BatchStatistics stats;
    } mc_control;

    // Side display names (user can rename)
    std::string side_display_names[6] = {"Blue", "Red", "Green", "Orange", "Yellow", "Neutral"};
    static constexpr const char* SIDE_KEYS[6] = {"blue", "red", "green", "orange", "yellow", "neutral"};

    // -- Analysis sub-state ---------------------------------------------------
    struct AnalysisState {
        bool has_data = false;
        std::vector<analysis::IterationResult> results;
        analysis::BatchStatistics stats;

        // Confidence indicator (v1.1.7)
        enum class Confidence { LOW, MEDIUM, HIGH };
        Confidence confidence = Confidence::LOW;
        std::string confidence_text;

        // Sensitivity summary (v1.1.7) — top factors, plain language
        std::vector<std::string> sensitivity_summary;
    } analysis;

    // -- Scenario validation warnings (v1.1.7) --------------------------------
    struct ValidationWarning {
        std::string message;
        bool is_critical = false;  // true = likely produces useless results
    };
    std::vector<ValidationWarning> validation_warnings;

    // -- Visualisation (populated during simulation) --------------------------
    struct EntityVis {
        EntityId id;
        std::string name;
        std::string side;
        f64 x = 0, y = 0;
        f64 heading = 0;
        bool alive  = true;
        f64 health  = 1.0;
        // v1.1.8: operational state
        f64 fatigue  = 0.0;
        f64 cohesion = 1.0;
        bool broken  = false;    // Operationally dead (cohesion collapse)
        bool fatigued = false;   // Performance degraded
        // v1.2.0
        f64 suppression = 0.0;
        bool suppressed = false;
        bool is_indirect = false;  // Artillery marker
        // v1.2.3: tooltip data
        f64 supply = 1.0;
        f64 morale = 1.0;
        f64 engagement_range = 0.0;
        const char* movement_type = "foot";
    };
    std::vector<EntityVis> entity_vis;
    std::vector<std::string> event_log;
};

// =============================================================================
// Unified Application
// =============================================================================

class UnifiedApp {
public:
    UnifiedApp();
    ~UnifiedApp();

    // -- Lifecycle (called from unified_main.cpp) -----------------------------
    bool init(AppMode mode, const std::string& data_path = "data/platforms");
    void update(f64 delta_time);
    void render();                         // implemented in integrated_gui.cpp
    void shutdown();
    bool should_close() const { return state_.should_close; }

    // -- Accessors ------------------------------------------------------------
    const PlatformDatabase& database() const { return state_.browser.db; }
    const UnifiedAppState::SimControl& sim_control() const { return state_.sim_control; }
    const UnifiedAppState::MCControl&  mc_control()  const { return state_.mc_control; }

    // -- Scenario I/O ---------------------------------------------------------
    bool load_scenario(const std::string& path);
    bool save_scenario(const std::string& path);

    // -- Simulation control ---------------------------------------------------
    bool start_simulation(Seed seed = 0);
    void pause_simulation();
    void resume_simulation();
    void stop_simulation();
    void step_simulation(u32 ticks = 1);
    u32  reorganize_side(Side side);  // v1.2.3: reset BROKEN units

    // -- Monte-Carlo ----------------------------------------------------------
    void configure_monte_carlo(u32 iterations, Seed seed, bool record_timeline);
    bool run_monte_carlo();
    using MCProgressCallback = std::function<void(u32 current, u32 total)>;
    void set_mc_progress_callback(MCProgressCallback cb) { mc_progress_cb_ = std::move(cb); }

    // -- Export ----------------------------------------------------------------
    bool export_results_json(const std::string& path);
    bool export_results_binary(const std::string& path);

private:
    UnifiedAppState state_;
    AppMode         mode_ = AppMode::GUI_FULL;

    // Callbacks
    MCProgressCallback mc_progress_cb_;

    // Monte-Carlo config held between configure + run
    analysis::BatchConfig mc_config_;

    // -- Background MC thread --------------------------------------------------
    std::thread              mc_thread_;
    std::atomic<bool>        mc_running_{false};
    std::atomic<u32>         mc_progress_{0};       // completed iterations
    std::atomic<bool>        mc_finished_{false};    // set when thread finishes
    std::atomic<bool>        mc_cancel_requested_{false};
    std::mutex               mc_result_mutex_;
    std::vector<analysis::IterationResult> mc_pending_results_;
    analysis::BatchStatistics              mc_pending_stats_;
    bool                                   mc_pending_ok_ = false;

    void check_mc_completion();  // called from update()
    void cancel_monte_carlo();  // request MC cancellation

    // -- Live simulation engine ------------------------------------------------
    //  These are instantiated in start_simulation() and destroyed in stop_sim().
    std::unique_ptr<Context>              sim_ctx_;
    std::unique_ptr<EntityManager>        sim_entities_;
    std::unique_ptr<systems::SystemsBundle> sim_systems_;
    std::unique_ptr<Scheduler>            sim_scheduler_;

    // -- Terrain engine (owned, shared between interactive + MC) ---------------
    std::unique_ptr<PhysicalTerrain>      sim_terrain_;
    std::unique_ptr<Environment>          sim_environment_;
    std::unique_ptr<TerrainSemantics>     sim_terrain_sem_;
    std::unique_ptr<Pathfinder>           sim_pathfinder_;  // v1.2.5: A* pathfinding

    // Actor name → entity index mapping (populated at start)
    std::vector<std::string> entity_names_;
    std::vector<std::string> entity_actor_ids_;

    // -- Path resolution (populated in init) ----------------------------------
    std::string exe_dir_;        // Directory containing the executable
    std::string scenarios_dir_;  // {exe_dir}/scenarios/ (auto-created)

    // -- Internal helpers -----------------------------------------------------
    void build_browser_data();
    void apply_browser_filters();
    void set_status(const std::string& msg,
                    UnifiedAppState::StatusLevel lvl = UnifiedAppState::StatusLevel::INFO);

    // Sync engine EntityStorage → state_.entity_vis for rendering
    void sync_entity_vis();

    // Push a timestamped message to the event log
    void log_event(const std::string& msg);

    // -- Render methods (implemented in integrated_gui.cpp) -------------------
    void render_menu_bar();
    void render_browser_view();
    void render_platform_detail();
    void render_scenario_view();
    void render_scenario_map();
    void render_scenario_toolbar();
    void render_actor_properties();
    void render_simulation_view();
    void render_simulation_map();
    void render_analysis_view();
    void render_analysis_charts();
    void render_comparative_view();
    void render_dialogs();
    void render_status_bar();

    // Auto-fit scenario map viewport to actor positions
    void auto_center_scenario_map();

    // Add a named MC variant result for comparison
    void store_variant_result(const std::string& name, Seed seed, u32 iterations,
                              const analysis::BatchStatistics& stats);

    // Write execution manifest JSON after MC run (determinism/audit trail)
    void write_execution_manifest(const analysis::BatchStatistics& stats,
                                  Seed seed, u32 iterations, f64 total_ms);

    // Scenario validation before MC run (v1.1.7)
    void validate_scenario();

    // Compute confidence level from MC results (v1.1.7)
    void compute_result_confidence();

    // Compute sensitivity summary from MC results (v1.1.7)
    void compute_sensitivity_summary();

    // Render outcome analysis panel (v1.1.7)
    void render_outcome_analysis();
};

// =============================================================================
// Free-standing entry points (unified_main.cpp)
// =============================================================================

int athena_main(int argc, char* argv[]);
int run_tui_browser(const std::string& data_path);

}  // namespace athena

#endif  // ATHENA_INTEGRATED_HPP
