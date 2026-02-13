/**
 * LOTRO Launcher - Character Detail Widget Implementation
 * 
 * Read-only tabbed view for saved character data.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "CharacterDetailWidget.hpp"

#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "companion/GameDatabase.hpp"
#include "companion/ItemDatabase.hpp"

#include <QDateTime>

namespace lotro {

CharacterDetailWidget::CharacterDetailWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    m_tabWidget = new QTabWidget();
    m_tabWidget->addTab(createOverviewTab(), tr("Overview"));
    m_tabWidget->addTab(createVirtuesTab(), tr("Virtues"));
    m_tabWidget->addTab(createReputationTab(), tr("Reputation"));
    m_tabWidget->addTab(createCraftingTab(), tr("Crafting"));
    m_tabWidget->addTab(createGearTab(), tr("Gear"));
    m_tabWidget->addTab(createTitlesEmotesTab(), tr("Titles & Emotes"));
    
    layout->addWidget(m_tabWidget);
}

// ============ Tab Creators ============

QWidget* CharacterDetailWidget::createOverviewTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    layout->setSpacing(10);
    
    auto* charGroup = new QGroupBox(tr("Character"));
    auto* charLayout = new QGridLayout(charGroup);
    charLayout->setSpacing(6);
    
    int row = 0;
    auto addRow = [&](const QString& label, QLabel*& valueLabel) {
        charLayout->addWidget(new QLabel(label), row, 0);
        valueLabel = new QLabel("-");
        charLayout->addWidget(valueLabel, row++, 1);
    };
    
    charLayout->addWidget(new QLabel(tr("Name:")), row, 0);
    m_nameLabel = new QLabel("-");
    m_nameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    charLayout->addWidget(m_nameLabel, row++, 1);
    
    addRow(tr("Class:"), m_classLabel);
    addRow(tr("Level:"), m_levelLabel);
    addRow(tr("Race:"), m_raceLabel);
    addRow(tr("Server:"), m_serverLabel);
    addRow(tr("Account:"), m_accountLabel);
    addRow(tr("Last Played:"), m_lastPlayedLabel);
    
    charLayout->setColumnStretch(1, 1);
    layout->addWidget(charGroup);
    
    auto* statsGroup = new QGroupBox(tr("Stats & Currency"));
    auto* statsLayout = new QGridLayout(statsGroup);
    statsLayout->setSpacing(6);
    
    row = 0;
    statsLayout->addWidget(new QLabel(tr("Morale:")), row, 0);
    m_moraleLabel = new QLabel("-");
    statsLayout->addWidget(m_moraleLabel, row++, 1);
    
    statsLayout->addWidget(new QLabel(tr("Power:")), row, 0);
    m_powerLabel = new QLabel("-");
    statsLayout->addWidget(m_powerLabel, row++, 1);
    
    statsLayout->addWidget(new QLabel(tr("Gold:")), row, 0);
    m_goldLabel = new QLabel("-");
    m_goldLabel->setStyleSheet("color: #fc0;");
    statsLayout->addWidget(m_goldLabel, row++, 1);
    
    statsLayout->addWidget(new QLabel(tr("Destiny Points:")), row, 0);
    m_destinyLabel = new QLabel("-");
    statsLayout->addWidget(m_destinyLabel, row++, 1);
    
    statsLayout->setColumnStretch(1, 1);
    layout->addWidget(statsGroup);
    
    layout->addStretch();
    return widget;
}

QWidget* CharacterDetailWidget::createVirtuesTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    
    m_virtuesTable = new QTableWidget(0, 3);
    m_virtuesTable->setHorizontalHeaderLabels({tr("Virtue"), tr("Rank"), tr("XP")});
    m_virtuesTable->horizontalHeader()->setStretchLastSection(true);
    m_virtuesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_virtuesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_virtuesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_virtuesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_virtuesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_virtuesTable->setAlternatingRowColors(true);
    m_virtuesTable->verticalHeader()->setVisible(false);
    
    layout->addWidget(m_virtuesTable, 1);
    return widget;
}

QWidget* CharacterDetailWidget::createReputationTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    
    m_reputationTree = new QTreeWidget();
    m_reputationTree->setHeaderLabels({tr("Faction"), tr("Tier"), tr("Reputation")});
    m_reputationTree->setAlternatingRowColors(true);
    m_reputationTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_reputationTree->header()->setStretchLastSection(true);
    m_reputationTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_reputationTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_reputationTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    
    layout->addWidget(m_reputationTree, 1);
    return widget;
}

QWidget* CharacterDetailWidget::createCraftingTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    
    m_craftingTable = new QTableWidget(0, 5);
    m_craftingTable->setHorizontalHeaderLabels({
        tr("Profession"), tr("Tier"), tr("Proficiency"), tr("Mastery"), tr("Mastered")
    });
    m_craftingTable->horizontalHeader()->setStretchLastSection(true);
    m_craftingTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_craftingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_craftingTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_craftingTable->setAlternatingRowColors(true);
    m_craftingTable->verticalHeader()->setVisible(false);
    
    layout->addWidget(m_craftingTable, 1);
    return widget;
}

QWidget* CharacterDetailWidget::createGearTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    
    m_gearTable = new QTableWidget(0, 3);
    m_gearTable->setHorizontalHeaderLabels({tr("Slot"), tr("Item ID"), tr("Item Name")});
    m_gearTable->horizontalHeader()->setStretchLastSection(true);
    m_gearTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_gearTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_gearTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_gearTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_gearTable->setAlternatingRowColors(true);
    m_gearTable->verticalHeader()->setVisible(false);
    
    layout->addWidget(m_gearTable, 1);
    return widget;
}

QWidget* CharacterDetailWidget::createTitlesEmotesTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    
    auto* titlesGroup = new QGroupBox(tr("Acquired Titles"));
    auto* titlesLayout = new QVBoxLayout(titlesGroup);
    m_titlesTable = new QTableWidget(0, 2);
    m_titlesTable->setHorizontalHeaderLabels({tr("ID"), tr("Title Name")});
    m_titlesTable->horizontalHeader()->setStretchLastSection(true);
    m_titlesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_titlesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_titlesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_titlesTable->setAlternatingRowColors(true);
    m_titlesTable->verticalHeader()->setVisible(false);
    titlesLayout->addWidget(m_titlesTable);
    
    auto* emotesGroup = new QGroupBox(tr("Known Emotes"));
    auto* emotesLayout = new QVBoxLayout(emotesGroup);
    m_emotesTable = new QTableWidget(0, 2);
    m_emotesTable->setHorizontalHeaderLabels({tr("ID"), tr("Emote")});
    m_emotesTable->horizontalHeader()->setStretchLastSection(true);
    m_emotesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_emotesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_emotesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_emotesTable->setAlternatingRowColors(true);
    m_emotesTable->verticalHeader()->setVisible(false);
    emotesLayout->addWidget(m_emotesTable);
    
    auto* splitter = new QSplitter(Qt::Vertical);
    splitter->addWidget(titlesGroup);
    splitter->addWidget(emotesGroup);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    
    layout->addWidget(splitter, 1);
    return widget;
}

// ============ Data Population ============

void CharacterDetailWidget::setCharacter(const Character& c) {
    // Overview
    m_nameLabel->setText(c.name);
    m_classLabel->setText(c.classString());
    m_levelLabel->setText(QString::number(c.level));
    m_raceLabel->setText(c.raceString());
    m_serverLabel->setText(c.server);
    m_accountLabel->setText(c.accountName.isEmpty() ? "-" : c.accountName);
    
    auto lastPlayedEpoch = std::chrono::duration_cast<std::chrono::seconds>(
        c.lastPlayed.time_since_epoch()).count();
    if (lastPlayedEpoch > 0) {
        QDateTime dt = QDateTime::fromSecsSinceEpoch(lastPlayedEpoch);
        m_lastPlayedLabel->setText(dt.toString("yyyy-MM-dd hh:mm"));
    } else {
        m_lastPlayedLabel->setText("-");
    }
    
    if (c.morale > 0) {
        m_moraleLabel->setText(QString::number(c.morale));
    } else {
        m_moraleLabel->setText("-");
    }
    
    if (c.power > 0) {
        m_powerLabel->setText(QString::number(c.power));
    } else {
        m_powerLabel->setText("-");
    }
    
    if (c.gold > 0 || c.silver > 0 || c.copper > 0) {
        m_goldLabel->setText(QString("%1g %2s %3c").arg(c.gold).arg(c.silver).arg(c.copper));
    } else {
        m_goldLabel->setText("-");
    }
    m_destinyLabel->setText(c.destinyPoints > 0 ? QString::number(c.destinyPoints) : "-");
    
    // Extended data
    if (!c.hasExtendedData) {
        // Show "no extended data" messages in all tabs
        auto setEmptyTable = [](QTableWidget* table, int cols, const QString& msg) {
            table->setRowCount(1);
            auto* item = new QTableWidgetItem(msg);
            item->setFlags(Qt::NoItemFlags);
            item->setForeground(QColor("#888"));
            table->setItem(0, 0, item);
            table->setSpan(0, 0, 1, cols);
        };
        
        QString noDataMsg = tr("No extended data — resync from game to capture");
        setEmptyTable(m_virtuesTable, 3, noDataMsg);
        setEmptyTable(m_craftingTable, 5, noDataMsg);
        setEmptyTable(m_gearTable, 3, noDataMsg);
        setEmptyTable(m_titlesTable, 2, noDataMsg);
        setEmptyTable(m_emotesTable, 2, noDataMsg);
        
        m_reputationTree->clear();
        auto* emptyItem = new QTreeWidgetItem(m_reputationTree);
        emptyItem->setText(0, noDataMsg);
        emptyItem->setForeground(0, QColor("#888"));
        return;
    }
    
    // Virtues
    m_virtuesTable->setRowCount(0);
    for (const auto& v : c.virtues) {
        int row = m_virtuesTable->rowCount();
        m_virtuesTable->insertRow(row);
        m_virtuesTable->setItem(row, 0, new QTableWidgetItem(v.name.isEmpty() ? v.key : v.name));
        auto* rankItem = new QTableWidgetItem(QString::number(v.rank));
        rankItem->setTextAlignment(Qt::AlignCenter);
        m_virtuesTable->setItem(row, 1, rankItem);
        auto* xpItem = new QTableWidgetItem(QString::number(v.xp));
        xpItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_virtuesTable->setItem(row, 2, xpItem);
    }
    
    // Reputation
    m_reputationTree->clear();
    auto tierName = [](int tier) -> QString {
        switch (tier) {
            case 1: return "Enemy";
            case 2: return "Outsider";
            case 3: return "Neutral";
            case 4: return "Acquaintance";
            case 5: return "Friend";
            case 6: return "Ally";
            case 7: return "Kindred";
            default: return QString("Tier %1").arg(tier);
        }
    };
    
    std::map<QString, std::vector<const SavedFaction*>> categorized;
    for (const auto& f : c.factions) {
        QString cat = f.category.isEmpty() ? tr("Other") : f.category;
        categorized[cat].push_back(&f);
    }
    for (const auto& [category, factions] : categorized) {
        auto* catItem = new QTreeWidgetItem(m_reputationTree);
        catItem->setText(0, category);
        catItem->setExpanded(true);
        QFont bold = catItem->font(0);
        bold.setBold(true);
        catItem->setFont(0, bold);
        
        for (const auto* f : factions) {
            auto* fItem = new QTreeWidgetItem(catItem);
            fItem->setText(0, f->name.isEmpty() ? f->key : f->name);
            fItem->setText(1, tierName(f->tier));
            fItem->setText(2, QString::number(f->reputation));
            
            QColor tierColor;
            if (f->tier >= 7) tierColor = QColor("#4a8");
            else if (f->tier >= 6) tierColor = QColor("#48a");
            else if (f->tier >= 5) tierColor = QColor("#8a4");
            else if (f->tier <= 2) tierColor = QColor("#a44");
            else tierColor = QColor("#aaa");
            fItem->setForeground(1, tierColor);
        }
    }
    
    // Crafting
    m_craftingTable->setRowCount(0);
    for (const auto& p : c.crafting.professions) {
        int row = m_craftingTable->rowCount();
        m_craftingTable->insertRow(row);
        m_craftingTable->setItem(row, 0, new QTableWidgetItem(p.name));
        auto* tierItem = new QTableWidgetItem(QString::number(p.tier));
        tierItem->setTextAlignment(Qt::AlignCenter);
        m_craftingTable->setItem(row, 1, tierItem);
        auto* profItem = new QTableWidgetItem(QString::number(p.proficiency));
        profItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_craftingTable->setItem(row, 2, profItem);
        auto* mastItem = new QTableWidgetItem(QString::number(p.mastery));
        mastItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_craftingTable->setItem(row, 3, mastItem);
        auto* masteredItem = new QTableWidgetItem(p.hasMastered ? "✓" : "✗");
        masteredItem->setTextAlignment(Qt::AlignCenter);
        masteredItem->setForeground(p.hasMastered ? QColor("#4a8") : QColor("#888"));
        m_craftingTable->setItem(row, 4, masteredItem);
    }
    
    // Gear
    m_gearTable->setRowCount(0);
    for (const auto& [slot, itemId] : c.equippedGear) {
        int row = m_gearTable->rowCount();
        m_gearTable->insertRow(row);
        m_gearTable->setItem(row, 0, new QTableWidgetItem(slot));
        auto* idItem = new QTableWidgetItem(QString::number(itemId));
        idItem->setTextAlignment(Qt::AlignCenter);
        m_gearTable->setItem(row, 1, idItem);
        auto item = ItemDatabase::instance().getItem(QString::number(itemId));
        m_gearTable->setItem(row, 2, new QTableWidgetItem(item ? item->name : tr("(Unknown)")));
    }
    
    // Titles
    m_titlesTable->setRowCount(0);
    auto& db = GameDatabase::instance();
    for (int titleId : c.titles) {
        int row = m_titlesTable->rowCount();
        m_titlesTable->insertRow(row);
        auto* idItem = new QTableWidgetItem(QString::number(titleId));
        idItem->setTextAlignment(Qt::AlignCenter);
        m_titlesTable->setItem(row, 0, idItem);
        auto title = db.getTitle(QString::number(titleId));
        m_titlesTable->setItem(row, 1, new QTableWidgetItem(title ? title->name : tr("(Unknown)")));
    }
    
    // Emotes
    m_emotesTable->setRowCount(0);
    for (int emoteId : c.emotes) {
        int row = m_emotesTable->rowCount();
        m_emotesTable->insertRow(row);
        auto* idItem = new QTableWidgetItem(QString::number(emoteId));
        idItem->setTextAlignment(Qt::AlignCenter);
        m_emotesTable->setItem(row, 0, idItem);
        auto emote = db.getEmote(QString::number(emoteId));
        m_emotesTable->setItem(row, 1, new QTableWidgetItem(emote ? emote->command : tr("(Unknown)")));
    }
}

void CharacterDetailWidget::clear() {
    m_nameLabel->setText("-");
    m_classLabel->setText("-");
    m_levelLabel->setText("-");
    m_raceLabel->setText("-");
    m_serverLabel->setText("-");
    m_accountLabel->setText("-");
    m_lastPlayedLabel->setText("-");
    m_moraleLabel->setText("-");
    m_powerLabel->setText("-");
    m_goldLabel->setText("-");
    m_destinyLabel->setText("-");
    
    m_virtuesTable->setRowCount(0);
    m_reputationTree->clear();
    m_craftingTable->setRowCount(0);
    m_gearTable->setRowCount(0);
    m_titlesTable->setRowCount(0);
    m_emotesTable->setRowCount(0);
}

} // namespace lotro
