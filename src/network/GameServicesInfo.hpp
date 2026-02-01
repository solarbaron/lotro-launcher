/**
 * LOTRO Launcher - Game Services Info
 * 
 * Fetches game service endpoints and configuration.
 * Based on OneLauncher's network/game_services_info.py
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <QFuture>
#include <QString>

namespace lotro {

/**
 * Basic world info from GetDatacenters response
 */
struct WorldInfo {
    QString name;           // Server name (e.g., "Mordor[EU]")
    QString statusUrl;      // URL to check status
    QString chatServerUrl;  // Chat server address
    int order = 0;          // Display order
    QString language;       // Server language
};

/**
 * Game services configuration
 * 
 * Contains URLs and settings fetched from SSG servers
 */
struct GameServicesInfo {
    QString authServer;          // GLS authentication server
    QString patchServer;         // Patch server URL
    QString launcherConfigUrl;   // Launcher configuration URL
    QString newsUrl;             // News feed URL
    QString supportUrl;          // Support page URL
    QString datacenterName;      // e.g., "LOTRO"
    QString datacenterGameName;  // e.g., "LOTRO"
    QString datacenterServiceUrl;// Original service URL
    
    std::vector<WorldInfo> worlds;  // Available worlds
    
    bool isValid() const {
        return !authServer.isEmpty() && !patchServer.isEmpty();
    }
};

/**
 * Known datacenter configuration URLs
 */
namespace DatacenterUrls {
    // LOTRO
    constexpr const char* LOTRO = 
        "http://gls.lotro.com/GLS.DataCenterServer/Service.asmx";
    constexpr const char* LOTRO_PREVIEW = 
        "http://gls-bullroarer.lotro.com/GLS.DataCenterServer/Service.asmx";
    
    // DDO
    constexpr const char* DDO = 
        "http://gls.ddo.com/GLS.DataCenterServer/Service.asmx";
    constexpr const char* DDO_PREVIEW = 
        "http://gls-lamannia.ddo.com/GLS.DataCenterServer/Service.asmx";
}

/**
 * Fetch game services information
 * 
 * @param datacenterUrl URL of the datacenter service
 * @param gameType Game type (lotro, ddo, etc.)
 * @return GameServicesInfo on success
 */
QFuture<std::optional<GameServicesInfo>> fetchGameServicesInfo(
    const QString& datacenterUrl,
    const QString& gameType
);

/**
 * Get the datacenter URL for a game type
 */
QString getDatacenterUrl(const QString& gameType);

} // namespace lotro
