/**
 * Native Patcher - Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "NativePatcher.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QDomDocument>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

#include <spdlog/spdlog.h>

namespace lotro {

NativePatcher::NativePatcher(
    const std::filesystem::path& gameDirectory,
    QObject* parent
)
    : QObject(parent)
    , m_gameDirectory(gameDirectory)
    , m_networkManager(new QNetworkAccessManager(this))
{
    spdlog::info("NativePatcher initialized for: {}", gameDirectory.string());
}

NativePatcher::~NativePatcher() = default;

bool NativePatcher::downloadSplashscreens(
    const QString& manifestUrl,
    NativePatchProgressCallback progress
) {
    if (m_isPatching) {
        m_lastError = "Already patching";
        return false;
    }
    
    m_isPatching = true;
    m_cancelled = false;
    m_progress = NativePatchProgress{};
    m_progress.phase = NativePatchProgress::FetchingManifest;
    m_progress.status = "Fetching manifest...";
    
    if (progress) progress(m_progress);
    
    spdlog::info("Fetching splashscreen manifest: {}", manifestUrl.toStdString());
    
    // Fetch manifest
    QByteArray manifestData = fetchUrl(manifestUrl);
    if (manifestData.isEmpty()) {
        m_lastError = "Failed to fetch manifest";
        m_isPatching = false;
        return false;
    }
    
    // Parse manifest
    auto files = parseSplashscreenManifest(manifestData);
    if (files.empty()) {
        spdlog::info("No splashscreen files to download");
        m_isPatching = false;
        return true;
    }
    
    spdlog::info("Found {} splashscreen files", files.size());
    
    m_progress.phase = NativePatchProgress::DownloadingFiles;
    m_progress.totalFiles = static_cast<int>(files.size());
    m_progress.currentFile = 0;
    
    // Download each file
    for (const auto& file : files) {
        if (m_cancelled) {
            m_lastError = "Cancelled by user";
            m_isPatching = false;
            return false;
        }
        
        m_progress.currentFile++;
        m_progress.currentFileName = file.relativePath;
        m_progress.status = QString("Downloading: %1").arg(file.description);
        if (progress) progress(m_progress);
        
        auto localPath = m_gameDirectory / file.relativePath.toStdString();
        
        // Create parent directories
        std::filesystem::create_directories(localPath.parent_path());
        
        // Download file
        if (!downloadFile(file.downloadUrl, localPath)) {
            spdlog::warn("Failed to download: {}", file.downloadUrl.toStdString());
            // Continue with other files, splashscreens are optional
        }
    }
    
    m_progress.phase = NativePatchProgress::Complete;
    m_progress.status = "Complete";
    if (progress) progress(m_progress);
    
    m_isPatching = false;
    emit finished(true);
    return true;
}

bool NativePatcher::downloadGameFiles(
    const QString& manifestUrl,
    const QString& baseDownloadUrl,
    NativePatchProgressCallback progress
) {
    if (m_isPatching) {
        m_lastError = "Already patching";
        return false;
    }
    
    m_isPatching = true;
    m_cancelled = false;
    m_progress = NativePatchProgress{};
    m_progress.phase = NativePatchProgress::FetchingManifest;
    m_progress.status = "Fetching game file manifest...";
    
    if (progress) progress(m_progress);
    
    spdlog::info("Fetching game manifest: {}", manifestUrl.toStdString());
    
    // Fetch manifest
    QByteArray manifestData = fetchUrl(manifestUrl);
    if (manifestData.isEmpty()) {
        m_lastError = "Failed to fetch game manifest";
        m_isPatching = false;
        return false;
    }
    
    // Parse manifest
    auto files = parsePatchingManifest(manifestData);
    spdlog::info("Found {} game files in manifest", files.size());
    
    // Check which files need to be downloaded
    m_progress.phase = NativePatchProgress::CheckingFiles;
    m_progress.status = "Checking existing files...";
    if (progress) progress(m_progress);
    
    std::vector<DownloadFile> filesToDownload;
    m_progress.totalBytes = 0;
    
    for (const auto& file : files) {
        auto localPath = m_gameDirectory / file.relativePath.toStdString();
        
        if (!std::filesystem::exists(localPath)) {
            filesToDownload.push_back(file);
            m_progress.totalBytes += file.size;
            spdlog::debug("Missing: {}", file.relativePath.toStdString());
        }
    }
    
    if (filesToDownload.empty()) {
        spdlog::info("No game files need downloading");
        m_progress.phase = NativePatchProgress::Complete;
        m_progress.status = "Up to date";
        if (progress) progress(m_progress);
        m_isPatching = false;
        emit finished(true);
        return true;
    }
    
    spdlog::info("Need to download {} files ({} bytes)", 
        filesToDownload.size(), m_progress.totalBytes);
    
    // Download missing files
    m_progress.phase = NativePatchProgress::DownloadingFiles;
    m_progress.totalFiles = static_cast<int>(filesToDownload.size());
    m_progress.currentFile = 0;
    m_progress.bytesDownloaded = 0;
    
    for (const auto& file : filesToDownload) {
        if (m_cancelled) {
            m_lastError = "Cancelled by user";
            m_isPatching = false;
            return false;
        }
        
        m_progress.currentFile++;
        m_progress.currentFileName = file.relativePath;
        m_progress.status = QString("Downloading: %1").arg(file.relativePath);
        if (progress) progress(m_progress);
        
        auto localPath = m_gameDirectory / file.relativePath.toStdString();
        
        // Create parent directories
        std::filesystem::create_directories(localPath.parent_path());
        
        // Build download URL
        QString downloadUrl = baseDownloadUrl;
        if (!downloadUrl.endsWith('/')) downloadUrl += '/';
        downloadUrl += file.relativeUrl;
        
        // Download file
        if (!downloadFile(downloadUrl, localPath)) {
            m_lastError = QString("Failed to download: %1").arg(file.relativePath);
            m_isPatching = false;
            return false;
        }
        
        // Verify hash if provided
        if (!file.md5Hash.isEmpty()) {
            if (!verifyMd5(localPath, file.md5Hash)) {
                spdlog::error("Hash mismatch for: {}", file.relativePath.toStdString());
                std::filesystem::remove(localPath);
                m_lastError = QString("Hash verification failed: %1").arg(file.relativePath);
                m_isPatching = false;
                return false;
            }
        }
        
        m_progress.bytesDownloaded += file.size;
        if (progress) progress(m_progress);
    }
    
    m_progress.phase = NativePatchProgress::Complete;
    m_progress.status = QString("Downloaded %1 files").arg(filesToDownload.size());
    if (progress) progress(m_progress);
    
    m_isPatching = false;
    emit finished(true);
    return true;
}

bool NativePatcher::isPatching() const {
    return m_isPatching;
}

void NativePatcher::cancel() {
    m_cancelled = true;
    spdlog::info("NativePatcher: cancel requested");
}

QString NativePatcher::lastError() const {
    return m_lastError;
}

std::vector<DownloadFile> NativePatcher::parseSplashscreenManifest(const QByteArray& xml) {
    std::vector<DownloadFile> files;
    
    QDomDocument doc;
    if (!doc.setContent(xml)) {
        spdlog::error("Failed to parse splashscreen manifest XML");
        return files;
    }
    
    QDomElement root = doc.documentElement();
    QDomNodeList fileNodes = root.elementsByTagName("File");
    
    for (int i = 0; i < fileNodes.count(); i++) {
        QDomElement fileElem = fileNodes.at(i).toElement();
        
        DownloadFile file;
        file.description = fileElem.firstChildElement("Description").text();
        file.relativePath = fileElem.firstChildElement("FileName").text().replace("\\", "/");
        file.downloadUrl = fileElem.firstChildElement("DownloadUrl").text();
        
        if (!file.downloadUrl.isEmpty()) {
            files.push_back(file);
        }
    }
    
    return files;
}

std::vector<DownloadFile> NativePatcher::parsePatchingManifest(const QByteArray& xml) {
    std::vector<DownloadFile> files;
    
    QDomDocument doc;
    if (!doc.setContent(xml)) {
        spdlog::error("Failed to parse patching manifest XML");
        return files;
    }
    
    QDomElement root = doc.documentElement();
    QDomNodeList fileNodes = root.elementsByTagName("File");
    
    for (int i = 0; i < fileNodes.count(); i++) {
        QDomElement fileElem = fileNodes.at(i).toElement();
        
        DownloadFile file;
        file.relativeUrl = fileElem.firstChildElement("From").text().replace("\\", "/");
        file.relativePath = fileElem.firstChildElement("To").text().replace("\\", "/");
        file.size = fileElem.firstChildElement("Size").text().toLongLong();
        file.md5Hash = fileElem.firstChildElement("MD5").text();
        
        if (!file.relativeUrl.isEmpty() && !file.relativePath.isEmpty()) {
            files.push_back(file);
        }
    }
    
    return files;
}

bool NativePatcher::downloadFile(const QString& url, const std::filesystem::path& localPath) {
    spdlog::debug("Downloading: {} -> {}", url.toStdString(), localPath.string());
    
    QNetworkRequest request{QUrl{url}};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, 
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    
    QNetworkReply* reply = m_networkManager->get(request);
    
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    // Timeout after 60 seconds
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(60000);
    
    loop.exec();
    
    if (!timer.isActive()) {
        spdlog::error("Download timeout: {}", url.toStdString());
        reply->abort();
        reply->deleteLater();
        return false;
    }
    
    timer.stop();
    
    if (reply->error() != QNetworkReply::NoError) {
        spdlog::error("Download error: {} - {}", url.toStdString(), 
                     reply->errorString().toStdString());
        reply->deleteLater();
        return false;
    }
    
    QByteArray data = reply->readAll();
    reply->deleteLater();
    
    // Write to file
    QFile file(QString::fromStdString(localPath.string()));
    if (!file.open(QIODevice::WriteOnly)) {
        spdlog::error("Failed to open file for writing: {}", localPath.string());
        return false;
    }
    
    file.write(data);
    file.close();
    
    spdlog::debug("Downloaded {} bytes to {}", data.size(), localPath.string());
    return true;
}

bool NativePatcher::verifyMd5(const std::filesystem::path& path, const QString& expectedHash) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(&file);
    file.close();
    
    QString actualHash = hash.result().toHex().toLower();
    return actualHash == expectedHash.toLower();
}

QByteArray NativePatcher::fetchUrl(const QString& url) {
    QNetworkRequest request{QUrl{url}};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, 
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    
    QNetworkReply* reply = m_networkManager->get(request);
    
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(30000);
    
    loop.exec();
    
    if (!timer.isActive()) {
        reply->abort();
        reply->deleteLater();
        return {};
    }
    
    timer.stop();
    
    if (reply->error() != QNetworkReply::NoError) {
        spdlog::error("Fetch error: {} - {}", url.toStdString(), 
                     reply->errorString().toStdString());
        reply->deleteLater();
        return {};
    }
    
    QByteArray data = reply->readAll();
    reply->deleteLater();
    return data;
}

} // namespace lotro
