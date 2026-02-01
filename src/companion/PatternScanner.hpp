/**
 * LOTRO Launcher - Pattern Scanner
 * 
 * Scans memory buffers for byte patterns with wildcards.
 * Compatible with lotro-companion pattern format (e.g. "4889...").
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace lotro {

/**
 * Byte pattern for scanning
 */
struct BytePattern {
    struct Entry {
        uint8_t byte = 0;
        bool isWildcard = false;
    };
    
    std::vector<Entry> entries;
    
    /**
     * Parse pattern from string
     * Supports:
     * - Hex bytes: "A0B1C2..."
     * - Wildcards: "?n" where n is a digit, skips n+1 bytes
     *   Example: "?3" skips 4 bytes
     */
    static BytePattern fromString(const std::string& pattern);
};

/**
 * Pattern scanner
 */
class PatternScanner {
public:
    /**
     * Scan buffer for pattern
     * @return Offset of match, or nullopt if not found
     */
    static std::optional<size_t> find(const std::vector<uint8_t>& data, const BytePattern& pattern);
    
    /**
     * Scan buffer for pattern string
     */
    static std::optional<size_t> find(const std::vector<uint8_t>& data, const std::string& patternStr);
};

} // namespace lotro
