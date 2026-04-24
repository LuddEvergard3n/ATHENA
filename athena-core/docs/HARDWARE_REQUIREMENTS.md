# ATHENA — Hardware Requirements

**Version:** 1.1.2  
**Status:** Specification  
**Classification:** Technical Reference

---

## 1. Overview

### 1.1 Compute Profile

ATHENA is a **CPU-bound** simulation system with optional GPU acceleration. Primary computational demand:

- Monte Carlo simulation (parallelizable)
- Sensitivity analysis (parallelizable)
- Graph operations (partially parallelizable)
- Numerical integration (parallelizable)

### 1.2 Design Principles

| Principle | Rationale |
|-----------|-----------|
| CPU-first | Maximum compatibility |
| GPU-optional | Acceleration where available, not mandatory |
| Memory-efficient | Large scenario support without exotic hardware |
| Storage-light | Reproducibility via seed, not result storage |
| Dual-mode networking | Connected for data ingest, air-gap optional for simulation |

### 1.3 Network Architecture

ATHENA requires network connectivity for **data ingestion** from open sources (SIPRI, IISS, NOAA, NASA, etc.).

Two deployment modes supported:

**Mode 1: Connected (Default)**
- Data ingestion module has internet access
- Pulls from OSINT sources in real-time or scheduled
- Simulation runs on same system
- Suitable for most commercial/contractor deployments

**Mode 2: Hybrid (High Security)**
- Data ingestion on connected workstation
- Export validated/hashed dataset package
- Transfer via approved media to air-gapped simulation environment
- Required for classified environments

```
MODE 1 (CONNECTED):
┌──────────────────────────────────────────────────┐
│                  SINGLE SYSTEM                    │
│  Internet ──▶ Ingest ──▶ Validate ──▶ Simulate   │
└──────────────────────────────────────────────────┘

MODE 2 (HYBRID):
┌───────────────────────┐       ┌───────────────────────┐
│   CONNECTED SYSTEM    │       │   AIR-GAPPED SYSTEM   │
│  Internet ──▶ Ingest  │  ══▶  │  Import ──▶ Simulate  │
│           ──▶ Package │       │                       │
└───────────────────────┘       └───────────────────────┘
      (approved media transfer)
```

---

## 2. Network Requirements

### 2.1 Architecture: Online-First, Offline-Capable

ATHENA is designed **online-first** for data acquisition, but **fully offline** for simulation execution.

```
┌─────────────────────────────────────────────────────────────────┐
│                    ONLINE OPERATIONS                             │
│  • Initial theater package download                              │
│  • Military data updates (SIPRI, IISS, etc.)                    │
│  • High-resolution terrain tiles (on-demand)                     │
│  • Climate data refresh                                          │
│  • Software updates                                              │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    OFFLINE OPERATIONS                            │
│  • All simulation execution                                      │
│  • Scenario creation/editing                                     │
│  • Results analysis                                              │
│  • Audit/logging                                                 │
│  • Full functionality with cached data                          │
└─────────────────────────────────────────────────────────────────┘
```

**Offline Duration:** Once theater packages are downloaded, ATHENA can operate fully offline indefinitely. Military data remains valid for months; terrain data is static.

### 2.2 Connectivity Requirements by Operation

| Operation | Network Required | Bandwidth | Frequency |
|-----------|------------------|-----------|-----------|
| Initial setup | Yes | 100 Mbps recommended | Once |
| Theater download | Yes | 100 Mbps recommended | Per theater, once |
| Military data sync | Yes | 10 Mbps sufficient | Weekly/monthly |
| High-res tiles | Yes (or skip) | 50 Mbps | On-demand |
| Climate refresh | Yes (or use cached) | 50 Mbps | Monthly |
| Scenario editing | **No** | — | — |
| Simulation run | **No** | — | — |
| Results analysis | **No** | — | — |
| Software update | Yes | 50 Mbps | Quarterly |

### 2.3 Data Source Connectivity

