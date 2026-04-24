// ATHENA Core - Manifest Implementation
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/manifest.hpp"
#include <chrono>
#include <ctime>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <random>

#ifdef __linux__
#include <sys/utsname.h>
#include <unistd.h>
#include <fstream>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

namespace athena {

// =============================================================================
// SHA-256 Implementation (minimal, self-contained)
// =============================================================================

namespace {

// SHA-256 constants
constexpr u32 SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

inline u32 rotr32(u32 x, u32 n) {
    return (x >> n) | (x << (32 - n));
}

inline u32 ch(u32 x, u32 y, u32 z) {
    return (x & y) ^ (~x & z);
}

inline u32 maj(u32 x, u32 y, u32 z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

inline u32 sigma0(u32 x) {
    return rotr32(x, 2) ^ rotr32(x, 13) ^ rotr32(x, 22);
}

inline u32 sigma1(u32 x) {
    return rotr32(x, 6) ^ rotr32(x, 11) ^ rotr32(x, 25);
}

inline u32 gamma0(u32 x) {
    return rotr32(x, 7) ^ rotr32(x, 18) ^ (x >> 3);
}

inline u32 gamma1(u32 x) {
    return rotr32(x, 17) ^ rotr32(x, 19) ^ (x >> 10);
}

Hash256 sha256(const u8* data, usize len) {
    // Initial hash values
    u32 h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    
    // Pre-processing: padding
    usize ml = len * 8;  // Message length in bits
    usize padded_len = ((len + 8 + 64) / 64) * 64;
    
    std::vector<u8> padded(padded_len, 0);
    std::memcpy(padded.data(), data, len);
    padded[len] = 0x80;
    
    // Append length in big-endian
    for (int i = 0; i < 8; ++i) {
        padded[padded_len - 1 - i] = static_cast<u8>(ml >> (i * 8));
    }
    
    // Process blocks
    for (usize block = 0; block < padded_len; block += 64) {
        u32 w[64];
        
        // First 16 words from block (big-endian)
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<u32>(padded[block + i*4]) << 24) |
                   (static_cast<u32>(padded[block + i*4 + 1]) << 16) |
                   (static_cast<u32>(padded[block + i*4 + 2]) << 8) |
                   (static_cast<u32>(padded[block + i*4 + 3]));
        }
        
        // Extend
        for (int i = 16; i < 64; ++i) {
            w[i] = gamma1(w[i-2]) + w[i-7] + gamma0(w[i-15]) + w[i-16];
        }
        
        // Working variables
        u32 a = h[0], b = h[1], c = h[2], d = h[3];
        u32 e = h[4], f = h[5], g = h[6], hh = h[7];
        
        // Compression
        for (int i = 0; i < 64; ++i) {
            u32 t1 = hh + sigma1(e) + ch(e, f, g) + SHA256_K[i] + w[i];
            u32 t2 = sigma0(a) + maj(a, b, c);
            hh = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        
        // Add to hash
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    
    // Output (big-endian)
    Hash256 result;
    for (int i = 0; i < 8; ++i) {
        result.bytes[i*4] = static_cast<u8>(h[i] >> 24);
        result.bytes[i*4 + 1] = static_cast<u8>(h[i] >> 16);
        result.bytes[i*4 + 2] = static_cast<u8>(h[i] >> 8);
        result.bytes[i*4 + 3] = static_cast<u8>(h[i]);
    }
    
    return result;
}

Hash256 sha256_string(const std::string& str) {
    return sha256(reinterpret_cast<const u8*>(str.data()), str.size());
}

}  // anonymous namespace

// =============================================================================
// ManifestBuilder Implementation
// =============================================================================

ManifestBuilder::ManifestBuilder()
    : manifest_()
    , platform_set_(false)
    , build_set_(false)
    , config_set_(false)
{
    manifest_.manifest_version = "1.0";
    manifest_.sequence_number = 0;
}

ManifestBuilder& ManifestBuilder::set_job_id(const std::string& job_id) {
    manifest_.job_id = job_id;
    return *this;
}

ManifestBuilder& ManifestBuilder::capture_platform() {
    manifest_.platform = manifest::detect_platform();
    platform_set_ = true;
    return *this;
}

ManifestBuilder& ManifestBuilder::set_build_info(const BuildInfo& info) {
    manifest_.build = info;
    build_set_ = true;
    return *this;
}

ManifestBuilder& ManifestBuilder::set_config(const Context& ctx, 
                                             const ExecutionConfig& exec_config) {
    manifest_.config = exec_config;
    manifest_.config.master_seed = ctx.config().seed;
    config_set_ = true;
    return *this;
}

ManifestBuilder& ManifestBuilder::set_scenario_hash(const Hash256& hash) {
    manifest_.scenario_hash = hash;
    return *this;
}

ManifestBuilder& ManifestBuilder::set_model_hash(const Hash256& hash) {
    manifest_.model_hash = hash;
    return *this;
}

ManifestBuilder& ManifestBuilder::set_parameters_hash(const Hash256& hash) {
    manifest_.parameters_hash = hash;
    return *this;
}

ManifestBuilder& ManifestBuilder::set_results(const ExecutionResults& results) {
    manifest_.results = results;
    return *this;
}

ManifestBuilder& ManifestBuilder::set_output_hash(const Hash256& hash) {
    manifest_.output_hash = hash;
    return *this;
}

ManifestBuilder& ManifestBuilder::set_previous_manifest(const Hash256& hash, u64 seq) {
    manifest_.previous_manifest_hash = hash;
    manifest_.sequence_number = seq;
    return *this;
}

Result<Manifest> ManifestBuilder::build() {
    if (!config_set_) {
        return Error(ErrorCode::INVALID_ARGUMENT,
            "Execution config must be set");
    }
    
    // Auto-capture platform if not set
    if (!platform_set_) {
        capture_platform();
    }
    
    // Auto-capture build info if not set
    if (!build_set_) {
        manifest_.build = manifest::get_build_info();
    }
    
    // Set timestamp
    manifest_.created_at = manifest::current_timestamp();
    
    // Generate job ID if not set
    if (manifest_.job_id.empty()) {
        manifest_.job_id = manifest::generate_job_id();
    }
    
    // Compute manifest hash
    manifest_.manifest_hash = manifest::compute_manifest_hash(manifest_);
    
    return manifest_;
}

// =============================================================================
// Manifest Utilities
// =============================================================================

namespace manifest {

PlatformInfo detect_platform() {
    PlatformInfo info;
    
#ifdef __linux__
    struct utsname un;
    if (uname(&un) == 0) {
        info.os_name = un.sysname;
        info.os_version = un.release;
    }
    
    // Read CPU info
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("vendor_id") != std::string::npos) {
                auto pos = line.find(':');
                if (pos != std::string::npos) {
                    info.cpu_vendor = line.substr(pos + 2);
                }
            } else if (line.find("model name") != std::string::npos) {
                auto pos = line.find(':');
                if (pos != std::string::npos) {
                    info.cpu_brand = line.substr(pos + 2);
                }
            } else if (line.find("flags") != std::string::npos) {
                auto pos = line.find(':');
                if (pos != std::string::npos) {
                    std::string flags = line.substr(pos + 2);
                    std::istringstream iss(flags);
                    std::string flag;
                    while (iss >> flag) {
                        // Only capture relevant flags
                        if (flag == "avx" || flag == "avx2" || flag == "avx512f" ||
                            flag == "fma" || flag == "sse4_2" || flag == "aes") {
                            info.cpu_features.push_back(flag);
                        }
                    }
                }
            }
        }
    }
    
