// ATHENA Core - Integrated Application (Non-Render Logic)
// Lifecycle, simulation control, Monte Carlo, and export.
// Render methods are in src/gui/integrated_gui.cpp
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/integrated.hpp"
#include "athena/json.hpp"
#include "athena/systems/tactical_ai.hpp"

#include <algorithm>
#include <set>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#undef ERROR    // windows.h defines ERROR macro, conflicts with our enum
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace athena {

// =============================================================================
// Constructor / Destructor
// =============================================================================

UnifiedApp::UnifiedApp() = default;
UnifiedApp::~UnifiedApp() {
    // Ensure background MC thread is joined before destruction
    if (mc_thread_.joinable()) {
        mc_thread_.join();
    }
}

// =============================================================================
// Lifecycle
// =============================================================================

bool UnifiedApp::init(AppMode mode, const std::string& data_path) {
    mode_ = mode;

    // =========================================================================
    // Resolve executable directory for portable path resolution.
    // Scenarios are saved/loaded relative to this directory.
    // =========================================================================
    {
#ifdef _WIN32
        char buf[MAX_PATH] = {0};
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        exe_dir_ = std::string(buf);
        auto pos = exe_dir_.find_last_of("\\/");
        if (pos != std::string::npos) exe_dir_ = exe_dir_.substr(0, pos);
        else exe_dir_ = ".";
#else
        char buf[4096] = {0};
        ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = '\0';
            exe_dir_ = std::string(buf);
            auto pos = exe_dir_.find_last_of('/');
            if (pos != std::string::npos) exe_dir_ = exe_dir_.substr(0, pos);
            else exe_dir_ = ".";
        } else {
            exe_dir_ = ".";
        }
#endif
        // Create scenarios directory next to executable
        scenarios_dir_ = exe_dir_ + "/scenarios";
#ifdef _WIN32
        CreateDirectoryA(scenarios_dir_.c_str(), nullptr);
#else
        mkdir(scenarios_dir_.c_str(), 0755);
#endif
    }

    // Load platform database
    auto result = state_.browser.db.load_all(data_path);
    if (!result.ok()) {
        fprintf(stderr, "[ATHENA] Failed to load platform database from: %s\n",
                data_path.c_str());
        return false;
    }

    build_browser_data();
    apply_browser_filters();

    // Initialize default scenario with valid temporal/bounds so save always
    // produces loadable JSON, even if user never clicked "New Scenario".
    {
        auto& sc = state_.scenario.scenario;
        sc.id      = "custom-scenario";
        sc.name    = "New Scenario";
        sc.version = "1.0.0";
        sc.scale   = ScenarioScale::MESO;
        sc.temporal.tick_duration_seconds = 3600.0;
        sc.temporal.max_ticks = 72;
        sc.spatial_bounds = {54.0, 56.0, 20.0, 22.0};
        sc.environment.terrain.base_type = "flat";
        sc.environment.terrain.roughness = 0.3;
        state_.scenario.is_new = true;
    }

    set_status("ATHENA " + std::string(IntegratedVersion::STRING) +
               " initialized | " + std::to_string(state_.browser.db.count()) +
               " platforms loaded");
    return true;
}

void UnifiedApp::update(f64 delta_time) {
    // FPS calculation (simple exponential average)
    if (delta_time > 0.0) {
        f64 instant_fps = 1.0 / delta_time;
        state_.fps = state_.fps * 0.9 + instant_fps * 0.1;
    }

    // Check if background MC finished
    check_mc_completion();

    // Update MC progress for UI
    if (mc_running_.load()) {
        state_.mc_control.completed_iterations = mc_progress_.load();
    }

    // Auto-step simulation when running and not paused
    auto& sc = state_.sim_control;
    if (sc.is_running && !sc.is_paused && sim_scheduler_) {
        // time_scale controls ticks-per-second (at 60fps, 1x = ~1 tick/s)
        // Accumulate fractional ticks via time_scale * delta_time
        static f64 tick_accumulator = 0.0;
        tick_accumulator += sc.time_scale * delta_time;

        u32 ticks_this_frame = static_cast<u32>(tick_accumulator);
        if (ticks_this_frame > 0) {
            tick_accumulator -= static_cast<f64>(ticks_this_frame);
            step_simulation(ticks_this_frame);
        }
    }
}

void UnifiedApp::shutdown() {
    stop_simulation();
    set_status("Shutdown");
}

// =============================================================================
// Browser Helpers
// =============================================================================

void UnifiedApp::build_browser_data() {
    auto& b = state_.browser;

    // Category counts
    b.category_counts.clear();
    for (int i = 1; i < static_cast<int>(PlatformCategory::MAX_CATEGORIES); ++i) {
        auto cat = static_cast<PlatformCategory>(i);
        int count = static_cast<int>(b.db.count_by_category(cat));
        if (count > 0) {
            b.category_counts[cat] = count;
        }
    }

    // Country list (sorted, unique)
    std::set<std::string> country_set;
    for (const auto& [id, spec] : b.db.all()) {
        if (!spec.country_of_origin.empty()) {
            country_set.insert(spec.country_of_origin);
        }
    }
    b.countries.assign(country_set.begin(), country_set.end());
}

void UnifiedApp::apply_browser_filters() {
    auto& b = state_.browser;
    b.filtered.clear();

    std::string search_lower;
    if (b.search_buffer[0] != '\0') {
        search_lower = b.search_buffer;
        std::transform(search_lower.begin(), search_lower.end(),
                       search_lower.begin(), ::tolower);
    }

    for (const auto& [id, spec] : b.db.all()) {
        // Category filter
        if (b.selected_category != PlatformCategory::UNKNOWN &&
            spec.category != b.selected_category) {
            continue;
        }
        // Country filter
        if (!b.selected_country.empty() &&
            spec.country_of_origin != b.selected_country) {
            continue;
        }
        // Text search
        if (!search_lower.empty()) {
            std::string haystack = spec.id + " " + spec.name + " " +
                                   spec.type + " " + spec.manufacturer;
            std::transform(haystack.begin(), haystack.end(),
                           haystack.begin(), ::tolower);
            if (haystack.find(search_lower) == std::string::npos) {
                continue;
            }
        }
        b.filtered.push_back(&spec);
    }

    // Sort by name
    std::sort(b.filtered.begin(), b.filtered.end(),
              [](const PlatformSpec* a, const PlatformSpec* b) {
                  return a->name < b->name;
              });

    // Reset selection if out of range
    if (b.selected_idx >= static_cast<int>(b.filtered.size())) {
        b.selected_idx = -1;
        b.selected     = nullptr;
    }
}

// =============================================================================
// Status
// =============================================================================

void UnifiedApp::set_status(const std::string& msg, UnifiedAppState::StatusLevel lvl) {
    state_.status_message = msg;
    state_.status_level   = lvl;
}

// =============================================================================
// Scenario I/O
// =============================================================================

bool UnifiedApp::load_scenario(const std::string& path) {
    ScenarioLoader loader;

    // Try multiple locations for the scenario file:
    //   1. Path as given (absolute or CWD-relative)
    //   2. Relative to scenarios_dir_
    //   3. Relative to exe_dir_
    std::string resolved = path;
    auto try_load = [&](const std::string& p) -> bool {
        FILE* f = fopen(p.c_str(), "r");
        if (f) { fclose(f); resolved = p; return true; }
        return false;
    };

    if (!try_load(path)) {
        if (!scenarios_dir_.empty() && !try_load(scenarios_dir_ + "/" + path)) {
            if (!exe_dir_.empty()) {
                if (!try_load(exe_dir_ + "/" + path)) {
                    // Also try athena-core/ prefixed paths for bundled examples
                    try_load(exe_dir_ + "/athena-core/" + path);
                }
            }
        }
    }

    auto result = loader.load(resolved);
    if (!result.ok()) {
        set_status("Failed to load: " + resolved + " | " + result.error.message,
                   UnifiedAppState::StatusLevel::ERROR);
        return false;
    }

    state_.scenario.scenario      = result.get();
    state_.scenario.scenario_path = path;
    state_.scenario.is_new        = false;
    state_.scenario.is_modified   = false;
    state_.scenario.selected_actor_idx = -1;

    // v1.2.5: Load weather state from scenario JSON if present
    {
        FILE* wf = fopen(resolved.c_str(), "r");
        if (wf) {
            fseek(wf, 0, SEEK_END);
            long sz = ftell(wf);
            fseek(wf, 0, SEEK_SET);
            std::string raw(static_cast<size_t>(sz), '\0');
            fread(&raw[0], 1, static_cast<size_t>(sz), wf);
            fclose(wf);
            auto parsed = json::parse(raw);
            if (parsed.ok()) {
                const auto& root = parsed.get();
                if (root.has("environment") && root["environment"].has("weather")) {
                    const auto& w = root["environment"]["weather"];
                    state_.scenario.weather_idx    = static_cast<int>(w.get_number("weather_type", 0.0));
                    state_.scenario.climate_idx    = static_cast<int>(w.get_number("climate_type", 0.0));
                    state_.scenario.visibility_km  = static_cast<float>(w.get_number("visibility_km", 10.0));
                    state_.scenario.temperature_c  = static_cast<float>(w.get_number("temperature_c", 15.0));
                    state_.scenario.wind_speed_kmh = static_cast<float>(w.get_number("wind_speed_kmh", 10.0));
                    state_.scenario.day_night      = w.get_bool("day_night", false);
                }
            }
        }
    }

    state_.sim_control.is_loaded = true;

    // Auto-center map on actors
    auto_center_scenario_map();

    set_status("Loaded scenario: " + state_.scenario.scenario.name +
               " (" + std::to_string(state_.scenario.scenario.actors.size()) + " units)");
    return true;
}