| Source | Domain(s) | Data Type | Size |
|--------|-----------|-----------|------|
| SIPRI | sipri.org | Military expenditure | ~50 MB |
| IISS | iiss.org | Military balance | ~100 MB |
| Global Firepower | globalfirepower.com | Force indices | ~10 MB |
| NASA Earthdata | earthdata.nasa.gov | Terrain (SRTM) | Per theater |
| OpenTopography | opentopography.org | High-res DEM | On-demand |
| NOAA | noaa.gov, ncei.noaa.gov | Climate historical | Per theater |
| ECMWF | ecmwf.int | Climate reanalysis | Per theater |
| Jane's (licensed) | janes.com | Defense intel | ~1-5 GB |

### 2.4 Network Security

| Control | Specification |
|---------|---------------|
| Egress filtering | Whitelist approved domains only |
| TLS verification | TLS 1.3, certificate validation |
| Proxy support | HTTP/HTTPS proxy compatible |
| Logging | All external requests logged |
| Data validation | Hash verification post-download |
| Integrity | Downloaded packages signed |

### 2.5 Bandwidth Planning

**Initial Deployment (one-time):**

| Component | Size | Time @ 100 Mbps |
|-----------|------|-----------------|
| ATHENA installer | 2 GB | 3 min |
| Global military data | 500 MB | 1 min |
| First theater package | 15 GB | 20 min |
| **Total first-run** | ~18 GB | ~25 min |

**Ongoing (monthly typical):**

| Component | Size | Time @ 100 Mbps |
|-----------|------|-----------------|
| Military data refresh | 100 MB | 10 sec |
| Climate updates | 500 MB | 45 sec |
| High-res tiles (if used) | 1-5 GB | 1-7 min |

### 2.6 Disconnected / Air-Gapped Deployment

For classified or disconnected environments:

**Option A: Pre-staged Packages**
1. Download theater packages on connected system
2. Export as signed package file
3. Transfer via approved media
4. Import on air-gapped system

**Option B: Media-based Distribution**
1. Vendor provides theater packages on encrypted media
2. Client imports via secure workstation
3. All updates via same channel

```
CONNECTED WORKSTATION          AIR-GAPPED SYSTEM
┌─────────────────────┐       ┌─────────────────────┐
│  Download theaters  │       │  Import packages    │
│  Verify signatures  │ ════▶ │  Verify signatures  │
│  Export to media    │       │  Full offline ops   │
└─────────────────────┘       └─────────────────────┘
```

### 2.7 Offline Indicators

ATHENA UI clearly indicates data freshness:

| Indicator | Meaning |
|-----------|---------|
| 🟢 Current | Data updated within policy window |
| 🟡 Stale | Data older than policy, still usable |
| 🔴 Unavailable | Required data missing, fetch needed |
| ⚫ Offline Mode | Network disabled, using cached data |

---

## 3. Hardware Tiers

### 3.1 Minimum (Development / Small Scenarios)

| Component | Specification | Notes |
|-----------|---------------|-------|
| **CPU** | 8 cores / 16 threads | x86-64, AVX2 required |
| **RAM** | 32 GB DDR4 | ECC recommended |
| **Storage** | 256 GB SSD | NVMe preferred |
| **GPU** | Not required | Integrated graphics sufficient for UI |
| **Network** | 10 Mbps | For data ingestion |

**Scenario Scale:** Up to 1,000 agents, 10,000 Monte Carlo iterations

---

### 3.2 Recommended (Production / Medium Scenarios)

| Component | Specification | Notes |
|-----------|---------------|-------|
| **CPU** | 16-32 cores / 32-64 threads | x86-64, AVX-512 preferred |
| **RAM** | 128 GB DDR4/DDR5 | ECC required |
| **Storage** | 1 TB NVMe SSD | For scenario libraries, logs |
| **GPU** | Optional (CUDA-capable) | RTX 3080+ or equivalent |
| **Network** | 100 Mbps | For data ingestion |

