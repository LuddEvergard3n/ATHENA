# ATHENA — Technical Foundations & Contracts

**Version:** 1.1.2  
**Status:** Specification (Pre-Implementation Freeze)  
**Classification:** Technical Reference

---

## 1. Determinism Contract

### 1.1 Guarantee Statement

> **ATHENA guarantees bitwise-identical outputs only under identical: binary, compiler, compiler flags, CPU microarchitecture class, operating system, and execution topology.**

Any deviation from these conditions **invalidates reproducibility claims**.

### 1.2 Determinism Scope

| Scope | Definition | Guarantee |
|-------|------------|-----------|
| **Strict Determinism** | Same binary, same hardware class, same seed | Bitwise identical outputs |
| **Statistical Determinism** | Same model, different platform | Statistically equivalent distributions |
| **No Guarantee** | Different model version, different parameters | Not comparable |

### 1.3 What is Controlled

| Component | Control Mechanism |
|-----------|-------------------|
| Random Number Generation | Explicit seed, fixed algorithm (PCG64) |
| Floating-point operations | IEEE-754 strict, no fast-math |
| Execution order | Deterministic scheduler, explicit topology |
| Memory layout | Fixed SoA patterns, no address-dependent behavior |
| Thread scheduling | Deterministic reduction order |
| I/O operations | Buffered, order-independent where possible |

### 1.4 What is NOT Controlled (Explicitly)

| Component | Reason | Impact |
|-----------|--------|--------|
| CPU frequency scaling | OS/hardware | None (same FP results) |
| Memory timing | Hardware | None (correctness unaffected) |
| Wall-clock time | Non-deterministic | Logged, not used in simulation |
| Cache behavior | Hardware | Performance only |

### 1.5 Execution Manifest Requirements

Every execution manifest MUST record:

```typescript
interface DeterminismRecord {
  // Binary identity
  binary_hash: Hash;
  compiler: string;           // "gcc-13.2.0"
  compiler_flags: string;     // "-O2 -march=x86-64-v3 -fno-fast-math"
  
  // Platform identity
  cpu_microarch: string;      // "zen4", "alderlake", "skylake"
  cpu_features: string[];     // ["avx2", "avx512f", "fma"]
  os: string;                 // "linux-6.5.0"
  
  // Execution topology
  threads: number;
  thread_affinity: string;    // "0-15" or "none"
  numa_policy: string;        // "local", "interleave", "none"
  
  // RNG state
  rng_algorithm: string;      // "pcg64"
  rng_seed: number;
  rng_stream: number;         // For parallel streams
}
```

### 1.6 Determinism Verification Procedure

```
1. Run simulation with seed S on platform P → Result R1
2. Run simulation with seed S on platform P → Result R2
3. Assert: hash(R1) == hash(R2)
4. If assertion fails → determinism violation → investigation required
```

This procedure is part of V&V (Class I scenarios).

---

## 2. Engine vs Model Determinism

### 2.1 Separation of Concerns

| Layer | Responsibility | Determinism Type |
|-------|----------------|------------------|
| **Engine** | Memory, RNG, scheduling, execution order | Strict (bitwise) |
| **Model** | Equations, parameters, distributions | Defined by model spec |

### 2.2 Critical Principle

> **A deterministic engine may still yield divergent distributions if the model contains epistemic uncertainty. This is not a bug.**

### 2.3 Examples

| Scenario | Engine Behavior | Model Behavior | Output |
|----------|-----------------|----------------|--------|
| Same seed, same model | Deterministic | Deterministic | Identical |
| Same seed, model with aleatory uncertainty | Deterministic | Samples from distribution | Identical (same samples) |
| Same seed, model with epistemic uncertainty | Deterministic | Varies by epistemic assumption | May differ (by design) |
| Different seed, same model | Different RNG stream | Same equations | Different samples, same distribution |

### 2.4 Implications for Validation

- **Engine validation:** Prove bitwise reproducibility (V&V Class I)
- **Model validation:** Prove statistical consistency (V&V Class II/III)
- **Combined validation:** Prove that uncertainty propagates correctly

---

## 3. Non-Goals (Technical)

### 3.1 Explicit Technical Non-Goals

