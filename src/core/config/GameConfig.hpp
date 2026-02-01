/**
 * LOTRO Launcher - Game Configuration
 * 
 * Per-game settings including installation path, client type, and locale.
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
 * Game type enumeration
 */
enum class GameType {
    LOTRO,
    LOTRO_Preview,
    DDO,
    DDO_Preview
};

/**
 * Client type enumeration
 */
enum class ClientType {
    Win64,
    Win32,
    Win32Legacy
};

/**
 * Per-game configuration
 * 
 * Based on OneLauncher's game_config.py
 */
struct GameConfig {
    // Identification
    std::string id;                                    // Unique identifier
    GameType gameType = GameType::LOTRO;
    
    // Installation
    std::filesystem::path gameDirectory;               // Game install directory
    std::filesystem::path settingsDirectory;           // Where saves/addons go
    
    // Client settings
    ClientType clientType = ClientType::Win64;
    bool highResEnabled = true;
    std::string locale = "en";
    
    // Launcher files
    std::string standardLauncherFilename = "LotroLauncher.exe";
    std::string patchClientFilename = "patchclient.dll";
    
    // Network
    std::string newsfeedUrl;
    
    // Utility methods
    std::filesystem::path getClientExecutable() const;
    std::filesystem::path getPluginsDirectory() const;
    std::filesystem::path getSkinsDirectory() const;
    std::filesystem::path getMusicDirectory() const;
    std::filesystem::path getUserPreferencesPath() const;
    
    // Serialization
    static GameConfig fromJson(const std::string& json);
    std::string toJson() const;
};

/**
 * Game-specific launcher configuration fetched from game servers
 * 
 * Based on OneLauncher's game_launcher_config.py
 */
struct GameLauncherConfig {
    std::string worldQueueUrl;
    std::string worldQueueParams;
    std::string patchServer;
    std::string launcherArgTemplate;
    std::string newsfeedUrl;
    std::string bannerImageUrl;
    
    // Splashscreen and download info
    struct DownloadFile {
        std::string url;
        std::string localPath;
        std::string hash;
    };
    std::vector<DownloadFile> downloads;
};

/**
 * Local game state that changes frequently
 */
struct GameLocalState {
    std::string lastPlayedAccount;
    std::string lastPlayedWorld;
    std::chrono::system_clock::time_point lastLaunchTime;
    bool patchingRequired = false;
};

} // namespace lotro
