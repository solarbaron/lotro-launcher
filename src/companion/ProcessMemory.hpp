/**
 * LOTRO Launcher - Process Memory Access
 * 
 * Cross-platform memory reading from external processes.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#endif

namespace lotro {

/**
 * Process information
 */
struct ProcessInfo {
    uint64_t pid = 0;
    std::string name;
    std::string path;
    bool is64Bit = true;
};

/**
 * Memory buffer for reading data
 */
class MemoryBuffer {
public:
    explicit MemoryBuffer(size_t size);
    ~MemoryBuffer() = default;
    
    MemoryBuffer(const MemoryBuffer&) = delete;
    MemoryBuffer& operator=(const MemoryBuffer&) = delete;
    MemoryBuffer(MemoryBuffer&&) = default;
    MemoryBuffer& operator=(MemoryBuffer&&) = default;
    
    /**
     * Get raw data pointer
     */
    const uint8_t* data() const { return m_data.data(); }
    uint8_t* data() { return m_data.data(); }
    
    /**
     * Get buffer size
     */
    size_t size() const { return m_data.size(); }
    
    /**
     * Read value at offset
     */
    template<typename T>
    T read(size_t offset) const {
        if (offset + sizeof(T) > m_data.size()) {
            return T{};
        }
        return *reinterpret_cast<const T*>(m_data.data() + offset);
    }
    
    /**
     * Read pointer (4 or 8 bytes depending on process architecture)
     */
    uint64_t readPointer(size_t offset, bool is64Bit) const;
    
    /**
     * Read null-terminated string at offset
     */
    std::string readString(size_t offset, size_t maxLen = 256) const;
    
    /**
     * Read wide string at offset
     */
    std::wstring readWideString(size_t offset, size_t maxLen = 256) const;

private:
    std::vector<uint8_t> m_data;
};

/**
 * Cross-platform process memory reader
 */
class ProcessMemory {
public:
    ProcessMemory();
    ~ProcessMemory();
    
    ProcessMemory(const ProcessMemory&) = delete;
    ProcessMemory& operator=(const ProcessMemory&) = delete;
    
    /**
     * Find all processes matching name
     */
    static std::vector<ProcessInfo> findProcesses(const std::string& namePattern);
    
    /**
     * Find LOTRO client process
     */
    static std::optional<ProcessInfo> findLotroClient();
    
    /**
     * Open a process for memory reading
     */
    bool open(uint64_t pid);
    
    /**
     * Close the process handle
     */
    void close();
    
    /**
     * Check if process is open
     */
    bool isOpen() const;
    
    /**
     * Get the process info
     */
    const ProcessInfo& processInfo() const { return m_processInfo; }
    
    /**
     * Module information
     */
    struct ModuleInfo {
        uint64_t baseAddress = 0;
        uint64_t size = 0;
    };
    
    /**
     * Get module info (base address and size)
     */
    std::optional<ModuleInfo> getModuleEx(const std::string& moduleName);
    
    /**
     * Get module base address (for lotroclient64.exe or lotroclient.exe)
     */
    std::optional<uint64_t> getModuleBaseAddress(const std::string& moduleName);
    
    /**
     * Read memory at address
     */
    std::optional<MemoryBuffer> readMemory(uint64_t address, size_t size);
    
    /**
     * Read a value from memory
     */
    template<typename T>
    std::optional<T> read(uint64_t address) {
        auto buffer = readMemory(address, sizeof(T));
        if (!buffer) {
            return std::nullopt;
        }
        return buffer->read<T>(0);
    }
    
    /**
     * Read a pointer from memory
     */
    std::optional<uint64_t> readPointer(uint64_t address);
    
    /**
     * Read string from memory
     */
    std::optional<std::string> readString(uint64_t address, size_t maxLen = 256);
    
    /**
     * Read wide string (UTF-16) from memory
     */
    std::optional<std::wstring> readWideString(uint64_t address, size_t maxLen = 256);

private:
    ProcessInfo m_processInfo;
    
#ifdef _WIN32
    HANDLE m_handle = nullptr;
#else
    int m_memFd = -1;
#endif
    
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace lotro
