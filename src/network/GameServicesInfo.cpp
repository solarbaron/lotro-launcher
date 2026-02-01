/**
 * LOTRO Launcher - Game Services Info Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "GameServicesInfo.hpp"
#include "SoapClient.hpp"

#include <QXmlStreamReader>
#include <QtConcurrent>

#include <spdlog/spdlog.h>

namespace lotro {

namespace {

GameServicesInfo parseDatacenterResponse(const QString& xml, const QString& datacenterUrl) {
    GameServicesInfo info;
    info.datacenterServiceUrl = datacenterUrl;
    
    QXmlStreamReader reader(xml);
    
    WorldInfo currentWorld;
    bool inWorld = false;
    
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.isStartElement()) {
            QString name = reader.name().toString();
            
            if (name == "AuthServer") {
                info.authServer = reader.readElementText();
            } else if (name == "PatchServer") {
                info.patchServer = reader.readElementText();
            } else if (name == "LauncherConfigurationServer") {
                info.launcherConfigUrl = reader.readElementText();
            } else if (name == "SupportURL") {
                info.supportUrl = reader.readElementText();
            } else if (name == "Name" && !inWorld) {
                // Datacenter name (outside of World element)
                info.datacenterName = reader.readElementText();
                info.datacenterGameName = info.datacenterName;
            } else if (name == "World") {
                // Start of a World element
                inWorld = true;
                currentWorld = WorldInfo();
            } else if (inWorld) {
                // Inside a World element
                if (name == "Name") {
                    currentWorld.name = reader.readElementText();
                } else if (name == "StatusServerUrl") {
                    currentWorld.statusUrl = reader.readElementText();
                } else if (name == "ChatServerUrl") {
                    currentWorld.chatServerUrl = reader.readElementText();
                } else if (name == "Order") {
                    currentWorld.order = reader.readElementText().toInt();
                } else if (name == "Language") {
                    currentWorld.language = reader.readElementText();
                }
            }
        } else if (reader.isEndElement()) {
            QString name = reader.name().toString();
            if (name == "World" && inWorld) {
                // End of World element - add to list
                if (!currentWorld.name.isEmpty()) {
                    info.worlds.push_back(currentWorld);
                    spdlog::debug("  Parsed world: {} (order={})", 
                                 currentWorld.name.toStdString(), 
                                 currentWorld.order);
                }
                inWorld = false;
            }
        }
    }
    
    // Sort worlds by order
    std::sort(info.worlds.begin(), info.worlds.end(), 
              [](const WorldInfo& a, const WorldInfo& b) {
                  return a.order < b.order;
              });
    
    if (reader.hasError()) {
        spdlog::warn("XML parsing error in datacenter response: {}", 
                     reader.errorString().toStdString());
    }
    
    return info;
}

} // anonymous namespace

QString getDatacenterUrl(const QString& gameType) {
    QString type = gameType.toLower();
    
    if (type == "lotro" || type == "lotro-live") {
        return DatacenterUrls::LOTRO;
    } else if (type == "lotro-preview" || type == "bullroarer") {
        return DatacenterUrls::LOTRO_PREVIEW;
    } else if (type == "ddo" || type == "ddo-live") {
        return DatacenterUrls::DDO;
    } else if (type == "ddo-preview" || type == "lamannia") {
        return DatacenterUrls::DDO_PREVIEW;
    }
    
    spdlog::warn("Unknown game type: {}, defaulting to LOTRO", gameType.toStdString());
    return DatacenterUrls::LOTRO;
}

QFuture<std::optional<GameServicesInfo>> fetchGameServicesInfo(
    const QString& datacenterUrl,
    const QString& gameType
) {
    return QtConcurrent::run([datacenterUrl, gameType]() -> std::optional<GameServicesInfo> {
        try {
            spdlog::info("Fetching game services info from: {}", datacenterUrl.toStdString());
            
            SoapClient client(datacenterUrl);
            
            // GetDatacenters SOAP operation
            QString params = QString("<game>%1</game>").arg(gameType.toUpper());
            
            auto future = client.call("GetDatacenters", params);
            future.waitForFinished();
            
            QString response = future.result();
            
            if (response.isEmpty()) {
                spdlog::error("Empty response from datacenter service");
                return std::nullopt;
            }
            
            auto info = parseDatacenterResponse(response, datacenterUrl);
            
            if (!info.isValid()) {
                spdlog::error("Invalid datacenter response - missing required fields");
                return std::nullopt;
            }
            
            spdlog::info("Game services info retrieved successfully");
            spdlog::debug("  Auth server: {}", info.authServer.toStdString());
            spdlog::debug("  Patch server: {}", info.patchServer.toStdString());
            spdlog::info("  Found {} worlds", info.worlds.size());
            
            return info;
            
        } catch (const SoapError& e) {
            spdlog::error("SOAP error fetching game services: {}", e.what());
            return std::nullopt;
        } catch (const std::exception& e) {
            spdlog::error("Error fetching game services: {}", e.what());
            return std::nullopt;
        }
    });
}

} // namespace lotro
