/**
 * @file DirectoryEntry.hpp
 * @brief Represents a directory entry in the DAT archive B-tree
 */

#pragma once

#include "FileEntry.hpp"
#include <QVector>
#include <memory>
#include <cstdint>

namespace lotro::dat {

class DirectoryEntry;
using DirectoryEntryPtr = std::shared_ptr<DirectoryEntry>;

/**
 * @class DirectoryEntry
 * @brief A node in the DAT archive B-tree structure
 * 
 * DAT archives use a B-tree structure for efficient file lookup.
 * Each directory entry can contain up to 61 file entries and
 * pointers to child directory entries.
 */
class DirectoryEntry {
public:
    DirectoryEntry(DirectoryEntry* parent, uint64_t offset, int blockSize)
        : m_parent(parent)
        , m_offset(offset)
        , m_blockSize(blockSize)
    {}
    
    DirectoryEntry* parent() const { return m_parent; }
    uint64_t offset() const { return m_offset; }
    int blockSize() const { return m_blockSize; }
    
    const QVector<FileEntry>& files() const { return m_files; }
    const QVector<DirectoryEntryPtr>& directories() const { return m_directories; }
    
    void addFile(const FileEntry& entry) { m_files.append(entry); }
    void addDirectory(DirectoryEntryPtr dir) { m_directories.append(dir); }
    
private:
    DirectoryEntry* m_parent = nullptr;
    uint64_t m_offset = 0;
    int m_blockSize = 0;
    QVector<FileEntry> m_files;
    QVector<DirectoryEntryPtr> m_directories;
};

} // namespace lotro::dat