bool UnifiedApp::save_scenario(const std::string& path) {
    const auto& sc = state_.scenario.scenario;

    // Build JSON object manually using athena::json types
    json::Object root;
    root["id"]      = json::String(sc.id.empty() ? "custom-scenario" : sc.id);
    root["name"]    = json::String(sc.name);
    root["version"] = json::String(sc.version.empty() ? "1.0.0" : sc.version);
    root["scale"]   = json::String("meso");

    // Temporal (use sensible defaults if unset)
    json::Object temporal;
    temporal["tick_duration_seconds"] = json::Number(
        sc.temporal.tick_duration_seconds > 0.0 ? sc.temporal.tick_duration_seconds : 3600.0);
    temporal["max_ticks"]             = json::Number(static_cast<f64>(
        sc.temporal.max_ticks > 0 ? sc.temporal.max_ticks : 72));
    root["temporal"] = json::Value(temporal);

    // Spatial bounds (compute from actors if empty)
    json::Object bounds;
    if (sc.actors.empty()) {
        bounds["min_lat"] = json::Number(54.0);
        bounds["max_lat"] = json::Number(56.0);
        bounds["min_lon"] = json::Number(20.0);
        bounds["max_lon"] = json::Number(22.0);
    } else {
        f64 min_lat = 90, max_lat = -90, min_lon = 180, max_lon = -180;
        for (const auto& a : sc.actors) {
            min_lat = std::min(min_lat, a.initial_position.lat);
            max_lat = std::max(max_lat, a.initial_position.lat);
            min_lon = std::min(min_lon, a.initial_position.lon);
            max_lon = std::max(max_lon, a.initial_position.lon);
        }
        f64 margin_lat = std::max((max_lat - min_lat) * 0.2, 0.5);
        f64 margin_lon = std::max((max_lon - min_lon) * 0.2, 0.5);
        bounds["min_lat"] = json::Number(min_lat - margin_lat);
        bounds["max_lat"] = json::Number(max_lat + margin_lat);
        bounds["min_lon"] = json::Number(min_lon - margin_lon);
        bounds["max_lon"] = json::Number(max_lon + margin_lon);
    }
    root["spatial_bounds"] = json::Value(bounds);

    // Actors (regenerate IDs to guarantee uniqueness)
    json::Array actors_arr;
    int actor_idx = 0;
    for (const auto& actor : sc.actors) {
        json::Object a;
        // Ensure unique ID even if user deleted/re-added actors
        std::string safe_id = actor.id.empty()
            ? (actor.side + "-" + std::to_string(actor_idx + 1))
            : actor.id;
        a["id"]   = json::String(safe_id);
        a["name"] = json::String(actor.name);
        a["type"] = json::String(actor.type.empty() ? "unit" : actor.type);
        a["side"] = json::String(actor.side);
        actor_idx++;
        if (!actor.platform_id.empty()) {
            a["platform_id"] = json::String(actor.platform_id);
        }

        json::Object pos;
        pos["lat"] = json::Number(actor.initial_position.lat);
        pos["lon"] = json::Number(actor.initial_position.lon);
        pos["alt"] = json::Number(actor.initial_position.alt);
        a["position"] = json::Value(pos);

        a["health"] = json::Number(actor.initial_health);
        a["supply"] = json::Number(actor.initial_supply);
        a["morale"] = json::Number(actor.initial_morale);

        if (actor.mobility.has_value()) {
            json::Object mob;
            mob["max_speed_kmh"]  = json::Number(actor.mobility->max_speed_kmh);
            mob["terrain_factor"] = json::Number(actor.mobility->terrain_factor);
            a["mobility"] = json::Value(mob);
        }

        if (actor.firepower.has_value()) {
            json::Object fp;
            fp["base_firepower"] = json::Number(actor.firepower->base_firepower);
            fp["range_km"]       = json::Number(actor.firepower->range_km);
            fp["accuracy"]       = json::Number(actor.firepower->accuracy);
            a["firepower"] = json::Value(fp);
        }

        if (actor.sensors.has_value()) {
            json::Object sr;
            sr["detection_range_km"]       = json::Number(actor.sensors->detection_range_km);
            sr["identification_range_km"]  = json::Number(actor.sensors->identification_range_km);
            a["sensors"] = json::Value(sr);
        }

        actors_arr.push_back(json::Value(a));
    }
    root["actors"] = json::Value(actors_arr);

    // Environment / Terrain (v1.1)
    json::Object env_obj;
    json::Object terrain_obj;
    terrain_obj["base_type"]       = json::String(sc.environment.terrain.base_type.empty()
                                       ? "flat" : sc.environment.terrain.base_type);
    terrain_obj["roughness"]       = json::Number(sc.environment.terrain.roughness);
    terrain_obj["forest_density"]  = json::Number(sc.environment.terrain.forest_density);
    terrain_obj["urban_density"]   = json::Number(sc.environment.terrain.urban_density);
    terrain_obj["rivers"]          = json::Bool(sc.environment.terrain.rivers_enabled);
    env_obj["terrain"] = json::Value(terrain_obj);

    // v1.2.5: Weather persistence — save GUI weather state into scenario JSON
    {
        json::Object weather_obj;
        weather_obj["weather_type"]   = json::Number(static_cast<f64>(state_.scenario.weather_idx));
        weather_obj["climate_type"]   = json::Number(static_cast<f64>(state_.scenario.climate_idx));
        weather_obj["visibility_km"]  = json::Number(static_cast<f64>(state_.scenario.visibility_km));
        weather_obj["temperature_c"]  = json::Number(static_cast<f64>(state_.scenario.temperature_c));
        weather_obj["wind_speed_kmh"] = json::Number(static_cast<f64>(state_.scenario.wind_speed_kmh));
        weather_obj["day_night"]      = json::Bool(state_.scenario.day_night);
        env_obj["weather"] = json::Value(weather_obj);
    }

    root["environment"] = json::Value(env_obj);

    // Serialize and write
    std::string json_str = json::to_string_pretty(json::Value(root));

    // Ensure parent directories exist
    {
        std::string dir = path;
        auto pos = dir.find_last_of("/\\");
        if (pos != std::string::npos) {
            dir = dir.substr(0, pos);
#ifdef _WIN32
            // Recursive mkdir for Windows
            for (size_t i = 0; i < dir.size(); ++i) {
                if (dir[i] == '/' || dir[i] == '\\') {
                    CreateDirectoryA(dir.substr(0, i).c_str(), nullptr);
                }
            }
            CreateDirectoryA(dir.c_str(), nullptr);
#else
            // Use system mkdir -p
            std::string cmd = "mkdir -p \"" + dir + "\"";
            (void)system(cmd.c_str());
#endif
        }
    }

    FILE* f = fopen(path.c_str(), "w");
    if (!f) {
        set_status("Failed to write: " + path, UnifiedAppState::StatusLevel::ERROR);
        return false;
    }
    fputs(json_str.c_str(), f);
    fclose(f);

    state_.scenario.scenario_path = path;
    state_.scenario.is_modified   = false;
    state_.scenario.is_new        = false;  // Allow Ctrl+S from now on
    set_status("Saved scenario: " + path);
    return true;
}

// =============================================================================
// Simulation Control — Real Engine Integration
// =============================================================================

