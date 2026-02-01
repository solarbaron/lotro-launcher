/**
 * LOTRO Launcher - Wine Prefix Setup (Linux only)
 * 
 * Handles WINEPREFIX initialization and configuration.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifdef PLATFORM_LINUX

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace lotro {

/**
 * Wine prefix setup and configuration utilities
 */
class WinePrefixSetup {
public:
    /**
     * Create a new WINEPREFIX at the specified path
     * 
     * @param prefixPath Path for the new prefix
     * @param wineExecutable Path to wine binary
     * @param is64bit Create a 64-bit prefix (default: true)
     * @return true if prefix created successfully
     */
    static bool createPrefix(
        const std::filesystem::path& prefixPath,
        const std::filesystem::path& wineExecutable,
        bool is64bit = true
    );
    
    /**
     * Validate an existing WINEPREFIX
     * 
     * @param prefixPath Path to the prefix
     * @return true if prefix appears valid
     */
    static bool validatePrefix(const std::filesystem::path& prefixPath);
    
    /**
     * Get the system32 directory path within a prefix
     */
    static std::filesystem::path getSystem32Path(
        const std::filesystem::path& prefixPath
    );
    
    /**
     * Get the syswow64 directory path within a 64-bit prefix
     */
    static std::filesystem::path getSysWow64Path(
        const std::filesystem::path& prefixPath
    );
    
    /**
     * Install DLLs to the prefix's system directories
     * 
     * @param prefixPath Path to the prefix
     * @param dlls Map of DLL name to source path
     * @param targetDir Target directory (system32 or syswow64)
     */
    static bool installDlls(
        const std::filesystem::path& prefixPath,
        const std::vector<std::pair<std::string, std::filesystem::path>>& dlls,
        const std::filesystem::path& targetDir
    );
    
    /**
     * Set a DLL override in the prefix registry
     * 
     * @param prefixPath Path to the prefix
     * @param dllName DLL name (without .dll extension)
     * @param mode Override mode: "native", "builtin", "native,builtin", etc.
     */
    static bool setDllOverride(
        const std::filesystem::path& prefixPath,
        const std::filesystem::path& wineExecutable,
        const std::string& dllName,
        const std::string& mode
    );
    
    /**
     * Set a Windows registry value
     */
    static bool setRegistryValue(
        const std::filesystem::path& prefixPath,
        const std::filesystem::path& wineExecutable,
        const std::string& key,
        const std::string& valueName,
        const std::string& value,
        const std::string& type = "REG_SZ"
    );
    
    /**
     * Create symlink from prefix Documents to real Documents
     * 
     * This ensures game saves and addons go to the expected location.
     * Similar to what Proton does automatically.
     */
    static bool symlinkDocuments(
        const std::filesystem::path& prefixPath,
        const std::filesystem::path& realDocuments
    );
};

} // namespace lotro

#endif // PLATFORM_LINUX
