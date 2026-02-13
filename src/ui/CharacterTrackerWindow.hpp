/**
 * LOTRO Launcher - Character Tracker Window
 * 
 * Full-featured character tracking with tabbed display for
 * all extractable data: overview, virtues, reputation, crafting, gear, titles.
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
class QTabWidget;
class QTableWidget;
class QTreeWidget;

namespace lotro {

/**
 * Character Tracker Window
 * 
 * Shows live character data extracted from the running LOTRO game client.
 * Provides a tabbed interface with Overview, Virtues, Reputation, Crafting,
 * Gear, and Titles & Emotes tabs.
 */
class CharacterTrackerWindow : public QDialog {
    Q_OBJECT

public:
    explicit CharacterTrackerWindow(const QString& gamePath = QString(), QWidget* parent = nullptr);
    ~CharacterTrackerWindow() override;

public slots:
    void connectToGame();
    void disconnectFromGame();
    void refresh();

private slots:
    void onAutoRefresh();

private:
    void setupUi();
    void setupConnections();
    
    // Tab creation helpers
    QWidget* createOverviewTab();
    QWidget* createVirtuesTab();
    QWidget* createReputationTab();
    QWidget* createCraftingTab();
    QWidget* createGearTab();
    QWidget* createTitlesEmotesTab();
    
    // Display updates
    void updateOverview(const CharacterInfo& info);
    void updateVirtues(const std::vector<VirtueStatus>& virtues);
    void updateReputation(const std::vector<FactionStatus>& factions);
    void updateCrafting(const CraftingStatus& crafting);
    void updateGear(const std::map<QString, int>& gear);
    void updateTitlesEmotes(const std::vector<int>& titles, const std::vector<int>& emotes);
    void clearDisplay();
    void setStatus(const QString& status, bool isError = false);
    
    // === UI Elements ===
    
    // Top bar
    QPushButton* m_connectButton;
    QPushButton* m_refreshButton;
    QPushButton* m_exportButton;
    QPushButton* m_saveButton;
    QLabel* m_statusLabel;
    
    // Tab widget
    QTabWidget* m_tabWidget;
    
    // Overview tab
    QLabel* m_nameLabel;
    QLabel* m_classLabel;
    QLabel* m_levelLabel;
    QLabel* m_raceLabel;
    QLabel* m_serverLabel;
    QLabel* m_accountLabel;
    QLabel* m_accountTypeLabel;
    QLabel* m_moraleLabel;
    QProgressBar* m_moraleBar;
    QLabel* m_powerLabel;
    QProgressBar* m_powerBar;
    QLabel* m_goldLabel;
    QLabel* m_destinyLabel;
    QLabel* m_lotroPointsLabel;
    
    // Virtues tab
    QTableWidget* m_virtuesTable;
    
    // Reputation tab
    QTreeWidget* m_reputationTree;
    
    // Crafting tab
    QTableWidget* m_craftingTable;
    
    // Gear tab
    QTableWidget* m_gearTable;
    
    // Titles & Emotes tab
    QTableWidget* m_titlesTable;
    QTableWidget* m_emotesTable;
    
    // State
    QString m_gamePath;
    std::unique_ptr<CharacterExtractor> m_extractor;
    std::unique_ptr<CharacterTracker> m_characterTracker;
    QTimer* m_autoRefreshTimer;
    bool m_connected = false;
    CharacterData m_lastCharacterData;  // Full character data for saving
    class DataExportWindow* m_exportWindow = nullptr;
};

} // namespace lotro