bool UnifiedApp::start_simulation(Seed seed) {
    if (!state_.sim_control.is_loaded) {
        set_status("No scenario loaded", UnifiedAppState::StatusLevel::ERROR);
        return false;
    }

    // Stop any previous simulation first
    if (sim_scheduler_) stop_simulation();

    const auto& scenario = state_.scenario.scenario;

    // 1. Create Context
    ContextConfig ctx_config;
    ctx_config.seed = seed;
    ctx_config.max_entities = 256;

    auto ctx_result = Context::create(ctx_config);
    if (!ctx_result.ok()) {
        set_status("Failed to create context: " + ctx_result.error.message,
                   UnifiedAppState::StatusLevel::ERROR);
        return false;
    }
    sim_ctx_ = std::move(ctx_result.get());

    // 2. Create EntityManager and convert scenario
    sim_entities_ = std::make_unique<EntityManager>();
    sim_entities_->init(256);

    Rng conv_rng(seed, 0x5C3A1210ULL);
    ScenarioConverter converter;
    converter.set_platform_database(&state_.browser.db);
    auto conv_result = converter.convert(scenario, *sim_entities_, conv_rng);
    if (!conv_result.ok()) {
        set_status("Scenario conversion failed: " + conv_result.error.message,
                   UnifiedAppState::StatusLevel::ERROR);
        sim_entities_.reset();
        sim_ctx_.reset();
        return false;
    }

    // Build name mapping (actor_id -> entity index)
    const auto& actor_map = converter.actor_mapping();
    entity_names_.clear();
    entity_actor_ids_.clear();
    entity_names_.resize(sim_entities_->storage().count, "?");
    entity_actor_ids_.resize(sim_entities_->storage().count, "?");
    for (const auto& actor : scenario.actors) {
        auto it = actor_map.find(actor.id);
        if (it != actor_map.end() && it->second < entity_names_.size()) {
            entity_names_[it->second] = actor.name;
            entity_actor_ids_[it->second] = actor.id;
        }
    }

    // 3. Initialize terrain from scenario config
    {
        const auto& tc = scenario.environment.terrain;
        const auto& sb = scenario.spatial_bounds;

        // Compute map dimensions in meters from spatial bounds
        constexpr f64 EARTH_R = 6371000.0;
        constexpr f64 D2R = 3.14159265358979323846 / 180.0;
        f64 center_lat_rad = ((sb.min_lat + sb.max_lat) / 2.0) * D2R;
        f64 map_w = EARTH_R * (sb.max_lon - sb.min_lon) * D2R * std::cos(center_lat_rad);
        f64 map_h = EARTH_R * (sb.max_lat - sb.min_lat) * D2R;
        if (map_w < 1000.0) map_w = 100000.0;  // fallback 100km
        if (map_h < 1000.0) map_h = 100000.0;

        // Map base_type string to enum
        TerrainBase base = TerrainBase::FLAT;
        if (tc.base_type == "rolling")    base = TerrainBase::HILLS;
        else if (tc.base_type == "mountains") base = TerrainBase::MOUNTAINS;
        else if (tc.base_type == "coastal")   base = TerrainBase::VALLEY;

        TerrainPhysicalConfig tp_cfg;
        tp_cfg.base = base;
        tp_cfg.roughness = tc.roughness;
        tp_cfg.seed = seed;
        tp_cfg.base_elevation_m = 100.0;
        tp_cfg.elevation_range_m = (base == TerrainBase::MOUNTAINS) ? 800.0 : 200.0;
        tp_cfg.width_m = map_w;
        tp_cfg.height_m = map_h;

        // Add overlays from scenario densities
        if (tc.forest_density > 0.01) {
            OverlayConfig oc;
            oc.type = OverlayType::FOREST;
            oc.density = tc.forest_density;
            oc.seed_offset = 1;
            tp_cfg.overlays.push_back(oc);
        }
        if (tc.urban_density > 0.01) {
            OverlayConfig oc;
            oc.type = OverlayType::URBAN;
            oc.density = tc.urban_density;
            oc.seed_offset = 2;
            tp_cfg.overlays.push_back(oc);
        }
        if (tc.rivers_enabled) {
            OverlayConfig oc;
            oc.type = OverlayType::RIVER;
            oc.density = 0.05;
            oc.param1 = 50.0;  // river width meters
            oc.seed_offset = 3;
            tp_cfg.overlays.push_back(oc);
        }

        sim_terrain_ = std::make_unique<PhysicalTerrain>();
        auto t_status = sim_terrain_->init(tp_cfg);
        if (!t_status.ok()) {
            log_event("Terrain init failed: " + t_status.error.message);
            sim_terrain_.reset();
        }

        // Environment (v1.2.0: use GUI weather settings)
        sim_environment_ = std::make_unique<Environment>();
        TerrainEnvironmentConfig env_cfg;
        env_cfg.climate = static_cast<Climate>(state_.scenario.climate_idx);
        env_cfg.weather = static_cast<Weather>(state_.scenario.weather_idx);
        env_cfg.visibility_km = state_.scenario.visibility_km;
        env_cfg.temperature_c = state_.scenario.temperature_c;
        env_cfg.wind_speed_kmh = state_.scenario.wind_speed_kmh;
        env_cfg.day_night_enabled = state_.scenario.day_night;
        sim_environment_->init(env_cfg);

        // Terrain semantics (connects physical + environment)
        if (sim_terrain_ && sim_terrain_->is_initialized()) {
            sim_terrain_sem_ = std::make_unique<TerrainSemantics>();
            sim_terrain_sem_->init(sim_terrain_.get(), sim_environment_.get());
        }
    }

    // 4. Initialize systems
    sim_systems_ = std::make_unique<systems::SystemsBundle>();

    systems::MovementConfig move_cfg;
    move_cfg.dt_seconds = scenario.temporal.tick_duration_seconds;
    move_cfg.use_terrain_semantics = (sim_terrain_sem_ != nullptr);
    systems::g_tactical_ai_dt_seconds.store(move_cfg.dt_seconds);
    systems::g_tactical_ai_terrain = sim_terrain_sem_.get();

    // v1.2.5: A* pathfinder for tactical AI obstacle navigation
    systems::g_tactical_ai_pathfinder = nullptr;
    if (sim_terrain_sem_) {
        sim_pathfinder_ = std::make_unique<Pathfinder>();
        PathfindingConfig pf_cfg;
        pf_cfg.cell_size = 200.0;       // 200m cells — good balance for tactical movement
        pf_cfg.max_iterations = 50000;   // Cap per-path computation
        pf_cfg.allow_diagonal = true;
        auto pf_status = sim_pathfinder_->init(sim_terrain_sem_.get(), pf_cfg);
        if (pf_status.ok()) {
            systems::g_tactical_ai_pathfinder = sim_pathfinder_.get();
        }
    }

    systems::CombatConfig combat_cfg;
    combat_cfg.use_terrain_semantics = (sim_terrain_sem_ != nullptr);
    systems::LogisticsConfig log_cfg;

    sim_systems_->init(256, move_cfg, combat_cfg, log_cfg);

    // Force global pointers to THIS simulation's systems.
    // SystemsBundle::init() already sets these, but we re-assert ownership
    // explicitly in case a prior MC run left stale pointers.
    systems::g_movement_system  = &sim_systems_->movement;
    systems::g_combat_system    = &sim_systems_->combat;
    systems::g_logistics_system = &sim_systems_->logistics;
    systems::g_detection_system = &sim_systems_->detection;
    systems::g_c2_system        = &sim_systems_->c2;

    // Connect terrain to systems
    if (sim_terrain_sem_) {
        sim_systems_->movement.set_terrain(sim_terrain_sem_.get());
        sim_systems_->combat.set_terrain(sim_terrain_sem_.get());
    }

    // v1.2.1: Set operational dt-scaling from scenario tick duration
    systems::g_operational_config.dt_seconds = scenario.temporal.tick_duration_seconds;

    // v1.2.1: Connect environment to logistics for weather consumption
    if (sim_environment_) {
        sim_systems_->logistics.set_environment(sim_environment_.get());
    }

    // 5. Initialize scheduler
    sim_scheduler_ = std::make_unique<Scheduler>();

    SchedulerConfig sched_cfg;
    sched_cfg.max_ticks = scenario.temporal.max_ticks;
    sched_cfg.compact_per_tick = false;  // Keep indices stable for vis

    auto init_s = sim_scheduler_->init(*sim_ctx_, *sim_entities_, sched_cfg);
    if (!init_s.ok()) {
        set_status("Scheduler init failed: " + init_s.error.message,
                   UnifiedAppState::StatusLevel::ERROR);
        sim_scheduler_.reset(); sim_systems_.reset();
        sim_entities_.reset(); sim_ctx_.reset();
        return false;
    }

    auto reg_s = systems::register_all_systems(*sim_scheduler_);
    if (!reg_s.ok()) {
        set_status("System registration failed: " + reg_s.error.message,
                   UnifiedAppState::StatusLevel::ERROR);
        sim_scheduler_.reset(); sim_systems_.reset();
        sim_entities_.reset(); sim_ctx_.reset();
        return false;
    }

    // v1.1.8: Register operational snapshot (captures health before combat)
    reg_s = sim_scheduler_->register_system(
        Scheduler::Phase::PRE_TICK, systems::operational_snapshot_update, "op_snapshot");
    if (!reg_s.ok()) {
        log_event("Operational snapshot registration failed (non-fatal)");
    }

    // Register simple seek-enemy AI as PRE_TICK
    reg_s = sim_scheduler_->register_system(
        Scheduler::Phase::PRE_TICK, systems::seek_enemy_ai_update, "seek_enemy_ai");
    if (!reg_s.ok()) {
        set_status("AI registration failed: " + reg_s.error.message,
                   UnifiedAppState::StatusLevel::ERROR);
        sim_scheduler_.reset(); sim_systems_.reset();
        sim_entities_.reset(); sim_ctx_.reset();
        return false;
    }

    // Register morale propagation as POST_TICK
    reg_s = sim_scheduler_->register_system(
        Scheduler::Phase::POST_TICK, systems::morale_propagation_update, "morale");
    if (!reg_s.ok()) {
        log_event("Morale system registration failed (non-fatal)");
    }

    // v1.1.8: Register operational wear (fatigue + cohesion) as POST_TICK
    reg_s = sim_scheduler_->register_system(
        Scheduler::Phase::POST_TICK, systems::operational_wear_update, "op_wear");
    if (!reg_s.ok()) {
        log_event("Operational wear registration failed (non-fatal)");
    }

    // 6. Start context
    sim_ctx_->start();

    // 6. Populate sim control state
    auto& sc = state_.sim_control;
    sc.is_running  = true;
    sc.is_paused   = true;   // Start paused so user can inspect initial state
    sc.is_complete = false;
    sc.current_tick = 0;
    sc.max_ticks    = scenario.temporal.max_ticks;
    sc.progress_percent = 0.0;
    sc.total_engagements = 0;

    // Count initial forces
    const auto& storage = sim_entities_->storage();
    sc.blue_total = 0; sc.red_total = 0; sc.green_total = 0;
    sc.orange_total = 0; sc.yellow_total = 0;
    for (usize i = 0; i < storage.count; ++i) {
        if (storage.is_active(i)) {
            if (storage.side[i] == Side::BLUE) ++sc.blue_total;
            else if (storage.side[i] == Side::RED) ++sc.red_total;
            else if (storage.side[i] == Side::GREEN) ++sc.green_total;
            else if (storage.side[i] == Side::ORANGE) ++sc.orange_total;
            else if (storage.side[i] == Side::YELLOW) ++sc.yellow_total;
        }
    }
    sc.blue_alive = sc.blue_total;
    sc.red_alive  = sc.red_total;
    sc.green_alive = sc.green_total;
    sc.orange_alive = sc.orange_total;
    sc.yellow_alive = sc.yellow_total;

    // v1.2.3: Infer MovementType from platform drive_type (from PlatformSpec),
    // falling back to UnitType only if no platform data.
    for (usize i = 0; i < storage.count; ++i) {
        if (!storage.is_active(i)) continue;
        MovementType mt = MovementType::FOOT;
        const auto& cp = storage.combat[i];
        if (cp.has_platform_data) {
            // Use drive_type from platform spec (set by platform_loader)
            switch (cp.drive_type_code) {
                case 1: mt = MovementType::WHEELED;    break;
                case 2: mt = MovementType::TRACKED;    break;
                case 3: mt = MovementType::FIXED_WING; break;
                case 4: mt = MovementType::HELICOPTER;  break;
                case 5: mt = MovementType::NAVAL;       break;
                default: mt = MovementType::FOOT;       break;
            }
        } else {
            // Fallback: infer from UnitType (legacy, no platform data)
            switch (storage.unit_type[i]) {
                case UnitType::ARMOR:       mt = MovementType::TRACKED;    break;
                case UnitType::MECHANIZED:  mt = MovementType::TRACKED;    break;
                case UnitType::ARTILLERY:   mt = MovementType::TRACKED;    break;
                case UnitType::AIR_DEFENSE: mt = MovementType::TRACKED;    break;
                case UnitType::LOGISTICS:   mt = MovementType::WHEELED;    break;
                case UnitType::RECON:       mt = MovementType::WHEELED;    break;
                case UnitType::ENGINEER:    mt = MovementType::TRACKED;    break;
                case UnitType::FIGHTER:     mt = MovementType::FIXED_WING; break;
                case UnitType::ATTACK_HELO: mt = MovementType::HELICOPTER; break;
                case UnitType::NAVAL:       mt = MovementType::NAVAL;      break;
                default: break;
            }
        }
        sim_systems_->movement.set_movement_type(i, mt);
    }

    // v1.2.5: Initialize A* path cache for tactical AI
    systems::tactical_ai_init_path_cache(storage.count);

    // Sync initial entity positions to visualization
    state_.event_log.clear();
    sync_entity_vis();

    log_event("Simulation started | Blue:" + std::to_string(sc.blue_total) +
              " Red:" + std::to_string(sc.red_total));

    set_status("Simulation running (paused) | " +
               std::to_string(storage.count) + " entities");
    return true;
}

