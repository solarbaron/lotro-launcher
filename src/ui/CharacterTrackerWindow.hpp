/**
 * LOTRO Launcher - Character Tracker Window
 * 
 * Displays live character information from the running game.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "../companion/CharacterExtractor.hpp"
#include "../companion/CharacterTracker.hpp"

#include <QDialog>
#include <QTimer>

#include <memory>

class QLabel;
class QProgressBar;
class QPushButton;
class QGroupBox;

namespace lotro {

/**
 * Character Tracker Window
 * 
 * Shows live character data extracted from the running LOTRO game client.
 */
class CharacterTrackerWindow : public QDialog {
    Q_OBJECT

public:
    /**
     * Construct the tracker window
     * @param gamePath Path to LOTRO installation directory (for DAT file access)
     * @param parent Parent widget
     */
    explicit CharacterTrackerWindow(const QString& gamePath = QString(), QWidget* parent = nullptr);
    ~CharacterTrackerWindow() override;

public slots:
    /**
     * Connect to the game client
     */
    void connectToGame();
    
    /**
     * Disconnect from the game client
     */
    void disconnectFromGame();
    
    /**
     * Refresh character data
     */
    void refresh();

private slots:
    void onAutoRefresh();

private:
    void setupUi();
    void setupConnections();
    void updateDisplay(const CharacterInfo& info);
    void clearDisplay();
    void setStatus(const QString& status, bool isError = false);
    
    // UI elements
    QPushButton* m_connectButton;
    QPushButton* m_refreshButton;
    QLabel* m_statusLabel;
    
    // Character info group
    QGroupBox* m_characterGroup;
    QLabel* m_nameLabel;
    QLabel* m_classLabel;
    QLabel* m_levelLabel;
    QLabel* m_raceLabel;
    QLabel* m_serverLabel;
    QLabel* m_accountLabel;
    QLabel* m_accountTypeLabel;
    
    // Stats group
    QGroupBox* m_statsGroup;
    QLabel* m_moraleLabel;
    QProgressBar* m_moraleBar;
    QLabel* m_powerLabel;
    QProgressBar* m_powerBar;
    
    // Currency group
    QGroupBox* m_currencyGroup;
    QLabel* m_goldLabel;
    QLabel* m_destinyLabel;
    QLabel* m_lotroPointsLabel;
    
    // Export/Save
    QPushButton* m_exportButton;
    QPushButton* m_saveButton;
    class DataExportWindow* m_exportWindow = nullptr;
    
    QString m_gamePath;
    std::unique_ptr<CharacterExtractor> m_extractor;
    std::unique_ptr<CharacterTracker> m_characterTracker;
    QTimer* m_autoRefreshTimer;
    bool m_connected = false;
    CharacterInfo m_lastCharacterInfo;  // Store last extracted character for saving
};

} // namespace lotro
