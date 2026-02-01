/**
 * LOTRO Launcher - Wine Configuration (Linux only)
 * 
 * Wine prefix and environment settings for running the game on Linux.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace lotro {

/**
 * Wine prefix management mode
 */
enum class WinePrefixMode {
    Builtin,    // Launcher manages Wine and prefix via umu-run
    Steam,      // Use existing Steam/Proton prefix
    User        // User provides Wine and prefix
};

/**
 * Wine configuration
 * 
 * Based on OneLauncher's wine_environment.py WineEnvironment class
 */
struct WineConfig {
    // Mode selection
    WinePrefixMode prefixMode = WinePrefixMode::Builtin;  // Default to launcher's own prefix
    
    // Steam prefix path (auto-detected or user-specified)
    std::filesystem::path steamPrefixPath;              // Steam compatdata prefix path
    
    // User-provided paths (only used when prefixMode == User)
    std::filesystem::path userWineExecutable;    // Path to wine binary
    std::filesystem::path userPrefixPath;        // Path to WINEPREFIX
    
    // Debug settings
    std::string debugLevel;                      // WINEDEBUG value (e.g., "-all")
    
    // DXVK settings
    bool dxvkEnabled = true;
    std::string dxvkVersion;                     // Specific version or empty for latest
    
    // Performance tweaks
    bool esyncEnabled = true;
    bool fsyncEnabled = true;
    
    // Serialization
    static WineConfig fromJson(const std::string& json);
    std::string toJson() const;
};

/**
 * Wine version information
 */
struct WineVersion {
    std::string name;           // e.g., "GE-Proton8-25"
    std::string downloadUrl;
    std::string sha256;
    bool isProton = false;
};

/**
 * DXVK version information
 */
struct DxvkVersion {
    std::string version;        // e.g., "2.3"
    std::string downloadUrl;
    std::string sha256;
};

} // namespace lotro
