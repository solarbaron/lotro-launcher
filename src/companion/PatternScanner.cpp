/**
 * LOTRO Launcher - Pattern Scanner Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "PatternScanner.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>

namespace lotro {

BytePattern BytePattern::fromString(const std::string& pattern) {
    BytePattern result;
    size_t i = 0;
    while (i < pattern.length()) {
        if (pattern[i] == '?') {
            // Wildcard: ?n
            // n is digit, skips n+1 bytes (e.g. ?3 -> 4 bytes)
            // lotro-memory BytesPattern.java: holeSize = input.charAt(qmIndex + 1) - 48 + 1;
            if (i + 1 < pattern.length() && isdigit(pattern[i+1])) {
                int n = pattern[i+1] - '0';
                int holeSize = n + 1;
                for (int j = 0; j < holeSize; ++j) {
                    result.entries.push_back({0, true});
                }
                i += 2;
            } else {
                // Invalid or simple ? (treat as 1 byte wildcard I guess?)
                result.entries.push_back({0, true});
                i++;
            }
        } else {
            // Hex byte
            if (i + 1 < pattern.length() && isxdigit(pattern[i]) && isxdigit(pattern[i+1])) {
                std::string byteStr = pattern.substr(i, 2);
                uint8_t byte = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
                result.entries.push_back({byte, false});
                i += 2;
            } else {
                // Skip invalid char
                i++;
            }
        }
    }
    return result;
}

std::optional<size_t> PatternScanner::find(const std::vector<uint8_t>& data, const BytePattern& pattern) {
    if (pattern.entries.empty() || data.size() < pattern.entries.size()) {
        return std::nullopt;
    }
    
    for (size_t i = 0; i <= data.size() - pattern.entries.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < pattern.entries.size(); ++j) {
            const auto& entry = pattern.entries[j];
            if (!entry.isWildcard && data[i + j] != entry.byte) {
                match = false;
                break;
            }
        }
        
        if (match) {
            return i;
        }
    }
    
    return std::nullopt;
}

std::optional<size_t> PatternScanner::find(const std::vector<uint8_t>& data, const std::string& patternStr) {
    return find(data, BytePattern::fromString(patternStr));
}

} // namespace lotro
