/**
 * LOTRO Launcher - Steam Integration Implementation (Linux only)
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef PLATFORM_LINUX

#include "SteamIntegration.hpp"

#include <dlfcn.h>
#include <fstream>
#include <cstdlib>

#include <spdlog/spdlog.h>

namespace lotro {

SteamIntegration::SteamIntegration() {
    m_available = loadSteamLibrary();
}

SteamIntegration::~SteamIntegration() {
    if (m_initialized) {
        shutdown();
    }
    unloadSteamLibrary();
}

SteamIntegration& SteamIntegration::instance() {
    static SteamIntegration instance;
    return instance;
}

bool SteamIntegration::loadSteamLibrary() {
    // Try common paths for libsteam_api.so
    std::vector<std::string> searchPaths = {
        "libsteam_api.so",  // System path / LD_LIBRARY_PATH
        "/usr/lib/libsteam_api.so",
        "/usr/lib64/libsteam_api.so",
        "/usr/local/lib/libsteam_api.so",
    };
    
    // Also check Steam runtime paths
    const char* home = std::getenv("HOME");
    if (home) {
        std::string homeStr(home);
        searchPaths.push_back(homeStr + "/.steam/sdk64/libsteam_api.so");
        searchPaths.push_back(homeStr + "/.steam/sdk32/libsteam_api.so");
        searchPaths.push_back(homeStr + "/.local/share/Steam/ubuntu12_64/libsteam_api.so");
        searchPaths.push_back(homeStr + "/.local/share/Steam/ubuntu12_32/libsteam_api.so");
        
        // Check common Steam library locations for games that ship libsteam_api.so
        std::vector<std::string> steamLibraries = {
            homeStr + "/.local/share/Steam/steamapps/common",
            homeStr + "/.steam/steam/steamapps/common",
            "/home/" + std::string(home).substr(std::string(home).rfind('/') + 1) + "/nvme/SteamLibrary/steamapps/common",
        };
        
        // Games known to ship libsteam_api.so
        std::vector<std::string> gameLibPaths = {
            "Baldurs Gate 3/bin/libsteam_api.so",
            "BeamNG.drive/BinLinux/libsteam_api.so",
            "PlagueInc/PlagueIncEvolved_Data/Plugins/libsteam_api.so",
        };
        
        for (const auto& lib : steamLibraries) {
            for (const auto& game : gameLibPaths) {
                searchPaths.push_back(lib + "/" + game);
            }
        }
    }
    
    for (const auto& path : searchPaths) {
        m_steamLib = dlopen(path.c_str(), RTLD_NOW);
        if (m_steamLib) {
            spdlog::info("Loaded Steam API from: {}", path);
            break;
        }
    }
    
    if (!m_steamLib) {
        spdlog::debug("Steam API library not found (Steam integration disabled)");
        return false;
    }
    
    // Resolve function pointers
    m_SteamAPI_Init = reinterpret_cast<SteamAPI_Init_t>(
        dlsym(m_steamLib, "SteamAPI_Init"));
    m_SteamAPI_Shutdown = reinterpret_cast<SteamAPI_Shutdown_t>(
        dlsym(m_steamLib, "SteamAPI_Shutdown"));
    m_SteamAPI_IsSteamRunning = reinterpret_cast<SteamAPI_IsSteamRunning_t>(
        dlsym(m_steamLib, "SteamAPI_IsSteamRunning"));
    m_SteamFriends = reinterpret_cast<SteamFriends_t>(
        dlsym(m_steamLib, "SteamFriends"));
    
    // These are methods on ISteamFriends interface - we'll handle differently
    // For now, just check the basic functions
    if (!m_SteamAPI_Init || !m_SteamAPI_Shutdown) {
        spdlog::warn("Steam API library loaded but missing required functions");
        dlclose(m_steamLib);
        m_steamLib = nullptr;
        return false;
    }
    
    spdlog::info("Steam API functions resolved successfully");
    return true;
}

void SteamIntegration::unloadSteamLibrary() {
    if (m_steamLib) {
        dlclose(m_steamLib);
        m_steamLib = nullptr;
    }
    m_SteamAPI_Init = nullptr;
    m_SteamAPI_Shutdown = nullptr;
    m_SteamAPI_IsSteamRunning = nullptr;
    m_SteamFriends = nullptr;
}

bool SteamIntegration::createAppIdFile(uint32_t appId) {
    // Create steam_appid.txt in the executable directory
    // This tells Steam which game we're running
    
    // Get executable path
    char exePath[4096];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len == -1) {
        spdlog::error("Failed to get executable path for steam_appid.txt");
        return false;
    }
    exePath[len] = '\0';
    
    m_appIdFilePath = std::filesystem::path(exePath).parent_path() / "steam_appid.txt";
    
    std::ofstream file(m_appIdFilePath);
    if (!file) {
        spdlog::error("Failed to create steam_appid.txt at: {}", m_appIdFilePath.string());
        return false;
    }
    
    file << appId;
    file.close();
    
    spdlog::debug("Created steam_appid.txt with App ID: {}", appId);
    return true;
}

void SteamIntegration::removeAppIdFile() {
    if (!m_appIdFilePath.empty() && std::filesystem::exists(m_appIdFilePath)) {
        std::error_code ec;
        std::filesystem::remove(m_appIdFilePath, ec);
        if (ec) {
            spdlog::warn("Failed to remove steam_appid.txt: {}", ec.message());
        }
    }
}

bool SteamIntegration::initialize(uint32_t appId) {
    if (m_initialized) {
        spdlog::debug("Steam already initialized");
        return true;
    }
    
    if (!m_available) {
        spdlog::debug("Steam integration not available");
        return false;
    }
    
    // Check if Steam is running
    if (m_SteamAPI_IsSteamRunning && !m_SteamAPI_IsSteamRunning()) {
        spdlog::info("Steam client is not running, skipping integration");
        return false;
    }
    
    // Create the app ID file
    if (!createAppIdFile(appId)) {
        return false;
    }
    
    // Initialize Steam API
    if (!m_SteamAPI_Init()) {
        spdlog::warn("SteamAPI_Init failed - Steam may not be running or app not owned");
        removeAppIdFile();
        return false;
    }
    
    m_initialized = true;
    spdlog::info("Steam integration initialized successfully (App ID: {})", appId);
    return true;
}

void SteamIntegration::shutdown() {
    if (!m_initialized) {
        return;
    }
    
    // Clear rich presence before shutdown
    clearRichPresence();
    
    // Shutdown Steam API
    if (m_SteamAPI_Shutdown) {
        m_SteamAPI_Shutdown();
    }
    
    // Clean up app ID file
    removeAppIdFile();
    
    m_initialized = false;
    spdlog::info("Steam integration shutdown");
}

bool SteamIntegration::setRichPresence(const std::string& status) {
    if (!m_initialized) {
        return false;
    }
    
    // Rich Presence requires ISteamFriends interface
    // For now, we just set the status via the environment
    // The actual Rich Presence would require more complex interface handling
    
    // Note: Full Rich Presence implementation would need to:
    // 1. Get ISteamFriends* via SteamFriends()
    // 2. Call SetRichPresence("steam_display", "#Status")
    // 3. Set up localized tokens in Steam partner backend
    
    // For this basic implementation, we rely on Steam showing "In Game"
    // which happens automatically when SteamAPI_Init succeeds
    
    spdlog::debug("Rich Presence status: {}", status);
    return true;
}

void SteamIntegration::clearRichPresence() {
    if (!m_initialized) {
        return;
    }
    
    // Would call ISteamFriends::ClearRichPresence() for full implementation
    spdlog::debug("Rich Presence cleared");
}

} // namespace lotro

#endif // PLATFORM_LINUX