ATHENA explicitly does **NOT** provide:

| Non-Goal | Rationale |
|----------|-----------|
| Real-time operation | Simulation is exploratory, not tactical |
| Live data ingestion during simulation | Data must be validated before use |
| Adaptive learning during execution | No model modification mid-run |
| Autonomous optimization | ATHENA explores, does not optimize |
| Prescriptive recommendations | DSS, not decision-maker |
| Deterministic prediction of real events | Validates consistency, not truth |
| Sub-second latency | Throughput over latency |
| GPU-only operation | GPU optional, CPU always sufficient |
| Cloud-native architecture | Offline-first by design |
| Multi-user concurrent simulation | Single simulation context at a time |

### 3.2 Boundary Clarifications

| What ATHENA Does | What ATHENA Does NOT Do |
|------------------|-------------------------|
| Batch simulation | Real-time simulation |
| Offline analysis | Online decision support |
| Distribution generation | Point prediction |
| Uncertainty quantification | Uncertainty elimination |
| Sensitivity analysis | Optimization |
| Historical parameter derivation | Live intelligence integration |
| Scenario comparison | Scenario recommendation |

---

## 4. Parallelism Model

### 4.1 Core Principle

> **ATHENA parallelizes exploration, not causality.**

### 4.2 What is Parallelized

| Component | Parallelism Type | Notes |
|-----------|------------------|-------|
| Monte Carlo iterations | Embarrassingly parallel | Independent runs |
| Sensitivity samples | Embarrassingly parallel | Independent parameter sets |
| Agent updates (within tick) | Data parallel (SIMD) | Same operation, different data |
| Output aggregation | Parallel reduction | Deterministic reduction order |

### 4.3 What is NOT Parallelized

| Component | Reason |
|-----------|--------|
| Causal chain within iteration | Must preserve causality |
| Time steps within iteration | Sequential dependency |
| Event processing order | Determinism requirement |
| Model state transitions | Causal integrity |

### 4.4 Parallelism Constraints

```
RULE: Parallel execution must not change results.
RULE: Reduction order must be deterministic.
RULE: No race conditions in state updates.
RULE: Thread count may affect performance, not correctness.
```

### 4.5 Execution Topology

```
┌─────────────────────────────────────────────────────────────┐
│                    BATCH CONTROLLER                          │
│                 (Sequential orchestration)                   │
└─────────────────────────┬───────────────────────────────────┘
                          │
        ┌─────────────────┼─────────────────┐
        │                 │                 │
        ▼                 ▼                 ▼
┌───────────────┐ ┌───────────────┐ ┌───────────────┐
│  MC Iteration │ │  MC Iteration │ │  MC Iteration │
│      0        │ │      1        │ │      N        │
│  (Thread 0)   │ │  (Thread 1)   │ │  (Thread N)   │
└───────┬───────┘ └───────┬───────┘ └───────┬───────┘
        │                 │                 │
        │    ┌────────────┼────────────┐    │
        │    │            │            │    │
        ▼    ▼            ▼            ▼    ▼
┌─────────────────────────────────────────────────────────────┐
│                 DETERMINISTIC REDUCTION                      │
│              (Ordered aggregation of results)                │
└─────────────────────────────────────────────────────────────┘
```

---

## 5. Core Implementation Contracts

### 5.1 Frozen Decisions (Pre-Implementation)

| Decision | Choice | Rationale | Frozen |
|----------|--------|-----------|--------|
| RNG Algorithm | PCG64 (pcg-random.org) | Fast, high quality, reproducible | ✓ |
| RNG Seeding | Single master seed → stream derivation | Deterministic parallel streams | ✓ |
| Memory Layout | SoA primary, AoSoA for hot paths | Cache efficiency | ✓ |
| Floating Point | IEEE-754 strict, no fast-math | Reproducibility | ✓ |
| Execution Order | Tick-based, deterministic agent order | Causality preservation | ✓ |
| Reduction Order | Indexed (0, 1, 2, ..., N) | Determinism | ✓ |

### 5.2 RNG Specification

