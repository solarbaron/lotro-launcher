/**
 * LOTRO Launcher - Sync Status Widget
 * 
 * Shows real-time sync status and controls.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QCheckBox;

namespace lotro {

class LiveSyncService;
struct CharacterInfo;

/**
 * Widget showing sync service status
 */
class SyncStatusWidget : public QWidget {
    Q_OBJECT

public:
    explicit SyncStatusWidget(QWidget* parent = nullptr);
    ~SyncStatusWidget() override;
    
    /**
     * Set the sync service to monitor
     */
    void setSyncService(LiveSyncService* service);

public slots:
    /**
     * Called when sync is stopped externally
     */
    void onSyncStopped();

private slots:
    void onConnectionChanged(bool connected);
    void onCharacterUpdated(const CharacterInfo& info);
    void onStatusChanged(const QString& status);

private:
    void setupUi();
    void updateIndicator(bool connected);
    
    LiveSyncService* m_syncService = nullptr;
    
    QLabel* m_statusIndicator = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_characterLabel = nullptr;
    QCheckBox* m_autoSaveCheck = nullptr;
};

} // namespace lotro
