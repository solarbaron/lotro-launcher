/**
 * LOTRO Launcher - Gear Simulator Widget Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "GearSimulatorWidget.hpp"
#include "companion/ItemDatabase.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QScrollArea>

namespace lotro {

GearSimulatorWidget::GearSimulatorWidget(QWidget* parent)
    : QWidget(parent)
{
    m_build.level = 150;
    setupUi();
}

GearSimulatorWidget::~GearSimulatorWidget() = default;

void GearSimulatorWidget::setLevel(int level) {
    m_build.level = level;
    recalculateStats();
}

void GearSimulatorWidget::setCharacterClass(const QString& className) {
    m_build.characterClass = className;
    populateItemList(m_activeSlot);
}

void GearSimulatorWidget::setupUi() {
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(12);
    
    // Left: Equipment slots
    auto* leftPanel = new QWidget();
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    auto* slotsGroup = new QGroupBox(tr("Equipment Slots"));
    auto* slotsLayout = new QVBoxLayout(slotsGroup);
    createSlotButtons();
    
    // Add slot buttons in order
    std::vector<EquipSlot> slotOrder = {
        EquipSlot::Head, EquipSlot::Shoulders, EquipSlot::Chest,
        EquipSlot::Hands, EquipSlot::Legs, EquipSlot::Feet,
        EquipSlot::Back, EquipSlot::MainHand, EquipSlot::OffHand,
        EquipSlot::Ranged, EquipSlot::Necklace, EquipSlot::Earring,
        EquipSlot::Bracelet, EquipSlot::Ring, EquipSlot::Pocket
    };
    
    for (auto slot : slotOrder) {
        if (m_slotButtons.find(slot) != m_slotButtons.end()) {
            slotsLayout->addWidget(m_slotButtons[slot]);
        }
    }
    
    slotsLayout->addStretch();
    leftLayout->addWidget(slotsGroup);
    
    // Clear button
    auto* clearBtn = new QPushButton(tr("Clear All Gear"));
    connect(clearBtn, &QPushButton::clicked, this, &GearSimulatorWidget::onClearAll);
    leftLayout->addWidget(clearBtn);
    
    mainLayout->addWidget(leftPanel);
    
    // Center: Item selection
    m_itemSelectGroup = new QGroupBox(tr("Select Item"));
    auto* centerLayout = new QVBoxLayout(m_itemSelectGroup);
    
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText(tr("Search items..."));
    connect(m_searchEdit, &QLineEdit::textChanged, 
            this, &GearSimulatorWidget::onSearchChanged);
    centerLayout->addWidget(m_searchEdit);
    
    m_itemList = new QListWidget();
    connect(m_itemList, &QListWidget::currentRowChanged,
            this, &GearSimulatorWidget::onItemSelected);
    centerLayout->addWidget(m_itemList);
    
    mainLayout->addWidget(m_itemSelectGroup, 1);
    
    // Right: Stats display
    auto* statsGroup = new QGroupBox(tr("Calculated Stats"));
    auto* statsScroll = new QScrollArea();
    statsScroll->setWidgetResizable(true);
    
    auto* statsWidget = new QWidget();
    m_statsGrid = new QGridLayout(statsWidget);
    m_statsGrid->setSpacing(4);
    createStatDisplay();
    
    statsScroll->setWidget(statsWidget);
    
    auto* statsLayout = new QVBoxLayout(statsGroup);
    statsLayout->addWidget(statsScroll);
    
    mainLayout->addWidget(statsGroup, 1);
    
    // Initialize first slot
    populateItemList(EquipSlot::Head);
    recalculateStats();
}

void GearSimulatorWidget::createSlotButtons() {
    std::vector<EquipSlot> equipSlots = {
        EquipSlot::Head, EquipSlot::Shoulders, EquipSlot::Chest,
        EquipSlot::Hands, EquipSlot::Legs, EquipSlot::Feet,
        EquipSlot::Back, EquipSlot::MainHand, EquipSlot::OffHand,
        EquipSlot::Ranged, EquipSlot::Necklace, EquipSlot::Earring,
        EquipSlot::Bracelet, EquipSlot::Ring, EquipSlot::Pocket
    };
    
    for (auto equipSlot : equipSlots) {
        auto* btn = new QPushButton(slotName(equipSlot));
        btn->setCheckable(true);
        btn->setMinimumHeight(28);
        
        connect(btn, &QPushButton::clicked, this, [this, equipSlot]() {
            onSlotClicked(static_cast<int>(equipSlot));
        });
        
        m_slotButtons[equipSlot] = btn;
    }
    
    // Select first slot
    if (!m_slotButtons.empty()) {
        m_slotButtons[EquipSlot::Head]->setChecked(true);
    }
}

void GearSimulatorWidget::createStatDisplay() {
    int row = 0;
    
    // Helper to add stat row
    auto addStatRow = [this, &row](const QString& name) {
        auto* nameLabel = new QLabel(name + ":");
        nameLabel->setStyleSheet("font-weight: bold;");
        m_statsGrid->addWidget(nameLabel, row, 0);
        
        auto* valueLabel = new QLabel("0");
        valueLabel->setAlignment(Qt::AlignRight);
        m_statsGrid->addWidget(valueLabel, row, 1);
        
        m_statLabels[name] = valueLabel;
        row++;
    };
    
    // Primary stats
    auto* primaryLabel = new QLabel(tr("Primary Stats"));
    primaryLabel->setStyleSheet("font-weight: bold; color: #4CAF50;");
    m_statsGrid->addWidget(primaryLabel, row++, 0, 1, 2);
    
    addStatRow("Might");
    addStatRow("Agility");
    addStatRow("Vitality");
    addStatRow("Will");
    addStatRow("Fate");
    
    // Resources
    auto* resourceLabel = new QLabel(tr("Resources"));
    resourceLabel->setStyleSheet("font-weight: bold; color: #2196F3;");
    m_statsGrid->addWidget(resourceLabel, row++, 0, 1, 2);
    
    addStatRow("Morale");
    addStatRow("Power");
    
    // Offensive
    auto* offenseLabel = new QLabel(tr("Offensive"));
    offenseLabel->setStyleSheet("font-weight: bold; color: #ff5722;");
    m_statsGrid->addWidget(offenseLabel, row++, 0, 1, 2);
    
    addStatRow("Physical Mastery");
    addStatRow("Tactical Mastery");
    addStatRow("Critical Rating");
    addStatRow("Crit Chance %");
    addStatRow("Finesse");
    
    // Defensive
    auto* defenseLabel = new QLabel(tr("Defensive"));
    defenseLabel->setStyleSheet("font-weight: bold; color: #9C27B0;");
    m_statsGrid->addWidget(defenseLabel, row++, 0, 1, 2);
    
    addStatRow("Armour");
    addStatRow("Critical Defence");
    addStatRow("Physical Mitigation");
    addStatRow("Phys Mit %");
    addStatRow("Tactical Mitigation");
    addStatRow("Tac Mit %");
}

void GearSimulatorWidget::onSlotClicked(int slot) {
    EquipSlot slotEnum = static_cast<EquipSlot>(slot);
    
    // Update button states
    for (auto& [s, btn] : m_slotButtons) {
        btn->setChecked(s == slotEnum);
    }
    
    m_activeSlot = slotEnum;
    m_itemSelectGroup->setTitle(tr("Select %1").arg(slotName(slotEnum)));
    populateItemList(slotEnum);
}

void GearSimulatorWidget::onItemSelected(int row) {
    if (row < 0 || row >= static_cast<int>(m_visibleItems.size())) {
        return;
    }
    
    const auto& item = m_visibleItems[row];
    m_build.equip(item);
    updateSlotButton(item.slot);
    recalculateStats();
}

void GearSimulatorWidget::onSearchChanged(const QString& text) {
    populateItemList(m_activeSlot);
}

void GearSimulatorWidget::onClearAll() {
    m_build.clearGear();
    
    for (auto& [slot, btn] : m_slotButtons) {
        btn->setText(slotName(slot));
        btn->setStyleSheet("");
    }
    
    recalculateStats();
}

void GearSimulatorWidget::recalculateStats() {
    auto stats = m_calculator.calculate(m_build);
    displayStats(stats);
}

void GearSimulatorWidget::updateSlotButton(EquipSlot slot) {
    auto it = m_build.equipment.find(slot);
    auto btnIt = m_slotButtons.find(slot);
    
    if (btnIt == m_slotButtons.end()) {
        return;
    }
    
    if (it != m_build.equipment.end()) {
        const auto& item = it->second;
        btnIt->second->setText(item.name);
        btnIt->second->setStyleSheet(
            QString("color: %1;").arg(qualityColor(item.quality))
        );
    } else {
        btnIt->second->setText(slotName(slot));
        btnIt->second->setStyleSheet("");
    }
}

void GearSimulatorWidget::populateItemList(EquipSlot slot) {
    m_itemList->clear();
    m_visibleItems.clear();
    
    auto& itemDb = ItemDatabase::instance();
    auto items = itemDb.getItemsBySlot(slot);
    
    QString filter = m_searchEdit->text().toLower();
    
    for (const auto& item : items) {
        // Filter by search
        if (!filter.isEmpty() && 
            !item.name.toLower().contains(filter)) {
            continue;
        }
        
        // Filter by class if set
        if (!m_build.characterClass.isEmpty() &&
            !item.requiredClass.isEmpty() &&
            item.requiredClass != m_build.characterClass) {
            continue;
        }
        
        m_visibleItems.push_back(item);
        
        auto* listItem = new QListWidgetItem(item.name);
        listItem->setForeground(QColor(qualityColor(item.quality)));
        listItem->setToolTip(QString("iLvl %1 - %2").arg(item.itemLevel).arg(qualityName(item.quality)));
        m_itemList->addItem(listItem);
    }
}

void GearSimulatorWidget::displayStats(const CalculatedStats& stats) {
    auto setValue = [this](const QString& name, int value) {
        auto it = m_statLabels.find(name);
        if (it != m_statLabels.end()) {
            it->second->setText(QString::number(value));
        }
    };
    
    auto setPercent = [this](const QString& name, double value) {
        auto it = m_statLabels.find(name);
        if (it != m_statLabels.end()) {
            it->second->setText(QString::number(value, 'f', 1) + "%");
        }
    };
    
    // Primary
    setValue("Might", stats.might);
    setValue("Agility", stats.agility);
    setValue("Vitality", stats.vitality);
    setValue("Will", stats.will);
    setValue("Fate", stats.fate);
    
    // Resources
    setValue("Morale", stats.morale);
    setValue("Power", stats.power);
    
    // Offensive
    setValue("Physical Mastery", stats.physicalMastery);
    setValue("Tactical Mastery", stats.tacticalMastery);
    setValue("Critical Rating", stats.criticalRating);
    setPercent("Crit Chance %", stats.criticalChance);
    setValue("Finesse", stats.finesse);
    
    // Defensive
    setValue("Armour", stats.armour);
    setValue("Critical Defence", stats.criticalDefence);
    setValue("Physical Mitigation", stats.physicalMitigation);
    setPercent("Phys Mit %", stats.physicalMitigationPercent);
    setValue("Tactical Mitigation", stats.tacticalMitigation);
    setPercent("Tac Mit %", stats.tacticalMitigationPercent);
}

} // namespace lotro
