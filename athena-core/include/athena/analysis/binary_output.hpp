/**
 * @file binary_output.hpp
 * @brief Binary serialization for simulation results
 * 
 * Compact binary format for large Monte Carlo results.
 * Zero external dependencies - uses standard C++ only.
 * 
 * FORMAT SPECIFICATION (v1.0):
 * ┌─────────────────────────────────────────────────────────────────┐
 * │ HEADER (32 bytes)                                               │
 * │   Magic: "ATHR" (4 bytes)                                       │
 * │   Version: u16 (major.minor)                                    │
 * │   Flags: u16                                                    │
 * │   Header CRC32: u32                                             │
 * │   Data CRC32: u32                                               │
 * │   Total size: u64                                               │
 * │   Iteration count: u32                                          │
 * │   Reserved: u32                                                 │
 * ├─────────────────────────────────────────────────────────────────┤
 * │ METADATA SECTION                                                │
 * │   Scenario name length: u16                                     │
 * │   Scenario name: char[N]                                        │
 * │   Timestamp: u64 (unix seconds)                                 │
 * │   Master seed: u64                                              │
 * │   Initial blue: u32                                             │
 * │   Initial red: u32                                              │
 * ├─────────────────────────────────────────────────────────────────┤
 * │ STATISTICS SECTION (64 bytes)                                   │
 * │   Blue wins: u32                                                │
 * │   Red wins: u32                                                 │
 * │   Draws: u32                                                    │
 * │   Reserved: u32                                                 │
 * │   Mean blue casualties: f64                                     │
 * │   Mean red casualties: f64                                      │
 * │   Stddev blue: f64                                              │
 * │   Stddev red: f64                                               │
 * │   Mean ticks: f64                                               │
 * │   Runtime seconds: f64                                          │
 * ├─────────────────────────────────────────────────────────────────┤
 * │ ITERATION DATA (N × 40 bytes each)                              │
 * │   Seed: u64                                                     │
 * │   Ticks: u32                                                    │
 * │   Blue surviving: u32                                           │
 * │   Red surviving: u32                                            │
 * │   Blue losses: u32                                              │
 * │   Red losses: u32                                               │
 * │   Reserved: u32                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * 
 * File extension: .athbin
 */

#ifndef ATHENA_ANALYSIS_BINARY_OUTPUT_HPP
#define ATHENA_ANALYSIS_BINARY_OUTPUT_HPP

#include "athena/types.hpp"
#include <string>
#include <vector>
#include <cstdio>

namespace athena {
namespace analysis {

// =============================================================================
// Format Constants
// =============================================================================

/// File magic number: "ATHR" in little-endian
constexpr u32 BINARY_MAGIC = 0x52485441;  // 'A','T','H','R'

/// Current format version
constexpr u16 BINARY_VERSION = 0x0100;  // 1.0

/// Flag bits
constexpr u16 FLAG_LITTLE_ENDIAN = 0x0001;
constexpr u16 FLAG_HAS_SOBOL     = 0x0002;
constexpr u16 FLAG_COMPRESSED    = 0x0004;  // Reserved for future

// =============================================================================
// Data Structures (packed for binary I/O)
// =============================================================================

#pragma pack(push, 1)

/// File header - exactly 32 bytes
struct BinaryHeader {
    u32 magic = BINARY_MAGIC;
    u16 version = BINARY_VERSION;
    u16 flags = FLAG_LITTLE_ENDIAN;
    u32 header_crc = 0;      // CRC of header (excluding this field)
    u32 data_crc = 0;        // CRC of all data after header
    u64 total_size = 0;      // Total file size in bytes
    u32 iteration_count = 0;
    u32 reserved = 0;
};

/// Statistics section - exactly 64 bytes
struct BinaryStatistics {
    u32 blue_wins = 0;
    u32 red_wins = 0;
    u32 draws = 0;
    u32 reserved1 = 0;
    f64 mean_blue_casualties = 0.0;
    f64 mean_red_casualties = 0.0;
    f64 stddev_blue = 0.0;
    f64 stddev_red = 0.0;
    f64 mean_ticks = 0.0;
    f64 runtime_seconds = 0.0;
};

/// Per-iteration record - exactly 40 bytes
struct BinaryIteration {
    u64 seed = 0;
    u32 ticks = 0;
    u32 blue_surviving = 0;
    u32 red_surviving = 0;
    u32 blue_losses = 0;
    u32 red_losses = 0;
    u32 reserved = 0;
};

#pragma pack(pop)

// Static assertions for binary compatibility
static_assert(sizeof(BinaryHeader) == 32, "BinaryHeader must be 32 bytes");
static_assert(sizeof(BinaryStatistics) == 64, "BinaryStatistics must be 64 bytes");
static_assert(sizeof(BinaryIteration) == 32, "BinaryIteration must be 32 bytes");

// =============================================================================
// Metadata (variable-length, not packed)
// =============================================================================

struct BinaryMetadata {
    std::string scenario_name;
    u64 timestamp = 0;
    u64 master_seed = 0;
    u32 initial_blue = 0;
    u32 initial_red = 0;
};

// =============================================================================
// Complete Results Structure (for reading)
// =============================================================================

struct BinaryResults {
    BinaryHeader header;
    BinaryMetadata metadata;
    BinaryStatistics statistics;
    std::vector<BinaryIteration> iterations;
    bool valid = false;
};

// =============================================================================
// Writer Class
// =============================================================================

/**
 * @brief Writes simulation results to binary format
 * 
 * Usage:
 *   BinaryWriter writer;
 *   if (writer.open("results.athbin")) {
 *       writer.write(results);
 *       writer.close();
 *   }
 */
class BinaryWriter {
public:
    BinaryWriter() = default;
    ~BinaryWriter();
    
