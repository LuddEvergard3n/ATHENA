# ATHENA Changelog

## v1.1.8 "Operational" (2026-02-22)

### Operational Wear System (NEW)
- **Fatigue accumulation** — units accumulate fatigue when moving (+0.015/tick), fighting (+0.025/tick), or even idle (+0.003/tick). Recovery requires sustained rest (3+ ticks without movement or combat). Above 0.7: FATIGUED flag set, movement speed reduced up to 40%, combat effectiveness degraded up to 50%
- **Force Cohesion** — organizational effectiveness metric [0, 1] distinct from morale. Degrades from: sustained casualties (rolling casualty rate * 0.30), high fatigue, isolation from friendlies (no allies within 5km). Recovers very slowly (0.003/tick) only when: not in combat, friendlies nearby, casualty rate near zero, cohesion above minimum recovery threshold (0.10)
- **Cohesion Collapse** — when cohesion falls below 0.25, unit is flagged BROKEN. Physically alive but operationally dead: cannot engage, cannot move purposefully. This models the real-world phenomenon where a unit exists on paper but has lost organizational ability to fight
- **Casualty Rate Memory** — rolling exponential accumulator of health loss per tick (decay 0.5x/tick). Sustained combat degrades cohesion much faster than a single hit. Combat is now cumulative, not per-tick independent
- **Health Snapshot System** — PRE_TICK phase captures health before combat for accurate casualty rate computation
- **Combat Integration** — fatigue and cohesion directly modify combat effectiveness. Fatigued, disorganized units fight at a fraction of nominal capability

### Recon Propagation (NEW)
- **Side-level shared detection** — when any unit detects an enemy, all friendly units on the same side receive the contact. Shared contacts have 2x position error and 0.7x identification confidence (communication degradation)
- **Detection drives targeting** — Tactical AI now uses detection system contacts instead of global enemy knowledge. Phase 1: direct LOS detection. Phase 2: shared intel from recon network. Phase 3: no contacts = hold position
- **get_side_contacts()** API — query all contacts known to a side (union of all units, best accuracy per target)
- This alone transforms ATHENA from local sensor simulation to operational-level reconnaissance modeling

### Tactical AI Overhaul
- **Intel-driven movement** — units without direct sensor contact now move toward last known enemy position from shared detection instead of omniscient global awareness
- **Broken unit behavior** — BROKEN units stop, cannot engage, cannot be given movement orders
- **Fatigue-degraded speed** — movement speed scales with fatigue (up to 40% reduction at max fatigue)
- **Three-phase targeting**: Direct (LOS) → Intel (shared contacts) → Blind (hold position)

### Force Builder (from v1.1.7+)
- **"Deploy Formation" button** — deploy N units (1-20) of the same platform in formation
- Formation patterns: Line (E-W), Column (N-S), Wedge (V), Spread (grid)
- Auto-spacing by platform type, auto-naming, cos-corrected longitude

### Auto-Type Inference (from v1.1.7+)
- Platform selection auto-sets UnitType (Tank→Armor, IFV→Mechanized, SAM→Air Defense, etc.)

### Visual Indicators
- **Broken units**: hollow circle with diagonal yellow line (operationally dead)
- **Fatigued units**: orange ring around unit marker
- **Cohesion bar**: purple/orange/red bar below health bar (appears when cohesion < 0.95)

---

## v1.1.7 "Audit" (2026-02-21)

### Outcome Analysis
- **Outcome Analysis panel** shown after every Monte Carlo run with:
  - Average time to first casualty, decisive outcome, and simulation duration
  - Top 5 sensitivity factors (Pearson correlation between metrics and outcome)
  - Plain-language summary: "Results most sensitive to: X, Y, Z."
  - Termination reason breakdown (decisive victory, time limit, no offense, mutual blindness, unreachable)
- Sensitivity factors displayed as impact bars (0-100%) in Outcome Analysis

### Result Confidence
- **Confidence indicator** (HIGH / MEDIUM / LOW) shown after MC run
  - HIGH: 1000+ runs, stddev < 3%, CV < 0.5 (or 10k+ with stricter thresholds)
  - MEDIUM: 100+ runs, stddev < 5%
  - LOW: insufficient data or high variance
- Displays: "Result stable. Simulation converged." or "Results still unstable — increase number of simulations."

### Run Manifest (Auditability)
- **Automatic `run_manifest.json`** written to `scenarios/manifests/` after every MC run
- Now includes: confidence level/text, sensitivity summary (plain language), sensitivity factors
- Filename includes scenario name for easier identification
- Full contents: ATHENA version, timestamp, platform (OS/arch/compiler/FP model), scenario (name/actors/terrain/hash), execution (seed/iterations/timing), results (win %/survival), actors list, terrain config, termination reason distribution

### Scenario Validation
- **Pre-run validation** with non-blocking warnings shown in MC dialog:
  - No actors, single combatant side, zero mobility, same position, no firepower, short duration
- Warnings classified as critical (red) or advisory (yellow)
- "Results may be unreliable. You can still run." — never blocks execution

### Stop Conditions
- **UNREACHABLE termination** now implemented: simulation ends early if remaining units cannot close distance to enemies within remaining time at max speed. Uses bounding-box gap + combined closing speed.
- Pre-existing: DECISIVE_VICTORY, NO_OFFENSIVE_CAPACITY, MUTUAL_BLINDNESS, MAX_TICKS
- `check_no_offensive()` cleanup: removed dead loop code

