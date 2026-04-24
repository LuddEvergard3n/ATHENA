# ATHENA Continuation File

## Current Version
**v1.2.5 "Tactical"** — 2026-02-22

## What Was Done in v1.2.5

### A* Pathfinding Integration
1. **Per-entity A* pathfinding in tactical AI.** Units compute A* paths to targets using the existing Pathfinder class. Replaces straight-line movement with angular offset avoidance.
2. **Path caching.** `EntityPathCache` stores waypoints, current index, goal position per entity. Re-paths when: no cached path, target moved >500m, or path exhausted.
3. **Smooth paths.** After A* computes raw grid path, `smooth_path()` removes redundant waypoints via line-of-sight checks. Reduces waypoint count and produces natural movement.
4. **Waypoint following.** Unit heads toward next waypoint (not target). Advances to next waypoint when within 150m. Displacement clamped to not overshoot waypoints.
5. **Legacy fallback.** When `g_tactical_ai_pathfinder == nullptr` (no terrain), original straight-line + angular offset avoidance is used unchanged.
6. **MC integration.** Each MC iteration creates a local `Pathfinder` from its local `TerrainSemantics` and initializes its own path cache. Deterministic: same terrain + seed = same paths.
7. **Grid parameters.** Cell size: 200m. Max iterations: 50,000. Diagonal movement enabled. Heuristic weight: 1.0 (standard A*).

### Weather Persistence
8. **Save weather to scenario JSON.** `save_scenario()` writes `environment.weather` object: `weather_type` (int enum index), `climate_type`, `visibility_km`, `temperature_c`, `wind_speed_kmh`, `day_night`.
9. **Load weather from scenario JSON.** `load_scenario()` re-reads the JSON to parse `environment.weather` and restores GUI state. Backward compatible: missing weather section = defaults.

## What Was Done in v1.2.3
- Suppression in tactical AI, movement fuel_modifier, rest_seconds_for_recovery, entity tooltip, MovementType from JSON, Reorganize command, weather export to manifest, keyboard shortcuts

## What Was Done in v1.2.2
- Weather consumption (logistics/detection/combat/MC passthrough), threat-facing AI, dt-scaling

## Files Modified in v1.2.5

### Headers
- **integrated.hpp**: Version 1.2.5, `#include pathfinding.hpp`, `sim_pathfinder_` member
- **systems/tactical_ai.hpp**: `#include pathfinding.hpp`, `g_tactical_ai_pathfinder` global, `EntityPathCache` struct, `tactical_ai_init_path_cache()`, `tactical_ai_clear_path_cache()`

### Sources
- **systems/tactical_ai.cpp**: `s_path_cache` static vector, path cache management functions, A* path computation and waypoint following in `seek_enemy_ai_update()`, legacy fallback preserved
- **integrated.cpp**: Create/destroy `Pathfinder` in start/stop_simulation, init path cache, set `g_tactical_ai_pathfinder`, weather save/load in save_scenario/load_scenario
- **analysis/montecarlo.cpp**: Local `Pathfinder` per MC iteration, path cache init

## Critical Notes
- **A* changes tactical behavior.** Units now route around impassable terrain intelligently. Rivers, urban barriers, dense forest are navigated around rather than causing units to halt. This affects simulation outcomes.
- **Performance.** A* at 200m cells with 50k iteration cap handles ~10km paths efficiently. Path smoothing and caching (re-path only when target moves >500m) keep per-tick cost low. Most ticks follow cached waypoints (zero pathfinding cost).
- **Determinism preserved.** Path cache is per-entity, A* expansion order is deterministic (ordered neighbor enumeration), and `smooth_path` is deterministic. Same inputs produce same paths.
- **Weather round-trip.** Save then load preserves exact weather state. Old scenario files without weather section load with defaults (clear, temperate, 10km vis, 15°C, 10km/h wind, daytime).
- **Thread safety.** `s_path_cache` is static but only accessed from seek_enemy_ai_update which runs sequentially. MC iterations reset it per-iteration. Interactive sim is paused during MC.

## Known Issues
- No road network / road preference (A* uses terrain costs only)
- No alliance system
- A* path visualization not shown on GUI map (paths are internal)

## Roadmap
- **v1.3** — Road network utilization, A* with road preference, 100k benchmark
- **v1.4** — Alliance system, path visualization overlay

## Test Results (v1.2.5)
| Suite | Result |
|---|---|
| test_determinism | 16/16 PASS |
| test_terrain | 16/16 PASS |
| test_integration | 12/12 PASS |
| test_scenario | 13/13 PASS |
| test_platform_loader | 10/10 PASS |
| test_pathfinding | 12/12 PASS |
| test_montecarlo | 7/7 PASS |
| test_detection | 12/12 PASS |
| test_c2 | 13/13 PASS |
| test_ai_doctrine | 9/9 PASS |

## Build
```bash
cd athena-core && make -f Makefile.unified athena -j$(nproc)
```
