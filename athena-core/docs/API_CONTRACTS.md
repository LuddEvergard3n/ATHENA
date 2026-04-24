# ATHENA — API Contracts

**Version:** 1.1.2  
**Status:** Specification  
**Classification:** Technical Reference

---

## 1. Overview

### 1.1 Architecture Context

```
┌─────────────────────────────────────────────────────────────────┐
│                           UI LAYER                               │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐            │
│  │ Scenario│  │ Results │  │  Audit  │  │  Admin  │            │
│  │  Editor │  │ Viewer  │  │  Viewer │  │  Panel  │            │
│  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘            │
│       │            │            │            │                   │
│       └────────────┴────────────┴────────────┘                   │
│                           │                                      │
│                    ┌──────▼──────┐                               │
│                    │  API Client │                               │
│                    └──────┬──────┘                               │
└───────────────────────────┼──────────────────────────────────────┘
                            │ IPC (Local Socket / Shared Memory)
┌───────────────────────────┼──────────────────────────────────────┐
│                    ┌──────▼──────┐                               │
│                    │  API Server │                               │
│                    └──────┬──────┘                               │
│                           │                                      │
│  ┌────────────────────────┼────────────────────────────┐        │
│  │                 CORE ENGINE                          │        │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌────────┐ │        │
│  │  │  Data   │  │Simulation│  │Analysis │  │ Audit  │ │        │
│  │  │ Ingest  │  │  Engine  │  │ Engine  │  │ Logger │ │        │
│  │  └─────────┘  └─────────┘  └─────────┘  └────────┘ │        │
│  └──────────────────────────────────────────────────────┘        │
│                         CORE LAYER                               │
└──────────────────────────────────────────────────────────────────┘
```

### 1.2 Design Principles

| Principle | Rationale |
|-----------|-----------|
| Strict separation | UI never touches simulation memory directly |
| Async by default | Long-running simulations don't block UI |
| Versioned | API versioning for backward compatibility |
| Auditable | All API calls logged |
| Language-agnostic | Binary protocol, any UI can integrate |

### 1.3 Communication Mechanisms

| Mechanism | Use Case | Latency |
|-----------|----------|---------|
| Unix Domain Socket | Default (Linux) | ~10 μs |
| Named Pipe | Default (Windows) | ~50 μs |
| TCP Localhost | Cross-container | ~100 μs |
| Shared Memory | High-frequency data (results streaming) | ~1 μs |

---

## 2. API Versioning

### 2.1 Version Format

```
MAJOR.MINOR.PATCH

MAJOR: Breaking changes
MINOR: New features, backward compatible
PATCH: Bug fixes only
```

**Current Version:** `1.0.0`

### 2.2 Compatibility Rules

| Client Version | Server Version | Compatible |
|----------------|----------------|------------|
| 1.0.x | 1.0.x | ✓ |
| 1.0.x | 1.1.x | ✓ |
| 1.0.x | 2.0.x | ✗ (must upgrade) |
| 1.1.x | 1.0.x | ✗ (server too old) |

### 2.3 Version Negotiation

```
Client → Server: VERSION_REQUEST { client_version: "1.0.0" }
Server → Client: VERSION_RESPONSE { 
  server_version: "1.1.0",
  compatible: true,
  min_supported: "1.0.0"
}
```

---

## 3. Message Protocol

### 3.1 Wire Format

All messages use **MessagePack** encoding for efficiency and cross-language support.

```
┌────────────┬────────────┬────────────┬─────────────────┐
│  Length    │  Msg Type  │  Request   │    Payload      │
│  (4 bytes) │  (2 bytes) │  ID (8 B)  │  (MessagePack)  │
└────────────┴────────────┴────────────┴─────────────────┘
```

| Field | Size | Description |
|-------|------|-------------|
| Length | 4 bytes | Total message length (big-endian) |
| Msg Type | 2 bytes | Message type ID |
| Request ID | 8 bytes | Unique request identifier |
| Payload | Variable | MessagePack-encoded data |

### 3.2 Message Types

