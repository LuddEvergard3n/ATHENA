// ATHENA Core - Binary Serialization Implementation
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/serialization.hpp"

#include <algorithm>
#include <cstring>

namespace athena {

// =============================================================================
// BinaryWriter Implementation
// =============================================================================

BinaryWriter::BinaryWriter(const std::string& path)
    : bytes_written_(0)
    , crc_active_(false)
{
    file_.open(path, std::ios::binary | std::ios::out);
}

BinaryWriter::~BinaryWriter() {
    if (file_.is_open()) {
        file_.close();
    }
}

void BinaryWriter::write_u8(u8 value) {
    file_.write(reinterpret_cast<const char*>(&value), 1);
    if (crc_active_) crc_.update(value);
    bytes_written_ += 1;
}

void BinaryWriter::write_u16(u16 value) {
    u8 bytes[2];
    bytes[0] = value & 0xFF;
    bytes[1] = (value >> 8) & 0xFF;
    file_.write(reinterpret_cast<const char*>(bytes), 2);
    if (crc_active_) crc_.update(bytes, 2);
    bytes_written_ += 2;
}

void BinaryWriter::write_u32(u32 value) {
    u8 bytes[4];
    bytes[0] = value & 0xFF;
    bytes[1] = (value >> 8) & 0xFF;
    bytes[2] = (value >> 16) & 0xFF;
    bytes[3] = (value >> 24) & 0xFF;
    file_.write(reinterpret_cast<const char*>(bytes), 4);
    if (crc_active_) crc_.update(bytes, 4);
    bytes_written_ += 4;
}

void BinaryWriter::write_u64(u64 value) {
    u8 bytes[8];
    for (int i = 0; i < 8; ++i) {
        bytes[i] = (value >> (i * 8)) & 0xFF;
    }
    file_.write(reinterpret_cast<const char*>(bytes), 8);
    if (crc_active_) crc_.update(bytes, 8);
    bytes_written_ += 8;
}

void BinaryWriter::write_i32(i32 value) {
    write_u32(static_cast<u32>(value));
}

void BinaryWriter::write_i64(i64 value) {
    write_u64(static_cast<u64>(value));
}

void BinaryWriter::write_f32(f32 value) {
    u32 bits;
    std::memcpy(&bits, &value, sizeof(f32));
    write_u32(bits);
}

void BinaryWriter::write_f64(f64 value) {
    u64 bits;
    std::memcpy(&bits, &value, sizeof(f64));
    write_u64(bits);
}

void BinaryWriter::write_bytes(const void* data, usize len) {
    file_.write(static_cast<const char*>(data), len);
    if (crc_active_) crc_.update(data, len);
    bytes_written_ += len;
}

void BinaryWriter::write_string(const std::string& str) {
    write_u32(static_cast<u32>(str.size()));
    if (!str.empty()) {
        write_bytes(str.data(), str.size());
    }
}

void BinaryWriter::write_f64_array(const std::vector<f64>& arr) {
    write_u64(arr.size());
    for (f64 v : arr) {
        write_f64(v);
    }
}

void BinaryWriter::write_u32_array(const std::vector<u32>& arr) {
    write_u64(arr.size());
    for (u32 v : arr) {
        write_u32(v);
    }
}

usize BinaryWriter::position() const {
    return static_cast<usize>(const_cast<std::ofstream&>(file_).tellp());
}

void BinaryWriter::seek(usize pos) {
    file_.seekp(pos);
}

void BinaryWriter::start_crc() {
    crc_.reset();
    crc_active_ = true;
}

u32 BinaryWriter::finish_crc() {
    crc_active_ = false;
    return crc_.finalize();
}

// =============================================================================
// BinaryReader Implementation
// =============================================================================

BinaryReader::BinaryReader(const std::string& path)
    : file_size_(0)
    , crc_active_(false)
{
    file_.open(path, std::ios::binary | std::ios::in);
    if (file_.is_open()) {
        file_.seekg(0, std::ios::end);
        file_size_ = static_cast<usize>(file_.tellg());
        file_.seekg(0, std::ios::beg);
    }
}

BinaryReader::~BinaryReader() {
    if (file_.is_open()) {
        file_.close();
    }
}

u8 BinaryReader::read_u8() {
    u8 value;
    file_.read(reinterpret_cast<char*>(&value), 1);
    if (crc_active_) crc_.update(value);
    return value;
}

u16 BinaryReader::read_u16() {
    u8 bytes[2];
    file_.read(reinterpret_cast<char*>(bytes), 2);
    if (crc_active_) crc_.update(bytes, 2);
    return static_cast<u16>(bytes[0]) | 
           (static_cast<u16>(bytes[1]) << 8);
}

u32 BinaryReader::read_u32() {
    u8 bytes[4];
    file_.read(reinterpret_cast<char*>(bytes), 4);
    if (crc_active_) crc_.update(bytes, 4);
    return static_cast<u32>(bytes[0]) |
           (static_cast<u32>(bytes[1]) << 8) |
           (static_cast<u32>(bytes[2]) << 16) |
           (static_cast<u32>(bytes[3]) << 24);
}

u64 BinaryReader::read_u64() {
    u8 bytes[8];
    file_.read(reinterpret_cast<char*>(bytes), 8);
    if (crc_active_) crc_.update(bytes, 8);
    u64 value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<u64>(bytes[i]) << (i * 8);
    }
    return value;
}

