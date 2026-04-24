# ATHENA — Decision Record

**Version:** 1.1.2  
**Status:** Final  
**Participants:** Project Lead

---

## Purpose

This document records all major decisions made during the ATHENA architecture review and refinement process. Each decision is marked as **Final** (no further debate) or **Open** (requires further input).

---

## Architecture Decisions

| ID | Decision | Status | Rationale |
|----|----------|--------|-----------|
| A-01 | C++17 as baseline | **Final** | Mature, widely supported, no client exclusion |
| A-02 | C++20/23 features optional and isolated | **Final** | Future-proofing without breaking compatibility |
| A-03 | ImGui+GLFW as GUI framework | **Final** | Lightweight, zero dependencies, cross-platform, replaced Qt |
| A-04 | UI is a replaceable layer | **Final** | Architectural flexibility |
| A-05 | JavaFX rejected as primary UI | **Final** | Declining community, uncertain future |
| A-06 | Electron rejected | **Final** | Large attack surface, poor security reputation |
| A-07 | ECS pure in simulation core | **Final** | Performance, cache efficiency |
| A-08 | OOP banned in critical loop | **Final** | Performance requirement |
| A-09 | IEEE-754 determinism (same binary/platform/seed) | **Final** | Reproducibility requirement |
| A-10 | Theater Package model for geospatial data | **Final** | Storage efficiency, practical deployability |
| A-11 | Online-first, offline-capable architecture | **Final** | Balance between data freshness and operational flexibility |
| A-12 | Resolution tiers by scenario scale | **Final** | Optimize storage vs. fidelity per use case |
| A-13 | PCG64 as RNG algorithm | **Final** | Fast, high quality, reproducible |
| A-14 | SoA memory layout (primary) | **Final** | Cache efficiency, SIMD friendly |
| A-15 | Tick-based deterministic execution order | **Final** | Causality preservation |
| A-16 | Indexed reduction order (0,1,2,...,N) | **Final** | Deterministic parallel reduction |
| A-17 | 3-layer terrain architecture | **Final** | Physical/Environmental/Semantic separation |
| A-18 | Procedural terrain generation (Perlin) | **Final** | Zero external data, deterministic |
| A-19 | Semantic layer always derived | **Final** | Never store opinions, only calculate |
| A-20 | Declarative terrain DSL (JSON) | **Final** | Auditable, reproducible, not natural language |

---

## Terrain System Decisions (NEW)

| ID | Decision | Status | Rationale |
|----|----------|--------|-----------|
| T-01 | Physical ≠ Environmental separation | **Final** | Terrain is geometry; climate modifies costs |
| T-02 | Semantic layer is DERIVED | **Final** | Never stored, always computed from L1+L2 |
| T-03 | Perlin noise for procedural generation | **Final** | Deterministic, no external dependencies |
| T-04 | Events as temporal functions | **Final** | flood(x,y,t), fire(x,y,t) with spreading |
| T-05 | No real geographic data | **Final** | Avoids political/legal issues, ensures reproducibility |
| T-06 | Heightmaps as functions, not files | **Final** | f64 elevation(x,y) via noise, not SRTM download |
| T-07 | Overlay system for terrain features | **Final** | FOREST, URBAN, SWAMP, etc. as probabilistic layers |
| T-08 | Movement cost tables from FM 5-33 | **Final** | Documented military source |
| T-09 | Weather modifies costs, not geometry | **Final** | Rain increases movement_cost, doesn't change elevation |

---

## Determinism Decisions

| ID | Decision | Status | Rationale |
|----|----------|--------|-----------|
| DET-01 | Bitwise determinism only under identical conditions | **Final** | Honest guarantee, legally defensible |
| DET-02 | Determinism scope declared in manifest | **Final** | Audit trail |
| DET-03 | Engine determinism ≠ Model determinism | **Final** | Epistemic uncertainty may cause valid divergence |
| DET-04 | No fast-math compiler flags | **Final** | IEEE-754 strict compliance |
| DET-05 | Thread count affects performance, not correctness | **Final** | Scalability without breaking reproducibility |
| DET-06 | Determinism verification in V&V Class I | **Final** | Automated testing |

---

## Data Architecture Decisions

| ID | Decision | Status | Rationale |
|----|----------|--------|-----------|
| D-01 | Military data always local (~500 MB) | **Final** | Small size, critical for all scenarios |
| D-02 | Terrain via Theater Packages (10-25 GB each) | **Final** | Only download what you need |
| D-03 | High-res 30m terrain on-demand with LRU cache | **Final** | Balances storage vs. availability |
| D-04 | Climate data per-theater with 14-day LRU | **Final** | Cacheable, refreshable |
| D-05 | Offline operation after initial download | **Final** | Critical for deployment flexibility |
| D-06 | Resolution scaling: 1km→90m→30m by scenario scale | **Final** | Macro doesn't need 30m resolution |
| D-07 | Signed theater packages | **Final** | Integrity verification |
| D-08 | Pre-staged packages for air-gapped deployment | **Final** | Supports classified environments |
| D-09 | MessagePack for runtime, JSON for human-readable | **Final** | Efficiency + debuggability |
| D-10 | CBOR for binary storage | **Final** | Compact, well-supported |
| D-11 | SQLite for indexed data (military, results) | **Final** | Query capability, integrity |
| D-12 | GeoTIFF for terrain, NetCDF for climate | **Final** | Industry standards, tool support |
| D-13 | SemVer for all schema versions | **Final** | Clear compatibility rules |
| D-14 | Ed25519 for manifest signatures | **Final** | Fast, secure, compact |
| D-15 | Uncertainty as first-class schema citizen | **Final** | Core to ATHENA philosophy |