| ID | Name | Direction | Description |
|----|------|-----------|-------------|
| 0x0001 | VERSION_REQUEST | C→S | Version negotiation |
| 0x0002 | VERSION_RESPONSE | S→C | Version response |
| 0x0010 | AUTH_REQUEST | C→S | Authentication |
| 0x0011 | AUTH_RESPONSE | S→C | Auth result + session |
| 0x0100 | SCENARIO_LIST | C→S | List scenarios |
| 0x0101 | SCENARIO_LIST_RESP | S→C | Scenario list |
| 0x0102 | SCENARIO_LOAD | C→S | Load scenario |
| 0x0103 | SCENARIO_LOAD_RESP | S→C | Scenario data |
| 0x0104 | SCENARIO_SAVE | C→S | Save scenario |
| 0x0105 | SCENARIO_SAVE_RESP | S→C | Save confirmation |
| 0x0200 | SIM_START | C→S | Start simulation |
| 0x0201 | SIM_STATUS | S→C | Status update |
| 0x0202 | SIM_PROGRESS | S→C | Progress update |
| 0x0203 | SIM_CANCEL | C→S | Cancel simulation |
| 0x0204 | SIM_COMPLETE | S→C | Simulation complete |
| 0x0205 | SIM_ERROR | S→C | Simulation error |
| 0x0300 | RESULT_LIST | C→S | List results |
| 0x0301 | RESULT_LIST_RESP | S→C | Result list |
| 0x0302 | RESULT_FETCH | C→S | Fetch result data |
| 0x0303 | RESULT_DATA | S→C | Result data (streamed) |
| 0x0400 | THEATER_LIST | C→S | List theater packages |
| 0x0401 | THEATER_LIST_RESP | S→C | Theater list |
| 0x0402 | THEATER_DOWNLOAD | C→S | Download theater |
| 0x0403 | THEATER_DOWNLOAD_STATUS | S→C | Download progress |
| 0x0404 | THEATER_DELETE | C→S | Delete theater |
| 0x0405 | THEATER_DELETE_RESP | S→C | Delete confirmation |
| 0x0410 | MILITARY_DATA_STATUS | C→S | Military data status |
| 0x0411 | MILITARY_DATA_STATUS_RESP | S→C | Status response |
| 0x0412 | MILITARY_DATA_SYNC | C→S | Sync military data |
| 0x0413 | MILITARY_DATA_SYNC_STATUS | S→C | Sync progress |
| 0x0420 | HIGHRES_TILE_STATUS | C→S | High-res tile status |
| 0x0421 | HIGHRES_TILE_STATUS_RESP | S→C | Tile status response |
| 0x0422 | HIGHRES_TILE_FETCH | C→S | Fetch high-res tiles |
| 0x0423 | HIGHRES_TILE_FETCH_STATUS | S→C | Fetch progress |
| 0x0424 | HIGHRES_TILE_PURGE | C→S | Purge tile cache |
| 0x0425 | HIGHRES_TILE_PURGE_RESP | S→C | Purge confirmation |
| 0x0430 | STORAGE_STATUS | C→S | Storage status |
| 0x0431 | STORAGE_STATUS_RESP | S→C | Storage info |
| 0x0432 | STORAGE_CLEANUP | C→S | Cleanup storage |
| 0x0433 | STORAGE_CLEANUP_RESP | S→C | Cleanup result |
| 0x0500 | AUDIT_QUERY | C→S | Query audit logs |
| 0x0501 | AUDIT_RESPONSE | S→C | Audit log entries |
| 0x0600 | SYSTEM_STATUS | C→S | System health check |
| 0x0601 | SYSTEM_STATUS_RESP | S→C | System status |
| 0xFFFF | ERROR | S→C | Generic error |

---

## 4. Authentication & Sessions

### 4.1 Authentication Flow

```
Client                                    Server
   │                                         │
   │──── AUTH_REQUEST ──────────────────────▶│
   │     { username, credential_hash }       │
   │                                         │
   │◀─── AUTH_RESPONSE ─────────────────────│
   │     { success, session_token,           │
   │       permissions, expiry }             │
   │                                         │
   │──── [Subsequent requests] ─────────────▶│
   │     { session_token in header }         │
   │                                         │
```

### 4.2 AUTH_REQUEST

```typescript
interface AuthRequest {
  username: string;           // User identifier
  credential_hash: string;    // SHA-256 of credential
  client_info: {
    version: string;
    platform: string;
    hostname: string;
  };
}
```