i32 BinaryReader::read_i32() {
    return static_cast<i32>(read_u32());
}

i64 BinaryReader::read_i64() {
    return static_cast<i64>(read_u64());
}

f32 BinaryReader::read_f32() {
    u32 bits = read_u32();
    f32 value;
    std::memcpy(&value, &bits, sizeof(f32));
    return value;
}

f64 BinaryReader::read_f64() {
    u64 bits = read_u64();
    f64 value;
    std::memcpy(&value, &bits, sizeof(f64));
    return value;
}

void BinaryReader::read_bytes(void* buffer, usize len) {
    file_.read(static_cast<char*>(buffer), len);
    if (crc_active_) crc_.update(buffer, len);
}

std::string BinaryReader::read_string() {
    u32 len = read_u32();
    if (len == 0) return "";
    std::string str(len, '\0');
    read_bytes(&str[0], len);
    return str;
}

std::vector<f64> BinaryReader::read_f64_array() {
    u64 size = read_u64();
    std::vector<f64> arr(size);
    for (u64 i = 0; i < size; ++i) {
        arr[i] = read_f64();
    }
    return arr;
}

std::vector<u32> BinaryReader::read_u32_array() {
    u64 size = read_u64();
    std::vector<u32> arr(size);
    for (u64 i = 0; i < size; ++i) {
        arr[i] = read_u32();
    }
    return arr;
}

usize BinaryReader::position() {
    return static_cast<usize>(file_.tellg());
}

void BinaryReader::seek(usize pos) {
    file_.seekg(pos);
}

bool BinaryReader::eof() const {
    return file_.eof();
}

void BinaryReader::start_crc() {
    crc_.reset();
    crc_active_ = true;
}

u32 BinaryReader::finish_crc() {
    crc_active_ = false;
    return crc_.finalize();
}

// =============================================================================
// SectionHeader Implementation
// =============================================================================

void SectionHeader::write(BinaryWriter& w) const {
    w.write_u32(static_cast<u32>(type));
    w.write_u64(offset);
    w.write_u64(size);
    w.write_u32(checksum);
}

void SectionHeader::read(BinaryReader& r) {
    type = static_cast<abf::SectionType>(r.read_u32());
    offset = r.read_u64();
    size = r.read_u64();
    checksum = r.read_u32();
}

// =============================================================================
// ABFHeader Implementation
// =============================================================================

bool ABFHeader::validate() const {
    if (magic != abf::MAGIC) return false;
    u32 major = (version >> 16) & 0xFFFF;
    if (major > abf::VERSION_MAJOR) return false;  // Incompatible version
    return true;
}

void ABFHeader::write(BinaryWriter& w) const {
    w.write_u64(magic);
    w.write_u32(version);
    w.write_u32(flags);
    w.write_u32(num_sections);
    w.write_u32(header_crc);
}

void ABFHeader::read(BinaryReader& r) {
    magic = r.read_u64();
    version = r.read_u32();
    flags = r.read_u32();
    num_sections = r.read_u32();
    header_crc = r.read_u32();
}

// =============================================================================
// ResultSerializer Implementation
// =============================================================================

ResultSerializer::ResultSerializer() = default;

