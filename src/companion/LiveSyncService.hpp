/**
 * LOTRO Launcher - Live Sync Service
 * 
 * Background service that monitors the LOTRO client and 
 * automatically updates character data in real-time.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QObject>
#include <QTimer>
#include <memory>

namespace lotro {

class CharacterExtractor;
class CharacterTracker;
struct CharacterInfo;

/**
 * Live synchronization service
 * 
 * Monitors the LOTRO client in the background and syncs
 * character data automatically when changes are detected.
 */
class LiveSyncService : public QObject {
    Q_OBJECT

public:
    explicit LiveSyncService(QObject* parent = nullptr);
    ~LiveSyncService() override;
    
    /**
     * Start the sync service
     * @param gamePath Path to LOTRO installation
     * @param tracker CharacterTracker for persistence
     */
    void start(const QString& gamePath, CharacterTracker* tracker);
    
    /**
     * Stop the sync service
     */
    void stop();
    
    /**
     * Check if service is running
     */
    bool isRunning() const { return m_running; }
    
    /**
     * Check if connected to game
     */
    bool isConnected() const { return m_connected; }
    
    /**
     * Set sync interval in milliseconds
     */
    void setSyncInterval(int ms);
    
    /**
     * Get current sync interval
     */
    int syncInterval() const { return m_syncInterval; }

signals:
    /**
     * Emitted when connection status changes
     */
    void connectionChanged(bool connected);
    
    /**
     * Emitted when character data is updated
     */
    void characterUpdated(const CharacterInfo& info);
    
    /**
     * Emitted when character is automatically saved
     */
    void characterSaved(const QString& name, const QString& server);
    
    /**
     * Emitted on sync error
     */
    void syncError(const QString& error);
    
    /**
     * Emitted with status updates
     */
    void statusChanged(const QString& status);

private slots:
    void onSyncTimer();
    void onConnectionCheck();

private:
    void tryConnect();
    void syncCharacter();
    void autoSaveCharacter(const CharacterInfo& info);
    
    std::unique_ptr<CharacterExtractor> m_extractor;
    CharacterTracker* m_tracker = nullptr;
    
    QTimer* m_syncTimer = nullptr;
    QTimer* m_connectionTimer = nullptr;
    
    QString m_gamePath;
    int m_syncInterval = 5000;  // 5 seconds default
    bool m_running = false;
    bool m_connected = false;
    
    // Track last character to detect changes
    QString m_lastCharacterName;
    QString m_lastCharacterServer;
    int m_lastLevel = 0;
};

} // namespace lotro
