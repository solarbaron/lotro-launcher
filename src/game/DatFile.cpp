/**
 * DatFile - Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "DatFile.hpp"

#include <spdlog/spdlog.h>

#include <QDataStream>
#include <QDir>

namespace lotro {

// Magic values in the superblock
constexpr uint32_t MAGIC_LP = 0x4C50;  // "LP" at offset 0x101
constexpr uint32_t MAGIC_TB = 0x5442;  // "TB" at offset 0x140

// Superblock offsets (relative to file start)
constexpr int SB_MAGIC_LP = 0x101;
constexpr int SB_MAGIC_TB = 0x140;
constexpr int SB_BLOCK_SIZE = 0x144;
constexpr int SB_FILE_SIZE = 0x148;
constexpr int SB_VERSION = 0x14C;
constexpr int SB_VERSION2 = 0x150;
constexpr int SB_FREE_HEAD = 0x154;
constexpr int SB_FREE_TAIL = 0x158;
constexpr int SB_FREE_SIZE = 0x15C;
constexpr int SB_DIRECTORY_OFFSET = 0x160;

DatFile::DatFile(const std::filesystem::path& path)
    : m_path(path)
    , m_file(QString::fromStdString(path.string()))
{
    if (!m_file.open(QIODevice::ReadOnly)) {
        m_lastError = QString("Failed to open file: %1").arg(m_file.errorString());
        spdlog::error("DatFile: {}", m_lastError.toStdString());
        return;
    }

    if (!readSuperblock()) {
        return;
    }

    if (!readDirectory(m_superblock.directoryOffset)) {
        return;
    }

    // Build index for fast lookup
    for (const auto& entry : m_fileEntries) {
        m_fileIndex[entry.fileId] = entry;
    }

    m_valid = true;
    spdlog::info("DatFile opened: {} ({} files, version {})", 
        path.filename().string(), m_fileEntries.size(), m_superblock.version);
}

DatFile::~DatFile() {
    if (m_file.isOpen()) {
        m_file.close();
    }
}

uint32_t DatFile::readDword(const QByteArray& buf, int offset) {
    if (offset + 4 > buf.size()) return 0;
    
    // Little-endian
    return static_cast<uint32_t>(
        (static_cast<uint8_t>(buf[offset])) |
        (static_cast<uint8_t>(buf[offset + 1]) << 8) |
        (static_cast<uint8_t>(buf[offset + 2]) << 16) |
        (static_cast<uint8_t>(buf[offset + 3]) << 24)
    );
}

bool DatFile::readSuperblock() {
    // Read first 1KB for superblock
    QByteArray buf = m_file.read(1024);
    if (buf.size() < 1024) {
        m_lastError = "File too small to contain superblock";
        return false;
    }

    // Verify magic values
    uint32_t magicLP = readDword(buf, SB_MAGIC_LP);
    uint32_t magicTB = readDword(buf, SB_MAGIC_TB);

    if (magicLP != MAGIC_LP) {
        m_lastError = QString("Invalid magic at 0x101: expected 0x%1, got 0x%2")
            .arg(MAGIC_LP, 4, 16, QChar('0'))
            .arg(magicLP, 4, 16, QChar('0'));
        spdlog::error("DatFile: {}", m_lastError.toStdString());
        return false;
    }

    if (magicTB != MAGIC_TB) {
        m_lastError = QString("Invalid magic at 0x140: expected 0x%1, got 0x%2")
            .arg(MAGIC_TB, 4, 16, QChar('0'))
            .arg(magicTB, 4, 16, QChar('0'));
        spdlog::error("DatFile: {}", m_lastError.toStdString());
        return false;
    }

    // Read superblock fields
    m_superblock.blockSize = readDword(buf, SB_BLOCK_SIZE);
    m_superblock.fileSize = readDword(buf, SB_FILE_SIZE);
    m_superblock.version = readDword(buf, SB_VERSION);
    m_superblock.version2 = readDword(buf, SB_VERSION2);
    m_superblock.freeHead = readDword(buf, SB_FREE_HEAD);
    m_superblock.freeTail = readDword(buf, SB_FREE_TAIL);
    m_superblock.freeSize = readDword(buf, SB_FREE_SIZE);
    m_superblock.directoryOffset = readDword(buf, SB_DIRECTORY_OFFSET);

    spdlog::debug("DatFile superblock: blockSize={}, version={}, directoryOffset=0x{:x}",
        m_superblock.blockSize, m_superblock.version, m_superblock.directoryOffset);

    return true;
}

bool DatFile::readDirectory(uint32_t offset) {
    return readDirectoryNode(offset);
}

bool DatFile::readDirectoryNode(uint32_t offset) {
    // Seek to directory node
    if (!m_file.seek(offset)) {
        m_lastError = QString("Failed to seek to directory at 0x%1").arg(offset, 8, 16, QChar('0'));
        return false;
    }

    // Read directory header (first 8 bytes should be zeros)
    QByteArray header = m_file.read(8);
    if (header.size() < 8) {
        m_lastError = "Failed to read directory header";
        return false;
    }

    // Read subdirectory pointers (62 entries, 8 bytes each)
    std::vector<std::pair<uint32_t, uint32_t>> subdirPtrs;  // block_size, dir_offset
    
    if (!m_file.seek(offset + 0x08)) return false;
    
    for (int i = 0; i < 62; i++) {
        QByteArray entry = m_file.read(8);
        if (entry.size() < 8) break;
        
        uint32_t blockSize = readDword(entry, 0);
        uint32_t dirOffset = readDword(entry, 4);
        
        if (blockSize == 0) break;
        
        subdirPtrs.emplace_back(blockSize, dirOffset);
    }

    // Read file count
    if (!m_file.seek(offset + (0x08 * 63))) return false;
    QByteArray countBuf = m_file.read(4);
    if (countBuf.size() < 4) return false;
    
    uint32_t fileCount = readDword(countBuf, 0);
    
    // Trim subdirectory pointers to fileCount + 1
    if (subdirPtrs.size() > fileCount + 1) {
        subdirPtrs.resize(fileCount + 1);
    }

    // Read file entries (32 bytes each)
    for (uint32_t i = 0; i < fileCount; i++) {
        QByteArray entryBuf = m_file.read(32);
        if (entryBuf.size() < 32) break;

        DatFileEntry entry;
        // uint32_t unknown1 = readDword(entryBuf, 0);
        entry.fileId = readDword(entryBuf, 4);
        entry.fileOffset = readDword(entryBuf, 8);
        entry.size = readDword(entryBuf, 12);
        entry.timestamp = readDword(entryBuf, 16);
        entry.version = readDword(entryBuf, 20);
        entry.compressedSize = readDword(entryBuf, 24);
        // uint32_t unknown2 = readDword(entryBuf, 28);

        if (entry.size > 0) {
            m_fileEntries.push_back(entry);
        }
    }

    // Recursively read subdirectories
    for (size_t i = 0; i < subdirPtrs.size(); i++) {
        if (!readDirectoryNode(subdirPtrs[i].second)) {
            // Log warning but continue
            spdlog::warn("Failed to read subdirectory at 0x{:x}", subdirPtrs[i].second);
        }
    }

    return true;
}

std::optional<DatFileEntry> DatFile::findFile(uint32_t fileId) const {
    auto it = m_fileIndex.find(fileId);
    if (it != m_fileIndex.end()) {
        return it->second;
    }
    return std::nullopt;
}

QByteArray DatFile::extractFile(const DatFileEntry& entry) {
    if (!m_file.seek(entry.fileOffset)) {
        m_lastError = QString("Failed to seek to file at 0x%1").arg(entry.fileOffset, 8, 16, QChar('0'));
        return {};
    }

    // Read the file data
    uint32_t readSize = (entry.compressedSize > 0) ? entry.compressedSize : entry.size;
    QByteArray data = m_file.read(readSize);
    
    if (data.size() < static_cast<qsizetype>(readSize)) {
        m_lastError = "Failed to read complete file data";
        return {};
    }

    // TODO: Handle decompression if compressedSize < size
    // The compression format needs more investigation

    return data;
}

uint32_t DatFile::getMaxFileVersion() const {
    uint32_t maxVersion = 0;
    for (const auto& entry : m_fileEntries) {
        if (entry.version > maxVersion) {
            maxVersion = entry.version;
        }
    }
    return maxVersion;
}

std::vector<DatVersionInfo> scanDatVersions(const std::filesystem::path& gameDirectory) {
    std::vector<DatVersionInfo> results;

    for (const auto& entry : std::filesystem::directory_iterator(gameDirectory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".dat") {
            DatFile dat(entry.path());
            if (dat.isValid()) {
                DatVersionInfo info;
                info.datPath = entry.path();
                info.datName = QString::fromStdString(entry.path().filename().string());
                info.version = dat.currentVersion();
                info.maxFileVersion = dat.getMaxFileVersion();
                info.fileCount = dat.fileCount();
                results.push_back(info);
            }
        }
    }

    // Sort by name
    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        return a.datName < b.datName;
    });

    return results;
}

} // namespace lotro
