/**
 * LOTRO Launcher - LotroInterface.com Client
 * 
 * Network client for fetching addon information from lotrointerface.com API.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <QFuture>
#include <QString>

#include "addons/AddonManager.hpp"

namespace lotro {

/**
 * Remote addon information from lotrointerface.com
 */
struct RemoteAddonInfo {
    QString interfaceId;      // Unique ID on lotrointerface.com
    QString name;
    QString category;
    QString version;
    QString author;
    QString latestRelease;    // Date of latest release
    QString downloadUrl;      // Direct download URL
    QString infoUrl;          // Page URL on lotrointerface.com
    AddonType type;
    
    // Additional metadata
    int downloads = 0;        // Download count
    int favourites = 0;       // Favourites/likes
    QString fileSize;         // File size string
};

/**
 * Client for lotrointerface.com API
 * 
 * Fetches addon lists and downloads addons from lotrointerface.com
 */
class LotroInterfaceClient {
public:
    LotroInterfaceClient();
    ~LotroInterfaceClient();
    
    /**
     * API endpoints for addon lists
     */
    static QString getPluginsUrl() { return "https://api.lotrointerface.com/fav/OneLauncher-Plugins.xml"; }
    static QString getSkinsUrl()   { return "https://api.lotrointerface.com/fav/OneLauncher-Themes.xml"; }
    static QString getMusicUrl()   { return "https://api.lotrointerface.com/fav/OneLauncher-Music.xml"; }
    
    /**
     * Fetch list of available addons from lotrointerface.com
     * 
     * @param type Type of addons to fetch
     * @return Vector of remote addon info
     */
    QFuture<std::vector<RemoteAddonInfo>> fetchAddonList(AddonType type);
    
    /**
     * Download an addon archive to a temporary file
     * 
     * @param downloadUrl URL of the addon archive
     * @param progressCallback Called with (bytesReceived, bytesTotal)
     * @return Path to downloaded file, or empty if failed
     */
    QFuture<QString> downloadAddon(
        const QString& downloadUrl,
        std::function<void(qint64, qint64)> progressCallback = nullptr
    );
    
    /**
     * Get addon info page URL
     */
    static QString getAddonPageUrl(const QString& interfaceId);
    
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace lotro