void UnifiedApp::pause_simulation() {
    if (state_.sim_control.is_running) {
        state_.sim_control.is_paused = true;
        set_status("Simulation paused at tick " +
                   std::to_string(state_.sim_control.current_tick));
    }
}

void UnifiedApp::resume_simulation() {
    if (state_.sim_control.is_running && state_.sim_control.is_paused) {
        state_.sim_control.is_paused = false;
        set_status("Simulation running");
    }
}

void UnifiedApp::stop_simulation() {
    auto& sc = state_.sim_control;

    if (sim_ctx_ && sim_ctx_->is_running()) {
        sim_ctx_->stop();
    }

    // Destroy engine objects in reverse order
    sim_scheduler_.reset();
    sim_systems_.reset();
    sim_entities_.reset();
    sim_ctx_.reset();
    sim_terrain_sem_.reset();
    sim_pathfinder_.reset();
    sim_environment_.reset();
    sim_terrain_.reset();
    systems::g_tactical_ai_terrain = nullptr;
    systems::g_tactical_ai_pathfinder = nullptr;
    systems::tactical_ai_clear_path_cache();
    entity_names_.clear();
    entity_actor_ids_.clear();

    sc.is_running  = false;
    sc.is_paused   = false;
    sc.is_complete = true;

    log_event("Simulation stopped at tick " + std::to_string(sc.current_tick));
}

