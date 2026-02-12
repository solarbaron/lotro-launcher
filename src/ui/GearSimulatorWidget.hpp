/**
 * LOTRO Launcher - Gear Simulator Widget
 * 
 * UI for simulating gear loadouts and calculating stats.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QWidget>
#include "companion/StatCalculator.hpp"

class QComboBox;
class QLabel;
class QPushButton;
class QListWidget;
class QGroupBox;
class QGridLayout;
class QLineEdit;

namespace lotro {

/**
 * Gear simulator widget
 * 
 * Allows users to:
 * - Select items for each equipment slot
 * - View calculated total stats
 * - Compare different equipment setups
 */
class GearSimulatorWidget : public QWidget {
    Q_OBJECT

public:
    explicit GearSimulatorWidget(QWidget* parent = nullptr);
    ~GearSimulatorWidget() override;
    
    /**
     * Set character level
     */
    void setLevel(int level);
    
    /**
     * Set character class for filtering
     */
    void setCharacterClass(const QString& className);

private slots:
    void onSlotClicked(int slot);
    void onItemSelected(int row);
    void onSearchChanged(const QString& text);
    void onClearAll();
    void recalculateStats();

private:
    void setupUi();
    void createSlotButtons();
    void createStatDisplay();
    void updateSlotButton(EquipSlot slot);
    void populateItemList(EquipSlot slot);
    void displayStats(const CalculatedStats& stats);
    
    CharacterBuild m_build;
    StatCalculator m_calculator;
    EquipSlot m_activeSlot = EquipSlot::Head;
    
    // Slot buttons
    std::map<EquipSlot, QPushButton*> m_slotButtons;
    
    // Item selection
    QGroupBox* m_itemSelectGroup = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QListWidget* m_itemList = nullptr;
    std::vector<GearItem> m_visibleItems;
    
    // Stats display
    QGridLayout* m_statsGrid = nullptr;
    std::map<QString, QLabel*> m_statLabels;
};

} // namespace lotro
