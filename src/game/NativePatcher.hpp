/**
 * Native Patcher - Replaces PatchClient.dll with native Qt implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <filesystem>
#include <functional>
#include <vector>

namespace lotro {

/**
 * Information about a file to download
 */
struct DownloadFile {
    QString relativeUrl;      // Relative URL from base
    QString relativePath;     // Local path relative to game dir
    QString downloadUrl;      // Full download URL (for splashscreens)
    qint64 size = 0;
    QString md5Hash;
    QString description;
};

/**
 * Progress information for patching
 */
struct NativePatchProgress {
    enum Phase {
        Idle,
        FetchingManifest,
        CheckingFiles,
        DownloadingFiles,
        ApplyingPatches,
        Complete,
        Failed
    };
    
    Phase phase = Idle;
    int currentFile = 0;
    int totalFiles = 0;
    qint64 bytesDownloaded = 0;
    qint64 totalBytes = 0;
    QString currentFileName;
    QString status;
    QString error;
    
    int percentage() const {
        if (totalBytes > 0) return static_cast<int>(bytesDownloaded * 100 / totalBytes);
        if (totalFiles > 0) return currentFile * 100 / totalFiles;
        return 0;
    }
};

using NativePatchProgressCallback = std::function<void(const NativePatchProgress&)>;

/**
 * Native patcher that downloads files from LOTRO servers
 * without requiring PatchClient.dll
 * 
 * Supports:
 * - Splashscreen/loading image downloads from Akamai CDN
 * - File manifest parsing
 * - Hash verification
 * - Progress reporting
 */
class NativePatcher : public QObject {
    Q_OBJECT

public:
    explicit NativePatcher(
        const std::filesystem::path& gameDirectory,
        QObject* parent = nullptr
    );
    ~NativePatcher() override;

    /**
     * Download splashscreens and loading images
     * These can be downloaded without authentication
     */
    bool downloadSplashscreens(
        const QString& manifestUrl,
        NativePatchProgressCallback progress = nullptr
    );

    /**
     * Download game files from Akamai CDN
     * Only downloads files that don't exist (initial install)
     */
    bool downloadGameFiles(
        const QString& manifestUrl,
        const QString& baseDownloadUrl,
        NativePatchProgressCallback progress = nullptr
    );

    /**
     * Check if currently patching
     */
    bool isPatching() const;

    /**
     * Cancel current operation
     */
    void cancel();

    /**
     * Get last error message
     */
    QString lastError() const;

signals:
    void progressChanged(const NativePatchProgress& progress);
    void finished(bool success);

private:
    // Manifest parsing
    std::vector<DownloadFile> parseSplashscreenManifest(const QByteArray& xml);
    std::vector<DownloadFile> parsePatchingManifest(const QByteArray& xml);
    
    // File operations
    bool downloadFile(const QString& url, const std::filesystem::path& localPath);
    bool verifyMd5(const std::filesystem::path& path, const QString& expectedHash);
    
    // Network
    QByteArray fetchUrl(const QString& url);
    
    std::filesystem::path m_gameDirectory;
    QNetworkAccessManager* m_networkManager;
    bool m_isPatching = false;
    bool m_cancelled = false;
    QString m_lastError;
    NativePatchProgress m_progress;
};

} // namespace lotro