**Scenario Scale:** Up to 10,000 agents, 100,000 Monte Carlo iterations

---

### 3.3 High Performance (Enterprise / Large Scenarios)

| Component | Specification | Notes |
|-----------|---------------|-------|
| **CPU** | 64+ cores / 128+ threads | Dual socket, EPYC/Xeon |
| **RAM** | 512 GB - 1 TB DDR5 | ECC required |
| **Storage** | 4 TB+ NVMe SSD (RAID) | For large scenario libraries |
| **GPU** | Multiple CUDA GPUs | A100/H100 class for maximum throughput |
| **Network** | 1 Gbps | For bulk data ingestion |

**Scenario Scale:** 100,000+ agents, 1,000,000+ Monte Carlo iterations

---

## 4. Component Specifications

### 4.1 CPU Requirements

| Requirement | Minimum | Recommended | Notes |
|-------------|---------|-------------|-------|
| Architecture | x86-64 | x86-64 | ARM64 future consideration |
| Instruction Set | AVX2 | AVX-512 | SIMD critical for performance |
| Cores | 8 | 16-64 | Monte Carlo parallelization |
| Base Clock | 3.0 GHz | 3.5 GHz+ | Single-thread for sequential phases |
| Cache | 16 MB L3 | 64 MB+ L3 | Data-oriented design benefits |

**Supported Processors (Examples):**

| Tier | Intel | AMD |
|------|-------|-----|
| Minimum | Core i7-10700 | Ryzen 7 5800X |
| Recommended | Xeon W-2255 | Ryzen 9 7950X |
| High Performance | Xeon Platinum 8380 | EPYC 9654 |

### 4.2 Memory Requirements

| Requirement | Minimum | Recommended | Notes |
|-------------|---------|-------------|-------|
| Capacity | 32 GB | 128 GB | Scales with scenario size |
| Type | DDR4-3200 | DDR5-4800 | Bandwidth matters |
| ECC | Recommended | Required | Data integrity |
| Channels | Dual | Quad+ | Memory bandwidth |

**Memory Scaling (Approximate):**

```
RAM (GB) ≈ 4 + (Agents × 0.01) + (MC_Iterations × 0.0001)
```

| Scenario | Agents | MC Iterations | Estimated RAM |
|----------|--------|---------------|---------------|
| Small | 1,000 | 10,000 | ~16 GB |
| Medium | 10,000 | 100,000 | ~64 GB |
| Large | 100,000 | 1,000,000 | ~256 GB |

### 4.3 Storage Requirements

| Requirement | Minimum | Recommended | Notes |
|-------------|---------|-------------|-------|
| Type | SATA SSD | NVMe SSD | I/O for logs, checkpoints |
| Capacity | 256 GB | 500 GB - 1 TB | Theater packages + scenarios |
| Speed | 500 MB/s | 3,000 MB/s+ | Memory-mapped terrain access |
| Endurance | Standard | High endurance | Frequent writes |

**Storage Allocation:**

| Component | Size | Notes |
|-----------|------|-------|
| ATHENA installation | 2-5 GB | Binaries, libraries |
| Global military data | 100-500 MB | SIPRI, IISS, GFP (always local) |
| Theater packages (active) | 10-25 GB each | See §4.4 |
| Scenario library | 10-50 GB | User-defined scenarios |
| Model library | 5-20 GB | Versioned models |
| Climate cache | 5-15 GB | LRU cache, purgeable |
| Audit logs | 10-50 GB/year | Immutable, compressed |
| Checkpoints/results | Variable | Depends on retention policy |
| OS + overhead | 50 GB | Operating system |

**Typical Deployment:**

| Profile | Theaters | Total Storage |
|---------|----------|---------------|
| Analyst (focused) | 1-2 | 150-200 GB |
| Planner (regional) | 3-4 | 250-350 GB |
| Enterprise (global) | 5+ | 500 GB - 1 TB |