### 4.3 AUTH_RESPONSE

```typescript
interface AuthResponse {
  success: boolean;
  session_token?: string;     // 256-bit token
  permissions: Permission[];  // User permissions
  expiry: number;            // Unix timestamp
  error?: ErrorInfo;
}

enum Permission {
  SCENARIO_READ = 1,
  SCENARIO_WRITE = 2,
  SCENARIO_DELETE = 4,
  SIM_EXECUTE = 8,
  SIM_CANCEL = 16,
  RESULT_READ = 32,
  RESULT_DELETE = 64,
  DATA_INGEST = 128,
  AUDIT_READ = 256,
  ADMIN = 512
}
```

### 4.4 Session Management

| Aspect | Specification |
|--------|---------------|
| Token format | 256-bit cryptographically random |
| Storage | Server-side only (no JWT) |
| Expiry | Configurable (default: 8 hours) |
| Renewal | Automatic on activity |
| Concurrent sessions | Configurable per user |

---

## 5. Scenario Management API

### 5.1 SCENARIO_LIST

**Request:**
```typescript
interface ScenarioListRequest {
  filter?: {
    name_contains?: string;
    created_after?: number;    // Unix timestamp
    created_by?: string;
    tags?: string[];
  };
  pagination: {
    offset: number;
    limit: number;             // Max 100
  };
  sort: {
    field: "name" | "created" | "modified";
    order: "asc" | "desc";
  };
}
```

**Response:**
```typescript
interface ScenarioListResponse {
  scenarios: ScenarioSummary[];
  total_count: number;
  has_more: boolean;
}

interface ScenarioSummary {
  id: string;                  // UUID
  name: string;
  description: string;
  version: number;
  created_by: string;
  created_at: number;
  modified_at: number;
  tags: string[];
  scale: "micro" | "meso" | "macro";
  agent_count: number;
  status: "draft" | "validated" | "archived";
}
```

### 5.2 SCENARIO_LOAD

**Request:**
```typescript
interface ScenarioLoadRequest {
  scenario_id: string;
  version?: number;            // Latest if omitted
  include_data?: boolean;      // Include full data or metadata only
}
```

**Response:**
```typescript
interface ScenarioLoadResponse {
  scenario: Scenario;
  data_hash: string;           // SHA-256 for integrity
}

interface Scenario {
  id: string;
  name: string;
  description: string;
  version: number;
  schema_version: string;      // Data schema version
  
  // Temporal configuration
  time_config: {
    start: number;             // Simulation start (abstract units)
    end: number;               // Simulation end
    step: number;              // Time step
  };
  
  // Actors
  actors: Actor[];
  
  // Environment
  environment: Environment;
  
  // Parameters
  parameters: ParameterSet;
  
  // Uncertainty declarations
  uncertainties: UncertaintyDeclaration[];
  
  // Metadata
  metadata: {
    created_by: string;
    created_at: number;
    modified_at: number;
    tags: string[];
    notes: string;
  };
}
```

### 5.3 SCENARIO_SAVE

**Request:**
```typescript
interface ScenarioSaveRequest {
  scenario: Scenario;
  create_new_version: boolean; // true = new version, false = overwrite
  validation_level: "none" | "syntax" | "full";
}
```

**Response:**
```typescript
interface ScenarioSaveResponse {
  success: boolean;
  scenario_id: string;
  version: number;
  validation_result?: ValidationResult;
  error?: ErrorInfo;
}

interface ValidationResult {
  valid: boolean;
  errors: ValidationError[];
  warnings: ValidationWarning[];
}
```

---

## 6. Simulation Execution API

### 6.1 SIM_START

**Request:**
```typescript
interface SimStartRequest {
  scenario_id: string;
  scenario_version?: number;
  
  // Execution configuration
  config: {
    monte_carlo: {
      iterations: number;       // Number of MC runs
      seed?: number;            // Optional fixed seed
      convergence_threshold?: number;
    };
    
    sensitivity: {
      enabled: boolean;
      method: "morris" | "sobol" | "both";
      samples?: number;
    };
    
    output: {
      detail_level: "summary" | "standard" | "full";
      checkpoints: boolean;
      checkpoint_interval?: number;
    };
    
    resources: {
      max_threads?: number;
      max_memory_gb?: number;
      use_gpu: boolean;
      priority: "low" | "normal" | "high";
    };
  };
  
  // Parameter overrides (optional)
  parameter_overrides?: ParameterOverride[];
}
```