void ResultSerializer::set_metadata(const std::string& key, const std::string& value) {
    // Update if exists
    for (auto& pair : metadata_) {
        if (pair.first == key) {
            pair.second = value;
            return;
        }
    }
    // Add new
    metadata_.emplace_back(key, value);
}

std::string ResultSerializer::get_metadata(const std::string& key) const {
    for (const auto& pair : metadata_) {
        if (pair.first == key) {
            return pair.second;
        }
    }
    return "";
}

void ResultSerializer::write_metadata_section(BinaryWriter& w) {
    w.write_u32(static_cast<u32>(metadata_.size()));
    for (const auto& pair : metadata_) {
        w.write_string(pair.first);
        w.write_string(pair.second);
    }
}

void ResultSerializer::write_config_section(BinaryWriter& w, 
                                            const analysis::BatchConfig& config) {
    w.write_u64(config.master_seed);
    w.write_u32(config.num_iterations);
    w.write_u64(config.max_ticks_per_iteration);
    w.write_u8(config.stop_on_decisive ? 1 : 0);
    w.write_f64(config.decisive_threshold);
    w.write_u32(config.thread_count);
    w.write_u64(config.entity_capacity);
}

void ResultSerializer::write_iterations_section(BinaryWriter& w,
    const std::vector<analysis::IterationResult>& results) {
    
    w.write_u64(results.size());
    
    for (const auto& r : results) {
        w.write_u32(r.iteration_id);
        w.write_u64(r.seed);
        w.write_u64(r.ticks_executed);
        w.write_u8(r.completed_normally ? 1 : 0);
        w.write_u32(r.blue_surviving);
        w.write_u32(r.red_surviving);
        w.write_u32(r.neutral_surviving);
        w.write_f64(r.blue_total_health);
        w.write_f64(r.red_total_health);
        w.write_f64(r.blue_avg_supply);
        w.write_f64(r.red_avg_supply);
        w.write_f64(r.blue_avg_morale);
        w.write_f64(r.red_avg_morale);
        w.write_u64(r.total_engagements);
        w.write_f64(r.total_blue_damage);
        w.write_f64(r.total_red_damage);
        w.write_u64(r.time_to_first_casualty);
        w.write_u64(r.time_to_decisive);
    }
}

void ResultSerializer::write_statistics_section(BinaryWriter& w,
    const analysis::BatchStatistics& stats) {
    
    w.write_u32(stats.total_iterations);
    w.write_u32(stats.completed_iterations);
    w.write_u32(stats.failed_iterations);
    w.write_u32(stats.blue_wins);
    w.write_u32(stats.red_wins);
    w.write_u32(stats.draws);
    
    // Helper to write MetricStats
    auto write_metric = [&w](const analysis::BatchStatistics::MetricStats& m) {
        w.write_f64(m.mean);
        w.write_f64(m.stddev);
        w.write_f64(m.min);
        w.write_f64(m.max);
        w.write_f64(m.median);
        w.write_f64(m.p5);
        w.write_f64(m.p95);
    };
    
    write_metric(stats.ticks_to_completion);
    write_metric(stats.blue_survival_rate);
    write_metric(stats.red_survival_rate);
    write_metric(stats.blue_final_health);
    write_metric(stats.red_final_health);
    
    // Parameter correlations
    w.write_u64(stats.parameter_correlations.size());
    for (const auto& pc : stats.parameter_correlations) {
        w.write_string(pc.first);
        w.write_f64(pc.second);
    }
}