---

## Security Decisions

| ID | Decision | Status | Rationale |
|----|----------|--------|-----------|
| S-01 | Air-gapped as default deployment | **Final** | Security credibility for defense market |
| S-02 | No external NTP dependency | **Final** | Air-gap compliance |
| S-03 | SBOM mandatory (SPDX or CycloneDX) | **Final** | Supply chain assurance |
| S-04 | Reproducible builds required | **Final** | Integrity verification |
| S-05 | Immutable logs with chained hashing | **Final** | Audit trail integrity |
| S-06 | Monotonic clock + cryptographic timestamp | **Final** | Non-repudiation without external dependencies |
| S-07 | STRIDE as threat modeling framework | **Final** | Industry-standard, auditor-friendly |

---

## Compliance Decisions

| ID | Decision | Status | Rationale |
|----|----------|--------|-----------|
| C-01 | "Certifiable, not certified" positioning | **Final** | Flexibility without overpromising |
| C-02 | EAL4+ as certification target | **Final** | Realistic, commercially viable |
| C-03 | CMMC Level 2 in roadmap | **Final** | Enables US DoD market |
| C-04 | NIST 800-53 control mapping | **Final** | Federal market requirement |
| C-05 | MIL-STD/DO-178C out of initial scope | **Final** | Contract-specific, not baseline |
| C-06 | NATO STANAG only if client requires | **Final** | Contract-specific |

---

## Commercial Decisions

| ID | Decision | Status | Rationale |
|----|----------|--------|-----------|
| M-01 | Perpetual license per installation | **Final** | Defense market preference |
| M-02 | Annual maintenance optional | **Final** | Flexibility |
| M-03 | Source code not delivered | **Final** | IP protection |
| M-04 | Escrow negotiable for governments | **Final** | Industry standard |
| M-05 | 10-15 year product lifecycle | **Final** | Competitive differentiator |
| M-06 | Defense contractors as primary buyer | **Final** | Budget, integration capability |
| M-07 | Armed forces as end user | **Final** | Validation, not direct purchase |

---

## V&V Decisions

| ID | Decision | Status | Rationale |
|----|----------|--------|-----------|
| V-01 | "Validates consistency, not truth" | **Final** | Official project phrase, legally defensible |
| V-02 | Synthetic scenarios only | **Final** | Auditable, reproducible, defensible |
| V-03 | No claims about "real conflicts" | **Final** | Legally and scientifically indefensible |
| V-04 | Quantitative acceptance criteria | **Final** | Auditor requirement |
| V-05 | V&V as release gate | **Final** | Quality assurance |
| V-06 | IVV (Independent V&V) support by design | **Final** | Procurement requirement |

---

## Epistemological Decisions

| ID | Decision | Status | Rationale |
|----|----------|--------|-----------|
| E-01 | ATHENA produces distributions, not decisions | **Final** | Core identity |
| E-02 | ATHENA is DSS, not decision-maker | **Final** | Liability management |
| E-03 | Aleatory vs epistemic uncertainty separation | **Final** | Mathematical rigor |
| E-04 | Never "choose" truth from conflicting sources | **Final** | Intellectual honesty |
| E-05 | Convergence failure is information, not error | **Final** | Analytical maturity |

---

## Documentation Decisions

| ID | Decision | Status | Rationale |
|----|----------|--------|-----------|
| D-01 | Philosophy restricted to prefatory material | **Final** | Body must be technical-operational |
| D-02 | "Prohibition" → "Design constraint" language | **Final** | Less dogmatic tone |
| D-03 | Three use cases (Micro/Meso/Macro) | **Final** | Demonstrates multi-scale capability |
| D-04 | Scenarios: realistic, unnamed, explicitly synthetic | **Final** | Credibility + neutrality |

---

## Key Phrases (Canonical)

| Context | Phrase | Status |
|---------|--------|--------|
| Positioning | "ATHENA is a Decision Support System (DSS). It does not recommend actions, issue orders, or optimize outcomes." | **Final** |
| Epistemology | "ATHENA validates consistency, not truth." | **Final** |
| Certification | "Certification is a contractual path, not an architectural limitation." | **Final** |
| Integration | "Integration with C4ISR systems is contractual and non-real-time." | **Final** |
| General | "ATHENA does not simulate to predict. ATHENA simulates to reveal." | **Final** |

---

## Open Items

| ID | Item | Blocker | Next Action |
|----|------|---------|-------------|
| O-01 | Specific C4ISR integration standards | Client requirements | Define per contract |
| O-02 | Exact CMMC timeline | Organizational readiness | Define in Compliance Roadmap |
| O-03 | Advisory board composition | Business development | TBD |

---

**Document End**