### 4.4 Theater Package Architecture

ATHENA uses a **Theater Package** model for geospatial data (terrain, climate, infrastructure). Users download only the theaters they need.

**Why Theater Packages:**

| Approach | Global Data | Storage | Practical |
|----------|-------------|---------|-----------|
| ❌ Download everything | ~1.5 TB | Massive | No |
| ✓ Theater packages | 10-25 GB/theater | Manageable | Yes |

**Theater Package Contents:**

| Component | Resolution | Size (typical) | Update Frequency |
|-----------|------------|----------------|------------------|
| Terrain (DEM) | 90m default | 3-8 GB | Static |
| Terrain (high-res) | 30m on-demand | +5-15 GB | Static |
| Climate (10-year) | Daily | 3-6 GB | Annual |
| Infrastructure | Vector | 500 MB - 1 GB | Quarterly |
| Boundaries/Admin | Vector | 100-200 MB | Annual |

**Available Theater Packages (Examples):**

| Theater | Region | Base Size | High-Res Add-on |
|---------|--------|-----------|-----------------|
| europe-central | Germany, Poland, Baltics, Ukraine | 15 GB | +12 GB |
| europe-west | France, UK, Benelux, Iberia | 12 GB | +10 GB |
| middle-east | Levant, Gulf, Iran | 10 GB | +8 GB |
| indo-pacific-north | Korea, Japan, Taiwan Strait | 14 GB | +11 GB |
| indo-pacific-south | SE Asia, SCS, Philippines | 18 GB | +14 GB |
| south-asia | India, Pakistan, Afghanistan | 16 GB | +13 GB |
| africa-north | Sahel, Maghreb, Libya | 12 GB | +9 GB |

**Resolution Tiers by Scenario Scale:**

| Scenario Scale | Terrain Res | Climate Res | Loaded Automatically |
|----------------|-------------|-------------|---------------------|
| Macro (strategic) | 1 km | Monthly | ✓ (base package) |
| Meso (operational) | 90 m | Daily | ✓ (base package) |
| Micro (tactical) | 30 m | Hourly | On-demand download |

### 4.5 Data Lifecycle Management

**Automatic Cache Management:**

| Data Type | Retention | Purge Policy |
|-----------|-----------|--------------|
| Military data (global) | Permanent | Never purged |
| Active theater packages | Permanent | Manual removal only |
| High-res terrain tiles | 30 days LRU | Auto-purge when unused |
| Climate cache | 14 days LRU | Auto-purge, re-fetch on demand |
| Inactive theaters | User-configured | Prompt after 90 days |

**Storage Pressure Response:**

```
IF available_storage < 20 GB:
  1. Purge climate cache (oldest first)
  2. Purge unused high-res tiles
  3. Prompt user to remove inactive theaters
  4. Block new downloads until space available
```

### 4.4 GPU Requirements (Optional)

| Requirement | Specification | Notes |
|-------------|---------------|-------|
| API | CUDA 11.0+ | NVIDIA only (current) |
| VRAM | 8 GB+ | 16 GB+ for large scenarios |
| Compute | 5,000+ CUDA cores | For meaningful acceleration |
| Driver | Latest stable | Certified drivers preferred |

**GPU Acceleration Use Cases:**

| Workload | GPU Benefit | Notes |
|----------|-------------|-------|
| Monte Carlo sampling | High | Embarrassingly parallel |
| Matrix operations | High | Covariance, sensitivity |
| Graph traversal | Medium | Depends on structure |
| UI rendering | Low | Not a bottleneck |

---

## 5. Operating System Requirements

### 5.1 Supported Operating Systems

| OS | Version | Support Level | Notes |
|----|---------|---------------|-------|
| **Linux (Primary)** | Ubuntu 22.04 LTS | Full | Recommended for production |
| | Ubuntu 24.04 LTS | Full | Latest LTS |
| | RHEL 8/9 | Full | Enterprise deployments |
| | Rocky Linux 8/9 | Full | RHEL-compatible |
| **Windows (Secondary)** | Server 2019 | Full | GPO/AppLocker compatible |
| | Server 2022 | Full | Latest LTS |
| | Windows 11 | Limited | Development only |

