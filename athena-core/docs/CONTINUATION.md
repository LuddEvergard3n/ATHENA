# ATHENA Continuation File

## Current Version
**v1.1.8 "Operational"** — 2026-02-22

## What Was Done in v1.1.8

### Operational Wear System
1. **New SoA fields in EntityStorage**: fatigue [0,1], cohesion [0,1], stress_ticks, rest_ticks, casualty_rate
2. **New entity flags**: FATIGUED (0x10), BROKEN (0x20)
3. **is_operational() helper** — ACTIVE && !BROKEN
4. **operational_snapshot_update()** (PRE_TICK) — captures health before combat for casualty rate computation
5. **operational_wear_update()** (POST_TICK) — accumulates fatigue, computes cohesion, checks collapse
6. **Fatigue accumulation**: engaged +0.025/tick, moving +0.015, idle +0.003, recovery -0.01/tick after 3+ rest ticks
7. **Cohesion**: degrades from casualty_rate * 0.30, excess fatigue * 0.10, isolation 0.02/tick. Recovers 0.003/tick when safe
8. **Cohesion collapse at 0.25**: unit flagged BROKEN, cannot move or engage

### Recon Propagation
9. **propagate_contacts_by_side()** — after detection update, all contacts shared within same side
10. **Shared contacts degraded**: position accuracy * 2.0, identification confidence * 0.7
11. **get_side_contacts()** — merged best-accuracy contact per target per side

### Combat Integration
12. **Fatigue in effectiveness**: (1.0 - fatigue * 0.5)
13. **Cohesion in effectiveness**: (0.3 + 0.7 * cohesion)
14. **BROKEN units blocked from can_engage()**

### Tactical AI Overhaul
15. **Phase 1**: Direct LOS detection (unchanged sensor range check)
16. **Phase 2**: Shared contacts from detection system (replaces global omniscient targeting)
17. **Phase 3**: No contacts = hold position (no more marching toward unknown enemies)
18. **BROKEN units**: vel = 0, skip AI processing
19. **Fatigue speed penalty**: unit_cruise *= (1.0 - fatigue * 0.4)

### GUI
20. **Broken units**: hollow circle + diagonal yellow line
21. **Fatigued units**: orange ring around marker
22. **Cohesion bar**: below health bar, color-coded (blue > orange > red)
23. **EntityVis**: added fatigue, cohesion, broken, fatigued fields

### Build
24. **New files**: include/athena/systems/operational.hpp, src/systems/operational.cpp
25. **Modified**: entities.hpp (5 SoA fields, 2 flags, is_operational), detection.hpp/cpp (propagation), combat.cpp (fatigue+cohesion in effectiveness, BROKEN check), tactical_ai.cpp (detection contacts, BROKEN, fatigue speed), integrated.hpp (EntityVis), integrated.cpp (sync + system registration), integrated_gui.cpp (visual indicators), montecarlo.cpp (system registration), systems.hpp (operational include), Makefile.unified (operational.cpp)

## What Already Existed (verified, not modified)
- `-fno-fast-math -ffp-contract=off` in Makefile.unified
- MC background thread + cancel + progress bar + ETA
- Quick/Standard/Custom MC dialog, seed in Advanced > Reproducibility
- Convergence (Wilson score CI), 15 unit types, 25 terrain presets, 6 sides
- Outcome analysis, confidence indicators, run manifests
- NaN guards in combat/detection/movement/logistics
- Force Builder dialog, Auto-Type inference (added late v1.1.7)

## Critical Notes
- **Recon propagation removes omniscient targeting.** Units without contacts will now hold position instead of marching toward enemies they shouldn't know about. Simulation dynamics will change significantly.
- **Cohesion collapse is permanent unless conditions improve.** A BROKEN unit cannot recover unless cohesion rises above 0.10 AND it has nearby friendlies AND is not taking casualties. In practice, most BROKEN units stay broken.
- **Fatigue rates assume 1-hour ticks.** With shorter ticks, fatigue accumulates proportionally faster (may need dt-scaling in future versions).
- **Casualty rate memory** means that a unit hit hard in early ticks carries that "trauma" for many subsequent ticks, even if no longer taking damage. This is intentional — sustained operations degrade capability.
- Old scenarios (v1.1.2) with `tick_duration_seconds: 0` still fail to load.

## Known Issues
- No alliance system, no keyboard shortcuts, no native file dialogs
- No per-cell terrain painting
- Sensitivity uses outcome-metric correlation (not Sobol)
- Force Builder limited to 20 units per deploy
- Fatigue/cohesion rates not yet dt-scaled (tuned for 1-hour ticks)
- No "reorganize" command to manually reset BROKEN status

## Roadmap
- **v1.2 "Pathfinder"** — A* pathfinding, 100k benchmark, performance optimization

## Build
```bash
# Linux
cd athena-core && make -f Makefile.unified athena -j$(nproc)

# Windows cross-compile (requires GLFW Win64 binaries)
CXX=x86_64-w64-mingw32-g++
CXXFLAGS="-std=c++17 -O2 -fno-fast-math -ffp-contract=off -DATHENA_WIN32"
# Compile each .cpp, link: -mwindows -static -lglfw3 -lopengl32 -lgdi32 -limm32 -lpthread
```

## Supported Platforms
- **Linux x86_64**: GCC 12+, strict FP
- **Windows x64**: MinGW cross-compiled, static, zero external DLLs
- **FP Model**: IEEE 754 strict, no FMA, no fast-math