### Robustness (NaN/Inf Protection)
- `safe::finite()`, `safe::div()`, `safe::clamp()` utility namespace in types.hpp
- Applied in: combat (damage clamp), detection (range division, probability cap), movement (position revert), logistics (resupply division, morale clamp, readiness, fuel/ammo)
- Every system that performs division or produces floating-point output is now NaN-safe

### Performance
- MC executor runs on background `std::thread`, never blocks UI
- Cancel button functional (atomic flag, checked per-iteration)
- Progress bar with real-time ETA based on completed runs
- Wilson score convergence early-stop (unchanged from v1.1.6)

### UI Cleanup
- Duration shown in hours/days only (ticks hidden)
- Seed in Advanced > Reproducibility only (auto-generated by default)
- Quick (10k) / Standard (100k) / Custom mode selector
- No engineering parameters exposed in main flow

### Force Builder
- **"Deploy Formation" button** in Scenario Editor deploys N units of the same platform in formation
  - Platform search with full database (same as single-unit picker)
  - Side selector (all 5 sides)
  - Count slider (1-20 units)
  - Formation patterns: Line (E-W), Column (N-S), Wedge (V), Spread (grid)
  - Auto-spacing from platform type (armor: 200m, artillery: 300m, naval: 2000m, etc.)
  - Manual spacing override with logarithmic slider (10-10000m)
  - Units auto-named: "M1A2 SEPv3 Abrams #1", "#2", etc.
  - Deploys at map center with correct lat/lon spacing (cos-corrected longitude)

### Auto-Type Inference
- **Platform selection now auto-sets UnitType** from PlatformCategory + spec.type
  - Tank -> Armor, IFV/APC -> Mechanized, SAM/MANPADS -> Air Defense, etc.
  - Secondary refinements: type string "recon*" -> Recon, "transport*" -> Logistics
  - Regional platforms use secondary type-string matching
  - Eliminates the most common manual step when setting up scenarios

---

## v1.1.6 "Operator" (2026-02-21)

### Determinism & Reliability
- Floating-point determinism flags (`-fno-fast-math -ffp-contract=off`) verified in build
- `constants::VERSION` synchronized to 1.1.6 across all modules
- Execution manifest written to `scenarios/manifests/` after every MC run (seed, version, scenario hash, iterations, timing, platform info, FP model)

### Combat System
- Support units (Medical fp=0.0, Logistics fp=0.1) now correctly excluded from combat engagement. Threshold raised to 0.15 to prevent non-combatants from participating in damage calculations.

### Detection System
- UnitType `detection_factor` now applied to observer detection probability. Recon units (2.5x) detect significantly earlier; support units (0.6x) detect later. Previously this modifier existed in the table but was not wired into the detection pipeline.

### UI: Operator-Friendly
- Single simulation seed auto-generated from clock by default (was hardcoded to 42)
- Seed control moved to collapsible "Advanced" section in simulation tab (matches MC dialog)
- Monte Carlo dialog: Quick (10k) / Standard (100k) / Custom mode selector (unchanged, verified)
- Duration displayed in hours/days throughout (unchanged, verified)
- Seed and internal parameters hidden from main flow in both single sim and MC

### Platform Data Quality
- Off-road speed default added (60% of road speed when not specified)
- Confidence field defaults to "estimated" when missing
- All platforms guaranteed to have: type, mobility, detection range, protection, engagement range, firepower rating

---

## v1.1.3 "Battle Fix" (2026-02-20)

### Decision Support System
- **Analysis view now provides actionable assessments.** Each Monte Carlo result shows an inline assessment: FAVORABLE / CONTESTED / UNFAVORABLE / CRITICAL with clear guidance.
- **Comparative COA Analysis rewritten.** When 2+ variants are saved, ATHENA now computes a composite score (Win% x 0.5 + Force Preservation x 0.3 + Speed x 0.2) and displays:
  - **RECOMMENDED COA** with justification and advantage over alternatives
  - Risk classification per variant (LOW / MODERATE / HIGH / CRITICAL)
  - Tradeoff warnings (e.g., "Plan B has better force preservation - consider if casualty minimization is priority")
  - Worst-case / best-case survival ranges (p5-p95)
- **Workflow guidance** in empty Analysis tab explains decision support methodology.

### Bug Fixes
- **Fixed units standing still in all scenarios.** Two-phase target acquisition: strategic (global C2 awareness, march toward enemy) + tactical (within sensor range, standoff/engage).
- **Fixed scenario save/load cycle.** New scenarios had tick_duration_seconds=0.0 (loader rejects). Now: sensible defaults on init (3600s tick, 72h, Baltic bounds), safety-net defaults on save, monotonic actor IDs.
- **Fixed scenario path resolution.** exe_dir resolved via /proc/self/exe (Linux) / GetModuleFileName (Windows). Scenarios dir auto-created. Load tries multiple fallback paths. Save creates parent dirs.
- **Error messages now show actual loader failure reason** in status bar.

### Distribution Changes
- Example scenarios bundled in dist/{platform}/examples/
- Empty scenarios/ directory with README for user-saved scenarios

---

## v1.1.2 "Database" (2026-02-15)

### Features
- Platform database: 1,226 unique platforms across 168 JSON files
- GUI platform browser with category/country filtering
- Scenario editor with platform database picker
- 15 unit types, 6 sides, 25 terrain presets
- Monte Carlo analysis with background threading
- Pre-compiled distributions for Linux x86_64 and Windows x64

### Bug Fixes
- Fixed MC/GUI race condition (zero engagement bug)
- Fixed Windows CMD window visibility (-mwindows)
