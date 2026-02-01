/**
 * LOTRO Launcher - Wine Config Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "WineConfig.hpp"

#include <nlohmann/json.hpp>

namespace lotro {

WineConfig WineConfig::fromJson(const std::string& json) {
    WineConfig config;
    
    try {
        auto j = nlohmann::json::parse(json);
        
        if (j.contains("prefixMode")) {
            std::string mode = j["prefixMode"].get<std::string>();
            if (mode == "Builtin") config.prefixMode = WinePrefixMode::Builtin;
            else if (mode == "User") config.prefixMode = WinePrefixMode::User;
        }
        if (j.contains("userWineExecutable")) {
            config.userWineExecutable = j["userWineExecutable"].get<std::string>();
        }
        if (j.contains("userPrefixPath")) {
            config.userPrefixPath = j["userPrefixPath"].get<std::string>();
        }
        if (j.contains("debugLevel")) {
            config.debugLevel = j["debugLevel"].get<std::string>();
        }
        if (j.contains("dxvkEnabled")) {
            config.dxvkEnabled = j["dxvkEnabled"].get<bool>();
        }
        if (j.contains("dxvkVersion")) {
            config.dxvkVersion = j["dxvkVersion"].get<std::string>();
        }
        if (j.contains("esyncEnabled")) {
            config.esyncEnabled = j["esyncEnabled"].get<bool>();
        }
        if (j.contains("fsyncEnabled")) {
            config.fsyncEnabled = j["fsyncEnabled"].get<bool>();
        }
        
    } catch (const std::exception& e) {
        // Return default on error
    }
    
    return config;
}

std::string WineConfig::toJson() const {
    nlohmann::json j;
    
    j["prefixMode"] = (prefixMode == WinePrefixMode::Builtin) ? "Builtin" : "User";
    j["userWineExecutable"] = userWineExecutable.string();
    j["userPrefixPath"] = userPrefixPath.string();
    j["debugLevel"] = debugLevel;
    j["dxvkEnabled"] = dxvkEnabled;
    j["dxvkVersion"] = dxvkVersion;
    j["esyncEnabled"] = esyncEnabled;
    j["fsyncEnabled"] = fsyncEnabled;
    
    return j.dump(2);
}

} // namespace lotro
