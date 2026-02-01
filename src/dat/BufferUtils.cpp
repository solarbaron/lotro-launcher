/**
 * @file BufferUtils.cpp
 * @brief Implementation of binary data reading utilities
 */

#include "BufferUtils.hpp"
#include <cstring>

namespace lotro::dat {

uint8_t BufferUtils::readUInt8(const char*& data) {
    return static_cast<uint8_t>(*data++);
}

uint16_t BufferUtils::readUInt16(const char*& data) {
    uint16_t low = static_cast<uint8_t>(*data++);
    uint16_t high = static_cast<uint8_t>(*data++);
    return (high << 8) | low;
}

uint32_t BufferUtils::readUInt32(const char*& data) {
    uint32_t b0 = static_cast<uint8_t>(*data++);
    uint32_t b1 = static_cast<uint8_t>(*data++);
    uint32_t b2 = static_cast<uint8_t>(*data++);
    uint32_t b3 = static_cast<uint8_t>(*data++);
    return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

uint64_t BufferUtils::readUInt32AsLong(const char*& data) {
    return static_cast<uint64_t>(readUInt32(data));
}

float BufferUtils::readFloat(const char*& data) {
    uint32_t bits = readUInt32(data);
    float result;
    std::memcpy(&result, &bits, sizeof(float));
    return result;
}

double BufferUtils::readDouble(const char*& data) {
    uint64_t bits = readUInt64(data);
    double result;
    std::memcpy(&result, &bits, sizeof(double));
    return result;
}

uint64_t BufferUtils::readUInt64(const char*& data) {
    uint64_t b0 = static_cast<uint8_t>(*data++);
    uint64_t b1 = static_cast<uint8_t>(*data++);
    uint64_t b2 = static_cast<uint8_t>(*data++);
    uint64_t b3 = static_cast<uint8_t>(*data++);
    uint64_t b4 = static_cast<uint8_t>(*data++);
    uint64_t b5 = static_cast<uint8_t>(*data++);
    uint64_t b6 = static_cast<uint8_t>(*data++);
    uint64_t b7 = static_cast<uint8_t>(*data++);
    return (b7 << 56) | (b6 << 48) | (b5 << 40) | (b4 << 32) | 
           (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

int BufferUtils::readVle(const char*& data) {
    int a = static_cast<uint8_t>(*data++);
    
    if (a == 0xE0) {
        // Full 4-byte value follows
        return static_cast<int>(readUInt32(data));
    }
    
    if ((a & 0x80) == 0) {
        // Single byte value
        return a;
    }
    
    int b = static_cast<uint8_t>(*data++);
    
    if ((a & 0x40) == 0) {
        // Two-byte value
        return b | ((a & 0x7F) << 8);
    }
    
    // Four-byte value with 6-bit prefix
    int c = readUInt16(data);
    return ((a & 0x3F) << 24) | (b << 16) | c;
}

int BufferUtils::readTSize(const char*& data) {
    // Skip one byte, then read VLE
    data++;
    return readVle(data);
}

QString BufferUtils::readPascalString(const char*& data) {
    int length = readVle(data);
    if (length <= 0 || length > 10000) {
        return QString();
    }
    
    // Read as ISO-8859-1 (Latin-1)
    QString result = QString::fromLatin1(data, length);
    data += length;
    return result;
}

QString BufferUtils::readPrefixedUtf16String(const char*& data) {
    uint32_t len = readUInt32(data);
    if (len == 0) return QString();
    if (len > 100000) return QString(); // Safety cap
    
    // Read UTF-16 data (Little Endian)
    // QString::fromUtf16 expects uint16_t* (native endian usually, but DAT is LE and x86 is LE)
    const char16_t* utf16Data = reinterpret_cast<const char16_t*>(data);
    QString str = QString::fromUtf16(utf16Data, len);
    
    data += len * 2;
    return str;
}

bool BufferUtils::readBoolean(const char*& data) {
    return readUInt8(data) != 0;
}

void BufferUtils::skip(const char*& data, size_t count) {
    data += count;
}

uint32_t BufferUtils::getDoubleWordAt(const char* buffer, size_t offset) {
    const char* ptr = buffer + offset;
    uint32_t b0 = static_cast<uint8_t>(ptr[0]);
    uint32_t b1 = static_cast<uint8_t>(ptr[1]);
    uint32_t b2 = static_cast<uint8_t>(ptr[2]);
    uint32_t b3 = static_cast<uint8_t>(ptr[3]);
    return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

uint64_t BufferUtils::getDoubleWordAtAsLong(const char* buffer, size_t offset) {
    return static_cast<uint64_t>(getDoubleWordAt(buffer, offset));
}

uint16_t BufferUtils::getWordAt(const char* buffer, size_t offset) {
    const char* ptr = buffer + offset;
    uint16_t low = static_cast<uint8_t>(ptr[0]);
    uint16_t high = static_cast<uint8_t>(ptr[1]);
    return (high << 8) | low;
}

} // namespace lotro::dat
