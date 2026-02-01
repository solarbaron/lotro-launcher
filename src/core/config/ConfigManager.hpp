/**
 * LOTRO Launcher - Configuration Manager
 * 
 * Manages application and game configuration files.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "GameConfig.hpp"
#include "AccountConfig.hpp"
#include "WineConfig.hpp"

namespace lotro {

/**
 * Program-wide settings
 */
struct ProgramConfig {
    std::string defaultLocale = "en";
    bool alwaysUseDefaultLocaleForUI = false;
    std::string gamesSortingMode = "last-played";  // priority, last-played, alphabetical
    std::string onGameStart = "stay";              // stay, close
    std::string logVerbosity = "info";             // debug, info, warning, error
};

/**
 * Central configuration manager
 * 
 * Handles loading, saving, and providing access to all configuration data.
 * Based on OneLauncher's config_manager.py
 */
class ConfigManager {
public:
    static ConfigManager& instance();
    
    // Lifecycle
    bool initialize(const std::filesystem::path& configDirectory);
    bool save();
    
    // State queries
    bool isFirstRun() const { return m_isFirstRun; }
    const std::filesystem::path& configDirectory() const { return m_configDirectory; }
    const std::filesystem::path& gamesDirectory() const { return m_gamesDirectory; }
    
    // Program config
    const ProgramConfig& programConfig() const { return m_programConfig; }
    void setProgramConfig(const ProgramConfig& config);
    
    // Game configs
    std::vector<std::string> getGameIds() const;
    std::optional<GameConfig> getGameConfig(const std::string& gameId) const;
    void setGameConfig(const std::string& gameId, const GameConfig& config);
    void removeGameConfig(const std::string& gameId);
    
    // Account configs (per game)
    std::vector<AccountConfig> getAccounts(const std::string& gameId) const;
    void setAccounts(const std::string& gameId, const std::vector<AccountConfig>& accounts);
    void addAccount(const std::string& gameId, const AccountConfig& account);
    void removeAccount(const std::string& gameId, const std::string& username);
    
#ifdef PLATFORM_LINUX
    // Wine config (per game, Linux only)
    std::optional<WineConfig> getWineConfig(const std::string& gameId) const;
    void setWineConfig(const std::string& gameId, const WineConfig& config);
#endif

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    bool loadProgramConfig();
    bool saveProgramConfig();
    bool loadGameConfigs();
    bool saveGameConfigs();
    
    std::filesystem::path m_configDirectory;
    std::filesystem::path m_gamesDirectory;
    bool m_isFirstRun = true;
    
    ProgramConfig m_programConfig;
    std::map<std::string, GameConfig> m_gameConfigs;
    std::map<std::string, std::vector<AccountConfig>> m_accountConfigs;
    
#ifdef PLATFORM_LINUX
    std::map<std::string, WineConfig> m_wineConfigs;
#endif
};

} // namespace lotro
