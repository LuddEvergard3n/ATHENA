# ATHENA — Project Summary

**Document Type:** Executive Summary & Decision Record  
**Version:** 1.1.2  
**Status:** Consolidated

---

## 1. What is ATHENA

**ATHENA** = Advanced Tactical & Heuristic Engagement & Network Analyzer

A **Decision Support System (DSS)** for contrafactual analysis of conflicts as complex adaptive systems.

### Core Principle

> *ATHENA does not recommend actions, issue orders, or optimize outcomes. It exposes constraints, uncertainties, and trade-off surfaces to support human judgment.*

### What ATHENA Does

- Produces **distributions**, not decisions
- Exposes **consequences**, not prescriptions
- Reveals **fragilities** and **uncertainty**
- Validates **consistency**, not truth

### What ATHENA Does NOT Do

- Prescribe actions
- Optimize victories
- Recommend strategies
- Predict outcomes deterministically

---

## 2. Foundational Philosophy

### 2.1 Conflicts as Complex Adaptive Systems (CAS)

| Premise | Implication |
|---------|-------------|
| Non-linearity | Small causes → large effects |
| Non-ergodicity | History matters, no statistical shortcuts |
| Initial condition sensitivity | Butterfly effects |
| Continuous feedback | Systems self-modify |
| Emergent patterns | Unpredictable outcomes |

### 2.2 Rejection of Deterministic Narrative

ATHENA rejects:
- "If X then Y" causality
- Single timelines
- Deterministic replay with realistic aesthetics

ATHENA accepts:
- Bifurcations
- Rare trajectories
- Unexpected collapses
- Uncomfortable results

### 2.3 Counterfactual Imperative

ATHENA answers only questions of the type:

> *"Under V₀, which small perturbations ε generate systemic bifurcations?"*

Seeks:
- Failure surfaces
- Instability points
- Chaotic zones

Does NOT seek:
- Best path
- Optimal decision

---

## 3. Multi-Scale Causal Hierarchy

| Scale | Layer | Models | Engine |
|-------|-------|--------|--------|
| **Micro** | Individual/Kinetic | Human variability, mechanical failures, cognitive latency, physical dispersion | ECS, agent-based, stochastic noise |
| **Meso** | Tactical/Organizational | Cohesion, C2 networks, communication, collective morale, informational friction | Dynamic graphs, probabilistic weights |
| **Macro** | Strategic/Systemic | Aggregated logistics, industrial capacity, political attrition, temporal sustainability | System dynamics, differential equations |

### Vertical Coupling Rule

```
Micro affects Meso
Meso deforms Macro
Macro constrains Micro
Never the direct inverse.
```

---

## 4. Mathematical Core

### 4.1 Multi-Fidelity

| Fidelity | Use Case |
|----------|----------|
| **Low** | Massive exploration, surrogate models, parameter sweeps |
| **High** | Critical regions only, rare events, calibration |

### 4.2 Monte Carlo

- Classical Monte Carlo
- Quasi-Monte Carlo (Sobol)
- Antithetic variables
- Statistical stopping criterion (no fixed iterations)

### 4.3 Global Sensitivity Analysis (GSA)

| Method | Purpose |
|--------|---------|
| Morris | Massive screening, thousands of variables |
| Sobol indices | Individual effects, non-linear interactions |

### 4.4 Uncertainty Quantification (UQ)

| Type | Nature | Treatment |
|------|--------|-----------|
| **Aleatory** | Irreducible (physical, human) | Model explicitly |
| **Epistemic** | Reducible (data gaps, incomplete models) | Declare explicitly |

**Never mix the two.**

---

## 5. Software Architecture

### 5.1 Simulation Core

| Aspect | Decision |
|--------|----------|
| Language | C++ (baseline C++17, optional C++20/23) |
| Paradigm | Data-Oriented Design, ECS pure |
| Memory | SoA / AoSoA, SIMD, cache-aware |
| Determinism | IEEE-754 (same binary, platform, seed) |
| OOP | Banned in critical loop |

### 5.2 Terrain System (3-Layer Architecture)

ATHENA uses a **3-layer terrain architecture**:

| Layer | Name | Characteristics | Examples |
|-------|------|-----------------|----------|
| **1** | Physical | Immutable during simulation, procedurally generated | elevation(x,y), slope(x,y), is_water(x,y) |
| **2** | Environmental | Varies with time, modifies costs | weather, temperature, visibility, events |
| **3** | Semantic | DERIVED (never stored), computed on-demand | movement_cost(), defense_value(), line_of_sight() |

**Key Principles:**
- Physical ≠ Environmental (terrain is geometry; climate modifies costs)
- Semantic layer is ALWAYS derived, never stored
- Procedural generation via Perlin noise (zero external data)
- Declarative DSL (JSON), not natural language

**Terrain DSL Example:**
```json
{
  "terrain": {
    "physical": {
      "base": "hills",
      "roughness": 0.6,
      "seed": 12345,
      "overlays": [
        {"type": "forest", "density": 0.4}
      ]
    },
    "environment": {
      "climate": "temperate",
      "weather": "rain",
      "visibility_km": 4.0
    },
    "events": [
      {"type": "flood", "start_tick": 48, "duration_ticks": 24, "intensity": 0.7}
    ]
  }
}
```

### 5.3 Human Interface (UI)

| Aspect | Decision |
|--------|----------|
| Framework | ImGui + GLFW (vendorized, zero external deps) |
| Status | Fully integrated (replaced Qt6 in v0.8.1) |
| Philosophy | UI explains, does not simulate |
| Alternatives evaluated | Qt6 Commercial (removed: heavy, expensive, licensing), JavaFX (rejected: declining), Electron (rejected: attack surface) |

### 5.4 Data Architecture: Theater Packages

ATHENA uses a **Theater Package** model for efficient data management:

**Philosophy:** Online-first for acquisition, offline-capable for operation.

| Data Type | Storage Model | Update Frequency |
|-----------|---------------|------------------|
| Military (SIPRI, IISS) | Always local (~500 MB) | Monthly |
| Terrain (base 90m) | Per-theater package | Static |
| Terrain (high-res 30m) | On-demand, cached | Static |
| Climate | Per-theater, cached | Monthly |
| Infrastructure | Per-theater package | Quarterly |

**Theater Package Sizing:**

| Theater | Base Size | With High-Res |
|---------|-----------|---------------|
| Typical | 10-18 GB | +8-15 GB |
| Coverage | Full region | Full region |
| Resolution | 90m terrain | 30m terrain |

**Offline Capability:**
- Download theaters once
- Operate fully offline indefinitely
- Military data valid for months
- Terrain data static (never expires)

### 5.4 Data Pipeline

- Open sources, versioned, normalized
- Source conflicts: probabilistic weights, explicit divergence
- ATHENA never "chooses" truth
- Theater packages signed and validated
- LRU cache for high-res tiles

### 5.5 Data Schema Architecture

All data in ATHENA is versioned and self-describing:

| Schema | Purpose | Format |
|--------|---------|--------|
| Scenario | Simulation setup, actors, parameters | JSON + CBOR |
| Model | Combat/logistics/C2 model definitions | JSON |
| Result | Output distributions, sensitivity, time series | JSON + CBOR |
| Manifest | Execution record, integrity chain | JSON (signed) |
| Theater | Geospatial data packages | JSON + GeoTIFF + NetCDF |
| Military | Force structure, equipment inventories | JSON + SQLite |

**Key Principles:**
- Explicit versioning (SemVer)
- Forward and backward compatibility
- Deterministic serialization (for hashing)
- Signed manifests (Ed25519)
- Merkle-tree integrity verification

**Uncertainty as First-Class Citizen:**
```
Every numeric parameter can carry:
├── value: number
└── uncertainty:
    ├── type: "aleatory" | "epistemic"
    ├── distribution: Distribution
    ├── source: string
    └── confidence: number
```

---

## 6. Technical Foundations (Pre-Implementation)

### 6.1 Determinism Contract

> **ATHENA guarantees bitwise-identical outputs only under identical: binary, compiler, compiler flags, CPU microarchitecture class, operating system, and execution topology.**

**Determinism Scope:**

| Scope | Guarantee |
|-------|-----------|
| Strict | Same binary + hardware + seed → bitwise identical |
| Statistical | Same model, different platform → equivalent distributions |
| None | Different model/params → not comparable |

### 6.2 Engine vs Model Determinism

> **A deterministic engine may still yield divergent distributions if the model contains epistemic uncertainty. This is not a bug.**

