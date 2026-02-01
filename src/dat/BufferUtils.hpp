/**
 * @file BufferUtils.hpp
 * @brief Binary data reading utilities for LOTRO DAT files
 * 
 * Provides functions to read little-endian binary data from byte arrays,
 * matching the format used in LOTRO DAT files.
 */

#pragma once

#include <QByteArray>
#include <QString>
#include <cstdint>

namespace lotro::dat {

/**
 * @class BufferUtils
 * @brief Static utilities for reading binary data from byte arrays
 * 
 * All methods read data in little-endian format as used by LOTRO DAT files.
 */
class BufferUtils {
public:
    /**
     * @brief Read an unsigned 8-bit integer
     */
    static uint8_t readUInt8(const char*& data);
    
    /**
     * @brief Read an unsigned 16-bit integer (little-endian)
     */
    static uint16_t readUInt16(const char*& data);
    
    /**
     * @brief Read an unsigned 32-bit integer (little-endian)
     */
    static uint32_t readUInt32(const char*& data);
    
    /**
     * @brief Read an unsigned 32-bit integer as uint64_t (little-endian)
     */
    static uint64_t readUInt32AsLong(const char*& data);
    
    /**
     * @brief Read a 32-bit floating point value
     */
    static float readFloat(const char*& data);
    
    /**
     * @brief Read a 64-bit floating point value
     */
    static double readDouble(const char*& data);

    /**
     * @brief Read an unsigned 64-bit integer (little-endian)
     */
    static uint64_t readUInt64(const char*& data);
    
    /**
     * @brief Read a variable-length encoded integer
     * 
     * VLE format:
     * - If first byte < 0x80: 1-byte value
     * - If first byte has bit 7 set but not bit 6: 2-byte value
     * - If first byte has bits 7 and 6 set: 4-byte value
     * - If first byte == 0xE0: read full 4-byte value
     */
    static int readVle(const char*& data);
    
    /**
     * @brief Read a TSize value (skip 1 byte, then read VLE)
     */
    static int readTSize(const char*& data);
    
    /**
     * @brief Read a Pascal-style string (VLE length prefix, ISO-8859-1 encoded)
     */
    static QString readPascalString(const char*& data);
    
    /**
     * @brief Read a length-prefixed UTF-16 string (UInt32 length, then Unicode chars)
     */
    static QString readPrefixedUtf16String(const char*& data);
    
    /**
     * @brief Read a boolean value (1 byte)
     */
    static bool readBoolean(const char*& data);
    
    /**
     * @brief Skip a number of bytes
     */
    static void skip(const char*& data, size_t count);
    
    /**
     * @brief Get a 32-bit unsigned integer at a specific offset in a buffer
     */
    static uint32_t getDoubleWordAt(const char* buffer, size_t offset);
    
    /**
     * @brief Get a 32-bit unsigned integer as uint64_t at a specific offset
     */
    static uint64_t getDoubleWordAtAsLong(const char* buffer, size_t offset);
    
    /**
     * @brief Get a 16-bit unsigned integer at a specific offset
     */
    static uint16_t getWordAt(const char* buffer, size_t offset);
};

} // namespace lotro::dat