**Response (immediate):**
```typescript
interface SimStartResponse {
  success: boolean;
  job_id?: string;              // UUID for tracking
  estimated_duration?: number;  // Seconds
  error?: ErrorInfo;
}
```

### 6.2 SIM_STATUS (Server Push)

```typescript
interface SimStatusMessage {
  job_id: string;
  status: "queued" | "initializing" | "running" | "finalizing" | "complete" | "cancelled" | "error";
  progress: {
    current_iteration: number;
    total_iterations: number;
    percent_complete: number;
    elapsed_seconds: number;
    estimated_remaining: number;
  };
  resources: {
    cpu_percent: number;
    memory_gb: number;
    gpu_percent?: number;
  };
}
```

### 6.3 SIM_PROGRESS (Server Push, Optional)

```typescript
interface SimProgressMessage {
  job_id: string;
  iteration: number;
  
  // Running statistics (updated periodically)
  running_stats?: {
    mean: number[];
    variance: number[];
    convergence_metric: number;
  };
}
```

### 6.4 SIM_CANCEL

**Request:**
```typescript
interface SimCancelRequest {
  job_id: string;
  save_partial_results: boolean;
}
```

**Response:**
```typescript
interface SimCancelResponse {
  success: boolean;
  final_status: string;
  partial_result_id?: string;   // If partial results saved
}
```

### 6.5 SIM_COMPLETE (Server Push)

```typescript
interface SimCompleteMessage {
  job_id: string;
  result_id: string;            // Reference to stored result
  
  summary: {
    iterations_completed: number;
    total_duration_seconds: number;
    convergence_achieved: boolean;
  };
  
  manifest: ExecutionManifest;
}

interface ExecutionManifest {
  job_id: string;
  scenario_id: string;
  scenario_version: number;
  core_version: string;
  model_version: string;
  seed: number;
  parameters_hash: string;
  input_hash: string;
  output_hash: string;
  timestamp: string;            // ISO 8601
  signature: string;            // Cryptographic signature
}
```

### 6.6 SIM_ERROR (Server Push)

```typescript
interface SimErrorMessage {
  job_id: string;
  error: ErrorInfo;
  recoverable: boolean;
  partial_result_id?: string;
}

interface ErrorInfo {
  code: string;                 // Machine-readable code
  message: string;              // Human-readable message
  details?: Record<string, any>;
  stack_trace?: string;         // Debug builds only
}
```

---

## 7. Results API

### 7.1 RESULT_LIST

**Request:**
```typescript
interface ResultListRequest {
  filter?: {
    scenario_id?: string;
    job_id?: string;
    created_after?: number;
    created_by?: string;
  };
  pagination: {
    offset: number;
    limit: number;
  };
}
```

**Response:**
```typescript
interface ResultListResponse {
  results: ResultSummary[];
  total_count: number;
}

interface ResultSummary {
  result_id: string;
  job_id: string;
  scenario_id: string;
  scenario_name: string;
  created_at: number;
  created_by: string;
  iterations: number;
  status: "complete" | "partial";
  manifest_hash: string;
}
```

### 7.2 RESULT_FETCH

**Request:**
```typescript
interface ResultFetchRequest {
  result_id: string;
  
  // What to include
  include: {
    distributions: boolean;
    sensitivity: boolean;
    raw_iterations?: number;    // Number of raw iterations (0 = none)
    time_series: boolean;
  };
  
  // Streaming options
  streaming: {
    enabled: boolean;
    chunk_size?: number;        // Bytes per chunk
  };
}
```

**Response (if not streaming):**
```typescript
interface ResultFetchResponse {
  result_id: string;
  manifest: ExecutionManifest;
  
  distributions?: {
    outputs: OutputDistribution[];
  };
  
  sensitivity?: {
    sobol_indices?: SobolResult[];
    morris_indices?: MorrisResult[];
  };
  
  raw_iterations?: RawIteration[];
  
  time_series?: TimeSeriesData[];
}

interface OutputDistribution {
  name: string;
  unit: string;
  percentiles: {
    p5: number;
    p10: number;
    p25: number;
    p50: number;
    p75: number;
    p90: number;
    p95: number;
  };
  mean: number;
  std_dev: number;
  histogram: {
    bins: number[];
    counts: number[];
  };
}

interface SobolResult {
  parameter: string;
  S1: number;                   // First-order index
  ST: number;                   // Total-order index
  S1_conf: [number, number];    // Confidence interval
  ST_conf: [number, number];
}
```