### 5.2 OS Configuration

| Requirement | Specification |
|-------------|---------------|
| Filesystem | ext4, XFS (Linux) / NTFS (Windows) |
| Kernel | 5.15+ (Linux) |
| SELinux/AppArmor | Supported, policies provided |
| Firewall | Egress whitelist for data sources |
| Time sync | NTP for connected mode; local RTC for air-gap |

---

## 6. Deployment Configurations

### 6.1 Single System (Connected)

**Use case:** Commercial deployments, contractor environments

| Component | Same System |
|-----------|-------------|
| Data ingestion | ✓ |
| Validation | ✓ |
| Simulation | ✓ |
| UI | ✓ |
| Network | Required (egress to data sources) |

### 6.2 Hybrid (Air-Gap Simulation)

**Use case:** Classified environments, high-security clients

| Component | Connected System | Air-Gapped System |
|-----------|------------------|-------------------|
| Data ingestion | ✓ | — |
| Validation | ✓ | Verify hashes |
| Simulation | — | ✓ |
| UI | — | ✓ |
| Network | Required | None |
| Transfer | Export package | Import package |

**Transfer Package Contents:**
- Validated datasets (hashed)
- Manifests
- Version metadata
- Integrity verification tools

### 6.3 Multi-Node (Future)

**Use case:** Very large scenarios, enterprise clusters

| Component | Head Node | Compute Nodes |
|-----------|-----------|---------------|
| Data ingestion | ✓ | — |
| Orchestration | ✓ | — |
| Simulation | Coordinator | Workers |
| UI | ✓ | — |
| Network | External + internal | Internal only |

*Note: Multi-node support is roadmap, not current release.*

---

## 7. Virtualization Support

### 7.1 Supported Hypervisors

| Hypervisor | Support | Notes |
|------------|---------|-------|
| VMware vSphere 7/8 | Full | Production validated |
| KVM/QEMU | Full | Linux native |
| Hyper-V | Full | Windows environments |
| Proxmox | Full | KVM-based |
| VirtualBox | Limited | Development only |

### 7.2 VM Configuration

| Resource | Minimum | Recommended |
|----------|---------|-------------|
| vCPUs | 8 | 16+ (dedicated) |
| RAM | 32 GB | 128 GB (reserved) |
| Storage | 256 GB (thick provisioned) | 1 TB |
| Network | 1 vNIC | 1 vNIC (dedicated VLAN) |

### 7.3 Virtualization Considerations

| Aspect | Recommendation |
|--------|----------------|
| CPU pinning | Recommended for production |
| NUMA awareness | Enable for large VMs |
| Memory reservation | 100% for determinism |
| Storage | Avoid thin provisioning |
| Snapshots | Disable during simulation |

---

## 8. Containerization

### 8.1 Container Runtime Support

| Runtime | Support | Notes |
|---------|---------|-------|
| Docker | Full | Development, CI/CD |
| Podman | Full | Rootless preferred |
| Kubernetes | Limited | Stateful considerations |

### 8.2 Container Requirements

```yaml
resources:
  requests:
    cpu: "8"
    memory: "32Gi"
  limits:
    cpu: "32"
    memory: "128Gi"
```

### 8.3 Container Considerations

| Aspect | Note |
|--------|------|
| Persistent storage | Required for data cache, logs |
| Network | Egress to data sources |
| Security context | Non-root, read-only rootfs |
| GPU passthrough | NVIDIA Container Toolkit required |

---

## 9. Performance Benchmarks (Reference)

### 9.1 Baseline System

| Component | Specification |
|-----------|---------------|
| CPU | AMD Ryzen 9 7950X (16C/32T) |
| RAM | 128 GB DDR5-5200 |
| Storage | 2 TB NVMe Gen4 |
| GPU | None |
| OS | Ubuntu 24.04 LTS |