Result<void> ResultSerializer::write(const std::string& path,
                                     const analysis::BatchConfig& config,
                                     const std::vector<analysis::IterationResult>& results,
                                     const analysis::BatchStatistics& stats) {
    BinaryWriter w(path);
    if (!w.is_open()) {
        last_error_ = "Failed to open file for writing: " + path;
        return Error(ErrorCode::IO_ERROR, last_error_);
    }
    
    // Prepare sections
    std::vector<SectionHeader> sections;
    sections.resize(4);  // metadata, config, iterations, statistics
    
    // Write header placeholder (will update later)
    ABFHeader header;
    header.num_sections = 4;
    usize header_pos = w.position();
    header.write(w);
    
    // Write section index placeholder
    usize section_index_pos = w.position();
    for (auto& sec : sections) {
        sec.write(w);
    }
    
    // Section 0: Metadata
    sections[0].type = abf::SectionType::METADATA;
    sections[0].offset = w.position();
    w.start_crc();
    write_metadata_section(w);
    sections[0].checksum = w.finish_crc();
    sections[0].size = w.position() - sections[0].offset;
    
    // Section 1: Config
    sections[1].type = abf::SectionType::CONFIG;
    sections[1].offset = w.position();
    w.start_crc();
    write_config_section(w, config);
    sections[1].checksum = w.finish_crc();
    sections[1].size = w.position() - sections[1].offset;
    
    // Section 2: Iterations
    sections[2].type = abf::SectionType::ITERATIONS;
    sections[2].offset = w.position();
    w.start_crc();
    write_iterations_section(w, results);
    sections[2].checksum = w.finish_crc();
    sections[2].size = w.position() - sections[2].offset;
    
    // Section 3: Statistics
    sections[3].type = abf::SectionType::STATISTICS;
    sections[3].offset = w.position();
    w.start_crc();
    write_statistics_section(w, stats);
    sections[3].checksum = w.finish_crc();
    sections[3].size = w.position() - sections[3].offset;
    
    // Calculate header CRC
    // We need to rewrite the header with correct CRC
    // CRC covers: magic + version + flags + num_sections
    u8 header_bytes[20];
    std::memcpy(header_bytes, &header.magic, 8);
    std::memcpy(header_bytes + 8, &header.version, 4);
    std::memcpy(header_bytes + 12, &header.flags, 4);
    std::memcpy(header_bytes + 16, &header.num_sections, 4);
    header.header_crc = CRC32::compute(header_bytes, 20);
    
    // Rewrite header
    w.seek(header_pos);
    header.write(w);
    
    // Rewrite section index
    w.seek(section_index_pos);
    for (const auto& sec : sections) {
        sec.write(w);
    }
    
    return Result<void>();
}

void ResultSerializer::read_metadata_section(BinaryReader& r, u64 size) {
    u32 count = r.read_u32();
    metadata_.clear();
    metadata_.reserve(count);
    for (u32 i = 0; i < count; ++i) {
        std::string key = r.read_string();
        std::string value = r.read_string();
        metadata_.emplace_back(key, value);
    }
}

void ResultSerializer::read_config_section(BinaryReader& r, 
                                           analysis::BatchConfig& config) {
    config.master_seed = r.read_u64();
    config.num_iterations = r.read_u32();
    config.max_ticks_per_iteration = r.read_u64();
    config.stop_on_decisive = (r.read_u8() != 0);
    config.decisive_threshold = r.read_f64();
    config.thread_count = r.read_u32();
    config.entity_capacity = r.read_u64();
}

void ResultSerializer::read_iterations_section(BinaryReader& r,
    std::vector<analysis::IterationResult>& results, u64 size) {
    
    u64 count = r.read_u64();
    results.clear();
    results.reserve(count);
    
    for (u64 i = 0; i < count; ++i) {
        analysis::IterationResult result;
        result.iteration_id = r.read_u32();
        result.seed = r.read_u64();
        result.ticks_executed = r.read_u64();
        result.completed_normally = (r.read_u8() != 0);
        result.blue_surviving = r.read_u32();
        result.red_surviving = r.read_u32();
        result.neutral_surviving = r.read_u32();
        result.blue_total_health = r.read_f64();
        result.red_total_health = r.read_f64();
        result.blue_avg_supply = r.read_f64();
        result.red_avg_supply = r.read_f64();
        result.blue_avg_morale = r.read_f64();
        result.red_avg_morale = r.read_f64();
        result.total_engagements = r.read_u64();
        result.total_blue_damage = r.read_f64();
        result.total_red_damage = r.read_f64();
        result.time_to_first_casualty = r.read_u64();
        result.time_to_decisive = r.read_u64();
        results.push_back(result);
    }
}

