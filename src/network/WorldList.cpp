/**
 * LOTRO Launcher - World List Implementation
 * 
 * Based on OneLauncher's network/world.py
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "WorldList.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QXmlStreamReader>
#include <QtConcurrent>
#include <QEventLoop>
#include <QTimer>

#include <spdlog/spdlog.h>

namespace lotro {

namespace {

/**
 * Parse world status XML response
 * 
 * The status response has format:
 * <Status>
 *   <name>ServerName</name>
 *   <loginservers>ip:port;ip:port;</loginservers>
 *   <queueurls>http://url1;http://url2;</queueurls>
 *   ...
 * </Status>
 */
struct WorldStatusData {
    QString name;
    QString loginServer;
    QString queueUrl;
    bool available = false;
};

WorldStatusData parseWorldStatusXml(const QString& xml) {
    WorldStatusData data;
    QXmlStreamReader reader(xml);
    
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.isStartElement()) {
            QString name = reader.name().toString().toLower();
            
            if (name == "name") {
                data.name = reader.readElementText();
            } else if (name == "loginservers") {
                QString servers = reader.readElementText();
                // Take first server from semicolon-separated list
                int pos = servers.indexOf(';');
                data.loginServer = pos > 0 ? servers.left(pos) : servers;
            } else if (name == "queueurls") {
                QString urls = reader.readElementText();
                spdlog::info("PARSING WORLD '{}' QueueURLs: {}", data.name.toStdString(), urls.toStdString());
                
                QStringList urlList = urls.split(';', Qt::SkipEmptyParts);
                
                data.queueUrl.clear();
                
                // Prioritize public IPs/hostnames
                for (const auto& url : urlList) {
                    // Check for Private IPs (RFC 1918)
                    // 10.0.0.0/8
                    if (url.contains("://10.")) {
                        continue;
                    }
                    // 192.168.0.0/16
                    if (url.contains("://192.168.")) {
                        continue;
                    }
                    // 172.16.0.0/12 (Simplified check)
                    if (url.contains("://172.16.") || url.contains("://172.17.") || 
                        url.contains("://172.18.") || url.contains("://172.19.") ||
                        url.contains("://172.2") || url.contains("://172.30.") || 
                        url.contains("://172.31.")) {
                        continue;
                    }
                    
                    data.queueUrl = url;
                    break;
                }
                
                // Fallback to first one if no public found
                if (data.queueUrl.isEmpty() && !urlList.isEmpty()) {
                    data.queueUrl = urlList.first();
                }
                
                data.available = !data.queueUrl.isEmpty();
            }
        }
    }
    
    return data;
}

} // anonymous namespace

QString World::statusString() const {
    switch (status) {
        case WorldStatus::Online:  return "Online";
        case WorldStatus::Offline: return "Offline";
        case WorldStatus::Busy:    return "Busy";
        case WorldStatus::Full:    return "Full";
        case WorldStatus::Locked:  return "Locked";
        default:                   return "Unknown";
    }
}

QFuture<World> fetchWorldStatus(const WorldInfo& worldInfo) {
    return QtConcurrent::run([worldInfo]() -> World {
        World world;
        world.name = worldInfo.name;
        world.displayName = worldInfo.name;
        world.statusUrl = worldInfo.statusUrl;
        world.order = worldInfo.order;
        world.language = worldInfo.language;
        world.status = WorldStatus::Unknown;
        
        if (worldInfo.statusUrl.isEmpty()) {
            spdlog::warn("No status URL for world: {}", worldInfo.name.toStdString());
            world.status = WorldStatus::Offline;
            return world;
        }
        
        try {
            spdlog::debug("Fetching status for world: {}", worldInfo.name.toStdString());
            
            QNetworkAccessManager manager;
            QNetworkRequest request(worldInfo.statusUrl);
            request.setHeader(QNetworkRequest::UserAgentHeader, "LOTRO-Launcher/1.0");
            
            QEventLoop loop;
            QNetworkReply* reply = manager.get(request);
            
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            
            QTimer timer;
            timer.setSingleShot(true);
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            timer.start(10000); // 10 second timeout
            
            loop.exec();
            
            if (!timer.isActive()) {
                spdlog::warn("Status request timed out for: {}", worldInfo.name.toStdString());
                reply->deleteLater();
                world.status = WorldStatus::Unknown;
                return world;
            }
            timer.stop();
            
            if (reply->error() != QNetworkReply::NoError) {
                spdlog::warn("Status request failed for {}: {}", 
                            worldInfo.name.toStdString(),
                            reply->errorString().toStdString());
                reply->deleteLater();
                world.status = WorldStatus::Offline;
                return world;
            }
            
            QString response = QString::fromUtf8(reply->readAll());
            reply->deleteLater();
            
            if (response.isEmpty()) {
                world.status = WorldStatus::Offline;
                return world;
            }
            
            auto statusData = parseWorldStatusXml(response);
            
            if (statusData.available) {
                world.status = WorldStatus::Online;
                world.queueUrl = statusData.queueUrl;
                world.loginServer = statusData.loginServer;
            } else {
                world.status = WorldStatus::Offline;
            }
            
            spdlog::debug("  World {} status: {}", 
                         world.name.toStdString(), 
                         world.statusString().toStdString());
            
            return world;
            
        } catch (const std::exception& e) {
            spdlog::warn("Error fetching status for {}: {}", 
                        worldInfo.name.toStdString(), e.what());
            world.status = WorldStatus::Unknown;
            return world;
        }
    });
}

