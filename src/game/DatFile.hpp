/**
 * DatFile - Native reader for Turbine .dat archive format
 * 
 * Based on reverse engineering by:
 * - Dancing On A Rock Hacker (DATUnpacker)
 * - jtauber/lotro (dat.py)
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QByteArray>
#include <QFile>
#include <QString>

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <vector>

namespace lotro {

/**
 * Entry for a file within a .dat archive
 */
struct DatFileEntry {
    uint32_t fileId;       // 8-char hex identifier
    uint32_t fileOffset;   // Offset within .dat file
    uint32_t size;         // File size
    uint32_t timestamp;    // Unix timestamp
    uint32_t version;      // Iteration version
    uint32_t compressedSize;
};

/**
 * Superblock information from .dat header
 */
struct DatSuperblock {
    uint32_t blockSize;
    uint32_t fileSize;
    uint32_t version;
    uint32_t version2;
    uint32_t freeHead;
    uint32_t freeTail;
    uint32_t freeSize;
    uint32_t directoryOffset;
};

/**
 * Native reader for Turbine .dat archive format
 * 
 * The .dat files are container archives with:
 * - Superblock at offset 0x100+ with metadata
 * - B-tree directory structure
 * - Individual files with compression support
 */
class DatFile {
public:
    /**
     * Open a .dat file for reading
     * @param path Path to the .dat file
     */
    explicit DatFile(const std::filesystem::path& path);
    ~DatFile();

    /**
     * Check if the file was opened successfully
     */
    bool isValid() const { return m_valid; }

    /**
     * Get the last error message
     */
    QString lastError() const { return m_lastError; }

    /**
     * Get superblock information
     */
    const DatSuperblock& superblock() const { return m_superblock; }

    /**
     * Get the current iteration version
     * This is used to determine if updates are needed
     */
    uint32_t currentVersion() const { return m_superblock.version; }

    /**
     * Get number of files in the archive
     */
    size_t fileCount() const { return m_fileEntries.size(); }

    /**
     * Get all file entries
     */
    const std::vector<DatFileEntry>& fileEntries() const { return m_fileEntries; }

    /**
     * Find a file entry by file ID
     * @param fileId The 8-char hex file identifier
     * @return Optional file entry if found
     */
    std::optional<DatFileEntry> findFile(uint32_t fileId) const;

    /**
     * Extract a file's raw data
     * @param entry The file entry to extract
     * @return The file data, or empty on error
     */
    QByteArray extractFile(const DatFileEntry& entry);

    /**
     * Get the highest file version (iteration) in this archive
     */
    uint32_t getMaxFileVersion() const;

private:
    bool readSuperblock();
    bool readDirectory(uint32_t offset);
    bool readDirectoryNode(uint32_t offset);

    static uint32_t readDword(const QByteArray& buf, int offset);

    std::filesystem::path m_path;
    QFile m_file;
    bool m_valid = false;
    QString m_lastError;
    
    DatSuperblock m_superblock{};
    std::vector<DatFileEntry> m_fileEntries;
    std::map<uint32_t, DatFileEntry> m_fileIndex;
};

/**
 * Utility to get iteration info from all .dat files in a game directory
 */
struct DatVersionInfo {
    std::filesystem::path datPath;
    QString datName;
    uint32_t version;
    uint32_t maxFileVersion;
    size_t fileCount;
};

/**
 * Scan game directory for .dat files and get their version info
 */
std::vector<DatVersionInfo> scanDatVersions(const std::filesystem::path& gameDirectory);

} // namespace lotro