---

## 8. Data Management API

### 8.1 Theater Package Management

#### THEATER_LIST

**Request:**
```typescript
interface TheaterListRequest {
  include_installed: boolean;
  include_available: boolean;
}
```

**Response:**
```typescript
interface TheaterListResponse {
  installed: TheaterPackage[];
  available: TheaterPackage[];
}

interface TheaterPackage {
  id: string;                   // e.g., "europe-central"
  name: string;                 // e.g., "Central Europe"
  description: string;
  version: string;
  
  // Size information
  base_size_gb: number;         // Base package (90m terrain)
  highres_size_gb: number;      // Optional 30m terrain
  total_size_gb: number;
  
  // Coverage
  bounds: {
    north: number;
    south: number;
    east: number;
    west: number;
  };
  countries: string[];
  
  // Status (for installed)
  status?: "complete" | "downloading" | "incomplete" | "update_available";
  installed_version?: string;
  last_updated?: number;
  disk_usage_gb?: number;
  
  // Content details
  contents: {
    terrain_resolution: "1km" | "90m" | "30m";
    climate_years: number;
    infrastructure_date: string;
  };
}
```

#### THEATER_DOWNLOAD

**Request:**
```typescript
interface TheaterDownloadRequest {
  theater_id: string;
  include_highres: boolean;     // Include 30m terrain
  priority: "background" | "normal" | "high";
}
```

**Response:**
```typescript
interface TheaterDownloadResponse {
  success: boolean;
  download_job_id: string;
  estimated_size_gb: number;
  estimated_time_minutes: number;
}
```

#### THEATER_DOWNLOAD_STATUS (Server Push)

```typescript
interface TheaterDownloadStatus {
  download_job_id: string;
  theater_id: string;
  status: "queued" | "downloading" | "extracting" | "validating" | "complete" | "error";
  progress: {
    bytes_downloaded: number;
    bytes_total: number;
    percent_complete: number;
    speed_mbps: number;
    eta_seconds: number;
  };
  current_component?: string;   // "terrain", "climate", "infrastructure"
  error?: string;
}
```

#### THEATER_DELETE

**Request:**
```typescript
interface TheaterDeleteRequest {
  theater_id: string;
  keep_scenarios: boolean;      // Keep scenarios that use this theater
}
```

**Response:**
```typescript
interface TheaterDeleteResponse {
  success: boolean;
  space_freed_gb: number;
  affected_scenarios: string[]; // Scenario IDs that used this theater
}
```

### 8.2 Military Data Sync

#### MILITARY_DATA_STATUS

**Request:**
```typescript
interface MilitaryDataStatusRequest {}
```

**Response:**
```typescript
interface MilitaryDataStatusResponse {
  sources: MilitaryDataSource[];
  last_full_sync: number;
  next_scheduled_sync: number;
}

interface MilitaryDataSource {
  id: string;                   // e.g., "sipri", "iiss", "gfp"
  name: string;
  status: "current" | "stale" | "error" | "updating";
  last_updated: number;
  record_count: number;
  size_mb: number;
  staleness_days: number;
  update_available: boolean;
}
```

#### MILITARY_DATA_SYNC

**Request:**
```typescript
interface MilitaryDataSyncRequest {
  sources?: string[];           // Specific sources, or all if omitted
  force_refresh: boolean;       // Ignore cache/staleness
}
```

**Response:**
```typescript
interface MilitaryDataSyncResponse {
  success: boolean;
  sync_job_id: string;
}
```

#### MILITARY_DATA_SYNC_STATUS (Server Push)

```typescript
interface MilitaryDataSyncStatus {
  sync_job_id: string;
  status: "running" | "complete" | "error";
  sources: {
    source_id: string;
    status: "pending" | "downloading" | "parsing" | "validating" | "complete" | "error";
    progress_percent: number;
    records_processed?: number;
    error?: string;
  }[];
}
```