```cpp
// PCG64 configuration
using RNG = pcg64;

// Stream derivation from master seed
RNG create_stream(uint64_t master_seed, uint64_t stream_id) {
    return RNG(master_seed, stream_id);
}

// Each MC iteration gets unique stream
// stream_id = iteration_index
// Guarantees: same master_seed + same iteration → same stream
```

### 5.3 Execution Order Guarantees

```
WITHIN EACH TICK:
1. Process events (deterministic order by event_id)
2. Update agents (deterministic order by agent_id)
3. Resolve interactions (deterministic order by interaction_id)
4. Advance state
5. Record outputs

ACROSS TICKS:
Sequential: tick 0 → tick 1 → ... → tick N
No parallelism across ticks within single iteration.
```

### 5.4 Memory Layout Rules

```cpp
// REQUIRED: Structure of Arrays (SoA) for entity data
struct Agents {
    std::vector<float> position_x;
    std::vector<float> position_y;
    std::vector<float> position_z;
    std::vector<float> health;
    std::vector<uint32_t> state;
    // NOT: std::vector<Agent> agents;  // AoS forbidden in hot path
};

// ALLOWED: AoSoA for SIMD-critical inner loops only
// Must document and justify each use
```

---

## 6. Core Simulation API

### 6.1 API Overview

Minimal API contract for core engine. Implementation details may change; interface semantics are frozen.

```cpp
namespace athena::core {

// === Context Management ===

// Create simulation context
// Returns: opaque context handle
// Thread-safety: not thread-safe, one context per thread
ContextHandle CreateContext(const ContextConfig& config);

// Destroy context and release resources
void DestroyContext(ContextHandle ctx);

// === Model Registration ===

// Register a model component
// Models are immutable after registration
ModelHandle RegisterModel(ContextHandle ctx, const ModelDefinition& model);

// Register uncertainty specification
// Links to registered model parameters
UncertaintyHandle RegisterUncertainty(
    ContextHandle ctx,
    ModelHandle model,
    const UncertaintySpec& spec
);

// === Scenario Loading ===

// Load scenario from path
// Validates against schema, returns error if invalid
Result<ScenarioHandle> LoadScenario(ContextHandle ctx, const std::string& path);

// Load theater data
// Lazy-loads terrain tiles as needed
Result<TheaterHandle> LoadTheater(ContextHandle ctx, const std::string& theater_id);

// === Execution ===

// Configure batch execution
void ConfigureBatch(ContextHandle ctx, const BatchConfig& config);

// Execute batch (blocking)
// Returns when all iterations complete or error
Result<BatchResult> ExecuteBatch(ContextHandle ctx);

// Execute batch (async)
// Returns immediately, use PollBatch for status
BatchJobHandle ExecuteBatchAsync(ContextHandle ctx);

// Poll async batch status
BatchStatus PollBatch(BatchJobHandle job);

// Cancel async batch
void CancelBatch(BatchJobHandle job);

// === Results ===

// Collect distribution results
Result<Distributions> CollectDistributions(ContextHandle ctx);

// Collect sensitivity results (if enabled)
Result<SensitivityResults> CollectSensitivity(ContextHandle ctx);

// Collect time series (if enabled)
Result<TimeSeries> CollectTimeSeries(ContextHandle ctx);

// === Manifest ===

// Export execution manifest
// Includes all determinism-relevant metadata
Result<Manifest> ExportManifest(ContextHandle ctx);

// Verify manifest against results
bool VerifyManifest(const Manifest& manifest, const std::string& result_path);

}  // namespace athena::core
```

### 6.2 Configuration Structures

```cpp
struct ContextConfig {
    uint32_t thread_count;        // 0 = auto-detect
    size_t memory_limit_gb;       // 0 = no limit
    std::string log_path;
    LogLevel log_level;
};

struct BatchConfig {
    uint64_t master_seed;
    uint32_t iterations;
    
    // Sensitivity analysis
    bool sensitivity_enabled;
    SensitivityMethod sensitivity_method;  // MORRIS, SOBOL, NONE
    uint32_t sensitivity_samples;
    
    // Output configuration
    OutputDetail output_detail;  // SUMMARY, STANDARD, FULL
    bool collect_time_series;
    uint32_t time_series_interval;  // Record every N ticks
    
    // Convergence
    bool convergence_check;
    double convergence_threshold;
    uint32_t convergence_window;
};

struct BatchResult {
    uint32_t iterations_completed;
    double duration_seconds;
    bool convergence_achieved;
    std::string manifest_hash;
};

struct BatchStatus {
    enum State { QUEUED, RUNNING, COMPLETE, CANCELLED, ERROR };
    State state;
    uint32_t current_iteration;
    uint32_t total_iterations;
    double progress_percent;
    std::optional<std::string> error_message;
};
```