### 9.2 Application Performance

| Operation | Target | Achieved |
|-----------|--------|----------|
| Cold start (app launch) | < 5 sec | 3.2 sec |
| Theater switch (cached) | < 3 sec | 1.8 sec |
| Scenario load (small) | < 1 sec | 0.4 sec |
| Scenario load (large, cached theater) | < 3 sec | 2.1 sec |
| Scenario load (new theater, download) | < 60 sec | 45 sec @ 100 Mbps |
| UI responsiveness during sim | < 100 ms | 50 ms |

### 9.3 Data Loading Performance

| Operation | Data Size | Time (NVMe) | Time (SATA SSD) |
|-----------|-----------|-------------|-----------------|
| Load military data (global) | 500 MB | 0.3 sec | 0.8 sec |
| Load theater base package | 15 GB | 2.1 sec | 6.5 sec |
| Load high-res terrain tile | 500 MB | 0.2 sec | 0.6 sec |
| Memory-map terrain (lazy) | N/A | < 10 ms | < 20 ms |

### 9.4 Simulation Performance

| Scenario | Agents | MC Iterations | Time | RAM Peak |
|----------|--------|---------------|------|----------|
| Class I (Sanity) | 100 | 1,000 | 8 sec | 4 GB |
| Class II (Stress) | 5,000 | 50,000 | 12 min | 32 GB |
| Class III (Analytical) | 10,000 | 100,000 | 45 min | 64 GB |
| Large Scale | 50,000 | 500,000 | 4.5 hr | 192 GB |

### 9.5 Scaling Characteristics

| Dimension | Scaling |
|-----------|---------|
| Agents | O(n log n) |
| MC Iterations | O(n) linear |
| CPU Cores | ~0.85 parallel efficiency |
| GPU (if enabled) | 3-8× speedup for MC |
| Storage I/O | Minimal after initial load (memory-mapped) |

### 9.6 Memory Usage by Component

| Component | RAM Usage | Notes |
|-----------|-----------|-------|
| Application base | 500 MB | UI + core engine |
| Military data (loaded) | 200-500 MB | Always in memory |
| Terrain index | 100-300 MB | Metadata, not full raster |
| Terrain tiles (active) | 1-4 GB | Memory-mapped, OS-managed |
| Climate data (active) | 500 MB - 2 GB | Temporal window |
| Scenario (loaded) | 100 MB - 1 GB | Depends on complexity |
| Simulation state | 2-50 GB | Scales with agents |
| Results buffer | 1-10 GB | Scales with iterations |

---

## 10. Procurement Guidance

### 10.1 Workstation (Single User)

**Recommended Configuration:**

| Component | Model/Spec | Est. Cost |
|-----------|------------|-----------|
| CPU | AMD Ryzen 9 7950X | $550 |
| Motherboard | X670E, ECC support | $400 |
| RAM | 128 GB DDR5 ECC | $600 |
| Storage | 2 TB NVMe Gen4 | $200 |
| GPU (optional) | RTX 4080 | $1,000 |
| PSU | 850W 80+ Gold | $150 |
| Case | Tower, good airflow | $150 |
| **Total** | | **$3,050-$4,050** |

### 10.2 Server (Multi-User / Production)

**Recommended Configuration:**

| Component | Model/Spec | Est. Cost |
|-----------|------------|-----------|
| Server | Dell PowerEdge R760 or equiv. | — |
| CPU | 2× Intel Xeon Gold 6430 | $8,000 |
| RAM | 512 GB DDR5 ECC RDIMM | $4,000 |
| Storage | 4× 2TB NVMe RAID10 | $2,000 |
| GPU (optional) | 2× NVIDIA A30 | $10,000 |
| Network | 10GbE | Included |
| **Total** | | **$25,000-$35,000** |

---

**Document End**
