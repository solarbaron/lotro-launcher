/**
 * LOTRO Launcher - Account Config Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "AccountConfig.hpp"

#include <nlohmann/json.hpp>

namespace lotro {

AccountConfig AccountConfig::fromJson(const std::string& json) {
    AccountConfig config;
    
    try {
        auto j = nlohmann::json::parse(json);
        
        if (j.contains("username")) {
            config.username = j["username"].get<std::string>();
        }
        if (j.contains("displayName")) {
            config.displayName = j["displayName"].get<std::string>();
        }
        if (j.contains("lastUsedWorld")) {
            config.lastUsedWorld = j["lastUsedWorld"].get<std::string>();
        }
        if (j.contains("autoLogin")) {
            config.autoLogin = j["autoLogin"].get<bool>();
        }
        if (j.contains("worldPlayTimes") && j["worldPlayTimes"].is_object()) {
            for (auto& [key, value] : j["worldPlayTimes"].items()) {
                config.worldPlayTimes[key] = value.get<int64_t>();
            }
        }
        
    } catch (const std::exception& e) {
        // Return default on error
    }
    
    return config;
}

std::string AccountConfig::toJson() const {
    nlohmann::json j;
    
    j["username"] = username;
    j["displayName"] = displayName;
    j["lastUsedWorld"] = lastUsedWorld;
    j["autoLogin"] = autoLogin;
    
    nlohmann::json playTimes = nlohmann::json::object();
    for (const auto& [world, timestamp] : worldPlayTimes) {
        playTimes[world] = timestamp;
    }
    j["worldPlayTimes"] = playTimes;
    
    return j.dump(2);
}

} // namespace lotro