    info.cpu_cores = static_cast<u32>(sysconf(_SC_NPROCESSORS_CONF));
    info.cpu_threads = static_cast<u32>(sysconf(_SC_NPROCESSORS_ONLN));
    
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    info.ram_bytes = static_cast<u64>(pages) * static_cast<u64>(page_size);
    
#elif defined(_WIN32)
    info.os_name = "Windows";
    
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    info.cpu_cores = sysinfo.dwNumberOfProcessors;
    info.cpu_threads = sysinfo.dwNumberOfProcessors;
    
    MEMORYSTATUSEX meminfo;
    meminfo.dwLength = sizeof(meminfo);
    GlobalMemoryStatusEx(&meminfo);
    info.ram_bytes = meminfo.ullTotalPhys;
#else
    info.os_name = "Unknown";
#endif
    
    // Detect microarch (simplified)
    if (info.cpu_vendor.find("Intel") != std::string::npos ||
        info.cpu_vendor.find("GenuineIntel") != std::string::npos) {
        info.cpu_microarch = "intel";  // Could be more specific
    } else if (info.cpu_vendor.find("AMD") != std::string::npos ||
               info.cpu_vendor.find("AuthenticAMD") != std::string::npos) {
        info.cpu_microarch = "amd";  // Could be more specific
    } else {
        info.cpu_microarch = "unknown";
    }
    
    return info;
}

BuildInfo get_build_info() {
    BuildInfo info;
    info.version = constants::VERSION;
    info.build_date = __DATE__ " " __TIME__;
    
#if defined(__GNUC__)
    info.compiler = "gcc-" + std::to_string(__GNUC__) + "." + 
                    std::to_string(__GNUC_MINOR__) + "." +
                    std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    info.compiler = "msvc-" + std::to_string(_MSC_VER);
#elif defined(__clang__)
    info.compiler = "clang-" + std::to_string(__clang_major__) + "." +
                    std::to_string(__clang_minor__);
#else
    info.compiler = "unknown";
#endif
    
    // Compiler flags would need to be injected at build time
    info.compiler_flags = "";
    
    // Binary hash would need to be computed externally
    info.binary_hash = Hash256{};
    
    return info;
}

