/**
 * LOTRO Launcher - Configuration Manager Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ConfigManager.hpp"
#include "GameConfig.hpp"

#include <fstream>

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

namespace lotro {

namespace {
    constexpr const char* PROGRAM_CONFIG_FILE = "config.json";
    constexpr const char* GAMES_DIR = "games";
}

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::initialize(const std::filesystem::path& configDirectory) {
    m_configDirectory = configDirectory;
    m_gamesDirectory = configDirectory / GAMES_DIR;
    
    // Create directories if they don't exist
    try {
        std::filesystem::create_directories(m_configDirectory);
        std::filesystem::create_directories(m_gamesDirectory);
    } catch (const std::exception& e) {
        spdlog::error("Failed to create config directories: {}", e.what());
        return false;
    }
    
    // Check if this is first run
    m_isFirstRun = !std::filesystem::exists(m_configDirectory / PROGRAM_CONFIG_FILE);
    
    if (!m_isFirstRun) {
        // Load existing configuration
        if (!loadProgramConfig()) {
            spdlog::warn("Failed to load program config, using defaults");
        }
        if (!loadGameConfigs()) {
            spdlog::warn("Failed to load game configs");
        }
    }
    
    spdlog::info("ConfigManager initialized at: {}", m_configDirectory.string());
    return true;
}

bool ConfigManager::save() {
    return saveProgramConfig() && saveGameConfigs();
}

void ConfigManager::setProgramConfig(const ProgramConfig& config) {
    m_programConfig = config;
    saveProgramConfig();
}

std::vector<std::string> ConfigManager::getGameIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_gameConfigs.size());
    for (const auto& [id, config] : m_gameConfigs) {
        ids.push_back(id);
    }
    return ids;
}

std::optional<GameConfig> ConfigManager::getGameConfig(const std::string& gameId) const {
    auto it = m_gameConfigs.find(gameId);
    if (it != m_gameConfigs.end()) {
        return it->second;
    }
    return std::nullopt;
}

void ConfigManager::setGameConfig(const std::string& gameId, const GameConfig& config) {
    m_gameConfigs[gameId] = config;
    saveGameConfigs();
}

void ConfigManager::removeGameConfig(const std::string& gameId) {
    m_gameConfigs.erase(gameId);
    saveGameConfigs();
}

std::vector<AccountConfig> ConfigManager::getAccounts(const std::string& gameId) const {
    auto it = m_accountConfigs.find(gameId);
    if (it != m_accountConfigs.end()) {
        return it->second;
    }
    return {};
}

void ConfigManager::setAccounts(const std::string& gameId, const std::vector<AccountConfig>& accounts) {
    m_accountConfigs[gameId] = accounts;
    saveGameConfigs();
}

void ConfigManager::addAccount(const std::string& gameId, const AccountConfig& account) {
    auto& accounts = m_accountConfigs[gameId];
    
    // Check if account already exists
    auto it = std::find(accounts.begin(), accounts.end(), account);
    if (it != accounts.end()) {
        *it = account;  // Update existing
    } else {
        accounts.push_back(account);
    }
    saveGameConfigs();
}

void ConfigManager::removeAccount(const std::string& gameId, const std::string& username) {
    auto it = m_accountConfigs.find(gameId);
    if (it != m_accountConfigs.end()) {
        auto& accounts = it->second;
        accounts.erase(
            std::remove_if(accounts.begin(), accounts.end(),
                [&username](const AccountConfig& acc) {
                    return acc.username == username;
                }),
            accounts.end()
        );
        saveGameConfigs();
    }
}

#ifdef PLATFORM_LINUX
std::optional<WineConfig> ConfigManager::getWineConfig(const std::string& gameId) const {
    auto it = m_wineConfigs.find(gameId);
    if (it != m_wineConfigs.end()) {
        return it->second;
    }
    return std::nullopt;
}

void ConfigManager::setWineConfig(const std::string& gameId, const WineConfig& config) {
    m_wineConfigs[gameId] = config;
    saveGameConfigs();
}
#endif

bool ConfigManager::loadProgramConfig() {
    auto configPath = m_configDirectory / PROGRAM_CONFIG_FILE;
    
    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            return false;
        }
        
        nlohmann::json j = nlohmann::json::parse(file);
        
        if (j.contains("defaultLocale")) {
            m_programConfig.defaultLocale = j["defaultLocale"].get<std::string>();
        }
        if (j.contains("alwaysUseDefaultLocaleForUI")) {
            m_programConfig.alwaysUseDefaultLocaleForUI = j["alwaysUseDefaultLocaleForUI"].get<bool>();
        }
        if (j.contains("gamesSortingMode")) {
            m_programConfig.gamesSortingMode = j["gamesSortingMode"].get<std::string>();
        }
        if (j.contains("onGameStart")) {
            m_programConfig.onGameStart = j["onGameStart"].get<std::string>();
        }
        if (j.contains("logVerbosity")) {
            m_programConfig.logVerbosity = j["logVerbosity"].get<std::string>();
        }
#ifdef PLATFORM_LINUX
        if (j.contains("steamIntegrationEnabled")) {
            m_programConfig.steamIntegrationEnabled = j["steamIntegrationEnabled"].get<bool>();
        }
#endif
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to load program config: {}", e.what());
        return false;
    }
}

bool ConfigManager::saveProgramConfig() {
    auto configPath = m_configDirectory / PROGRAM_CONFIG_FILE;
    
    try {
        nlohmann::json j;
        j["defaultLocale"] = m_programConfig.defaultLocale;
        j["alwaysUseDefaultLocaleForUI"] = m_programConfig.alwaysUseDefaultLocaleForUI;
        j["gamesSortingMode"] = m_programConfig.gamesSortingMode;
        j["onGameStart"] = m_programConfig.onGameStart;
        j["logVerbosity"] = m_programConfig.logVerbosity;
#ifdef PLATFORM_LINUX
        j["steamIntegrationEnabled"] = m_programConfig.steamIntegrationEnabled;
#endif
        
        std::ofstream file(configPath);
        file << j.dump(2);
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to save program config: {}", e.what());
        return false;
    }
}

bool ConfigManager::loadGameConfigs() {
    // Load each game config from games directory
    try {
        for (const auto& entry : std::filesystem::directory_iterator(m_gamesDirectory)) {
            if (entry.is_directory()) {
                auto gameId = entry.path().filename().string();
                
                // Load game config
                auto configFile = entry.path() / "config.json";
                if (std::filesystem::exists(configFile)) {
                    std::ifstream file(configFile);
                    if (file.is_open()) {
                        std::string content((std::istreambuf_iterator<char>(file)),
                                            std::istreambuf_iterator<char>());
                        
                        auto config = GameConfig::fromJson(content);
                        if (!config.gameDirectory.empty()) {
                            config.id = gameId;
                            m_gameConfigs[gameId] = config;
                            spdlog::debug("Loaded game config: {} ({})", 
                                         gameId, config.gameDirectory.string());
                        } else {
                            spdlog::warn("Game config {} has empty game directory", gameId);
                        }
                    }
                }
                
                // Load account configs
                auto accountsFile = entry.path() / "accounts.json";
                if (std::filesystem::exists(accountsFile)) {
                    std::ifstream file(accountsFile);
                    if (file.is_open()) {
                        try {
                            nlohmann::json j = nlohmann::json::parse(file);
                            std::vector<AccountConfig> accounts;
                            
                            if (j.is_array()) {
                                for (const auto& accountJson : j) {
                                    auto account = AccountConfig::fromJson(accountJson.dump());
                                    if (!account.username.empty()) {
                                        accounts.push_back(account);
                                    }
                                }
                            }
                            
                            if (!accounts.empty()) {
                                m_accountConfigs[gameId] = accounts;
                                spdlog::debug("Loaded {} accounts for game: {}", 
                                             accounts.size(), gameId);
                            }
                        } catch (const std::exception& e) {
                            spdlog::warn("Failed to parse accounts.json for {}: {}", 
                                        gameId, e.what());
                        }
                    }
                }
            }
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to load game configs: {}", e.what());
        return false;
    }
}

bool ConfigManager::saveGameConfigs() {
    try {
        for (const auto& [gameId, config] : m_gameConfigs) {
            auto gameDir = m_gamesDirectory / gameId;
            std::filesystem::create_directories(gameDir);
            
            // Save game config
            auto configFile = gameDir / "config.json";
            std::ofstream file(configFile);
            file << config.toJson();
            
            spdlog::debug("Saved game config: {} -> {}", gameId, configFile.string());
        }
        
        // Save account configs (includes games that might not have a game config yet)
        for (const auto& [gameId, accounts] : m_accountConfigs) {
            if (accounts.empty()) {
                continue;
            }
            
            auto gameDir = m_gamesDirectory / gameId;
            std::filesystem::create_directories(gameDir);
            
            nlohmann::json accountsJson = nlohmann::json::array();
            for (const auto& account : accounts) {
                accountsJson.push_back(nlohmann::json::parse(account.toJson()));
            }
            
            auto accountsFile = gameDir / "accounts.json";
            std::ofstream file(accountsFile);
            file << accountsJson.dump(2);
            
            spdlog::debug("Saved {} accounts for game: {} -> {}", 
                         accounts.size(), gameId, accountsFile.string());
        }
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to save game configs: {}", e.what());
        return false;
    }
}

} // namespace lotro