void UnifiedApp::step_simulation(u32 ticks) {
    if (!sim_scheduler_ || !sim_ctx_) return;

    // SAFETY: Restore global system pointers if MC thread invalidated them.
    // The MC thread creates a local SystemsBundle whose destructor zeroes
    // the global pointers when it goes out of scope. This guard ensures
    // the interactive simulation's pointers are always current.
    if (sim_systems_) {
        if (systems::g_combat_system   != &sim_systems_->combat ||
            systems::g_movement_system != &sim_systems_->movement ||
            systems::g_detection_system!= &sim_systems_->detection ||
            systems::g_logistics_system!= &sim_systems_->logistics) {
            systems::g_combat_system    = &sim_systems_->combat;
            systems::g_movement_system  = &sim_systems_->movement;
            systems::g_logistics_system = &sim_systems_->logistics;
            systems::g_detection_system = &sim_systems_->detection;
            systems::g_c2_system        = &sim_systems_->c2;
            log_event("WARNING: Global system pointers were stale — restored.");
        }
    }

    auto& sc = state_.sim_control;

    for (u32 t = 0; t < ticks; ++t) {
        if (sim_scheduler_->is_complete()) {
            sc.is_complete = true;
            sc.is_running  = false;
            log_event("Simulation complete at tick " + std::to_string(sc.current_tick));
            set_status("Simulation complete | " +
                       std::to_string(sc.current_tick) + " ticks");
            break;
        }

        // v1.2.0: Update environment (weather progression, events)
        if (sim_environment_) {
            sim_environment_->update(sc.current_tick);
        }

        auto status = sim_scheduler_->run_tick();
        if (!status.ok()) {
            // SIM_TICK_OVERFLOW = normal completion
            if (status.error.code == ErrorCode::SIM_TICK_OVERFLOW) {
                sc.is_complete = true;
                sc.is_running  = false;
                log_event("Simulation complete (max ticks)");
            } else {
                log_event("ERROR: " + status.error.message);
                set_status("Sim error: " + status.error.message,
                           UnifiedAppState::StatusLevel::ERROR);
                sc.is_running = false;
            }
            break;
        }

        sc.current_tick = sim_ctx_->current_tick();
        sc.progress_percent = (sc.max_ticks > 0)
            ? 100.0 * sc.current_tick / sc.max_ticks : 0.0;

        // Diagnostic dump on first tick — confirms systems are wired and
        // entities have valid combat parameters (firepower, ranges, etc.)
        if (sc.current_tick == 1) {
            const auto& diag_storage = sim_entities_->storage();
            char diag[512];
            snprintf(diag, sizeof(diag),
                     "T1 diag: %zu entities, combat=%s, movement=%s",
                     diag_storage.count,
                     systems::g_combat_system   ? "OK" : "NULL",
                     systems::g_movement_system ? "OK" : "NULL");
            log_event(diag);

            for (usize i = 0; i < diag_storage.count && i < 8; ++i) {
                if (!diag_storage.is_active(i)) continue;
                snprintf(diag, sizeof(diag),
                         "  E%zu side=%d pos=(%.0f,%.0f) fp=%.1f er=%.0f dr=%.0f",
                         i, (int)diag_storage.side[i],
                         diag_storage.pos_x[i], diag_storage.pos_y[i],
                         diag_storage.firepower[i],
                         diag_storage.engagement_range[i],
                         diag_storage.detection_range[i]);
                log_event(diag);
            }
        }

        // Log combat engagements from this tick
        const auto& engagements = sim_systems_->combat.last_engagements();
        for (const auto& eng : engagements) {
            sc.total_engagements++;
            std::string atk_name = (eng.attacker < entity_names_.size())
                ? entity_names_[eng.attacker] : "Entity#" + std::to_string(eng.attacker);
            std::string def_name = (eng.defender < entity_names_.size())
                ? entity_names_[eng.defender] : "Entity#" + std::to_string(eng.defender);
            char buf[256];
            snprintf(buf, sizeof(buf), "T%u: %s -> %s (dmg %.2f/%.2f)",
                     sc.current_tick, atk_name.c_str(), def_name.c_str(),
                     eng.defender_damage, eng.attacker_damage);
            log_event(buf);
        }

        // Check casualties
        const auto& storage = sim_entities_->storage();
        u32 blue_alive = 0, red_alive = 0, green_alive = 0;
        u32 orange_alive = 0, yellow_alive = 0;
        for (usize i = 0; i < storage.count; ++i) {
            if (!storage.is_active(i)) continue;
            if (storage.side[i] == Side::BLUE) ++blue_alive;
            else if (storage.side[i] == Side::RED) ++red_alive;
            else if (storage.side[i] == Side::GREEN) ++green_alive;
            else if (storage.side[i] == Side::ORANGE) ++orange_alive;
            else if (storage.side[i] == Side::YELLOW) ++yellow_alive;
        }

        // Log kills
        if (blue_alive < sc.blue_alive) {
            u32 lost = sc.blue_alive - blue_alive;
            log_event("T" + std::to_string(sc.current_tick) +
                      ": Blue lost " + std::to_string(lost) + " unit(s)");
        }
        if (red_alive < sc.red_alive) {
            u32 lost = sc.red_alive - red_alive;
            log_event("T" + std::to_string(sc.current_tick) +
                      ": Red lost " + std::to_string(lost) + " unit(s)");
        }
        if (green_alive < sc.green_alive) {
            u32 lost = sc.green_alive - green_alive;
            log_event("T" + std::to_string(sc.current_tick) +
                      ": Green lost " + std::to_string(lost) + " unit(s)");
        }
        if (orange_alive < sc.orange_alive) {
            u32 lost = sc.orange_alive - orange_alive;
            log_event("T" + std::to_string(sc.current_tick) +
                      ": Orange lost " + std::to_string(lost) + " unit(s)");
        }
        if (yellow_alive < sc.yellow_alive) {
            u32 lost = sc.yellow_alive - yellow_alive;
            log_event("T" + std::to_string(sc.current_tick) +
                      ": Yellow lost " + std::to_string(lost) + " unit(s)");
        }
        sc.blue_alive  = blue_alive;
        sc.red_alive   = red_alive;
        sc.green_alive = green_alive;
        sc.orange_alive = orange_alive;
        sc.yellow_alive = yellow_alive;

        // === AUTO-STOP: stop when only one side remains (multi-side) ===
        {
            // Count sides that started with >0 units and still have survivors
            u32 sides_started = 0, sides_alive = 0;
            if (sc.blue_total > 0)   { sides_started++; if (blue_alive > 0)   sides_alive++; }
            if (sc.red_total > 0)    { sides_started++; if (red_alive > 0)    sides_alive++; }
            if (sc.green_total > 0)  { sides_started++; if (green_alive > 0)  sides_alive++; }
            if (sc.orange_total > 0) { sides_started++; if (orange_alive > 0) sides_alive++; }
            if (sc.yellow_total > 0) { sides_started++; if (yellow_alive > 0) sides_alive++; }

            if (sides_started >= 2 && sides_alive <= 1) {
                sc.is_complete = true;
                sc.is_running  = false;
                const char* winner = (blue_alive > 0)   ? "Blue" :
                                     (red_alive > 0)    ? "Red"  :
                                     (green_alive > 0)  ? "Green" :
                                     (orange_alive > 0) ? "Orange" :
                                     (yellow_alive > 0) ? "Yellow" : "Draw";
                log_event("DECISIVE: " + std::string(winner) +
                          " wins at tick " + std::to_string(sc.current_tick));
                set_status(std::string("Simulation decisive: ") + winner +
                           " wins | tick " + std::to_string(sc.current_tick));
                break;
            }
        }
    }

    // Sync visualization after all ticks
    sync_entity_vis();
}

// v1.2.3: Reorganize — reset BROKEN units of a side (simulates reinforcement/regroup)
// Returns number of units reorganized.
u32 UnifiedApp::reorganize_side(Side side) {
    if (!sim_entities_) return 0;
    auto& storage = sim_entities_->storage();
    u32 count = 0;

    for (usize i = 0; i < storage.count; ++i) {
        if (storage.side[i] != side) continue;
        // Only reorganize alive but BROKEN units (health > 0, cohesion collapsed)
        if (storage.health[i] <= 0.0) continue;
        if ((storage.flags[i] & entity_flags::BROKEN) == 0) continue;

        // Clear BROKEN flag, restore partial cohesion
        storage.flags[i] &= ~entity_flags::BROKEN;
        storage.cohesion[i]    = std::max(storage.cohesion[i], 0.40);
        storage.fatigue[i]     = std::min(storage.fatigue[i], 0.50);
        storage.suppression[i] = 0.0;
        storage.flags[i] &= ~entity_flags::SUPPRESSED;
        storage.morale[i]      = std::max(storage.morale[i], 0.50);
        ++count;
    }

    if (count > 0) {
        const char* side_name = (side == Side::BLUE)   ? "Blue" :
                                (side == Side::RED)    ? "Red" :
                                (side == Side::GREEN)  ? "Green" :
                                (side == Side::ORANGE) ? "Orange" :
                                (side == Side::YELLOW) ? "Yellow" : "?";
        log_event(std::string("REORGANIZE: ") + std::to_string(count) + " " +
                  side_name + " unit(s) reformed");
        sync_entity_vis();
    }
    return count;
}

// =============================================================================
// Entity Visualization Sync
// =============================================================================

void UnifiedApp::sync_entity_vis() {
    if (!sim_entities_) return;

    const auto& storage = sim_entities_->storage();
    state_.entity_vis.clear();
    state_.entity_vis.reserve(storage.count);

    f64 min_x =  1e18, max_x = -1e18;
    f64 min_y =  1e18, max_y = -1e18;

    for (usize i = 0; i < storage.count; ++i) {
        UnifiedAppState::EntityVis ev;
        ev.id   = storage.id[i];
        ev.name = (i < entity_names_.size()) ? entity_names_[i] : "?";
        ev.side = (storage.side[i] == Side::BLUE)   ? "blue" :
                  (storage.side[i] == Side::RED)    ? "red"  :
                  (storage.side[i] == Side::GREEN)  ? "green" :
                  (storage.side[i] == Side::ORANGE) ? "orange" :
                  (storage.side[i] == Side::YELLOW) ? "yellow" : "neutral";
        ev.x      = storage.pos_x[i];
        ev.y      = storage.pos_y[i];
        ev.alive  = storage.is_active(i);
        ev.health = storage.health[i];
        ev.heading = 0.0;

        // v1.1.8: operational wear state
        ev.fatigue  = storage.fatigue[i];
        ev.cohesion = storage.cohesion[i];
        ev.broken   = (storage.flags[i] & entity_flags::BROKEN)  != 0;
        ev.fatigued = (storage.flags[i] & entity_flags::FATIGUED) != 0;

        // v1.2.0: suppression, indirect fire marker, and heading from storage
        ev.suppression = storage.suppression[i];
        ev.suppressed  = (storage.flags[i] & entity_flags::SUPPRESSED) != 0;
        ev.is_indirect = storage.combat[i].is_indirect;

        // v1.2.3: tooltip data
        ev.supply = storage.supply[i];
        ev.morale = storage.morale[i];
        ev.engagement_range = storage.engagement_range[i];
        // Movement type from movement system
        if (sim_systems_) {
            auto mt = sim_systems_->movement.get_state(i).movement_type;
            switch (mt) {
                case MovementType::FOOT:    ev.movement_type = "Foot";    break;
                case MovementType::WHEELED: ev.movement_type = "Wheeled"; break;
                case MovementType::TRACKED: ev.movement_type = "Tracked"; break;
                default:                    ev.movement_type = "Other";   break;
            }
        }

        // Heading: use stored heading (updated by movement system), fallback to velocity
        ev.heading = storage.heading[i];
        if (storage.vel_x[i] != 0.0 || storage.vel_y[i] != 0.0) {
            ev.heading = std::atan2(storage.vel_y[i], storage.vel_x[i]);
        }

        state_.entity_vis.push_back(ev);

        // Track bounds (only alive entities)
        if (ev.alive) {
            min_x = std::min(min_x, ev.x);
            max_x = std::max(max_x, ev.x);
            min_y = std::min(min_y, ev.y);
            max_y = std::max(max_y, ev.y);
        }
    }

    // Set bounds with 10% margin
    if (min_x < max_x && min_y < max_y) {
        f64 margin_x = (max_x - min_x) * 0.1 + 1000.0;  // min 1km margin
        f64 margin_y = (max_y - min_y) * 0.1 + 1000.0;
        state_.sim_control.bounds_min_x = min_x - margin_x;
        state_.sim_control.bounds_max_x = max_x + margin_x;
        state_.sim_control.bounds_min_y = min_y - margin_y;
        state_.sim_control.bounds_max_y = max_y + margin_y;
    }
}