### 8.3 High-Resolution Tile Management

#### HIGHRES_TILE_STATUS

**Request:**
```typescript
interface HighresTileStatusRequest {
  theater_id: string;
  bounds?: {                    // Optional area of interest
    north: number;
    south: number;
    east: number;
    west: number;
  };
}
```

**Response:**
```typescript
interface HighresTileStatusResponse {
  theater_id: string;
  tiles: TileInfo[];
  cache_size_gb: number;
  cache_limit_gb: number;
}

interface TileInfo {
  tile_id: string;
  bounds: { north: number; south: number; east: number; west: number };
  resolution: "30m";
  size_mb: number;
  status: "available" | "cached" | "downloading";
  last_accessed?: number;
  expires?: number;             // LRU expiry
}
```

#### HIGHRES_TILE_FETCH

**Request:**
```typescript
interface HighresTileFetchRequest {
  theater_id: string;
  tile_ids: string[];           // Specific tiles
  // OR
  bounds?: {                    // Area (system determines tiles)
    north: number;
    south: number;
    east: number;
    west: number;
  };
  priority: "background" | "immediate";
}
```

**Response:**
```typescript
interface HighresTileFetchResponse {
  success: boolean;
  tiles_queued: number;
  estimated_size_mb: number;
  estimated_time_seconds: number;
}
```

#### HIGHRES_TILE_PURGE

**Request:**
```typescript
interface HighresTilePurgeRequest {
  theater_id?: string;          // Specific theater, or all
  older_than_days?: number;     // Purge tiles not accessed in N days
  target_size_gb?: number;      // Purge until cache is below target
}
```

**Response:**
```typescript
interface HighresTilePurgeResponse {
  success: boolean;
  tiles_purged: number;
  space_freed_gb: number;
}
```

### 8.4 Storage Management

#### STORAGE_STATUS

**Request:**
```typescript
interface StorageStatusRequest {}
```

**Response:**
```typescript
interface StorageStatusResponse {
  total_gb: number;
  used_gb: number;
  available_gb: number;
  
  breakdown: {
    installation: number;
    military_data: number;
    theater_packages: number;
    highres_cache: number;
    climate_cache: number;
    scenarios: number;
    results: number;
    audit_logs: number;
    other: number;
  };
  
  theaters: {
    theater_id: string;
    name: string;
    size_gb: number;
    highres_cached_gb: number;
  }[];
  
  warnings: {
    low_space: boolean;
    threshold_gb: number;
  };
}
```

#### STORAGE_CLEANUP

**Request:**
```typescript
interface StorageCleanupRequest {
  actions: {
    purge_highres_cache: boolean;
    purge_climate_cache: boolean;
    purge_old_results: boolean;
    results_older_than_days?: number;
  };
}
```

**Response:**
```typescript
interface StorageCleanupResponse {
  success: boolean;
  space_freed_gb: number;
  items_removed: {
    highres_tiles: number;
    climate_files: number;
    result_files: number;
  };
}
```

---

## 9. Audit API

### 9.1 AUDIT_QUERY

**Request:**
```typescript
interface AuditQueryRequest {
  filter: {
    start_time: number;         // Unix timestamp
    end_time: number;
    user?: string;
    action_types?: string[];
    resource_id?: string;
  };
  pagination: {
    offset: number;
    limit: number;              // Max 1000
  };
}
```

**Response:**
```typescript
interface AuditQueryResponse {
  entries: AuditEntry[];
  total_count: number;
  integrity_verified: boolean;  // Hash chain valid
}

interface AuditEntry {
  id: string;
  timestamp: string;            // ISO 8601
  user: string;
  action: string;
  resource_type: string;
  resource_id: string;
  details: Record<string, any>;
  client_info: {
    ip: string;
    platform: string;
  };
  hash: string;                 // Entry hash
  prev_hash: string;            // Chain link
}
```

---

## 10. System Status API

### 10.1 SYSTEM_STATUS

**Request:**
```typescript
interface SystemStatusRequest {
  include: {
    health: boolean;
    resources: boolean;
    jobs: boolean;
    data_sources: boolean;
  };
}
```

