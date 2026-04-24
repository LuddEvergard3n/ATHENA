// ATHENA Core - Types Implementation
//
// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 ATHENA Project

#include "athena/types.hpp"

namespace athena {

// =============================================================================
// Hash256 Implementation
// =============================================================================

std::string Hash256::to_hex() const {
    static constexpr char HEX_CHARS[] = "0123456789abcdef";
    
    std::string result;
    result.reserve(64);
    
    for (usize i = 0; i < 32; ++i) {
        result.push_back(HEX_CHARS[(bytes[i] >> 4) & 0x0F]);
        result.push_back(HEX_CHARS[bytes[i] & 0x0F]);
    }
    
    return result;
}

std::optional<Hash256> Hash256::from_hex(const std::string& hex) {
    if (hex.size() != 64) {
        return std::nullopt;
    }
    
    Hash256 result;
    
    for (usize i = 0; i < 32; ++i) {
        char high = hex[i * 2];
        char low = hex[i * 2 + 1];
        
        u8 high_val, low_val;
        
        if (high >= '0' && high <= '9') {
            high_val = static_cast<u8>(high - '0');
        } else if (high >= 'a' && high <= 'f') {
            high_val = static_cast<u8>(high - 'a' + 10);
        } else if (high >= 'A' && high <= 'F') {
            high_val = static_cast<u8>(high - 'A' + 10);
        } else {
            return std::nullopt;
        }
        
        if (low >= '0' && low <= '9') {
            low_val = static_cast<u8>(low - '0');
        } else if (low >= 'a' && low <= 'f') {
            low_val = static_cast<u8>(low - 'a' + 10);
        } else if (low >= 'A' && low <= 'F') {
            low_val = static_cast<u8>(low - 'A' + 10);
        } else {
            return std::nullopt;
        }
        
        result.bytes[i] = static_cast<u8>((high_val << 4) | low_val);
    }
    
    return result;
}

}  // namespace athena