void UnifiedApp::log_event(const std::string& msg) {
    state_.event_log.push_back(msg);
    // Keep log bounded
    if (state_.event_log.size() > 1000) {
        state_.event_log.erase(state_.event_log.begin(),
                               state_.event_log.begin() + 500);
    }
}

// =============================================================================
// Monte-Carlo
// =============================================================================

void UnifiedApp::configure_monte_carlo(u32 iterations, Seed seed,
                                        bool record_timeline) {
    mc_config_.num_iterations    = iterations;
    mc_config_.master_seed       = seed;
    mc_config_.record_timeline   = record_timeline;

    state_.mc_control.num_iterations = iterations;
    state_.mc_control.master_seed    = seed;
}

bool UnifiedApp::run_monte_carlo() {
    if (!state_.sim_control.is_loaded) {
        set_status("No scenario loaded for Monte Carlo",
                   UnifiedAppState::StatusLevel::ERROR);
        return false;
    }

    // Don't start if already running
    if (mc_running_.load()) {
        set_status("Monte Carlo already running",
                   UnifiedAppState::StatusLevel::WARNING);
        return false;
    }

    // Don't start MC while interactive simulation is active — the MC thread
    // creates its own SystemsBundle which will clobber the global pointers
    // used by the interactive simulation when it destructs.
    if (state_.sim_control.is_running && sim_scheduler_) {
        set_status("Stop interactive simulation before running Monte Carlo",
                   UnifiedAppState::StatusLevel::WARNING);
        return false;
    }

    // Join any previous thread
    if (mc_thread_.joinable()) mc_thread_.join();

    auto& mc = state_.mc_control;
    mc.is_running = true;
    mc.completed_iterations = 0;
    mc.start_time_ms = std::chrono::duration<f64, std::milli>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    mc_running_.store(true);
    mc_finished_.store(false);
    mc_cancel_requested_.store(false);
    mc_progress_.store(0);
    mc_pending_ok_ = false;

    // Capture config and scenario by value for thread safety
    auto config = mc_config_;
    auto scenario_copy = state_.scenario.scenario;

    // v1.2.1: Pass weather/environment settings from GUI to MC
    config.env_config.climate = static_cast<Climate>(state_.scenario.climate_idx);
    config.env_config.weather = static_cast<Weather>(state_.scenario.weather_idx);
    config.env_config.visibility_km = state_.scenario.visibility_km;
    config.env_config.temperature_c = state_.scenario.temperature_c;
    config.env_config.wind_speed_kmh = state_.scenario.wind_speed_kmh;
    config.env_config.day_night_enabled = state_.scenario.day_night;

    // Progress callback: update atomic counter (thread-safe, no UI calls)
    config.progress_callback = [this](u32 current, u32 /*total*/) {
        mc_progress_.store(current);
    };

    // Launch background thread
    mc_thread_ = std::thread([this, config, scenario_copy]() mutable {
        analysis::MonteCarloExecutor executor;
        // Override progress callback to also check cancel flag
        config.progress_callback = [this, &executor](u32 current, u32 /*total*/) {
            mc_progress_.store(current);
            if (mc_cancel_requested_.load()) {
                executor.cancel();
            }
        };
        executor.configure(config);
        executor.set_scenario(scenario_copy);
        executor.set_platform_database(&state_.browser.db);

        auto status = executor.execute();

        // Store results under mutex
        {
            std::lock_guard<std::mutex> lock(mc_result_mutex_);
            mc_pending_ok_ = status.ok();
            if (status.ok()) {
                mc_pending_results_ = executor.results();
                mc_pending_stats_   = executor.compute_statistics();
            }
        }

        mc_finished_.store(true);
        mc_running_.store(false);
    });

    set_status("Monte Carlo running in background...");
    return true;
}

// Called from update() every frame — harvests results when MC thread finishes
void UnifiedApp::cancel_monte_carlo() {
    mc_cancel_requested_.store(true);
    set_status("Monte Carlo cancellation requested...");
}

void UnifiedApp::check_mc_completion() {
    if (!mc_finished_.load()) return;
    mc_finished_.store(false);

    // Join the thread
    if (mc_thread_.joinable()) mc_thread_.join();

    auto& mc = state_.mc_control;
    mc.is_running = false;

    std::lock_guard<std::mutex> lock(mc_result_mutex_);

    if (!mc_pending_ok_) {
        set_status("Monte Carlo execution failed",
                   UnifiedAppState::StatusLevel::ERROR);
        return;
    }

    // Harvest results into app state
    state_.analysis.results  = std::move(mc_pending_results_);
    state_.analysis.stats    = mc_pending_stats_;
    state_.analysis.has_data = true;
    mc.stats                 = mc_pending_stats_;
    mc.completed_iterations  = mc_config_.num_iterations;

    // Write execution manifest (audit trail)
    f64 total_ms = mc_pending_stats_.total_batch_time_ms;

    // v1.1.7: Compute confidence and sensitivity BEFORE manifest
    // so the manifest includes these fields
    compute_result_confidence();
    compute_sensitivity_summary();

    write_execution_manifest(mc_pending_stats_, mc.master_seed,
                             mc_pending_stats_.completed_iterations, total_ms);

    set_status("Monte Carlo complete: " +
               std::to_string(mc.completed_iterations) + " iterations");
    state_.active_view = UnifiedAppState::View::ANALYSIS;
}

// =============================================================================
// Export
// =============================================================================

bool UnifiedApp::export_results_json(const std::string& path) {
    if (!state_.analysis.has_data) {
        set_status("No results to export", UnifiedAppState::StatusLevel::WARNING);
        return false;
    }

    // Use the executor's JSON export as a simple path
    // For now, write a minimal summary
    FILE* f = fopen(path.c_str(), "w");
    if (!f) {
        set_status("Cannot open file: " + path,
                   UnifiedAppState::StatusLevel::ERROR);
        return false;
    }

    const auto& s = state_.analysis.stats;
    fprintf(f, "{\n");
    fprintf(f, "  \"version\": \"%s\",\n", IntegratedVersion::STRING);
    fprintf(f, "  \"iterations\": %u,\n", s.completed_iterations);
    fprintf(f, "  \"blue_wins\": %u,\n", s.blue_wins);
    fprintf(f, "  \"red_wins\": %u,\n", s.red_wins);
    fprintf(f, "  \"draws\": %u,\n", s.draws);
    fprintf(f, "  \"blue_survival_mean\": %.4f,\n", s.blue_survival_rate.mean);
    fprintf(f, "  \"red_survival_mean\": %.4f\n", s.red_survival_rate.mean);
    fprintf(f, "}\n");
    fclose(f);

    set_status("Exported results to: " + path);
    return true;
}

bool UnifiedApp::export_results_binary(const std::string& path) {
    // TODO: Binary export via ResultSerializer
    (void)path;
    set_status("Binary export not yet implemented",
               UnifiedAppState::StatusLevel::WARNING);
    return false;
}

// =============================================================================
// Auto-Center Scenario Map
// =============================================================================