| Layer | Determinism |
|-------|-------------|
| Engine (memory, RNG, scheduling) | Strict (bitwise) |
| Model (equations, distributions) | Defined by specification |

### 6.3 Technical Non-Goals

ATHENA explicitly does **NOT** provide:

- Real-time operation
- Live data ingestion during simulation
- Adaptive learning during execution
- Autonomous optimization
- Prescriptive recommendations
- Sub-second latency
- GPU-only operation
- Cloud-native architecture

### 6.4 Parallelism Model

> **ATHENA parallelizes exploration, not causality.**

| Parallelized | NOT Parallelized |
|--------------|------------------|
| Monte Carlo iterations | Causal chain within iteration |
| Sensitivity samples | Time steps within iteration |
| Agent updates (SIMD) | Event processing order |
| Output aggregation | Model state transitions |

### 6.5 Frozen Implementation Decisions

| Decision | Choice |
|----------|--------|
| RNG | PCG64 |
| Memory Layout | SoA primary |
| Floating Point | IEEE-754 strict, no fast-math |
| Execution Order | Tick-based, deterministic agent order |
| Reduction Order | Indexed (0, 1, 2, ..., N) |

---

## 7. Security & Integrity

### 7.1 Core Principles

- Self-hash on boot
- Lockdown on alteration
- Immutable logs (chained hash)
- Seed registered
- Full parameter logging
- Absolute reproducibility

### 7.2 Operational Environment

- Air-gapped by design
- No external NTP dependency
- Offline updates only (quarterly cadence)
- Removable media: scanning + whitelist + registry

---

## 8. Installation & Longevity

### 8.1 Installer

| Aspect | Specification |
|--------|---------------|
| Technology | WiX v4 + Burn |
| Format | Single signed bundle |
| Operation | Fully offline |
| Rollback | Transactional |

### 8.2 Update Philosophy

- Not automatic
- Not silent
- Not background
- Each update uses its own wizard
- Historical compatibility preserved
- Retroactive reproducibility guaranteed

### 8.3 Product Lifecycle

- Minimum 10-15 years support
- Semantic versioning
- Legacy models preserved

---

## 9. Commercial Positioning

### 9.1 Target Buyer

| Role | Position |
|------|----------|
| **Primary buyer** | Defense contractors, system integrators |
| **End user** | National armed forces, alliances |
| **Secondary** | Think tanks, intelligence agencies (prestige validation) |

### 9.2 Key Differentiators

| Differentiator | Value |
|----------------|-------|
| "Validates consistency, not truth" | Legally defensible |
| "Certifiable, not certified" | Flexible compliance path |
| 10-15 year lifecycle | Long-term commitment |
| Air-gapped by design | Security credibility |
| Full audit trail | Procurement-friendly |
| Non-prescriptive | Reduced liability |

### 9.3 Licensing Model

| Aspect | Decision |
|--------|----------|
| Type | Perpetual per installation |
| Maintenance | Annual (optional) |
| Default | Air-gapped version |
| Source code | Not delivered; escrow negotiable for governments |
| Export control | Wassenaar-compliant; EAR case-by-case |

---

## 10. Compliance Roadmap

### 9.1 Target Certifications

| Standard | Target | Status |
|----------|--------|--------|
| ISO/IEC 27001 | Baseline | Roadmap |
| Common Criteria | EAL4+ | Realistic target |
| NIST SP 800-53 | Control mapping | Documented |
| CMMC Level 2 | Conditional (DoD contracts) | Preliminary alignment |
| MIL-STD / DO-178C | Out of initial scope | Contract-specific |
| NATO STANAG | Only if client requires | Contract-specific |

### 9.2 Key Message

> *Certification is a contractual path, not an architectural limitation.*

---

## 11. Operational Use Cases (Summary)

### Case 1 — Meso (Operational): Logistical Sustainability

| Aspect | Content |
|--------|---------|
| **Context** | Continental temperate theater, division-scale, 45-60 days |
| **Question** | Under what interdiction/attrition/climate combinations do logistics corridors fail? |
| **Output** | Time-to-failure distribution, sensitivity map, bifurcation surface |
| **Value** | Quantifies systemic logistical risk |

### Case 2 — Micro (Tactical): Position Defense Variability

