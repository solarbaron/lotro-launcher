/**
 * @file FileEntry.hpp
 * @brief Represents a file entry in a DAT archive
 */

#pragma once

#include <cstdint>

namespace lotro::dat {

/**
 * @class FileEntry
 * @brief Metadata for a single file within a DAT archive
 */
class FileEntry {
public:
    FileEntry() = default;
    
    FileEntry(int index, uint64_t fileId, uint64_t fileOffset, int version,
              uint64_t timestamp, int size, int blockSize, int flags, int policy)
        : m_index(index)
        , m_fileId(fileId)
        , m_fileOffset(fileOffset)
        , m_version(version)
        , m_timestamp(timestamp)
        , m_size(size)
        , m_blockSize(blockSize)
        , m_flags(flags)
        , m_policy(policy)
    {}
    
    int index() const { return m_index; }
    uint64_t fileId() const { return m_fileId; }
    uint64_t fileOffset() const { return m_fileOffset; }
    int version() const { return m_version; }
    uint64_t timestamp() const { return m_timestamp; }
    int size() const { return m_size; }
    int blockSize() const { return m_blockSize; }
    int flags() const { return m_flags; }
    int policy() const { return m_policy; }
    
    /**
     * @brief Check if this entry is compressed
     * Flag bit 0x100 indicates compression
     */
    bool isCompressed() const { return (m_flags & 0x100) != 0; }
    
private:
    int m_index = 0;
    uint64_t m_fileId = 0;
    uint64_t m_fileOffset = 0;
    int m_version = 0;
    uint64_t m_timestamp = 0;
    int m_size = 0;
    int m_blockSize = 0;
    int m_flags = 0;
    int m_policy = 0;
};

} // namespace lotro::dat
