// ATHENA Core - Binary Serialization (ABF - ATHENA Binary Format)
// Contract: Efficient binary serialization for large simulation results.
//
// FORMAT: Custom binary format optimized for f64 arrays
// FEATURES:
// - Version header for compatibility
// - CRC32 checksum for integrity
// - Section-based layout for streaming
// - Little-endian byte order
// - Zero external dependencies
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#ifndef ATHENA_SERIALIZATION_HPP
#define ATHENA_SERIALIZATION_HPP

#include "athena/types.hpp"
#include "athena/analysis/montecarlo.hpp"

#include <vector>
#include <string>
#include <fstream>
#include <cstring>

namespace athena {

// =============================================================================
// ABF Format Constants
// =============================================================================

namespace abf {

// Magic number: "ATHENABF" in ASCII
constexpr u64 MAGIC = 0x4642414E45485441ULL;  // "ATHENABF" little-endian

// Format version
constexpr u32 VERSION_MAJOR = 1;
constexpr u32 VERSION_MINOR = 0;
constexpr u32 VERSION = (VERSION_MAJOR << 16) | VERSION_MINOR;

// Section types
enum class SectionType : u32 {
    METADATA    = 0x0001,  // Key-value pairs (strings)
    CONFIG      = 0x0002,  // BatchConfig
    ITERATIONS  = 0x0003,  // Raw iteration results
    STATISTICS  = 0x0004,  // Aggregated statistics
    TIMESERIES  = 0x0005,  // Time series data
    HISTOGRAM   = 0x0006,  // Distribution histograms
    CUSTOM      = 0xFFFF   // User-defined section
};

// Flags
constexpr u32 FLAG_COMPRESSED   = 0x0001;  // Reserved for future
constexpr u32 FLAG_ENCRYPTED    = 0x0002;  // Reserved for future
constexpr u32 FLAG_HAS_MANIFEST = 0x0004;  // Contains execution manifest

}  // namespace abf

// =============================================================================
// CRC32 Implementation (IEEE polynomial)
// =============================================================================

class CRC32 {
public:
    CRC32() : crc_(0xFFFFFFFF) {
        init_table();
    }
    
    void update(const void* data, usize len) {
        const u8* bytes = static_cast<const u8*>(data);
        for (usize i = 0; i < len; ++i) {
            u8 index = (crc_ ^ bytes[i]) & 0xFF;
            crc_ = (crc_ >> 8) ^ table_[index];
        }
    }
    
    void update(u8 byte) {
        u8 index = (crc_ ^ byte) & 0xFF;
        crc_ = (crc_ >> 8) ^ table_[index];
    }
    
    u32 finalize() const {
        return crc_ ^ 0xFFFFFFFF;
    }
    
    void reset() {
        crc_ = 0xFFFFFFFF;
    }
    
    // Convenience: compute CRC32 of a buffer in one call
    static u32 compute(const void* data, usize len) {
        CRC32 crc;
        crc.update(data, len);
        return crc.finalize();
    }

private:
    void init_table() {
        constexpr u32 POLYNOMIAL = 0xEDB88320;
        for (u32 i = 0; i < 256; ++i) {
            u32 c = i;
            for (int j = 0; j < 8; ++j) {
                if (c & 1) {
                    c = POLYNOMIAL ^ (c >> 1);
                } else {
                    c >>= 1;
                }
            }
            table_[i] = c;
        }
    }
    
    u32 crc_;
    u32 table_[256];
};

// =============================================================================
// Binary Writer
// =============================================================================

class BinaryWriter {
public:
    explicit BinaryWriter(const std::string& path);
    ~BinaryWriter();
    
    bool is_open() const { return file_.is_open(); }
    usize bytes_written() const { return bytes_written_; }
    
    // Write primitives (little-endian)
    void write_u8(u8 value);
    void write_u16(u16 value);
    void write_u32(u32 value);
    void write_u64(u64 value);
    void write_i32(i32 value);
    void write_i64(i64 value);
    void write_f32(f32 value);
    void write_f64(f64 value);
    
    // Write arrays
    void write_bytes(const void* data, usize len);
    void write_string(const std::string& str);
    void write_f64_array(const std::vector<f64>& arr);
    void write_u32_array(const std::vector<u32>& arr);
    