QFuture<std::vector<World>> fetchWorldsWithStatus(const GameServicesInfo& servicesInfo) {
    return QtConcurrent::run([servicesInfo]() -> std::vector<World> {
        std::vector<World> worlds;
        
        spdlog::info("Fetching status for {} worlds", servicesInfo.worlds.size());
        
        // Fetch status for each world
        // Note: We do this sequentially to avoid overwhelming the server
        for (const auto& worldInfo : servicesInfo.worlds) {
            auto future = fetchWorldStatus(worldInfo);
            future.waitForFinished();
            worlds.push_back(future.result());
        }
        
        // Sort by order
        std::sort(worlds.begin(), worlds.end(), [](const World& a, const World& b) {
            return a.order < b.order;
        });
        
        spdlog::info("Fetched status for {} worlds, {} online", 
                    worlds.size(),
                    std::count_if(worlds.begin(), worlds.end(), 
                                  [](const World& w) { return w.canLogin(); }));
        
        return worlds;
    });
}

// Legacy function - for backward compatibility
QFuture<std::vector<World>> fetchWorldList(const QString& worldStatusUrl) {
    return QtConcurrent::run([worldStatusUrl]() -> std::vector<World> {
        spdlog::warn("Using deprecated fetchWorldList - use fetchWorldsWithStatus instead");
        return {}; // Return empty - caller should use new API
    });
}

QFuture<int> checkWorldQueue(const World& world, const QString& ticket) {
    return QtConcurrent::run([world, ticket]() -> int {
        try {
            if (world.queueUrl.isEmpty()) {
                spdlog::debug("No queue URL for world {}, assuming no queue", 
                             world.name.toStdString());
                return 0;
            }
            
            spdlog::debug("Checking queue for world: {}", world.name.toStdString());
            
            // Build queue check URL with ticket
            QString url = world.queueUrl;
            if (!url.contains("?")) {
                url += "?";
            } else {
                url += "&";
            }
            url += QString("ticket=%1").arg(ticket);
            
            QNetworkAccessManager manager;
            QNetworkRequest request(url);
            
            QEventLoop loop;
            QNetworkReply* reply = manager.get(request);
            
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            
            QTimer timer;
            timer.setSingleShot(true);
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            timer.start(10000);
            
            loop.exec();
            
            if (!timer.isActive()) {
                reply->deleteLater();
                return 0; // Assume no queue on timeout
            }
            timer.stop();
            
            if (reply->error() != QNetworkReply::NoError) {
                reply->deleteLater();
                return 0;
            }
            
            QString response = QString::fromUtf8(reply->readAll());
            reply->deleteLater();
            
            // Parse queue position from response
            // Expected format: <QueuePosition>N</QueuePosition> or just a number
            QXmlStreamReader reader(response);
            while (!reader.atEnd()) {
                reader.readNext();
                if (reader.isStartElement() && 
                    reader.name().toString().contains("Position", Qt::CaseInsensitive)) {
                    return reader.readElementText().toInt();
                }
            }
            
            // Try parsing as plain number
            bool ok;
            int pos = response.trimmed().toInt(&ok);
            if (ok) {
                return pos;
            }
            
            return 0; // No queue
            
        } catch (const std::exception& e) {
            spdlog::warn("Error checking world queue: {}", e.what());
            return 0;
        }
    });
}

} // namespace lotro