void UnifiedApp::auto_center_scenario_map() {
    const auto& actors = state_.scenario.scenario.actors;
    auto& sc = state_.scenario;

    if (actors.empty()) {
        // Default: Suwalki Gap area
        sc.map_center_lat = 55.0;
        sc.map_center_lon = 21.0;
        sc.map_span_lat   = 2.5;
        sc.map_span_lon   = 3.0;
        return;
    }

    // Compute bounding box of all actors
    f64 min_lat = 90, max_lat = -90;
    f64 min_lon = 180, max_lon = -180;
    for (const auto& a : actors) {
        min_lat = std::min(min_lat, a.initial_position.lat);
        max_lat = std::max(max_lat, a.initial_position.lat);
        min_lon = std::min(min_lon, a.initial_position.lon);
        max_lon = std::max(max_lon, a.initial_position.lon);
    }

    sc.map_center_lat = (min_lat + max_lat) * 0.5;
    sc.map_center_lon = (min_lon + max_lon) * 0.5;

    // Add 30% margin, minimum 0.2 degrees
    sc.map_span_lat = std::max((max_lat - min_lat) * 1.6, 0.2);
    sc.map_span_lon = std::max((max_lon - min_lon) * 1.6, 0.2);
}

// =============================================================================
// Store Variant Result for Comparative Analysis
// =============================================================================

void UnifiedApp::store_variant_result(const std::string& name, Seed seed,
                                       u32 iterations,
                                       const analysis::BatchStatistics& stats) {
    UnifiedAppState::VariantResult vr;
    vr.name       = name;
    vr.seed       = seed;
    vr.iterations = iterations;
    vr.stats      = stats;
    state_.variant_results.push_back(vr);
}

// =============================================================================
// Execution Manifest — audit trail for determinism verification
// =============================================================================