void ResultSerializer::read_statistics_section(BinaryReader& r,
    analysis::BatchStatistics& stats) {
    
    stats.total_iterations = r.read_u32();
    stats.completed_iterations = r.read_u32();
    stats.failed_iterations = r.read_u32();
    stats.blue_wins = r.read_u32();
    stats.red_wins = r.read_u32();
    stats.draws = r.read_u32();
    
    // Helper to read MetricStats
    auto read_metric = [&r]() -> analysis::BatchStatistics::MetricStats {
        analysis::BatchStatistics::MetricStats m;
        m.mean = r.read_f64();
        m.stddev = r.read_f64();
        m.min = r.read_f64();
        m.max = r.read_f64();
        m.median = r.read_f64();
        m.p5 = r.read_f64();
        m.p95 = r.read_f64();
        return m;
    };
    
    stats.ticks_to_completion = read_metric();
    stats.blue_survival_rate = read_metric();
    stats.red_survival_rate = read_metric();
    stats.blue_final_health = read_metric();
    stats.red_final_health = read_metric();
    
    // Parameter correlations
    u64 corr_count = r.read_u64();
    stats.parameter_correlations.clear();
    stats.parameter_correlations.reserve(corr_count);
    for (u64 i = 0; i < corr_count; ++i) {
        std::string name = r.read_string();
        f64 value = r.read_f64();
        stats.parameter_correlations.emplace_back(name, value);
    }
}

Result<void> ResultSerializer::read(const std::string& path,
                                    analysis::BatchConfig& config,
                                    std::vector<analysis::IterationResult>& results,
                                    analysis::BatchStatistics& stats) {
    BinaryReader r(path);
    if (!r.is_open()) {
        last_error_ = "Failed to open file for reading: " + path;
        return Error(ErrorCode::IO_ERROR, last_error_);
    }
    
    // Read and validate header
    ABFHeader header;
    header.read(r);
    
    if (!header.validate()) {
        last_error_ = "Invalid ABF file header";
        return Error(ErrorCode::INVALID_FORMAT, last_error_);
    }
    
    // Read section index
    std::vector<SectionHeader> sections(header.num_sections);
    for (u32 i = 0; i < header.num_sections; ++i) {
        sections[i].read(r);
    }
    
    // Read each section
    for (const auto& sec : sections) {
        r.seek(sec.offset);
        
        switch (sec.type) {
            case abf::SectionType::METADATA:
                read_metadata_section(r, sec.size);
                break;
            case abf::SectionType::CONFIG:
                read_config_section(r, config);
                break;
            case abf::SectionType::ITERATIONS:
                read_iterations_section(r, results, sec.size);
                break;
            case abf::SectionType::STATISTICS:
                read_statistics_section(r, stats);
                break;
            default:
                // Skip unknown sections
                break;
        }
    }
    
    return Result<void>();
}

Result<bool> ResultSerializer::validate_file(const std::string& path) {
    BinaryReader r(path);
    if (!r.is_open()) {
        return Error(ErrorCode::IO_ERROR, "Failed to open file: " + path);
    }
    
    // Read and validate header
    ABFHeader header;
    header.read(r);
    
    if (!header.validate()) {
        return false;
    }
    
    // Verify header CRC
    r.seek(0);
    u8 header_bytes[20];
    r.read_bytes(header_bytes, 20);
    u32 expected_crc = CRC32::compute(header_bytes, 20);
    if (expected_crc != header.header_crc) {
        return false;
    }
    
    // Read and verify each section
    r.seek(24);  // After header
    std::vector<SectionHeader> sections(header.num_sections);
    for (u32 i = 0; i < header.num_sections; ++i) {
        sections[i].read(r);
    }
    
    for (const auto& sec : sections) {
        if (sec.offset + sec.size > r.file_size()) {
            return false;  // Section extends beyond file
        }
        
        // Verify section CRC
        r.seek(sec.offset);
        r.start_crc();
        
        // Read section data
        std::vector<u8> buffer(sec.size);
        r.read_bytes(buffer.data(), sec.size);
        
        u32 actual_crc = r.finish_crc();
        if (actual_crc != sec.checksum) {
            return false;
        }
    }
    
    return true;
}

// =============================================================================
// Convenience Functions
// =============================================================================

Result<void> save_results_abf(const std::string& path,
                              const analysis::BatchConfig& config,
                              const std::vector<analysis::IterationResult>& results,
                              const analysis::BatchStatistics& stats) {
    ResultSerializer serializer;
    return serializer.write(path, config, results, stats);
}

Result<void> load_results_abf(const std::string& path,
                              analysis::BatchConfig& config,
                              std::vector<analysis::IterationResult>& results,
                              analysis::BatchStatistics& stats) {
    ResultSerializer serializer;
    return serializer.read(path, config, results, stats);
}

Result<bool> validate_abf(const std::string& path) {
    return ResultSerializer::validate_file(path);
}

}  // namespace athena
