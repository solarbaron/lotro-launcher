/**
 * LOTRO Launcher - Addon Manager
 * 
 * Central management of plugins, skins, and music addons.
 * Based on OneLauncher's addon_manager_window.py
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <QFuture>
#include <QString>

namespace lotro {

/**
 * Addon type enumeration
 */
enum class AddonType {
    Plugin,
    Skin,
    Music
};

/**
 * Addon installation status
 */
enum class AddonStatus {
    NotInstalled,
    Installed,
    UpdateAvailable,
    Installing,
    Error
};

/**
 * Addon information
 * 
 * Contains metadata from compendium files or lotrointerface.com
 */
struct AddonInfo {
    // Identification
    QString id;                      // Interface ID from lotrointerface.com
    QString name;
    AddonType type;
    
    // Metadata
    QString version;
    QString author;
    QString category;
    QString description;
    QString infoUrl;                 // Link to addon page
    QString downloadUrl;             // Direct download link
    
    // Additional metadata from lotrointerface.com
    QString releaseDate;             // Date of last update/release
    QString fileSize;                // File size string
    int downloadCount = 0;           // Number of downloads
    int favourites = 0;              // Number of favourites/likes
    
    // Dependencies and scripts
    std::vector<QString> dependencies;  // Interface IDs of required addons
    QString startupScript;              // Optional Python script path
    
    // For plugins: descriptor files
    std::vector<QString> descriptors;   // Paths to .plugin files
    
    // File path to the compendium file (for removal)
    std::filesystem::path compendiumFilePath;
    
    // Status
    AddonStatus status = AddonStatus::NotInstalled;
    QString installedVersion;
    QString latestVersion;
    
    /**
     * Check if an update is available
     */
    bool hasUpdate() const {
        return !installedVersion.isEmpty() && 
               !latestVersion.isEmpty() &&
               installedVersion != latestVersion;
    }
};

/**
 * Progress callback for addon operations
 */
using AddonProgressCallback = std::function<void(int current, int total, const QString& status)>;

/**
 * Addon Manager
 * 
 * Handles installation, removal, and updates of game addons.
 * Supports plugins, skins, and music from lotrointerface.com
 */
class AddonManager {
public:
    /**
     * Create addon manager for the specified game settings directory
     * 
     * @param settingsDir Game settings directory (contains Plugins, ui/skins, Music)
     */
    explicit AddonManager(const std::filesystem::path& settingsDir);
    ~AddonManager();
    
    // =====================
    // Installed addon queries
    // =====================
    
    /**
     * Get all installed addons of a type
     */
    std::vector<AddonInfo> getInstalledAddons(AddonType type) const;
    
    /**
     * Get a specific installed addon by ID
     */
    std::optional<AddonInfo> getInstalledAddon(const QString& id, AddonType type) const;
    
    /**
     * Refresh the list of installed addons
     */
    void refreshInstalledAddons();
    
    // =====================
    // Remote addon queries
    // =====================
    
    /**
     * Search for addons on lotrointerface.com
     */
    QFuture<std::vector<AddonInfo>> searchRemoteAddons(
        const QString& query,
        AddonType type
    );
    
    /**
     * Get addon info from lotrointerface.com by ID
     */
    QFuture<std::optional<AddonInfo>> getRemoteAddon(
        const QString& id,
        AddonType type
    );
    
    /**
     * Get the list of available addons from lotrointerface.com
     */
    QFuture<std::vector<AddonInfo>> fetchRemoteAddonList(AddonType type);
    
    // =====================
    // Installation/Removal
    // =====================
    
    /**
     * Install an addon from lotrointerface.com
     * 
     * @param id Addon interface ID
     * @param type Addon type
     * @param progress Progress callback
     * @return true if installation succeeded
     */
    QFuture<bool> installAddon(
        const QString& id,
        AddonType type,
        AddonProgressCallback progress = nullptr
    );
    
    /**
     * Install addon from a local zip file
     */
    bool installFromFile(
        const std::filesystem::path& zipPath,
        AddonType type
    );
    
    /**
     * Remove an installed addon
     */
    bool removeAddon(const QString& id, AddonType type);
    
    /**
     * Update an addon to the latest version
     */
    QFuture<bool> updateAddon(
        const QString& id,
        AddonType type,
        AddonProgressCallback progress = nullptr
    );
    
    /**
     * Update all addons with available updates
     */
    QFuture<int> updateAllAddons(
        AddonType type,
        AddonProgressCallback progress = nullptr
    );
    
    // =====================
    // Utility methods
    // =====================
    
    /**
     * Get the installation directory for an addon type
     */
    std::filesystem::path getAddonDirectory(AddonType type) const;
    
    /**
     * Parse a compendium file
     */
    std::optional<AddonInfo> parseCompendiumFile(const std::filesystem::path& path) const;
    
    /**
     * Get addons with enabled startup scripts
     */
    std::vector<AddonInfo> getAddonsWithStartupScripts() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * Get the lotrointerface.com URL for an addon type
 */
QString getLotroInterfaceUrl(AddonType type);

/**
 * Convert addon type to string
 */
QString addonTypeToString(AddonType type);

/**
 * Convert string to addon type
 */
AddonType stringToAddonType(const QString& str);

} // namespace lotro
