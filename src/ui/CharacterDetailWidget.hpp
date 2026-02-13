/**
 * LOTRO Launcher - Character Detail Widget
 * 
 * Read-only tabbed view for displaying saved character data,
 * including extended companion data (virtues, reputation, etc.)
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "../companion/CharacterTracker.hpp"

#include <QWidget>

class QLabel;
class QProgressBar;
class QTabWidget;
class QTableWidget;
class QTreeWidget;

namespace lotro {

/**
 * Read-only character detail display.
 * Shows all saved character data in a tabbed layout.
 */
class CharacterDetailWidget : public QWidget {
    Q_OBJECT

public:
    explicit CharacterDetailWidget(QWidget* parent = nullptr);
    
    /**
     * Load and display data for the given character
     */
    void setCharacter(const Character& character);
    
    /**
     * Clear all displayed data
     */
    void clear();

private:
    QWidget* createOverviewTab();
    QWidget* createVirtuesTab();
    QWidget* createReputationTab();
    QWidget* createCraftingTab();
    QWidget* createGearTab();
    QWidget* createTitlesEmotesTab();
    
    // Tab widget
    QTabWidget* m_tabWidget;
    
    // Overview
    QLabel* m_nameLabel;
    QLabel* m_classLabel;
    QLabel* m_levelLabel;
    QLabel* m_raceLabel;
    QLabel* m_serverLabel;
    QLabel* m_accountLabel;
    QLabel* m_moraleLabel;
    QLabel* m_powerLabel;
    QLabel* m_goldLabel;
    QLabel* m_destinyLabel;
    QLabel* m_lotroPointsLabel;
    QLabel* m_lastPlayedLabel;
    
    // Data tabs
    QTableWidget* m_virtuesTable;
    QTreeWidget* m_reputationTree;
    QTableWidget* m_craftingTable;
    QTableWidget* m_gearTable;
    QTableWidget* m_titlesTable;
    QTableWidget* m_emotesTable;
};

} // namespace lotro
