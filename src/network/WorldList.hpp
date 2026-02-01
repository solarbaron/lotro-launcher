/**
 * LOTRO Launcher - World List
 * 
 * Server/world list retrieval and status.
 * Based on OneLauncher's network/world.py
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>
#include <vector>

#include <QFuture>
#include <QString>

#include "GameServicesInfo.hpp"

namespace lotro {

/**
 * World/server status
 */
enum class WorldStatus {
    Online,
    Offline,
    Busy,       // High population
    Full,       // Cannot accept new players
    Locked,     // VIP only or temporarily locked
    Unknown
};

/**
 * World/server information with status
 */
struct World {
    QString name;
    QString displayName;        // Localized name
    QString statusUrl;          // URL to check detailed status
    QString queueUrl;           // Login queue URL (from status check)
    QString loginServer;        // Login server address (from status check)
    WorldStatus status = WorldStatus::Unknown;
    int order = 0;              // Display order
    QString language;           // en, de, fr
    bool isPreferred = false;   // Recommended server
    
    /**
     * Check if this world can accept logins
     */
    bool canLogin() const {
        return status == WorldStatus::Online || status == WorldStatus::Busy;
    }
    
    /**
     * Get status as display string
     */
    QString statusString() const;
};

/**
 * Get worlds with status from GameServicesInfo
 * 
 * This fetches status for each world in the services info
 * 
 * @param servicesInfo Game services info containing world list
 * @return List of worlds with status
 */
QFuture<std::vector<World>> fetchWorldsWithStatus(const GameServicesInfo& servicesInfo);

/**
 * Fetch status for a single world
 * 
 * @param worldInfo Basic world info from datacenter
 * @return World with status populated
 */
QFuture<World> fetchWorldStatus(const WorldInfo& worldInfo);

/**
 * Check the login queue for a world
 * 
 * @param world World to check queue for
 * @param ticket Session ticket from login
 * @return Queue position (0 = no queue, can login immediately)
 */
QFuture<int> checkWorldQueue(const World& world, const QString& ticket);

// Legacy function - deprecated
QFuture<std::vector<World>> fetchWorldList(const QString& worldStatusUrl);

} // namespace lotro
