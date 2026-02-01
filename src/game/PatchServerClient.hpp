/**
 * PatchServerClient - Native client for LOTRO patch server protocol
 * 
 * Communicates with patch.lotro.com:6015 to get iteration updates
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "DatFile.hpp"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QSslSocket>

#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace lotro {

/**
 * Information about a required patch iteration
 */
struct PatchIteration {
    QString name;
    uint32_t version;
    QString downloadUrl;
    qint64 size;
    QString md5Hash;
};

/**
 * Result of checking for patches
 */
struct PatchCheckResult {
    bool success = false;
    QString error;
    
    // Files that need downloading  
    std::vector<QString> filesToDownload;
    qint64 fileBytesToDownload = 0;
    
    // Data iterations that need applying
    std::vector<PatchIteration> iterationsToApply;
    qint64 iterationBytesToDownload = 0;
    
    bool isUpToDate() const {
        return filesToDownload.empty() && iterationsToApply.empty();
    }
    
    qint64 totalBytesToDownload() const {
        return fileBytesToDownload + iterationBytesToDownload;
    }
};

/**
 * Progress callback for patch operations
 */
struct PatchServerProgress {
    enum Phase {
        Connecting,
        Authenticating,
        CheckingVersions,
        DownloadingFiles,
        DownloadingIterations,
        ApplyingIterations,
        Complete,
        Failed
    };
    
    Phase phase = Connecting;
    int currentItem = 0;
    int totalItems = 0;
    qint64 bytesDownloaded = 0;
    qint64 totalBytes = 0;
    QString currentFile;
    QString status;
};

using PatchServerProgressCallback = std::function<void(const PatchServerProgress&)>;

/**
 * Client for the LOTRO patch server
 * 
 * The patch server uses an encrypted XML-RPC protocol.
 * This is a work-in-progress implementation.
 */
class PatchServerClient : public QObject {
    Q_OBJECT

public:
    /**
     * Create a patch server client
     * @param gameDirectory Path to the LOTRO installation
     * @param parent Optional parent QObject
     */
    explicit PatchServerClient(
        const std::filesystem::path& gameDirectory,
        QObject* parent = nullptr
    );
    ~PatchServerClient() override;

    /**
     * Set the patch server address (default: patch.lotro.com:6015)
     */
    void setPatchServer(const QString& host, quint16 port = 6015);

    /**
     * Check what patches are needed
     * This reads .dat file versions and queries the patch server
     */
    PatchCheckResult checkForPatches();

    /**
     * Download and apply all needed patches
     */
    bool applyPatches(PatchServerProgressCallback progress = nullptr);

    /**
     * Get current .dat file versions
     */
    std::map<QString, uint32_t> getCurrentVersions() const;

    /**
     * Get last error message
     */
    QString lastError() const { return m_lastError; }

signals:
    void progressChanged(const PatchServerProgress& progress);
    void patchingComplete(bool success);

private:
    // Network communication
    bool connectToServer();
    void disconnectFromServer();
    QByteArray sendRequest(const QByteArray& request);
    
    // Protocol implementation
    QByteArray encryptRequest(const QByteArray& plaintext);
    QByteArray decryptResponse(const QByteArray& ciphertext);
    QByteArray buildVersionCheckRequest();
    bool parseVersionCheckResponse(const QByteArray& response, PatchCheckResult& result);

    std::filesystem::path m_gameDirectory;
    QString m_patchServer = "patch.lotro.com";
    quint16 m_patchPort = 6015;
    
    QSslSocket* m_socket = nullptr;
    QString m_lastError;
    
    // Cached version info
    std::map<QString, DatVersionInfo> m_datVersions;
};

} // namespace lotro
