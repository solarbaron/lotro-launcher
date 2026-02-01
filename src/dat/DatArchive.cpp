/**
 * @file DatArchive.cpp
 * @brief Implementation of DAT archive reader
 */

#include "DatArchive.hpp"
#include "BufferUtils.hpp"

#include <QDataStream>
#include <zlib.h>
#include <spdlog/spdlog.h>

namespace lotro::dat {

DatArchive::DatArchive(const QString& path)
    : m_path(path)
    , m_file(path)
{
}

DatArchive::~DatArchive() {
    close();
}

bool DatArchive::open() {
    if (!m_file.open(QIODevice::ReadOnly)) {
        spdlog::error("Failed to open DAT file: {}", m_path.toStdString());
        return false;
    }
    
    int rootOffset = readSuperBlock();
    if (rootOffset < 0) {
        spdlog::error("Failed to read DAT superblock");
        m_file.close();
        return false;
    }
    
    m_rootEntry = std::make_shared<DirectoryEntry>(nullptr, rootOffset, 2460);
    spdlog::info("Opened DAT archive: {} (block size: {})", m_path.toStdString(), m_blockSize);
    return true;
}

void DatArchive::close() {
    if (m_file.isOpen()) {
        m_file.close();
    }
    m_loadedDirs.clear();
    m_rootEntry.reset();
    m_blockSize = 0;
}

int DatArchive::readSuperBlock() {
    if (!m_file.seek(320)) {
        return -1;
    }
    
    QByteArray buffer = m_file.read(104);
    if (buffer.size() != 104) {
        return -1;
    }
    
    const char* data = buffer.constData();
    
    int magic = BufferUtils::getDoubleWordAt(data, 0);
    if (magic != MAGIC) {
        spdlog::warn("DAT magic mismatch: got {}, expected {}", magic, MAGIC);
    }
    
    m_blockSize = BufferUtils::getDoubleWordAt(data, 4);
    uint64_t totalFileSize = BufferUtils::getDoubleWordAtAsLong(data, 8);
    int version = BufferUtils::getDoubleWordAt(data, 12);
    int rootNodeOffset = BufferUtils::getDoubleWordAt(data, 32);
    m_datPackVersion = BufferUtils::getDoubleWordAt(data, 52);
    
    spdlog::debug("DAT: block_size={}, version=0x{:04X}, datpack_version={}, root_offset={}",
                  m_blockSize, version, m_datPackVersion, rootNodeOffset);
    
    return rootNodeOffset;
}

void DatArchive::readDirectory(DirectoryEntryPtr entry) {
    uint64_t offset = entry->offset();
    if (offset == 0 || static_cast<int64_t>(offset) < 0) {
        return;
    }
    
    int dirBlockSize = entry->blockSize();
    QByteArray dirData = readBlockAt(offset, dirBlockSize, DIRECTORY_RAW_SIZE);
    if (dirData.size() < DIRECTORY_RAW_SIZE) {
        spdlog::warn("Failed to read directory at offset {}", offset);
        return;
    }
    
    const char* data = dirData.constData();
    int filesCount = BufferUtils::getDoubleWordAt(data, BASE_FILE_ENTRIES_OFFSET);
    
    if (filesCount > MAX_ENTRIES) {
        spdlog::error("Too many entries in directory: {}", filesCount);
        return;
    }
    
    // Read subdirectory pointers
    for (int i = 0; i <= filesCount; ++i) {
        int blockSize = BufferUtils::getDoubleWordAt(data, i * 8);
        uint64_t dirOffset = BufferUtils::getDoubleWordAtAsLong(data, 4 + i * 8);
        
        if (blockSize != 0) {
            auto subDir = std::make_shared<DirectoryEntry>(entry.get(), dirOffset, blockSize);
            entry->addDirectory(subDir);
        }
    }
    
    // Read file entries
    for (int i = 0; i < filesCount; ++i) {
        int baseOffset = 500 + i * ENTRY_RAW_SIZE;
        
        int flags = BufferUtils::getWordAt(data, baseOffset);
        int policy = BufferUtils::getWordAt(data, baseOffset + 2);
        uint64_t fileId = BufferUtils::getDoubleWordAtAsLong(data, baseOffset + 4);
        uint64_t fileOffset = BufferUtils::getDoubleWordAtAsLong(data, baseOffset + 8);
        int size = BufferUtils::getDoubleWordAt(data, baseOffset + 12);
        uint64_t timestamp = BufferUtils::getDoubleWordAtAsLong(data, baseOffset + 16);
        int version = BufferUtils::getDoubleWordAt(data, baseOffset + 20);
        int blockSize = BufferUtils::getDoubleWordAt(data, baseOffset + 24);
        
        FileEntry fileEntry(i, fileId, fileOffset, version, timestamp, size, blockSize, flags, policy);
        entry->addFile(fileEntry);
    }
}

void DatArchive::ensureLoaded(DirectoryEntryPtr entry) {
    uint64_t offset = entry->offset();
    if (!m_loadedDirs.contains(offset)) {
        readDirectory(entry);
        m_loadedDirs[offset] = entry;
    }
}

FileEntry* DatArchive::getFileById(DirectoryEntryPtr dir, uint64_t fileId) {
    ensureLoaded(dir);
    
    const auto& files = dir->files();
    int nbFiles = files.size();
    int l = 0;
    int u = nbFiles - 1;
    
    // Binary search through file entries
    while (l <= u) {
        int p = (l + u) / 2;
        const FileEntry& entry = files[p];
        uint64_t currentId = entry.fileId();
        
        if (currentId < fileId) {
            l = p + 1;
        } else if (currentId > fileId) {
            u = p - 1;
        } else {
            // Found it - need to return non-const pointer
            // We'll use a workaround since QVector doesn't give us a mutable ref easily
            return const_cast<FileEntry*>(&files[p]);
        }
    }
    
    // Not found in this directory, check subdirectories
    const auto& subDirs = dir->directories();
    if (!subDirs.isEmpty() && l < subDirs.size()) {
        return getFileById(subDirs[l], fileId);
    }
    
    return nullptr;
}

QByteArray DatArchive::readBlockAt(uint64_t offset, int blockSize, int size) {
    if (!m_file.seek(offset)) {
        return QByteArray();
    }
    
    // Read header
    QByteArray header = m_file.read(8);
    if (header.size() != 8) {
        return QByteArray();
    }
    
    const char* hdr = header.constData();
    int numExtraBlocks = BufferUtils::getDoubleWordAt(hdr, 0);
    int legacy = BufferUtils::getDoubleWordAt(hdr, 4);
    
    if (legacy != 0) {
        // Old format
        return readOldBlockAt(offset, blockSize, size);
    }
    
    // Calculate first chunk size
    int firstChunkSize = blockSize - 8 - numExtraBlocks * 8;
    if (firstChunkSize > size) {
        firstChunkSize = size;
    }
    
    // Read first chunk
    QByteArray firstChunk = m_file.read(firstChunkSize);
    if (firstChunk.size() != firstChunkSize) {
        return QByteArray();
    }
    
    // Read extra block info
    QByteArray extraInfo = m_file.read(numExtraBlocks * 8);
    if (extraInfo.size() != numExtraBlocks * 8) {
        return firstChunk; // Return what we have
    }
    
    // Build result
    QByteArray result(size, Qt::Uninitialized);
    memcpy(result.data(), firstChunk.constData(), firstChunkSize);
    
    int index = firstChunkSize;
    const char* extra = extraInfo.constData();
    
    for (int i = 0; i < numExtraBlocks && index < size; ++i) {
        int extraBlockSize = BufferUtils::getDoubleWordAt(extra, i * 8);
        uint64_t extraOffset = BufferUtils::getDoubleWordAtAsLong(extra, i * 8 + 4);
        
        if (!m_file.seek(extraOffset)) {
            break;
        }
        
        int sizeToRead = qMin(extraBlockSize, size - index);
        QByteArray chunk = m_file.read(sizeToRead);
        memcpy(result.data() + index, chunk.constData(), chunk.size());
        index += chunk.size();
    }
    
    return result;
}

QByteArray DatArchive::readOldBlockAt(uint64_t offset, int blockSize, int totalSize) {
    QByteArray result(totalSize, Qt::Uninitialized);
    int bytesRead = 0;
    int pos = totalSize;
    
    if (!m_file.seek(offset)) {
        return QByteArray();
    }
    
    int currentBlockSize = blockSize;
    
    for (int steps = 0; steps < 1000 && bytesRead < totalSize; ++steps) {
        QByteArray header = m_file.read(8);
        if (header.size() != 8) {
            break;
        }
        
        const char* hdr = header.constData();
        int nextBlockSize = BufferUtils::getDoubleWordAt(hdr, 0);
        int nextOffset = BufferUtils::getDoubleWordAt(hdr, 4);
        
        if (nextBlockSize == 0) {
            int toRead = totalSize - bytesRead;
            if (toRead > 0) {
                QByteArray chunk = m_file.read(toRead);
                memcpy(result.data(), chunk.constData(), chunk.size());
            }
            return result;
        }
        
        int toRead = currentBlockSize - 8;
        pos -= toRead;
        
        QByteArray chunk = m_file.read(toRead);
        memcpy(result.data() + pos, chunk.constData(), chunk.size());
        bytesRead += chunk.size();
        
        if (!m_file.seek(nextOffset)) {
            break;
        }
        currentBlockSize = nextBlockSize;
    }
    
    return result;
}

QByteArray DatArchive::loadEntry(const FileEntry& entry) {
    QByteArray data = readBlockAt(entry.fileOffset(), entry.blockSize(), entry.size());
    
    if (data.isEmpty()) {
        return data;
    }
    
    if (entry.isCompressed()) {
        // Skip first 4 bytes and decompress
        if (data.size() <= 4) {
            return QByteArray();
        }
        
        // Use zlib to decompress
        QByteArray compressed = data.mid(4);
        
        // Get uncompressed size from first 4 bytes (if available in format)
        // For now, we'll try to decompress with a reasonable buffer
        uLongf destLen = entry.size() * 10; // Estimate
        QByteArray decompressed(destLen, Qt::Uninitialized);
        
        int result = uncompress(
            reinterpret_cast<Bytef*>(decompressed.data()),
            &destLen,
            reinterpret_cast<const Bytef*>(compressed.constData()),
            compressed.size()
        );
        
        if (result == Z_OK) {
            decompressed.resize(destLen);
            return decompressed;
        } else {
            spdlog::warn("Failed to decompress entry, zlib error: {}", result);
            return QByteArray();
        }
    }
    
    return data;
}

QByteArray DatArchive::loadData(uint64_t fileId) {
    if (!isOpen() || !m_rootEntry) {
        return QByteArray();
    }
    
    FileEntry* entry = getFileById(m_rootEntry, fileId);
    if (!entry) {
        spdlog::debug("File ID 0x{:08X} not found in DAT archive", fileId);
        return QByteArray();
    }
    
    return loadEntry(*entry);
}

} // namespace lotro::dat
