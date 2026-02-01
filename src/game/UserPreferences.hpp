/**
 * LOTRO Launcher - User Preferences
 * 
 * Handling of LOTRO's UserPreferences.ini file.
 * Based on OneLauncher's update_game_user_preferences function.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include <QString>

namespace lotro {

/**
 * UserPreferences.ini manager
 * 
 * Reads and modifies the game's UserPreferences.ini file which controls
 * graphics settings, audio, and other client options.
 */
class UserPreferences {
public:
    /**
     * Load preferences from file
     */
    explicit UserPreferences(const std::filesystem::path& path);
    
    /**
     * Check if preferences were loaded successfully
     */
    bool isValid() const { return m_valid; }
    
    /**
     * Get a preference value
     * 
     * @param section Section name (e.g., "Display")
     * @param key Key name (e.g., "Adapter")
     * @return Value if found
     */
    std::optional<QString> get(
        const QString& section,
        const QString& key
    ) const;
    
    /**
     * Set a preference value
     * 
     * @param section Section name
     * @param key Key name
     * @param value Value to set
     */
    void set(
        const QString& section,
        const QString& key,
        const QString& value
    );
    
    /**
     * Remove a preference
     */
    void remove(const QString& section, const QString& key);
    
    /**
     * Save preferences back to file
     */
    bool save();
    
    /**
     * Save preferences to a different file
     */
    bool saveAs(const std::filesystem::path& path);
    
    /**
     * Get the file path
     */
    const std::filesystem::path& path() const { return m_path; }
    
    // =========================
    // Convenience methods for common settings
    // =========================
    
    /**
     * Set the graphics adapter index
     */
    void setAdapter(int adapterIndex);
    
    /**
     * Set the display resolution
     */
    void setResolution(int width, int height);
    
    /**
     * Set fullscreen mode
     */
    void setFullscreen(bool fullscreen);
    
    /**
     * Set the graphics quality preset
     */
    void setGraphicsQuality(int quality);
    
    /**
     * Get the last selected world
     */
    std::optional<QString> getLastWorld() const;
    
    /**
     * Set the last selected world
     */
    void setLastWorld(const QString& world);
    
private:
    bool load();
    
    std::filesystem::path m_path;
    bool m_valid = false;
    
    // Section -> (Key -> Value)
    std::map<QString, std::map<QString, QString>> m_data;
};

/**
 * Find the UserPreferences.ini file for a game
 * 
 * @param settingsDir Game settings directory
 * @param is64bit Looking for 64-bit preferences
 * @return Path to preferences file if found
 */
std::optional<std::filesystem::path> findUserPreferences(
    const std::filesystem::path& settingsDir,
    bool is64bit = true
);

} // namespace lotro
