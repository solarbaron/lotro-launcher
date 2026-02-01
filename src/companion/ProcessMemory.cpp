/**
 * LOTRO Launcher - Process Memory Access Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ProcessMemory.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <regex>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace lotro {

// ============================================================================
// MemoryBuffer implementation
// ============================================================================

MemoryBuffer::MemoryBuffer(size_t size)
    : m_data(size, 0)
{
}

uint64_t MemoryBuffer::readPointer(size_t offset, bool is64Bit) const {
    if (is64Bit) {
        return read<uint64_t>(offset);
    } else {
        return read<uint32_t>(offset);
    }
}

std::string MemoryBuffer::readString(size_t offset, size_t maxLen) const {
    if (offset >= m_data.size()) {
        return "";
    }
    
    size_t len = 0;
    while (offset + len < m_data.size() && len < maxLen && m_data[offset + len] != 0) {
        ++len;
    }
    
    return std::string(reinterpret_cast<const char*>(m_data.data() + offset), len);
}

std::wstring MemoryBuffer::readWideString(size_t offset, size_t maxLen) const {
    if (offset >= m_data.size()) {
        return L"";
    }
    
    std::wstring result;
    size_t chars = 0;
    
    while (offset + chars * 2 + 1 < m_data.size() && chars < maxLen) {
        wchar_t ch = read<uint16_t>(offset + chars * 2);
        if (ch == 0) break;
        result += ch;
        ++chars;
    }
    
    return result;
}

// ============================================================================
// ProcessMemory implementation - Platform specific
// ============================================================================

class ProcessMemory::Impl {
public:
    // Platform specific implementation details
};

ProcessMemory::ProcessMemory()
    : m_impl(std::make_unique<Impl>())
{
}

ProcessMemory::~ProcessMemory() {
    close();
}

#ifdef _WIN32
// ============================================================================
// Windows Implementation
// ============================================================================

std::vector<ProcessInfo> ProcessMemory::findProcesses(const std::string& namePattern) {
    std::vector<ProcessInfo> results;
    
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        spdlog::error("Failed to create process snapshot");
        return results;
    }
    
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(pe32);
    
    if (Process32FirstW(snapshot, &pe32)) {
        do {
            // Convert wide string to narrow
            std::wstring wname(pe32.szExeFile);
            std::string name(wname.begin(), wname.end());
            
            // Check if name matches pattern
            if (name.find(namePattern) != std::string::npos) {
                ProcessInfo info;
                info.pid = pe32.th32ProcessID;
                info.name = name;
                
                // Determine if 64-bit by checking if it's WOW64
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pe32.th32ProcessID);
                if (hProcess) {
                    BOOL isWow64 = FALSE;
                    IsWow64Process(hProcess, &isWow64);
                    info.is64Bit = !isWow64;
                    CloseHandle(hProcess);
                }
                
                results.push_back(info);
                spdlog::info("Found process: {} (PID: {})", name, info.pid);
            }
        } while (Process32NextW(snapshot, &pe32));
    }
    
    CloseHandle(snapshot);
    return results;
}

std::optional<ProcessInfo> ProcessMemory::findLotroClient() {
    // Try 64-bit client first
    auto processes = findProcesses("lotroclient64.exe");
    if (!processes.empty()) {
        return processes[0];
    }
    
    // Try 32-bit client
    processes = findProcesses("lotroclient.exe");
    if (!processes.empty()) {
        return processes[0];
    }
    
    return std::nullopt;
}

bool ProcessMemory::open(uint64_t pid) {
    close();
    
    // PROCESS_QUERY_INFORMATION | PROCESS_VM_READ = 0x0410
    m_handle = OpenProcess(0x0410, FALSE, static_cast<DWORD>(pid));
    
    if (m_handle == nullptr) {
        spdlog::error("Failed to open process {}: error {}", pid, GetLastError());
        return false;
    }
    
    m_processInfo.pid = pid;
    
    // Determine if 64-bit
    BOOL isWow64 = FALSE;
    IsWow64Process(m_handle, &isWow64);
    m_processInfo.is64Bit = !isWow64;
    
    spdlog::info("Opened process {} ({})", pid, m_processInfo.is64Bit ? "64-bit" : "32-bit");
    return true;
}

void ProcessMemory::close() {
    if (m_handle != nullptr) {
        CloseHandle(m_handle);
        m_handle = nullptr;
    }
    m_processInfo = ProcessInfo{};
}

bool ProcessMemory::isOpen() const {
    return m_handle != nullptr;
}

std::optional<uint64_t> ProcessMemory::getModuleBaseAddress(const std::string& moduleName) {
    auto info = getModuleEx(moduleName);
    if (info) {
        return info->baseAddress;
    }
    return std::nullopt;
}

std::optional<ProcessMemory::ModuleInfo> ProcessMemory::getModuleEx(const std::string& moduleName) {
    if (!isOpen()) {
        return std::nullopt;
    }
    
    HMODULE modules[1024];
    DWORD cbNeeded;
    
    if (EnumProcessModulesEx(m_handle, modules, sizeof(modules), &cbNeeded, LIST_MODULES_ALL)) {
        for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            char szModName[MAX_PATH];
            
            if (GetModuleBaseNameA(m_handle, modules[i], szModName, sizeof(szModName))) {
                if (_stricmp(szModName, moduleName.c_str()) == 0) {
                    MODULEINFO modInfo;
                    if (GetModuleInformation(m_handle, modules[i], &modInfo, sizeof(modInfo))) {
                        ModuleInfo result;
                        result.baseAddress = reinterpret_cast<uint64_t>(modInfo.lpBaseOfDll);
                        result.size = modInfo.SizeOfImage;
                        spdlog::info("Module {} found: Base=0x{:X}, Size=0x{:X}", 
                                     moduleName, result.baseAddress, result.size);
                        return result;
                    }
                }
            }
        }
    }
    
    spdlog::error("Module {} not found", moduleName);
    return std::nullopt;
}

std::optional<MemoryBuffer> ProcessMemory::readMemory(uint64_t address, size_t size) {
    if (!isOpen()) {
        return std::nullopt;
    }
    
    MemoryBuffer buffer(size);
    SIZE_T bytesRead = 0;
    
    if (!ReadProcessMemory(m_handle, reinterpret_cast<LPCVOID>(address), 
                          buffer.data(), size, &bytesRead)) {
        DWORD error = GetLastError();
        if (error != 299) { // Partial read OK
            spdlog::debug("ReadProcessMemory failed at 0x{:X}: error {}", address, error);
        }
        if (bytesRead == 0) {
            return std::nullopt;
        }
    }
    
    return buffer;
}

#else
// ============================================================================
// Linux Implementation (for Wine processes)
// ============================================================================

std::vector<ProcessInfo> ProcessMemory::findProcesses(const std::string& namePattern) {
    std::vector<ProcessInfo> results;
    
    DIR* procDir = opendir("/proc");
    if (!procDir) {
        spdlog::error("Failed to open /proc");
        return results;
    }
    
    struct dirent* entry;
    while ((entry = readdir(procDir)) != nullptr) {
        // Check if entry is a PID directory
        if (entry->d_type != DT_DIR) continue;
        
        bool isPid = true;
        for (const char* c = entry->d_name; *c; ++c) {
            if (!isdigit(*c)) {
                isPid = false;
                break;
            }
        }
        if (!isPid) continue;
        
        // Read cmdline
        std::string cmdlinePath = std::string("/proc/") + entry->d_name + "/cmdline";
        std::ifstream cmdlineFile(cmdlinePath);
        if (!cmdlineFile) continue;
        
        std::string cmdline;
        std::getline(cmdlineFile, cmdline, '\0');
        
        // Check if it matches our pattern
        std::string lowerCmdline = cmdline;
        std::transform(lowerCmdline.begin(), lowerCmdline.end(), lowerCmdline.begin(), ::tolower);
        
        if (lowerCmdline.find(namePattern) != std::string::npos) {
            ProcessInfo info;
            info.pid = std::stoull(entry->d_name);
            info.path = cmdline;
            
            // Extract name from path
            size_t lastSlash = cmdline.rfind('/');
            info.name = (lastSlash != std::string::npos) ? cmdline.substr(lastSlash + 1) : cmdline;
            
            // Check if 64-bit by examining exe
            std::string exePath = std::string("/proc/") + entry->d_name + "/exe";
            std::ifstream exeFile(exePath, std::ios::binary);
            if (exeFile) {
                char elfHeader[5] = {0};
                exeFile.read(elfHeader, 5);
                // ELF magic + class (1=32bit, 2=64bit)
                if (elfHeader[0] == 0x7f && elfHeader[1] == 'E' && elfHeader[2] == 'L' && elfHeader[3] == 'F') {
                    info.is64Bit = (elfHeader[4] == 2);
                }
            }
            
            results.push_back(info);
            spdlog::info("Found process: {} (PID: {})", info.name, info.pid);
        }
    }
    
    closedir(procDir);
    return results;
}

std::optional<ProcessInfo> ProcessMemory::findLotroClient() {
    // Look for Wine running lotroclient
    auto processes = findProcesses("lotroclient64.exe");
    if (!processes.empty()) {
        return processes[0];
    }
    
    processes = findProcesses("lotroclient.exe");
    if (!processes.empty()) {
        return processes[0];
    }
    
    // Also check for wine-preloader running lotroclient
    processes = findProcesses("lotroclient");
    for (const auto& proc : processes) {
        if (proc.path.find("wine") != std::string::npos || 
            proc.path.find("lotroclient") != std::string::npos) {
            return proc;
        }
    }
    
    return std::nullopt;
}

bool ProcessMemory::open(uint64_t pid) {
    close();
    
    // Open /proc/[pid]/mem for reading
    std::string memPath = "/proc/" + std::to_string(pid) + "/mem";
    m_memFd = ::open(memPath.c_str(), O_RDONLY);
    
    if (m_memFd < 0) {
        spdlog::error("Failed to open {}: {}", memPath, strerror(errno));
        return false;
    }
    
    m_processInfo.pid = pid;
    
    // Try to determine if 64-bit
    std::string exePath = "/proc/" + std::to_string(pid) + "/exe";
    std::ifstream exeFile(exePath, std::ios::binary);
    if (exeFile) {
        char elfHeader[5] = {0};
        exeFile.read(elfHeader, 5);
        if (elfHeader[0] == 0x7f && elfHeader[1] == 'E') {
            m_processInfo.is64Bit = (elfHeader[4] == 2);
        }
    }
    
    spdlog::info("Opened process {} ({})", pid, m_processInfo.is64Bit ? "64-bit" : "32-bit");
    return true;
}

void ProcessMemory::close() {
    if (m_memFd >= 0) {
        ::close(m_memFd);
        m_memFd = -1;
    }
    m_processInfo = ProcessInfo{};
}

bool ProcessMemory::isOpen() const {
    return m_memFd >= 0;
}

std::optional<uint64_t> ProcessMemory::getModuleBaseAddress(const std::string& moduleName) {
    auto info = getModuleEx(moduleName);
    if (info) {
        return info->baseAddress;
    }
    return std::nullopt;
}

std::optional<ProcessMemory::ModuleInfo> ProcessMemory::getModuleEx(const std::string& moduleName) {
    if (!isOpen()) {
        return std::nullopt;
    }
    
    // Parse /proc/[pid]/maps to find module
    std::string mapsPath = "/proc/" + std::to_string(m_processInfo.pid) + "/maps";
    std::ifstream mapsFile(mapsPath);
    if (!mapsFile) {
        spdlog::error("Failed to open {}", mapsPath);
        return std::nullopt;
    }
    
    std::string line;
    std::string lowerModuleName = moduleName;
    std::transform(lowerModuleName.begin(), lowerModuleName.end(), lowerModuleName.begin(), ::tolower);
    
    uint64_t firstStart = 0;
    uint64_t lastEnd = 0;
    bool found = false;
    
    while (std::getline(mapsFile, line)) {
        std::string lowerLine = line;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
        
        if (lowerLine.find(lowerModuleName) != std::string::npos) {
            // Parse the address range (format: start-end perms ...)
            uint64_t startAddr, endAddr;
            if (sscanf(line.c_str(), "%lx-%lx", &startAddr, &endAddr) == 2) {
                if (!found) {
                    firstStart = startAddr;
                    found = true;
                }
                lastEnd = endAddr;
            }
        }
    }
    
    if (found) {
        ModuleInfo info;
        info.baseAddress = firstStart;
        
        // Try to read PE header to get exact size (reliable for Wine)
        // e_lfanew is at offset 0x3C
        auto dosHeader = readMemory(info.baseAddress, 0x40);
        if (dosHeader) {
            uint32_t e_lfanew = dosHeader->read<uint32_t>(0x3C);
            // PE Header start = base + e_lfanew. 
            // OptionalHeader starts at PE Header + 4 (Sig) + 20 (FileHeader) = +24
            // SizeOfImage is at offset 56 (0x38) in OptionalHeader (for both 32/64 bit)
            uint64_t sizeOfImageAddr = info.baseAddress + e_lfanew + 24 + 56;
            
            auto sizeBuffer = readMemory(sizeOfImageAddr, 4);
            if (sizeBuffer) {
                uint32_t sizeOfImage = sizeBuffer->read<uint32_t>(0);
                if (sizeOfImage > 0) {
                    info.size = sizeOfImage;
                    spdlog::info("Module {} size from PE header: 0x{:X}", moduleName, info.size);
                } else {
                    info.size = lastEnd - firstStart;
                }
            } else {
                info.size = lastEnd - firstStart;
            }
        } else {
            info.size = lastEnd - firstStart;
        }

        spdlog::info("Module {} found: Base=0x{:X}, Size=0x{:X}", 
                     moduleName, info.baseAddress, info.size);
        return info;
    }
    
    spdlog::error("Module {} not found in maps", moduleName);
    return std::nullopt;
}

std::optional<MemoryBuffer> ProcessMemory::readMemory(uint64_t address, size_t size) {
    if (!isOpen()) {
        return std::nullopt;
    }
    
    MemoryBuffer buffer(size);
    
    // Use process_vm_readv for better performance
    struct iovec local;
    struct iovec remote;
    
    local.iov_base = buffer.data();
    local.iov_len = size;
    remote.iov_base = reinterpret_cast<void*>(address);
    remote.iov_len = size;
    
    ssize_t nread = process_vm_readv(m_processInfo.pid, &local, 1, &remote, 1, 0);
    
    if (nread < 0) {
        spdlog::debug("process_vm_readv failed at 0x{:X}: {}", address, strerror(errno));
        return std::nullopt;
    }
    
    return buffer;
}

#endif

// ============================================================================
// Common implementation
// ============================================================================

std::optional<uint64_t> ProcessMemory::readPointer(uint64_t address) {
    if (m_processInfo.is64Bit) {
        return read<uint64_t>(address);
    } else {
        auto val = read<uint32_t>(address);
        if (val) {
            return static_cast<uint64_t>(*val);
        }
        return std::nullopt;
    }
}

std::optional<std::string> ProcessMemory::readString(uint64_t address, size_t maxLen) {
    auto buffer = readMemory(address, maxLen);
    if (!buffer) {
        return std::nullopt;
    }
    return buffer->readString(0, maxLen);
}

std::optional<std::wstring> ProcessMemory::readWideString(uint64_t address, size_t maxLen) {
    // Read maxLen * 2 bytes for wide chars
    auto buffer = readMemory(address, maxLen * 2);
    if (!buffer) {
        return std::nullopt;
    }
    return buffer->readWideString(0, maxLen);
}

} // namespace lotro
