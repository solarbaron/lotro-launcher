/**
 * @file DatArchive.hpp
 * @brief DAT archive reader for LOTRO game files
 * 
 * Provides read access to LOTRO's proprietary DAT file format.
 */

#pragma once

#include "DirectoryEntry.hpp"
#include "FileEntry.hpp"

#include <QFile>
#include <QString>
#include <QByteArray>
#include <QMap>
#include <memory>

namespace lotro::dat {

/**
 * @class DatArchive
 * @brief Reads files from a LOTRO DAT archive
 * 
 * DAT archives are binary files that store game data using a B-tree
 * structure for efficient lookup by data ID.
 */
class DatArchive {
public:
    explicit DatArchive(const QString& path);
    ~DatArchive();
    
    /**
     * @brief Open the DAT archive for reading
     * @return true if successful
     */
    bool open();
    
    /**
     * @brief Close the archive
     */
    void close();
    
    /**
     * @brief Check if the archive is open
     */
    bool isOpen() const { return m_file.isOpen(); }
    
    /**
     * @brief Load data by file ID
     * @param fileId The data ID to load
     * @return The raw data, or empty array if not found
     */
    QByteArray loadData(uint64_t fileId);
    
    /**
     * @brief Get the input file path
     */
    QString filePath() const { return m_path; }
    
private:
    // Read the superblock (header) and return root node offset
    int readSuperBlock();
    
    // Read a directory entry from the given offset
    void readDirectory(DirectoryEntryPtr entry);
    
    // Ensure a directory is loaded
    void ensureLoaded(DirectoryEntryPtr entry);
    
    // Find a file entry by ID using binary search
    FileEntry* getFileById(DirectoryEntryPtr dir, uint64_t fileId);
    
    // Read a block of data
    QByteArray readBlockAt(uint64_t offset, int blockSize, int size);
    QByteArray readOldBlockAt(uint64_t offset, int blockSize, int totalSize);
    
    // Load an entry's data
    QByteArray loadEntry(const FileEntry& entry);
    
private:
    QString m_path;
    QFile m_file;
    int m_blockSize = 0;
    int m_datPackVersion = 0;
    DirectoryEntryPtr m_rootEntry;
    QMap<uint64_t, DirectoryEntryPtr> m_loadedDirs;
    
    static constexpr int MAGIC = 21570;
    static constexpr int MAX_ENTRIES = 61;
    static constexpr int ENTRY_RAW_SIZE = 32;
    static constexpr int POINTER_RAW_SIZE = 8;
    static constexpr int DIRECTORY_RAW_SIZE = 2452;
    static constexpr int BASE_FILE_ENTRIES_OFFSET = 496;
};

} // namespace lotro::dat