| Aspect | Content |
|--------|---------|
| **Context** | Company-scale urban defense, 6-18 hours |
| **Question** | Why can the same position resist 2h or 14h? |
| **Output** | Resistance time distribution, casualty distributions, sensitivity analysis |
| **Value** | Models human variability and tactical friction |

### Case 3 — Macro (Strategic): Prolonged Conflict Sustainability

| Aspect | Content |
|--------|---------|
| **Context** | Interstate conflict, medium-high intensity, 12-36 months |
| **Question** | Under what conditions does each belligerent reach capacity exhaustion? |
| **Output** | Exhaustion time distribution, collapse surfaces, bifurcation scenarios |
| **Value** | Sustainability analysis with explicit uncertainty |

### Unified Message

> *"ATHENA does not simulate to predict. ATHENA simulates to reveal — distributions, sensitivities, and the limits of what we can know."*

---

## 12. Key Decisions Record

| Decision | Status | Rationale |
|----------|--------|-----------|
| C++17 baseline | **Final** | Mature, widely supported |
| ImGui+GLFW for GUI | **Final** | Lightweight, zero deps, cross-platform (replaced Qt6) |
| EAL4+ target | **Final** | Realistic and commercially viable |
| "Validates consistency, not truth" | **Final** | Official project phrase |
| Synthetic scenarios only | **Final** | Auditable, reproducible, defensible |
| Escrow for governments | **Final** | Industry standard |
| 10-15 year lifecycle | **Final** | Competitive differentiator |
| CMMC in roadmap | **Final** | Enables US market |
| Air-gap as default | **Final** | Security credibility |
| SBOM mandatory | **Final** | Supply chain assurance |
| **3-layer terrain architecture** | **Final** | Physical/Environmental/Semantic separation |
| **Procedural terrain (Perlin)** | **Final** | Zero external data, deterministic |
| **Semantic layer derived** | **Final** | Never store opinions, always calculate |
| **Declarative terrain DSL** | **Final** | JSON, not natural language |

---

## 13. Remaining Backlog

### High Priority

| Item | Status |
|------|--------|
| Threat Model | ✅ Done |
| V&V Strategy | ✅ Done |
| Compliance Roadmap (full) | ✅ Done |
| Hardware Requirements | ✅ Done |
| Core API Contracts | ✅ Done |
| Data Schema (versioned) | ✅ Done |
| **Core Implementation (Phase 0-3)** | ✅ Done |
| **Terrain System (3 Layers)** | ✅ Done |
| **Terrain Integration (full stack)** | ✅ Done (v1.1.0) |
| Detection System | ✅ Done |
| CLI Runner | ✅ Done |
| AI/Doctrine System | ✅ Done (v0.9.3) |
| Tactical AI (shared MC) | ✅ Done (v1.0.0) |
| MC Convergence (auto-stop) | ✅ Done (v1.1.0) |
| MC Profiling (chrono) | ✅ Done (v1.1.1) |
| PDF Reports | ✅ Done (v1.1.1) |
| Pre-compiled Binaries | ✅ Done (v1.1.2) |

### Medium Priority

| Item | Status |
|------|--------|
| 100k MC Benchmark | Pending (v1.2) |
| A* Pathfinding (terrain cost) | Pending (v1.2) |
| Technical Glossary | Pending |
| Training Plan | Pending |
| Advanced Export (Protobuf, HDF5) | Pending |

### Lower Priority (Commercial)

| Item | Status |
|------|--------|
| Licensing Details | Pending |
| Team/Advisory Section | Pending |
| C# Bindings | Pending |

---

## 14. Key Phrases (Reusable)

| Context | Phrase |
|---------|--------|
| Positioning | "ATHENA is a Decision Support System (DSS). It does not recommend actions, issue orders, or optimize outcomes." |
| Epistemology | "ATHENA validates consistency, not truth." |
| Certification | "Certification is a contractual path, not an architectural limitation." |
| Sales (Meso) | "ATHENA doesn't say what to do. ATHENA shows what can break — and under what conditions." |
| Sales (Micro) | "ATHENA doesn't promise to predict battles. ATHENA shows why the same position can resist 2 hours or 14 — and what makes the difference." |
| Sales (Macro) | "ATHENA doesn't predict who wins. ATHENA shows under what conditions each side breaks first — and how confident you can be about it." |
| General | "ATHENA does not simulate to predict. ATHENA simulates to reveal." |

---

**Document End**