void UnifiedApp::write_execution_manifest(const analysis::BatchStatistics& stats,
                                           Seed seed, u32 iterations, f64 total_ms) {
    // Build JSON manifest
    json::Object root;
    root["athena_version"]  = json::String(IntegratedVersion::STRING);
    root["manifest_version"] = json::String("1.0");

    // Timestamp
    {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        char ts[64];
        std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", std::localtime(&t));
        root["timestamp"] = json::String(ts);
    }

    // Platform info
    json::Object platform;
#if defined(__linux__)
    platform["os"] = json::String("linux");
#elif defined(_WIN32)
    platform["os"] = json::String("windows");
#else
    platform["os"] = json::String("unknown");
#endif
#if defined(__x86_64__) || defined(_M_X64)
    platform["arch"] = json::String("x86_64");
#elif defined(__aarch64__)
    platform["arch"] = json::String("aarch64");
#else
    platform["arch"] = json::String("unknown");
#endif
#if defined(__GNUC__)
    char comp[64];
    snprintf(comp, sizeof(comp), "gcc %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
    platform["compiler"] = json::String(comp);
#elif defined(_MSC_VER)
    platform["compiler"] = json::String("msvc " + std::to_string(_MSC_VER));
#endif
    platform["fp_model"] = json::String("strict (fno-fast-math, ffp-contract=off)");
    root["platform"] = json::Value(platform);

    // Scenario info
    json::Object scenario;
    scenario["id"]   = json::String(state_.scenario.scenario.id);
    scenario["name"] = json::String(state_.scenario.scenario.name);
    scenario["actors"] = json::Number(static_cast<f64>(state_.scenario.scenario.actors.size()));
    scenario["tick_duration_seconds"] = json::Number(state_.scenario.scenario.temporal.tick_duration_seconds);
    scenario["max_ticks"] = json::Number(static_cast<f64>(state_.scenario.scenario.temporal.max_ticks));

    // Simple scenario hash (sum of actor positions + firepower for change detection)
    u64 sc_hash = 0;
    for (const auto& a : state_.scenario.scenario.actors) {
        u64 lat_bits, lon_bits;
        std::memcpy(&lat_bits, &a.initial_position.lat, sizeof(f64));
        std::memcpy(&lon_bits, &a.initial_position.lon, sizeof(f64));
        sc_hash ^= lat_bits * 2654435761ULL;
        sc_hash ^= lon_bits * 40503ULL;
    }
    char hash_buf[20];
    snprintf(hash_buf, sizeof(hash_buf), "%016llx", (unsigned long long)sc_hash);
    scenario["hash"] = json::String(hash_buf);
    root["scenario"] = json::Value(scenario);

    // Execution
    json::Object exec;
    exec["seed"]       = json::Number(static_cast<f64>(seed));
    exec["iterations"] = json::Number(static_cast<f64>(iterations));
    exec["completed"]  = json::Number(static_cast<f64>(stats.completed_iterations));
    exec["converged_early"] = json::Bool(stats.converged_early);
    exec["total_time_ms"]   = json::Number(total_ms);
    exec["ms_per_iteration"] = json::Number(stats.iteration_time_ms.mean);
    root["execution"] = json::Value(exec);

    // Results summary
    json::Object results;
    f64 total = std::max(stats.completed_iterations, 1u);
    results["blue_win_pct"] = json::Number(100.0 * stats.blue_wins / total);
    results["red_win_pct"]  = json::Number(100.0 * stats.red_wins / total);
    results["draw_pct"]     = json::Number(100.0 * stats.draws / total);
    results["blue_survival_mean"] = json::Number(stats.blue_survival_rate.mean);
    results["red_survival_mean"]  = json::Number(stats.red_survival_rate.mean);
    root["results"] = json::Value(results);

    // v1.1.7: Actors (units list for auditability)
    json::Array actors_arr;
    for (const auto& a : state_.scenario.scenario.actors) {
        json::Object actor;
        actor["id"]   = json::String(a.id);
        actor["name"] = json::String(a.name);
        actor["side"] = json::String(a.side);
        actor["type"] = json::String(a.type);
        if (!a.platform_id.empty())
            actor["platform"] = json::String(a.platform_id);
        actor["lat"] = json::Number(a.initial_position.lat);
        actor["lon"] = json::Number(a.initial_position.lon);
        actors_arr.push_back(json::Value(actor));
    }
    root["actors"] = json::Value(actors_arr);

    // v1.1.7: Terrain config
    {
        const auto& tc = state_.scenario.scenario.environment.terrain;
        json::Object terrain;
        terrain["base_type"]       = json::String(tc.base_type);
        terrain["roughness"]       = json::Number(tc.roughness);
        terrain["forest_density"]  = json::Number(tc.forest_density);
        terrain["urban_density"]   = json::Number(tc.urban_density);
        terrain["rivers_enabled"]  = json::Bool(tc.rivers_enabled);
        root["terrain"] = json::Value(terrain);
    }

    // v1.2.3: Weather/environment conditions (audit trail for MC)
    {
        const auto& ss = state_.scenario;
        json::Object weather;
        weather["weather"]       = json::String(weather_name(
            static_cast<Weather>(ss.weather_idx)));
        weather["climate"]       = json::String(climate_name(
            static_cast<Climate>(ss.climate_idx)));
        weather["visibility_km"] = json::Number(static_cast<f64>(ss.visibility_km));
        weather["temperature_c"] = json::Number(static_cast<f64>(ss.temperature_c));
        weather["wind_speed_kmh"]= json::Number(static_cast<f64>(ss.wind_speed_kmh));
        weather["day_night"]     = json::Bool(ss.day_night);
        root["weather"] = json::Value(weather);
    }

    // v1.1.7: Termination reason distribution
    {
        json::Object term;
        term["decisive_victory"]      = json::Number(static_cast<f64>(stats.term_decisive));
        term["time_limit"]            = json::Number(static_cast<f64>(stats.term_max_ticks));
        term["no_offensive_capacity"] = json::Number(static_cast<f64>(stats.term_no_offensive));
        term["mutual_blindness"]      = json::Number(static_cast<f64>(stats.term_mutual_blind));
        term["unreachable"]           = json::Number(static_cast<f64>(stats.term_unreachable));
        root["termination_reasons"] = json::Value(term);
    }

    // v1.1.7: Sensitivity factors (if computed)
    if (!stats.sensitivity_factors.empty()) {
        json::Array sens_arr;
        for (const auto& [name, impact] : stats.sensitivity_factors) {
            json::Object f;
            f["factor"] = json::String(name);
            f["impact"] = json::Number(impact);
            sens_arr.push_back(json::Value(f));
        }
        root["sensitivity_factors"] = json::Value(sens_arr);
    }

    // v1.1.7: Confidence assessment (computed before this call)
    {
        const auto& an = state_.analysis;
        const char* conf_str = "low";
        switch (an.confidence) {
            case UnifiedAppState::AnalysisState::Confidence::HIGH:   conf_str = "high"; break;
            case UnifiedAppState::AnalysisState::Confidence::MEDIUM: conf_str = "medium"; break;
            default: break;
        }
        json::Object conf;
        conf["level"] = json::String(conf_str);
        conf["text"]  = json::String(an.confidence_text);
        root["confidence"] = json::Value(conf);
    }

    // v1.1.7: Sensitivity summary (plain language)
    if (!state_.analysis.sensitivity_summary.empty()) {
        json::Array sum_arr;
        for (const auto& line : state_.analysis.sensitivity_summary) {
            sum_arr.push_back(json::String(line));
        }
        root["sensitivity_summary"] = json::Value(sum_arr);
    }

    // Serialize and write to {scenarios_dir}/manifests/
    std::string manifest_dir = scenarios_dir_ + "/manifests";
#ifdef _WIN32
    CreateDirectoryA(manifest_dir.c_str(), nullptr);
#else
    mkdir(manifest_dir.c_str(), 0755);
#endif

    // Filename: manifest_{scenario_name}_{seed}_{timestamp}.json
    // Sanitize scenario name for filename (alphanumeric + underscore only)
    std::string safe_name;
    for (char c : state_.scenario.scenario.name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')
            safe_name += c;
        else if (c == ' ')
            safe_name += '_';
    }
    if (safe_name.empty()) safe_name = "unnamed";
    if (safe_name.size() > 40) safe_name.resize(40);

    auto now = std::chrono::system_clock::now();
    auto epoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    std::string fname = manifest_dir + "/manifest_" + safe_name + "_" +
                        std::to_string(seed) + "_" +
                        std::to_string(epoch) + ".json";

    std::string json_str = json::to_string_pretty(json::Value(root));
    FILE* f = fopen(fname.c_str(), "w");
    if (f) {
        fwrite(json_str.c_str(), 1, json_str.size(), f);
        fclose(f);
    }
}

// =============================================================================
// Scenario Validation (v1.1.7)
// =============================================================================

void UnifiedApp::validate_scenario() {
    auto& warnings = state_.validation_warnings;
    warnings.clear();
    
    const auto& sc = state_.scenario.scenario;
    
    // Check: no actors at all
    if (sc.actors.empty()) {
        warnings.push_back({"No units in scenario", true});
        return;
    }
    
    // Check: sides without units
    std::map<std::string, int> side_counts;
    for (const auto& a : sc.actors) {
        side_counts[a.side]++;
    }
    
    // Need at least 2 opposing sides
    int combatant_sides = 0;
    for (const auto& [side, count] : side_counts) {
        if (side != "neutral" && count > 0) combatant_sides++;
    }
    if (combatant_sides < 2) {
        warnings.push_back({"Only one combatant side — no engagement possible", true});
    }
    
    // Check each actor
    for (const auto& a : sc.actors) {
        // Units without mobility
        if (a.mobility.has_value() && a.mobility->max_speed_kmh <= 0.0 &&
            a.side != "neutral") {
            warnings.push_back({"Unit \"" + a.name + "\" has zero mobility", false});
        }
        
        // Position check: all units at same location
        // (collected below)
    }
    
    // Check: all units at same position (distance < 100m)
    if (sc.actors.size() >= 2) {
        bool all_same = true;
        f64 ref_lat = sc.actors[0].initial_position.lat;
        f64 ref_lon = sc.actors[0].initial_position.lon;
        for (size_t i = 1; i < sc.actors.size(); ++i) {
            f64 dlat = sc.actors[i].initial_position.lat - ref_lat;
            f64 dlon = sc.actors[i].initial_position.lon - ref_lon;
            if (std::abs(dlat) > 0.001 || std::abs(dlon) > 0.001) {
                all_same = false;
                break;
            }
        }
        if (all_same) {
            warnings.push_back({"All units at same position — initial distance is zero", false});
        }
    }
    
    // Check: no firepower on any side
    bool any_firepower = false;
    for (const auto& a : sc.actors) {
        if (a.side == "neutral") continue;
        if (a.firepower.has_value() && a.firepower->range_km > 0) {
            any_firepower = true;
            break;
        }
        // Also check platform reference — harder to verify without DB lookup,
        // so we trust the platform_id presence as proxy
        if (!a.platform_id.empty()) {
            any_firepower = true;
            break;
        }
    }
    if (!any_firepower) {
        warnings.push_back({"No unit has firepower — combat impossible", true});
    }
    
    // Duration check
    f64 total_hours = sc.temporal.tick_duration_seconds * sc.temporal.max_ticks / 3600.0;
    if (total_hours < 1.0) {
        warnings.push_back({"Simulation duration under 1 hour — may end before first contact", false});
    }
}

// =============================================================================
// Result Confidence (v1.1.7)
// =============================================================================

void UnifiedApp::compute_result_confidence() {
    auto& a = state_.analysis;
    if (!a.has_data || a.stats.completed_iterations == 0) {
        a.confidence = UnifiedAppState::AnalysisState::Confidence::LOW;
        a.confidence_text = "No data available";
        return;
    }
    
    const auto& s = a.stats;
    u32 n = s.completed_iterations;
    
    // Variance of blue win rate (binomial: p*(1-p)/n)
    f64 total = std::max(n, 1u);
    f64 p = s.blue_wins / total;
    f64 variance = (n >= 2) ? p * (1.0 - p) / (n - 1) : 1.0;
    f64 stddev = std::sqrt(variance);
    
    // Coefficient of variation of survival rates
    f64 blue_cv = safe::div(s.blue_survival_rate.stddev, 
                            std::max(s.blue_survival_rate.mean, 0.001));
    f64 red_cv  = safe::div(s.red_survival_rate.stddev, 
                            std::max(s.red_survival_rate.mean, 0.001));
    f64 avg_cv = (blue_cv + red_cv) / 2.0;
    
    // Scoring: high confidence needs many runs, low variance, convergence
    bool converged = s.converged_early;
    
    if (n >= 10000 && stddev < 0.01 && avg_cv < 0.3) {
        a.confidence = UnifiedAppState::AnalysisState::Confidence::HIGH;
        a.confidence_text = "Result stable. Simulation converged.";
    } else if (n >= 1000 && stddev < 0.03 && avg_cv < 0.5) {
        a.confidence = UnifiedAppState::AnalysisState::Confidence::HIGH;
        a.confidence_text = converged ? "Simulation converged." 
                                      : "Result stable with high sample count.";
    } else if (n >= 100 && stddev < 0.05) {
        a.confidence = UnifiedAppState::AnalysisState::Confidence::MEDIUM;
        a.confidence_text = "Results reasonably stable. More simulations may refine estimates.";
    } else {
        a.confidence = UnifiedAppState::AnalysisState::Confidence::LOW;
        char buf[128];
        snprintf(buf, sizeof(buf), "Results still unstable (variance %.1f%%). "
                 "Increase number of simulations.", stddev * 100);
        a.confidence_text = buf;
    }
}

// =============================================================================
// Sensitivity Summary (v1.1.7) — plain language
// =============================================================================

void UnifiedApp::compute_sensitivity_summary() {
    auto& a = state_.analysis;
    a.sensitivity_summary.clear();
    
    if (!a.has_data) return;
    
    const auto& factors = a.stats.sensitivity_factors;
    if (factors.empty()) {
        a.sensitivity_summary.push_back("Not enough data to identify key factors.");
        return;
    }
    
    // Build plain-language summary
    std::string top_line = "Results most sensitive to: ";
    for (size_t i = 0; i < std::min<size_t>(3, factors.size()); ++i) {
        if (i > 0) top_line += ", ";
        top_line += factors[i].first;
    }
    top_line += ".";
    a.sensitivity_summary.push_back(top_line);
    
    // Termination reasons summary
    const auto& s = a.stats;
    u32 total = s.completed_iterations;
    if (total > 0) {
        if (s.term_decisive > 0) {
            char buf[128];
            snprintf(buf, sizeof(buf), "%.0f%% ended by decisive victory.",
                     100.0 * s.term_decisive / total);
            a.sensitivity_summary.push_back(buf);
        }
        if (s.term_max_ticks > 0) {
            char buf[128];
            snprintf(buf, sizeof(buf), "%.0f%% reached time limit without resolution.",
                     100.0 * s.term_max_ticks / total);
            a.sensitivity_summary.push_back(buf);
        }
        if (s.term_no_offensive > 0) {
            char buf[128];
            snprintf(buf, sizeof(buf), "%.0f%% ended by loss of offensive capacity.",
                     100.0 * s.term_no_offensive / total);
            a.sensitivity_summary.push_back(buf);
        }
        if (s.term_mutual_blind > 0) {
            char buf[128];
            snprintf(buf, sizeof(buf), "%.0f%% ended by mutual detection failure.",
                     100.0 * s.term_mutual_blind / total);
            a.sensitivity_summary.push_back(buf);
        }
    }
}

// =============================================================================
// TUI Browser (free function)
// =============================================================================

int run_tui_browser(const std::string& data_path) {
    // Forward to existing TUI browser if available, else minimal fallback
    PlatformDatabase db;
    auto result = db.load_all(data_path);
    if (!result.ok()) {
        fprintf(stderr, "Failed to load platforms from: %s\n", data_path.c_str());
        return 1;
    }
    printf("ATHENA %s - Platform Database\n", IntegratedVersion::STRING);
    printf("Loaded %zu platforms\n", db.count());
    printf("Use 'gui' mode for interactive browser.\n");
    return 0;
}

}  // namespace athena
