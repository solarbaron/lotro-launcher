/**
 * LOTRO Launcher - Steam Integration (Linux only)
 * 
 * Provides optional integration with Steam client to show "Playing" status
 * in Steam friends list when launching the game through this launcher.
 * 
 * Uses dlopen/dlsym to dynamically load libsteam_api.so at runtime,
 * so there's no hard dependency on the Steamworks SDK.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifdef PLATFORM_LINUX

#include <filesystem>
#include <string>

namespace lotro {

/**
 * Steam App IDs for SSG games
 */
namespace SteamAppIds {
    constexpr uint32_t LOTRO = 212500;
    constexpr uint32_t DDO = 206480;
}

/**
 * Steam integration singleton
 * 
 * Provides functionality to:
 * - Show game as "Playing" in Steam friends list
 * - Set Rich Presence status text
 * 
 * The Steam client must be running for this to work.
 * If Steam is not available, all methods gracefully return false/no-op.
 * 
 * Usage:
 *   auto& steam = SteamIntegration::instance();
 *   if (steam.initialize()) {
 *       // Game will now show as "Playing" in Steam
 *       steam.setRichPresence("Exploring Middle-earth");
 *   }
 *   // ... game runs ...
 *   steam.shutdown();
 */
class SteamIntegration {
public:
    static SteamIntegration& instance();
    
    /**
     * Check if Steam integration is available
     * 
     * Returns true if:
     * - libsteam_api.so was loaded successfully
     * - Required API functions were resolved
     * 
     * Does NOT check if Steam client is running.
     */
    bool isAvailable() const { return m_available; }
    
    /**
     * Check if currently initialized with Steam
     */
    bool isInitialized() const { return m_initialized; }
    
    /**
     * Initialize Steam integration
     * 
     * Creates steam_appid.txt and calls SteamAPI_Init().
     * If successful, the game will appear as "Playing" in Steam.
     * 
     * @param appId Steam App ID (default: LOTRO)
     * @return true if initialization succeeded
     */
    bool initialize(uint32_t appId = SteamAppIds::LOTRO);
    
    /**
     * Shutdown Steam integration
     * 
     * Calls SteamAPI_Shutdown() and cleans up.
     * The "Playing" status will be cleared.
     */
    void shutdown();
    
    /**
     * Set Rich Presence status text
     * 
     * @param status Status text to show (e.g., "Exploring Middle-earth")
     * @return true if set successfully
     */
    bool setRichPresence(const std::string& status);
    
    /**
     * Clear Rich Presence status
     */
    void clearRichPresence();
    
private:
    SteamIntegration();
    ~SteamIntegration();
    SteamIntegration(const SteamIntegration&) = delete;
    SteamIntegration& operator=(const SteamIntegration&) = delete;
    
    bool loadSteamLibrary();
    void unloadSteamLibrary();
    bool createAppIdFile(uint32_t appId);
    void removeAppIdFile();
    
    // Dynamic library handle
    void* m_steamLib = nullptr;
    
    // API function pointers
    using SteamAPI_Init_t = bool (*)();
    using SteamAPI_Shutdown_t = void (*)();
    using SteamAPI_IsSteamRunning_t = bool (*)();
    using SteamFriends_t = void* (*)();
    using SetRichPresence_t = bool (*)(void*, const char*, const char*);
    using ClearRichPresence_t = void (*)(void*);
    
    SteamAPI_Init_t m_SteamAPI_Init = nullptr;
    SteamAPI_Shutdown_t m_SteamAPI_Shutdown = nullptr;
    SteamAPI_IsSteamRunning_t m_SteamAPI_IsSteamRunning = nullptr;
    SteamFriends_t m_SteamFriends = nullptr;
    SetRichPresence_t m_SetRichPresence = nullptr;
    ClearRichPresence_t m_ClearRichPresence = nullptr;
    
    bool m_available = false;
    bool m_initialized = false;
    std::filesystem::path m_appIdFilePath;
};

} // namespace lotro

#endif // PLATFORM_LINUX
