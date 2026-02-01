/**
 * LOTRO Launcher - LotroInterface.com Client Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "LotroInterfaceClient.hpp"

#include <QDir>
#include <QDateTime>
#include <QEventLoop>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QXmlStreamReader>
#include <QtConcurrent>

#include <spdlog/spdlog.h>

namespace lotro {

class LotroInterfaceClient::Impl {
public:
    // Nothing needed for now - each request creates its own manager
};

LotroInterfaceClient::LotroInterfaceClient()
    : m_impl(std::make_unique<Impl>())
{
}

LotroInterfaceClient::~LotroInterfaceClient() = default;

namespace {

QString getApiUrl(AddonType type) {
    switch (type) {
        case AddonType::Plugin:
            return LotroInterfaceClient::getPluginsUrl();
        case AddonType::Skin:
            return LotroInterfaceClient::getSkinsUrl();
        case AddonType::Music:
            return LotroInterfaceClient::getMusicUrl();
    }
    return "";
}

std::vector<RemoteAddonInfo> parseAddonListXml(const QString& xml, AddonType type) {
    std::vector<RemoteAddonInfo> addons;
    QXmlStreamReader reader(xml);
    
    RemoteAddonInfo current;
    current.type = type;
    bool inUi = false;
    
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.isStartElement()) {
            QString name = reader.name().toString();
            
            if (name == "Ui") {
                inUi = true;
                current = RemoteAddonInfo();
                current.type = type;
            } else if (inUi) {
                if (name == "UIName") {
                    current.name = reader.readElementText();
                } else if (name == "UIAuthorName") {
                    current.author = reader.readElementText();
                } else if (name == "UIVersion") {
                    current.version = reader.readElementText();
                } else if (name == "UIUpdated") {
                    // Unix timestamp - convert to readable date
                    QString timestamp = reader.readElementText();
                    bool ok;
                    qint64 unixTime = timestamp.toLongLong(&ok);
                    if (ok && unixTime > 0) {
                        QDateTime dt = QDateTime::fromSecsSinceEpoch(unixTime);
                        current.latestRelease = dt.toString("yyyy-MM-dd");
                    } else {
                        current.latestRelease = timestamp;
                    }
                } else if (name == "UICategory") {
                    current.category = reader.readElementText();
                } else if (name == "UID") {  // Note: UID not UIID
                    current.interfaceId = reader.readElementText();
                } else if (name == "UIFileURL") {
                    current.downloadUrl = reader.readElementText();
                } else if (name == "UIFile" && current.downloadUrl.isEmpty()) {
                    // UIFile is just filename, need to build full URL
                    current.downloadUrl = reader.readElementText();
                } else if (name == "UIDownloads") {
                    current.downloads = reader.readElementText().toInt();
                } else if (name == "UIFavorite") {
                    current.favourites = reader.readElementText().toInt();
                } else if (name == "UISize") {
                    // Size in bytes - format it nicely
                    QString sizeStr = reader.readElementText();
                    qint64 bytes = sizeStr.toLongLong();
                    if (bytes >= 1024 * 1024) {
                        current.fileSize = QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
                    } else if (bytes >= 1024) {
                        current.fileSize = QString::number(bytes / 1024.0, 'f', 1) + " KB";
                    } else {
                        current.fileSize = QString::number(bytes) + " B";
                    }
                }
            }
        } else if (reader.isEndElement()) {
            if (reader.name().toString() == "Ui" && inUi) {
                if (!current.name.isEmpty() && !current.interfaceId.isEmpty()) {
                    // Build info URL
                    current.infoUrl = QString("https://www.lotrointerface.com/downloads/info%1").arg(current.interfaceId);
                    addons.push_back(current);
                    spdlog::debug("Parsed addon: {} (ID: {})", current.name.toStdString(), current.interfaceId.toStdString());
                }
                inUi = false;
            }
        }
    }
    
    if (reader.hasError()) {
        spdlog::warn("XML parsing error: {}", reader.errorString().toStdString());
    }
    
    spdlog::info("Parsed {} remote addons of type {}", addons.size(), 
                 addonTypeToString(type).toStdString());
    
    return addons;
}

} // anonymous namespace

QFuture<std::vector<RemoteAddonInfo>> LotroInterfaceClient::fetchAddonList(AddonType type) {
    return QtConcurrent::run([type]() -> std::vector<RemoteAddonInfo> {
        try {
            QString url = getApiUrl(type);
            spdlog::info("Fetching addon list from: {}", url.toStdString());
            
            QNetworkAccessManager manager;
            QNetworkRequest request(url);
            request.setHeader(QNetworkRequest::UserAgentHeader, "LOTRO-Launcher/1.0");
            
            // Configure SSL
            QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
            sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
            request.setSslConfiguration(sslConfig);
            
            // Allow redirects
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, 
                                 QNetworkRequest::NoLessSafeRedirectPolicy);
            
            QEventLoop loop;
            QNetworkReply* reply = manager.get(request);
            
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            
            QTimer timer;
            timer.setSingleShot(true);
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            timer.start(30000); // 30 second timeout
            
            loop.exec();
            
            if (!timer.isActive()) {
                spdlog::error("Addon list fetch timed out");
                reply->deleteLater();
                return {};
            }
            timer.stop();
            
            if (reply->error() != QNetworkReply::NoError) {
                spdlog::error("Addon list fetch failed: {}", 
                             reply->errorString().toStdString());
                reply->deleteLater();
                return {};
            }
            
            QString content = QString::fromUtf8(reply->readAll());
            reply->deleteLater();
            
            return parseAddonListXml(content, type);
            
        } catch (const std::exception& e) {
            spdlog::error("Exception fetching addon list: {}", e.what());
            return {};
        }
    });
}

QFuture<QString> LotroInterfaceClient::downloadAddon(
    const QString& downloadUrl,
    std::function<void(qint64, qint64)> progressCallback
) {
    return QtConcurrent::run([downloadUrl, progressCallback]() -> QString {
        try {
            spdlog::info("Downloading addon from: {}", downloadUrl.toStdString());
            
            QNetworkAccessManager manager;
            QNetworkRequest request(downloadUrl);
            request.setHeader(QNetworkRequest::UserAgentHeader, "LOTRO-Launcher/1.0");
            
            // Configure SSL
            QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
            sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
            request.setSslConfiguration(sslConfig);
            
            // Allow redirects
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, 
                                 QNetworkRequest::NoLessSafeRedirectPolicy);
            
            QEventLoop loop;
            QNetworkReply* reply = manager.get(request);
            
            if (progressCallback) {
                QObject::connect(reply, &QNetworkReply::downloadProgress,
                    [&progressCallback](qint64 received, qint64 total) {
                        progressCallback(received, total);
                    });
            }
            
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            
            QTimer timer;
            timer.setSingleShot(true);
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            timer.start(300000); // 5 minute timeout for downloads
            
            loop.exec();
            
            if (!timer.isActive()) {
                spdlog::error("Addon download timed out");
                reply->deleteLater();
                return QString();
            }
            timer.stop();
            
            if (reply->error() != QNetworkReply::NoError) {
                spdlog::error("Addon download failed: {}", 
                             reply->errorString().toStdString());
                reply->deleteLater();
                return QString();
            }
            
            // Save to temp file
            QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            QString tempPath = tempDir + "/lotro-launcher-addon-" + 
                              QString::number(QDateTime::currentMSecsSinceEpoch()) + ".zip";
            
            QFile file(tempPath);
            if (!file.open(QIODevice::WriteOnly)) {
                spdlog::error("Failed to create temp file: {}", tempPath.toStdString());
                reply->deleteLater();
                return QString();
            }
            
            file.write(reply->readAll());
            file.close();
            reply->deleteLater();
            
            spdlog::info("Addon downloaded to: {}", tempPath.toStdString());
            return tempPath;
            
        } catch (const std::exception& e) {
            spdlog::error("Exception downloading addon: {}", e.what());
            return QString();
        }
    });
}

QString LotroInterfaceClient::getAddonPageUrl(const QString& interfaceId) {
    return QString("https://www.lotrointerface.com/downloads/info%1").arg(interfaceId);
}

} // namespace lotro
