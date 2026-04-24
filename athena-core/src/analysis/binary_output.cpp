/**
 * @file binary_output.cpp
 * @brief Binary serialization implementation
 * 
 * Implements compact binary format for simulation results.
 * Uses CRC32 for data integrity verification.
 */

#include "athena/analysis/binary_output.hpp"
#include <cstring>
#include <ctime>

namespace athena {
namespace analysis {

// =============================================================================
// CRC32 Implementation (IEEE polynomial)
// =============================================================================

namespace {

// CRC32 lookup table (IEEE polynomial 0xEDB88320)
constexpr u32 CRC32_TABLE[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

u32 calc_crc32(const void* data, size_t size, u32 crc = 0xFFFFFFFF) {
    const u8* bytes = static_cast<const u8*>(data);
    for (size_t i = 0; i < size; ++i) {
        crc = CRC32_TABLE[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

}  // anonymous namespace

// =============================================================================
// BinaryWriter Implementation
// =============================================================================

BinaryWriter::~BinaryWriter() {
    if (file_) {
        close();
    }
}

bool BinaryWriter::open(const std::string& path) {
    if (file_) {
        error_ = "File already open";
        return false;
    }
    
    file_ = std::fopen(path.c_str(), "wb");
    if (!file_) {
        error_ = "Failed to open file for writing: " + path;
        return false;
    }
    
    bytes_written_ = 0;
    error_.clear();
    return true;
}

bool BinaryWriter::close() {
    if (!file_) {
        return true;
    }
    
    bool ok = (std::fclose(file_) == 0);
    file_ = nullptr;
    return ok;
}

bool BinaryWriter::write_raw(const void* data, size_t size) {
    if (!file_) return false;
    size_t written = std::fwrite(data, 1, size, file_);
    if (written != size) {
        error_ = "Write failed";
        return false;
    }
    bytes_written_ += size;
    return true;
}

bool BinaryWriter::write_string(const std::string& str) {
    u16 len = static_cast<u16>(str.size());
    if (!write_raw(&len, sizeof(len))) return false;
    if (len > 0) {
        if (!write_raw(str.data(), len)) return false;
    }
    return true;
}

u32 BinaryWriter::compute_crc(const void* data, size_t size) const {
    return calc_crc32(data, size);
}

bool BinaryWriter::write(const BinaryMetadata& metadata,
                         const BinaryStatistics& stats,
                         const std::vector<BinaryIteration>& iterations) {
    if (!file_) {
        error_ = "File not open";
        return false;
    }
    
    // Calculate total size
    size_t metadata_size = 2 + metadata.scenario_name.size() + 8 + 8 + 4 + 4;
    size_t iterations_size = iterations.size() * sizeof(BinaryIteration);
    u64 total_size = sizeof(BinaryHeader) + metadata_size + sizeof(BinaryStatistics) + iterations_size;
    
    // Build header
    BinaryHeader header;
    header.magic = BINARY_MAGIC;
    header.version = BINARY_VERSION;
    header.flags = FLAG_LITTLE_ENDIAN;
    header.total_size = total_size;
    header.iteration_count = static_cast<u32>(iterations.size());
    
    // Calculate data CRC (everything after header)
    u32 data_crc = 0xFFFFFFFF;
    
    // CRC of metadata
    u16 name_len = static_cast<u16>(metadata.scenario_name.size());
    data_crc = calc_crc32(&name_len, sizeof(name_len), data_crc ^ 0xFFFFFFFF) ^ 0xFFFFFFFF;
    if (name_len > 0) {
        data_crc = calc_crc32(metadata.scenario_name.data(), name_len, data_crc ^ 0xFFFFFFFF) ^ 0xFFFFFFFF;
    }
    data_crc = calc_crc32(&metadata.timestamp, sizeof(metadata.timestamp), data_crc ^ 0xFFFFFFFF) ^ 0xFFFFFFFF;
    data_crc = calc_crc32(&metadata.master_seed, sizeof(metadata.master_seed), data_crc ^ 0xFFFFFFFF) ^ 0xFFFFFFFF;
    data_crc = calc_crc32(&metadata.initial_blue, sizeof(metadata.initial_blue), data_crc ^ 0xFFFFFFFF) ^ 0xFFFFFFFF;
    data_crc = calc_crc32(&metadata.initial_red, sizeof(metadata.initial_red), data_crc ^ 0xFFFFFFFF) ^ 0xFFFFFFFF;
    
    // CRC of statistics
    data_crc = calc_crc32(&stats, sizeof(stats), data_crc ^ 0xFFFFFFFF) ^ 0xFFFFFFFF;
    
    // CRC of iterations
    if (!iterations.empty()) {
        data_crc = calc_crc32(iterations.data(), iterations_size, data_crc ^ 0xFFFFFFFF) ^ 0xFFFFFFFF;
    }
    
    header.data_crc = data_crc;
    
    // Calculate header CRC (excluding the crc field itself)
    u32 header_crc = calc_crc32(&header.magic, 8);  // magic + version + flags
    header_crc = calc_crc32(&header.data_crc, 20, header_crc ^ 0xFFFFFFFF) ^ 0xFFFFFFFF;  // rest of header
    header.header_crc = header_crc;
    
    // Write header
    if (!write_raw(&header, sizeof(header))) return false;
    
    // Write metadata
    if (!write_string(metadata.scenario_name)) return false;
    if (!write_raw(&metadata.timestamp, sizeof(metadata.timestamp))) return false;
    if (!write_raw(&metadata.master_seed, sizeof(metadata.master_seed))) return false;
    if (!write_raw(&metadata.initial_blue, sizeof(metadata.initial_blue))) return false;
    if (!write_raw(&metadata.initial_red, sizeof(metadata.initial_red))) return false;
    
    // Write statistics
    if (!write_raw(&stats, sizeof(stats))) return false;
    
    // Write iterations
    if (!iterations.empty()) {
        if (!write_raw(iterations.data(), iterations_size)) return false;
    }
    
    return true;
}

// =============================================================================
// BinaryReader Implementation
// =============================================================================

BinaryReader::~BinaryReader() {
    close();
}

bool BinaryReader::open(const std::string& path) {
    if (file_) {
        close();
    }
    
    file_ = std::fopen(path.c_str(), "rb");
    if (!file_) {
        error_ = "Failed to open file: " + path;
        return false;
    }
    
    // Read and validate header
    if (!read_header(header_)) {
        close();
        return false;
    }
    
    error_.clear();
    return true;
}

void BinaryReader::close() {
    if (file_) {
        std::fclose(file_);
        file_ = nullptr;
    }
    header_ = BinaryHeader{};
    iterations_offset_ = 0;
}

bool BinaryReader::read_raw(void* data, size_t size) {
    if (!file_) return false;
    size_t read = std::fread(data, 1, size, file_);
    return read == size;
}

bool BinaryReader::read_string(std::string& str) {
    u16 len;
    if (!read_raw(&len, sizeof(len))) return false;
    str.resize(len);
    if (len > 0) {
        if (!read_raw(&str[0], len)) return false;
    }
    return true;
}

u32 BinaryReader::compute_crc(const void* data, size_t size) const {
    return calc_crc32(data, size);
}

bool BinaryReader::read_header(BinaryHeader& header) {
    std::rewind(file_);
    
    if (!read_raw(&header, sizeof(header))) {
        error_ = "Failed to read header";
        return false;
    }
    
    // Validate magic
    if (header.magic != BINARY_MAGIC) {
        error_ = "Invalid file format (bad magic)";
        return false;
    }
    
    // Check version compatibility
    u16 major = header.version >> 8;
    if (major > (BINARY_VERSION >> 8)) {
        error_ = "Unsupported file version";
        return false;
    }
    
    return true;
}

bool BinaryReader::verify() {
    if (!file_) {
        error_ = "File not open";
        return false;
    }
    
    // Verify header CRC
    u32 computed_header_crc = calc_crc32(&header_.magic, 8);
    computed_header_crc = calc_crc32(&header_.data_crc, 20, computed_header_crc ^ 0xFFFFFFFF) ^ 0xFFFFFFFF;
    
    if (computed_header_crc != header_.header_crc) {
        error_ = "Header CRC mismatch";
        return false;
    }
    
    // TODO: Verify data CRC (requires reading entire file)
    
    return true;
}

bool BinaryReader::read(BinaryResults& results) {
    if (!file_) {
        error_ = "File not open";
        return false;
    }
    
    // Seek past header
    std::fseek(file_, sizeof(BinaryHeader), SEEK_SET);
    
    results.header = header_;
    
    // Read metadata
    if (!read_string(results.metadata.scenario_name)) {
        error_ = "Failed to read metadata";
        return false;
    }
    if (!read_raw(&results.metadata.timestamp, sizeof(results.metadata.timestamp))) return false;
    if (!read_raw(&results.metadata.master_seed, sizeof(results.metadata.master_seed))) return false;
    if (!read_raw(&results.metadata.initial_blue, sizeof(results.metadata.initial_blue))) return false;
    if (!read_raw(&results.metadata.initial_red, sizeof(results.metadata.initial_red))) return false;
    
    // Read statistics
    if (!read_raw(&results.statistics, sizeof(results.statistics))) {
        error_ = "Failed to read statistics";
        return false;
    }
    
    // Store iterations offset for random access
    iterations_offset_ = std::ftell(file_);
    
    // Read iterations
    results.iterations.resize(header_.iteration_count);
    if (header_.iteration_count > 0) {
        size_t iter_size = header_.iteration_count * sizeof(BinaryIteration);
        if (!read_raw(results.iterations.data(), iter_size)) {
            error_ = "Failed to read iterations";
            return false;
        }
    }
    
    results.valid = true;
    return true;
}

bool BinaryReader::read_iteration(u32 index, BinaryIteration& iter) {
    if (!file_) {
        error_ = "File not open";
        return false;
    }
    
    if (index >= header_.iteration_count) {
        error_ = "Iteration index out of range";
        return false;
    }
    
    if (iterations_offset_ == 0) {
        // Need to find iterations offset first
        BinaryResults temp;
        if (!read(temp)) return false;
    }
    
    long offset = iterations_offset_ + (index * sizeof(BinaryIteration));
    std::fseek(file_, offset, SEEK_SET);
    
    return read_raw(&iter, sizeof(iter));
}

// =============================================================================
// Utility Functions
// =============================================================================

bool is_athena_binary(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    
    u32 magic;
    bool ok = (std::fread(&magic, sizeof(magic), 1, f) == 1);
    std::fclose(f);
    
    return ok && (magic == BINARY_MAGIC);
}

}  // namespace analysis
}  // namespace athena
