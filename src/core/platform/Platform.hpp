/**
 * LOTRO Launcher - Platform Abstraction
 * 
 * Cross-platform utilities for file paths, system info, etc.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace lotro {

/**
 * Platform abstraction layer
 * 
 * Provides platform-specific implementations for:
 * - Configuration paths
 * - Data directories
 * - System capabilities
 */
class Platform {
public:
    /**
     * Get the configuration directory path
     * 
     * Linux:   ~/.config/lotro-launcher/
     * Windows: %APPDATA%\lotro-launcher\
     * macOS:   ~/Library/Application Support/lotro-launcher/
     */
    static std::filesystem::path getConfigPath();
    
    /**
     * Get the data directory path (for caches, downloads, etc.)
     * 
     * Linux:   ~/.local/share/lotro-launcher/
     * Windows: %LOCALAPPDATA%\lotro-launcher\
     * macOS:   ~/Library/Application Support/lotro-launcher/
     */
    static std::filesystem::path getDataPath();
    
    /**
     * Get the cache directory path
     * 
     * Linux:   ~/.cache/lotro-launcher/
     * Windows: %LOCALAPPDATA%\lotro-launcher\cache\
     * macOS:   ~/Library/Caches/lotro-launcher/
     */
    static std::filesystem::path getCachePath();
    
    /**
     * Get the user's Documents directory
     * 
     * This is where LOTRO stores its settings/addons by default
     */
    static std::filesystem::path getDocumentsPath();
    
    /**
     * Detect existing LOTRO installation paths
     * 
     * Searches common locations for LOTRO installations
     */
    static std::vector<std::filesystem::path> detectGameInstallations();
    
    /**
     * Get the default LOTRO settings directory
     * 
     * Usually Documents/The Lord of the Rings Online/
     */
    static std::filesystem::path getDefaultLotroSettingsPath();
    
    /**
     * Check if running on Linux
     */
    static constexpr bool isLinux() {
#ifdef PLATFORM_LINUX
        return true;
#else
        return false;
#endif
    }
    
    /**
     * Check if running on Windows
     */
    static constexpr bool isWindows() {
#ifdef PLATFORM_WINDOWS
        return true;
#else
        return false;
#endif
    }
    
    /**
     * Get system open file limit (Linux only, for esync)
     */
    static std::optional<size_t> getOpenFileLimit();
    
    /**
     * Open a URL in the default browser
     */
    static bool openUrl(const std::string& url);
    
    /**
     * Open a file manager at the specified path
     */
    static bool openFileManager(const std::filesystem::path& path);
};

} // namespace lotro