    // Position management
    usize position() const;
    void seek(usize pos);
    
    // CRC tracking
    void start_crc();
    u32 finish_crc();
    
private:
    std::ofstream file_;
    usize bytes_written_;
    CRC32 crc_;
    bool crc_active_;
};

// =============================================================================
// Binary Reader
// =============================================================================

class BinaryReader {
public:
    explicit BinaryReader(const std::string& path);
    ~BinaryReader();
    
    bool is_open() const { return file_.is_open(); }
    usize file_size() const { return file_size_; }
    
    // Read primitives (little-endian)
    u8 read_u8();
    u16 read_u16();
    u32 read_u32();
    u64 read_u64();
    i32 read_i32();
    i64 read_i64();
    f32 read_f32();
    f64 read_f64();
    
    // Read arrays
    void read_bytes(void* buffer, usize len);
    std::string read_string();
    std::vector<f64> read_f64_array();
    std::vector<u32> read_u32_array();
    
    // Position management
    usize position();
    void seek(usize pos);
    bool eof() const;
    
    // CRC tracking
    void start_crc();
    u32 finish_crc();
    
private:
    std::ifstream file_;
    usize file_size_;
    CRC32 crc_;
    bool crc_active_;
};

// =============================================================================
// Section Header
// =============================================================================

struct SectionHeader {
    abf::SectionType type;
    u64 offset;
    u64 size;
    u32 checksum;
    
    void write(BinaryWriter& w) const;
    void read(BinaryReader& r);
};

// =============================================================================
// ABF File Header
// =============================================================================

struct ABFHeader {
    u64 magic = abf::MAGIC;
    u32 version = abf::VERSION;
    u32 flags = 0;
    u32 num_sections = 0;
    u32 header_crc = 0;
    
    bool validate() const;
    void write(BinaryWriter& w) const;
    void read(BinaryReader& r);
};

// =============================================================================
// Result Serializer
// =============================================================================

class ResultSerializer {
public:
    ResultSerializer();
    
    // Write Monte Carlo results to ABF file
    Result<void> write(const std::string& path,
                       const analysis::BatchConfig& config,
                       const std::vector<analysis::IterationResult>& results,
                       const analysis::BatchStatistics& stats);
    
    // Read Monte Carlo results from ABF file
    Result<void> read(const std::string& path,
                      analysis::BatchConfig& config,
                      std::vector<analysis::IterationResult>& results,
                      analysis::BatchStatistics& stats);
    
    // Add custom metadata
    void set_metadata(const std::string& key, const std::string& value);
    std::string get_metadata(const std::string& key) const;
    
    // Validation
    static Result<bool> validate_file(const std::string& path);
    
    // Get last error
    const std::string& last_error() const { return last_error_; }

private:
    std::vector<std::pair<std::string, std::string>> metadata_;
    std::string last_error_;
    
    void write_metadata_section(BinaryWriter& w);
    void write_config_section(BinaryWriter& w, const analysis::BatchConfig& config);
    void write_iterations_section(BinaryWriter& w, 
                                  const std::vector<analysis::IterationResult>& results);
    void write_statistics_section(BinaryWriter& w, 
                                  const analysis::BatchStatistics& stats);
    
    void read_metadata_section(BinaryReader& r, u64 size);
    void read_config_section(BinaryReader& r, analysis::BatchConfig& config);
    void read_iterations_section(BinaryReader& r, 
                                 std::vector<analysis::IterationResult>& results,
                                 u64 size);
    void read_statistics_section(BinaryReader& r, analysis::BatchStatistics& stats);
};

// =============================================================================
// Convenience Functions
// =============================================================================

// Save results to ABF file
Result<void> save_results_abf(const std::string& path,
                              const analysis::BatchConfig& config,
                              const std::vector<analysis::IterationResult>& results,
                              const analysis::BatchStatistics& stats);

// Load results from ABF file
Result<void> load_results_abf(const std::string& path,
                              analysis::BatchConfig& config,
                              std::vector<analysis::IterationResult>& results,
                              analysis::BatchStatistics& stats);

// Validate ABF file integrity
Result<bool> validate_abf(const std::string& path);

}  // namespace athena

#endif  // ATHENA_SERIALIZATION_HPP