**Response:**
```typescript
interface SystemStatusResponse {
  health: {
    status: "healthy" | "degraded" | "unhealthy";
    components: {
      core_engine: "ok" | "error";
      data_pipeline: "ok" | "error";
      audit_system: "ok" | "error";
      storage: "ok" | "warning" | "error";
    };
    last_check: number;
  };
  
  resources?: {
    cpu_percent: number;
    memory_used_gb: number;
    memory_total_gb: number;
    storage_used_gb: number;
    storage_total_gb: number;
    gpu_percent?: number;
    gpu_memory_gb?: number;
  };
  
  jobs?: {
    queued: number;
    running: number;
    completed_today: number;
    failed_today: number;
  };
  
  data_sources?: {
    total: number;
    up_to_date: number;
    stale: number;
    error: number;
  };
  
  version: {
    core: string;
    api: string;
    schema: string;
  };
}
```

---

## 11. Error Handling

### 11.1 Error Codes

| Code | Category | Description |
|------|----------|-------------|
| E1000 | Auth | Authentication failed |
| E1001 | Auth | Session expired |
| E1002 | Auth | Insufficient permissions |
| E2000 | Scenario | Scenario not found |
| E2001 | Scenario | Invalid scenario format |
| E2002 | Scenario | Validation failed |
| E2003 | Scenario | Version conflict |
| E3000 | Simulation | Job not found |
| E3001 | Simulation | Resource limit exceeded |
| E3002 | Simulation | Numerical error |
| E3003 | Simulation | Convergence failed |
| E4000 | Result | Result not found |
| E4001 | Result | Result corrupted |
| E5000 | Data | Source unavailable |
| E5001 | Data | Validation failed |
| E5002 | Data | Network error |
| E9000 | System | Internal error |
| E9001 | System | Service unavailable |

### 11.2 Error Response Format

```typescript
interface ErrorResponse {
  error: {
    code: string;
    message: string;
    details?: Record<string, any>;
    request_id: string;
    timestamp: string;
  };
}
```

---

## 12. Rate Limiting & Quotas

### 12.1 Rate Limits

| Endpoint Category | Limit | Window |
|-------------------|-------|--------|
| Authentication | 10 requests | 1 minute |
| Scenario read | 100 requests | 1 minute |
| Scenario write | 20 requests | 1 minute |
| Simulation start | 10 requests | 1 minute |
| Result fetch | 50 requests | 1 minute |
| Audit query | 20 requests | 1 minute |

### 12.2 Quota Headers

```
X-RateLimit-Limit: 100
X-RateLimit-Remaining: 95
X-RateLimit-Reset: 1704067200
```

### 12.3 Resource Quotas (Per User)

| Resource | Default Quota |
|----------|---------------|
| Concurrent simulations | 2 |
| Max MC iterations | 1,000,000 |
| Max scenario size | 100 MB |
| Result retention | 90 days |
| Storage | 100 GB |

---

## 13. SDK Support

### 13.1 Official SDKs

| Language | Package | Status |
|----------|---------|--------|
| C++ | `libathena` | Planned |
| C# | `Athena.Client` | Planned |

### 13.2 SDK Example (C++)

```cpp
#include <athena/client.hpp>

int main() {
    using namespace athena;
    
    // Connect
    Client client("localhost:9000");
    client.authenticate("analyst", "credential");
    
    // Load scenario
    auto scenario = client.scenarios().load("scenario-uuid");
    if (!scenario.ok()) {
        std::cerr << scenario.error().message << "\n";
        return 1;
    }
    
    // Configure simulation
    SimConfig config;
    config.monte_carlo_iterations = 100000;
    config.sensitivity_method = SensitivityMethod::SOBOL;
    config.use_gpu = true;
    
    // Run simulation
    auto job = client.simulations().start(scenario.get(), config);
    
    // Monitor progress
    job.on_progress([](const SimStatus& status) {
        std::cout << "Progress: " << status.percent_complete << "%\n";
    });
    
    // Wait and get results
    auto result = job.wait();
    if (!result.ok()) {
        std::cerr << result.error().message << "\n";
        return 1;
    }
    
    // Access distributions
    const auto& dist = result.get().distributions.at("time_to_failure");
    std::cout << "P50: " << dist.percentile(50) << "\n";
    std::cout << "P95: " << dist.percentile(95) << "\n";
    
    return 0;
}
```

---

**Document End**