### 6.3 Error Handling

```cpp
// Result type for operations that can fail
template<typename T>
struct Result {
    std::optional<T> value;
    std::optional<Error> error;
    
    bool ok() const { return value.has_value(); }
    const T& get() const { return *value; }
    const Error& err() const { return *error; }
};

struct Error {
    ErrorCode code;
    std::string message;
    std::string detail;
    std::optional<std::string> stack_trace;  // Debug builds only
};

enum class ErrorCode {
    // General
    OK = 0,
    INTERNAL_ERROR = 1,
    INVALID_ARGUMENT = 2,
    
    // Context
    CONTEXT_NOT_INITIALIZED = 100,
    CONTEXT_ALREADY_RUNNING = 101,
    
    // Model
    MODEL_NOT_FOUND = 200,
    MODEL_INVALID = 201,
    MODEL_VERSION_MISMATCH = 202,
    
    // Scenario
    SCENARIO_NOT_FOUND = 300,
    SCENARIO_INVALID = 301,
    SCENARIO_SCHEMA_MISMATCH = 302,
    
    // Theater
    THEATER_NOT_FOUND = 400,
    THEATER_INCOMPLETE = 401,
    
    // Execution
    EXECUTION_FAILED = 500,
    CONVERGENCE_FAILED = 501,
    RESOURCE_EXHAUSTED = 502,
    
    // Determinism
    DETERMINISM_VIOLATION = 600,
    MANIFEST_MISMATCH = 601,
};
```

---

## 7. Implementation Phases

### 7.1 Phase 0: Skeleton (Current Target)

Minimal viable structure, no real simulation:

| Component | Description | Status |
|-----------|-------------|--------|
| Context | Create/destroy, config | To implement |
| RNG | PCG64 with stream derivation | To implement |
| Scheduler | Tick loop, agent iteration order | To implement |
| Dummy Agent | Minimal state, no behavior | To implement |
| Manifest | Generate determinism record | To implement |

**Success Criteria:**
- Two runs with same seed produce identical manifest hash
- Different seeds produce different results
- Thread count doesn't affect results (only performance)

### 7.2 Phase 1: Data Loading

| Component | Description |
|-----------|-------------|
| Scenario loader | Parse JSON, validate schema |
| Theater loader | Load terrain index, lazy-load tiles |
| Military data | Load from SQLite |
| Parameter binding | Connect parameters to models |

### 7.3 Phase 2: Basic Simulation

| Component | Description |
|-----------|-------------|
| Agent system | ECS implementation |
| Movement | Basic mobility model |
| Attrition | Simple combat resolution |
| Logistics | Supply consumption |

### 7.4 Phase 3: Analysis

| Component | Description |
|-----------|-------------|
| Monte Carlo | Batch execution, distribution collection |
| Morris screening | Elementary effects |
| Sobol indices | Variance decomposition |
| Time series | State history recording |

### 7.5 Phase 4: Integration

| Component | Description |
|-----------|-------------|
| API server | MessagePack over socket |
| Result export | JSON, CBOR, CSV |
| Manifest signing | Ed25519 signatures |

---

## 8. Quality Gates

### 8.1 Pre-Commit

Every commit must pass:

- [ ] Compiles with `-Wall -Werror`
- [ ] All unit tests pass
- [ ] No memory leaks (ASan clean)
- [ ] No undefined behavior (UBSan clean)
- [ ] Determinism test passes

### 8.2 Pre-Release

Every release must pass:

- [ ] All Class I V&V scenarios pass
- [ ] All Class II V&V scenarios pass
- [ ] Performance benchmarks within spec
- [ ] Determinism verified across platforms
- [ ] Manifest signing works
- [ ] API compatibility verified

---

**Document End**
