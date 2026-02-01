/**
 * LOTRO Launcher - Game Config Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "GameConfig.hpp"

#include <nlohmann/json.hpp>

namespace lotro {

std::filesystem::path GameConfig::getClientExecutable() const {
    std::string clientName;
    
    switch (clientType) {
        case ClientType::Win64:
            clientName = "x64/lotroclient64.exe";
            break;
        case ClientType::Win32:
            clientName = "lotroclient.exe";
            break;
        case ClientType::Win32Legacy:
            clientName = "lotroclient_awesomium.exe";
            break;
    }
    
    return gameDirectory / clientName;
}

std::filesystem::path GameConfig::getPluginsDirectory() const {
    return settingsDirectory / "Plugins";
}

std::filesystem::path GameConfig::getSkinsDirectory() const {
    return settingsDirectory / "ui" / "skins";
}

std::filesystem::path GameConfig::getMusicDirectory() const {
    return settingsDirectory / "Music";
}

std::filesystem::path GameConfig::getUserPreferencesPath() const {
    if (clientType == ClientType::Win64) {
        return settingsDirectory / "UserPreferences64.ini";
    }
    return settingsDirectory / "UserPreferences.ini";
}

GameConfig GameConfig::fromJson(const std::string& json) {
    GameConfig config;
    
    try {
        auto j = nlohmann::json::parse(json);
        
        if (j.contains("id")) {
            config.id = j["id"].get<std::string>();
        }
        if (j.contains("gameType")) {
            std::string type = j["gameType"].get<std::string>();
            if (type == "LOTRO") config.gameType = GameType::LOTRO;
            else if (type == "LOTRO_Preview") config.gameType = GameType::LOTRO_Preview;
            else if (type == "DDO") config.gameType = GameType::DDO;
            else if (type == "DDO_Preview") config.gameType = GameType::DDO_Preview;
        }
        if (j.contains("gameDirectory")) {
            config.gameDirectory = j["gameDirectory"].get<std::string>();
        }
        if (j.contains("settingsDirectory")) {
            config.settingsDirectory = j["settingsDirectory"].get<std::string>();
        }
        if (j.contains("clientType")) {
            std::string type = j["clientType"].get<std::string>();
            if (type == "Win64") config.clientType = ClientType::Win64;
            else if (type == "Win32") config.clientType = ClientType::Win32;
            else if (type == "Win32Legacy") config.clientType = ClientType::Win32Legacy;
        }
        if (j.contains("highResEnabled")) {
            config.highResEnabled = j["highResEnabled"].get<bool>();
        }
        if (j.contains("locale")) {
            config.locale = j["locale"].get<std::string>();
        }
        if (j.contains("newsfeedUrl")) {
            config.newsfeedUrl = j["newsfeedUrl"].get<std::string>();
        }
        
    } catch (const std::exception& e) {
        // Return default config on parse error
    }
    
    return config;
}

std::string GameConfig::toJson() const {
    nlohmann::json j;
    
    j["id"] = id;
    
    switch (gameType) {
        case GameType::LOTRO: j["gameType"] = "LOTRO"; break;
        case GameType::LOTRO_Preview: j["gameType"] = "LOTRO_Preview"; break;
        case GameType::DDO: j["gameType"] = "DDO"; break;
        case GameType::DDO_Preview: j["gameType"] = "DDO_Preview"; break;
    }
    
    j["gameDirectory"] = gameDirectory.string();
    j["settingsDirectory"] = settingsDirectory.string();
    
    switch (clientType) {
        case ClientType::Win64: j["clientType"] = "Win64"; break;
        case ClientType::Win32: j["clientType"] = "Win32"; break;
        case ClientType::Win32Legacy: j["clientType"] = "Win32Legacy"; break;
    }
    
    j["highResEnabled"] = highResEnabled;
    j["locale"] = locale;
    j["newsfeedUrl"] = newsfeedUrl;
    
    return j.dump(2);
}

} // namespace lotro