    // Non-copyable
    BinaryWriter(const BinaryWriter&) = delete;
    BinaryWriter& operator=(const BinaryWriter&) = delete;
    
    /**
     * @brief Open file for writing
     * @param path Output file path
     * @return true on success
     */
    bool open(const std::string& path);
    
    /**
     * @brief Close file and finalize
     */
    bool close();
    
    /**
     * @brief Check if open
     */
    bool is_open() const { return file_ != nullptr; }
    
    /**
     * @brief Write complete results
     * @param metadata Scenario metadata
     * @param stats Aggregated statistics
     * @param iterations Per-iteration data
     * @return true on success
     */
    bool write(const BinaryMetadata& metadata,
               const BinaryStatistics& stats,
               const std::vector<BinaryIteration>& iterations);
    
    /**
     * @brief Get error message
     */
    const std::string& error() const { return error_; }
    
    /**
     * @brief Get bytes written
     */
    u64 bytes_written() const { return bytes_written_; }

private:
    bool write_raw(const void* data, size_t size);
    bool write_string(const std::string& str);
    u32 compute_crc(const void* data, size_t size) const;
    
    FILE* file_ = nullptr;
    std::string error_;
    u64 bytes_written_ = 0;
};

// =============================================================================
// Reader Class
// =============================================================================

/**
 * @brief Reads simulation results from binary format
 * 
 * Usage:
 *   BinaryReader reader;
 *   if (reader.open("results.athbin")) {
 *       BinaryResults results;
 *       if (reader.read(results)) {
 *           // Use results
 *       }
 *       reader.close();
 *   }
 */
class BinaryReader {
public:
    BinaryReader() = default;
    ~BinaryReader();
    
    // Non-copyable
    BinaryReader(const BinaryReader&) = delete;
    BinaryReader& operator=(const BinaryReader&) = delete;
    
    /**
     * @brief Open file for reading
     * @param path Input file path
     * @return true on success
     */
    bool open(const std::string& path);
    
    /**
     * @brief Close file
     */
    void close();
    
    /**
     * @brief Check if open
     */
    bool is_open() const { return file_ != nullptr; }
    
    /**
     * @brief Verify file integrity (CRC check)
     * @return true if valid
     */
    bool verify();
    
    /**
     * @brief Read complete results
     * @param results Output results structure
     * @return true on success
     */
    bool read(BinaryResults& results);
    
    /**
     * @brief Read header only (fast)
     * @param header Output header
     * @return true on success
     */
    bool read_header(BinaryHeader& header);
    
    /**
     * @brief Read single iteration by index
     * @param index 0-based iteration index
     * @param iter Output iteration data
     * @return true on success
     */
    bool read_iteration(u32 index, BinaryIteration& iter);
    
    /**
     * @brief Get iteration count (from header)
     */
    u32 iteration_count() const { return header_.iteration_count; }
    
    /**
     * @brief Get error message
     */
    const std::string& error() const { return error_; }

private:
    bool read_raw(void* data, size_t size);
    bool read_string(std::string& str);
    u32 compute_crc(const void* data, size_t size) const;
    
    FILE* file_ = nullptr;
    std::string error_;
    BinaryHeader header_;
    long iterations_offset_ = 0;
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Check if file is ATHENA binary format
 * @param path File path to check
 * @return true if valid ATHENA binary file
 */
bool is_athena_binary(const std::string& path);

/**
 * @brief Get binary file extension
 */
inline const char* binary_extension() { return ".athbin"; }

}  // namespace analysis
}  // namespace athena

#endif  // ATHENA_ANALYSIS_BINARY_OUTPUT_HPP
