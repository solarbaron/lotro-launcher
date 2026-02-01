/**
 * LOTRO Launcher - Account Configuration
 * 
 * User account settings and credential management.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <map>
#include <optional>
#include <string>
#include <cstdint>

namespace lotro {

/**
 * Account configuration
 * 
 * Note: Passwords are stored separately in the system keyring,
 * not in this configuration structure.
 * 
 * Based on OneLauncher's game_account_config.py
 */
struct AccountConfig {
    std::string username;
    std::string displayName;         // User-friendly name shown in UI
    std::string lastUsedWorld;       // Last server played on
    bool autoLogin = false;          // Whether to automatically login on startup
    
    // Map of world name -> last played Unix timestamp
    std::map<std::string, int64_t> worldPlayTimes;
    
    /**
     * Get the last played timestamp for a world
     * @return Unix timestamp, or 0 if never played
     */
    int64_t getWorldPlayTime(const std::string& worldName) const {
        auto it = worldPlayTimes.find(worldName);
        return it != worldPlayTimes.end() ? it->second : 0;
    }
    
    /**
     * Set the last played timestamp for a world
     */
    void setWorldPlayTime(const std::string& worldName, int64_t timestamp) {
        worldPlayTimes[worldName] = timestamp;
    }
    
    // Serialization
    static AccountConfig fromJson(const std::string& json);
    std::string toJson() const;
    
    // Comparison for uniqueness
    bool operator==(const AccountConfig& other) const {
        return username == other.username;
    }
};


/**
 * Account credentials - not persisted directly
 * 
 * This structure is used for passing credentials between
 * the credential store and the login system.
 */
struct AccountCredentials {
    std::string username;
    std::string password;
    
    bool isValid() const {
        return !username.empty() && !password.empty();
    }
};

} // namespace lotro