std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &time);
#else
    gmtime_r(&time, &tm_buf);
#endif
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string generate_job_id() {
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
    
    // Simple ID: timestamp + random suffix
    // Note: Using time-based seed is acceptable here since this is not simulation
    std::ostringstream oss;
    oss << "job_" << std::hex << millis;
    
    return oss.str();
}

std::string to_json(const Manifest& manifest) {
    std::ostringstream oss;
    
    oss << "{\n";
    oss << "  \"manifest_version\": \"" << manifest.manifest_version << "\",\n";
    oss << "  \"created_at\": \"" << manifest.created_at << "\",\n";
    oss << "  \"job_id\": \"" << manifest.job_id << "\",\n";
    
    // Platform
    oss << "  \"platform\": {\n";
    oss << "    \"os_name\": \"" << manifest.platform.os_name << "\",\n";
    oss << "    \"os_version\": \"" << manifest.platform.os_version << "\",\n";
    oss << "    \"cpu_vendor\": \"" << manifest.platform.cpu_vendor << "\",\n";
    oss << "    \"cpu_brand\": \"" << manifest.platform.cpu_brand << "\",\n";
    oss << "    \"cpu_microarch\": \"" << manifest.platform.cpu_microarch << "\",\n";
    oss << "    \"cpu_cores\": " << manifest.platform.cpu_cores << ",\n";
    oss << "    \"cpu_threads\": " << manifest.platform.cpu_threads << ",\n";
    oss << "    \"ram_bytes\": " << manifest.platform.ram_bytes << "\n";
    oss << "  },\n";
    
    // Build
    oss << "  \"build\": {\n";
    oss << "    \"version\": \"" << manifest.build.version << "\",\n";
    oss << "    \"compiler\": \"" << manifest.build.compiler << "\",\n";
    oss << "    \"build_date\": \"" << manifest.build.build_date << "\"\n";
    oss << "  },\n";
    
    // Config
    oss << "  \"config\": {\n";
    oss << "    \"master_seed\": " << manifest.config.master_seed << ",\n";
    oss << "    \"rng_stream\": " << manifest.config.rng_stream << ",\n";
    oss << "    \"thread_count\": " << manifest.config.thread_count << ",\n";
    oss << "    \"start_tick\": " << manifest.config.start_tick << ",\n";
    oss << "    \"end_tick\": " << manifest.config.end_tick << "\n";
    oss << "  },\n";
    
    // Hashes
    oss << "  \"scenario_hash\": \"" << manifest.scenario_hash.to_hex() << "\",\n";
    oss << "  \"model_hash\": \"" << manifest.model_hash.to_hex() << "\",\n";
    oss << "  \"parameters_hash\": \"" << manifest.parameters_hash.to_hex() << "\",\n";
    oss << "  \"output_hash\": \"" << manifest.output_hash.to_hex() << "\",\n";
    
    // Results
    oss << "  \"results\": {\n";
    oss << "    \"ticks_executed\": " << manifest.results.ticks_executed << ",\n";
    oss << "    \"entities_created\": " << manifest.results.entities_created << ",\n";
    oss << "    \"duration_seconds\": " << manifest.results.duration_seconds << ",\n";
    oss << "    \"completed_normally\": " << (manifest.results.completed_normally ? "true" : "false") << "\n";
    oss << "  },\n";
    
    // Chain
    oss << "  \"sequence_number\": " << manifest.sequence_number << ",\n";
    
    // Manifest hash
    oss << "  \"manifest_hash\": \"" << manifest.manifest_hash.to_hex() << "\"\n";
    
    oss << "}\n";
    
    return oss.str();
}

Hash256 compute_manifest_hash(const Manifest& manifest) {
    // Create a canonical string representation (excluding manifest_hash)
    std::ostringstream oss;
    
    oss << manifest.manifest_version;
    oss << manifest.created_at;
    oss << manifest.job_id;
    oss << manifest.platform.os_name;
    oss << manifest.platform.os_version;
    oss << manifest.platform.cpu_microarch;
    oss << manifest.build.version;
    oss << manifest.build.compiler;
    oss << manifest.config.master_seed;
    oss << manifest.config.rng_stream;
    oss << manifest.config.thread_count;
    oss << manifest.scenario_hash.to_hex();
    oss << manifest.model_hash.to_hex();
    oss << manifest.parameters_hash.to_hex();
    oss << manifest.output_hash.to_hex();
    oss << manifest.results.ticks_executed;
    oss << manifest.results.completed_normally;
    oss << manifest.sequence_number;
    
    return sha256_string(oss.str());
}

bool verify_manifest(const Manifest& manifest) {
    Hash256 computed = compute_manifest_hash(manifest);
    return computed == manifest.manifest_hash;
}

}  // namespace manifest

}  // namespace athena
