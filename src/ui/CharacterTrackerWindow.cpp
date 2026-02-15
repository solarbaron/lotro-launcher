/**
 * LOTRO Launcher - Character Tracker Window Implementation
 * 
 * Full-featured tabbed character tracking interface.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "CharacterTrackerWindow.hpp"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTime>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "DataExportWindow.hpp"
#include "companion/export/DataExporter.hpp"
#include "companion/GameDatabase.hpp"
#include "companion/ItemDatabase.hpp"
#include "dat/DataFacade.hpp"
#include "companion/ProcessMemory.hpp"

#include <spdlog/spdlog.h>
#include <QStandardPaths>

namespace lotro {

CharacterTrackerWindow::CharacterTrackerWindow(const QString& gamePath, QWidget* parent)
    : QDialog(parent)
    , m_gamePath(gamePath)
    , m_extractor(std::make_unique<CharacterExtractor>(gamePath))
    , m_autoRefreshTimer(new QTimer(this))
{
    setWindowTitle(tr("Character Tracker"));
    setMinimumSize(650, 600);
    resize(750, 700);
    
    auto dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_characterTracker = std::make_unique<CharacterTracker>(
        std::filesystem::path(dataDir.toStdString()) / "companion"
    );
    
    setupUi();
    setupConnections();
    m_autoRefreshTimer->setInterval(5000);
}

CharacterTrackerWindow::~CharacterTrackerWindow() {
    disconnectFromGame();
}

void CharacterTrackerWindow::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    
    // Table & tree selection styling — ensures alternating rows still highlight when selected
    setStyleSheet(R"(
        QTableWidget {
            background-color: #1a1a2e;
            alternate-background-color: #151525;
            color: #e0e0e0;
            gridline-color: #252542;
            border: 1px solid #3a3a5c;
        }
        QTableWidget::item:selected {
            background-color: #1a6d63;
            color: #ffffff;
        }
        QTableWidget::item:alternate:selected {
            background-color: #1a6d63;
            color: #ffffff;
        }
        QTreeWidget {
            background-color: #1a1a2e;
            alternate-background-color: #151525;
            color: #e0e0e0;
            border: 1px solid #3a3a5c;
        }
        QTreeWidget::item:selected {
            background-color: #1a6d63;
            color: #ffffff;
        }
        QTreeWidget::item:alternate:selected {
            background-color: #1a6d63;
            color: #ffffff;
        }
        QHeaderView::section {
            background-color: #1a1a2e;
            color: #c9a227;
            padding: 6px;
            border: none;
            border-right: 1px solid #252542;
            border-bottom: 2px solid #c9a227;
            font-weight: bold;
        }
    )");
    
    // === Top control bar ===
    auto* controlLayout = new QHBoxLayout();
    
    m_connectButton = new QPushButton(tr("Connect to Game"));
    m_connectButton->setMinimumHeight(36);
    controlLayout->addWidget(m_connectButton);
    
    m_refreshButton = new QPushButton(tr("Refresh"));
    m_refreshButton->setEnabled(false);
    m_refreshButton->setMinimumHeight(36);
    controlLayout->addWidget(m_refreshButton);
    
    m_exportButton = new QPushButton(tr("Export Data..."));
    m_exportButton->setMinimumHeight(36);
    controlLayout->addWidget(m_exportButton);
    
    m_saveButton = new QPushButton(tr("Save Character"));
    m_saveButton->setMinimumHeight(36);
    m_saveButton->setEnabled(false);
    m_saveButton->setToolTip(tr("Save current character to the Saved Characters list"));
    controlLayout->addWidget(m_saveButton);
    
    mainLayout->addLayout(controlLayout);
    
    // Status
    m_statusLabel = new QLabel(tr("Not connected"));
    m_statusLabel->setStyleSheet("color: #888; font-style: italic;");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_statusLabel);
    
    // === Tab widget ===
    m_tabWidget = new QTabWidget();
    m_tabWidget->addTab(createOverviewTab(), tr("Overview"));
    m_tabWidget->addTab(createVirtuesTab(), tr("Virtues"));
    m_tabWidget->addTab(createReputationTab(), tr("Reputation"));
    m_tabWidget->addTab(createCraftingTab(), tr("Crafting"));
    m_tabWidget->addTab(createGearTab(), tr("Gear"));
    m_tabWidget->addTab(createTitlesEmotesTab(), tr("Titles & Emotes"));
    
    mainLayout->addWidget(m_tabWidget, 1);
}

// ============ Tab Creators ============

QWidget* CharacterTrackerWindow::createOverviewTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    layout->setSpacing(12);
    
    // Character info group
    auto* charGroup = new QGroupBox(tr("Character"));
    auto* charLayout = new QGridLayout(charGroup);
    charLayout->setSpacing(6);
    
    int row = 0;
    charLayout->addWidget(new QLabel(tr("Name:")), row, 0);
    m_nameLabel = new QLabel("-");
    m_nameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    charLayout->addWidget(m_nameLabel, row++, 1);
    
    charLayout->addWidget(new QLabel(tr("Class:")), row, 0);
    m_classLabel = new QLabel("-");
    charLayout->addWidget(m_classLabel, row++, 1);
    
    charLayout->addWidget(new QLabel(tr("Level:")), row, 0);
    m_levelLabel = new QLabel("-");
    charLayout->addWidget(m_levelLabel, row++, 1);
    
    charLayout->addWidget(new QLabel(tr("Race:")), row, 0);
    m_raceLabel = new QLabel("-");
    charLayout->addWidget(m_raceLabel, row++, 1);
    
    charLayout->addWidget(new QLabel(tr("Server:")), row, 0);
    m_serverLabel = new QLabel("-");
    charLayout->addWidget(m_serverLabel, row++, 1);
    
    charLayout->addWidget(new QLabel(tr("Account:")), row, 0);
    m_accountLabel = new QLabel("-");
    charLayout->addWidget(m_accountLabel, row++, 1);
    
    charLayout->addWidget(new QLabel(tr("Acct Type:")), row, 0);
    m_accountTypeLabel = new QLabel("-");
    charLayout->addWidget(m_accountTypeLabel, row++, 1);
    
    charLayout->setColumnStretch(1, 1);
    layout->addWidget(charGroup);
    
    // Stats group
    auto* statsGroup = new QGroupBox(tr("Vitals"));
    auto* statsLayout = new QGridLayout(statsGroup);
    statsLayout->setSpacing(6);
    
    auto barStyle = QStringLiteral(
        "QProgressBar { border: 1px solid #444; border-radius: 3px; background: #222; text-align: center; color: #ddd; }"
        "QProgressBar::chunk { border-radius: 2px; }"
    );
    
    statsLayout->addWidget(new QLabel(tr("Morale:")), 0, 0);
    m_moraleLabel = new QLabel("-");
    statsLayout->addWidget(m_moraleLabel, 0, 1);
    
    m_moraleBar = new QProgressBar();
    m_moraleBar->setRange(0, 100);
    m_moraleBar->setValue(0);
    m_moraleBar->setFormat("%v / %m");
    m_moraleBar->setStyleSheet(barStyle + "QProgressBar::chunk { background: #4a8; }");
    statsLayout->addWidget(m_moraleBar, 1, 0, 1, 2);
    
    statsLayout->addWidget(new QLabel(tr("Power:")), 2, 0);
    m_powerLabel = new QLabel("-");
    statsLayout->addWidget(m_powerLabel, 2, 1);
    
    m_powerBar = new QProgressBar();
    m_powerBar->setRange(0, 100);
    m_powerBar->setValue(0);
    m_powerBar->setFormat("%v / %m");
    m_powerBar->setStyleSheet(barStyle + "QProgressBar::chunk { background: #48a; }");
    statsLayout->addWidget(m_powerBar, 3, 0, 1, 2);
    
    statsLayout->setColumnStretch(1, 1);
    layout->addWidget(statsGroup);
    
    // Currency group
    auto* currencyGroup = new QGroupBox(tr("Currency"));
    auto* currencyLayout = new QGridLayout(currencyGroup);
    currencyLayout->setSpacing(6);
    
    currencyLayout->addWidget(new QLabel(tr("Gold:")), 0, 0);
    m_goldLabel = new QLabel("-");
    m_goldLabel->setStyleSheet("color: #fc0;");
    currencyLayout->addWidget(m_goldLabel, 0, 1);
    
    currencyLayout->addWidget(new QLabel(tr("Destiny Points:")), 1, 0);
    m_destinyLabel = new QLabel("-");
    currencyLayout->addWidget(m_destinyLabel, 1, 1);
    
    currencyLayout->setColumnStretch(1, 1);
    layout->addWidget(currencyGroup);
    
    layout->addStretch();
    return widget;
}

QWidget* CharacterTrackerWindow::createVirtuesTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    
    auto* infoLabel = new QLabel(tr("Virtue ranks and XP progress for the current character."));
    infoLabel->setStyleSheet("color: #888; margin-bottom: 6px;");
    layout->addWidget(infoLabel);
    
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

QWidget* CharacterTrackerWindow::createReputationTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    
    auto* infoLabel = new QLabel(tr("Faction reputation standings grouped by region."));
    infoLabel->setStyleSheet("color: #888; margin-bottom: 6px;");
    layout->addWidget(infoLabel);
    
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

QWidget* CharacterTrackerWindow::createCraftingTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    
    auto* infoLabel = new QLabel(tr("Crafting profession progress and mastery status."));
    infoLabel->setStyleSheet("color: #888; margin-bottom: 6px;");
    layout->addWidget(infoLabel);
    
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

QWidget* CharacterTrackerWindow::createGearTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    
    auto* infoLabel = new QLabel(tr("Currently equipped gear by slot."));
    infoLabel->setStyleSheet("color: #888; margin-bottom: 6px;");
    layout->addWidget(infoLabel);
    
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

QWidget* CharacterTrackerWindow::createTitlesEmotesTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    
    // Titles section
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
    
    // Emotes section
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
    
    // Use splitter for resizable sections
    auto* splitter = new QSplitter(Qt::Vertical);
    splitter->addWidget(titlesGroup);
    splitter->addWidget(emotesGroup);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    
    layout->addWidget(splitter, 1);
    return widget;
}

// ============ Connections ============

void CharacterTrackerWindow::setupConnections() {
    connect(m_connectButton, &QPushButton::clicked, this, [this]() {
        if (m_connected) {
            disconnectFromGame();
        } else {
            connectToGame();
        }
    });
    
    connect(m_refreshButton, &QPushButton::clicked, this, &CharacterTrackerWindow::refresh);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &CharacterTrackerWindow::onAutoRefresh);
    
    connect(m_exportButton, &QPushButton::clicked, this, [this]() {
        if (!m_exportWindow) {
            auto facade = new dat::DataFacade(m_gamePath);
            auto mem = new ProcessMemory();
            auto proc = ProcessMemory::findLotroClient();
            if (proc && mem->open(proc->pid)) {
                spdlog::info("Export MemoryReader connected");
            } else {
                spdlog::warn("Export MemoryReader failed to connect");
            }
            
            auto exporter = new DataExporter(facade, mem, this);
            m_exportWindow = new DataExportWindow(exporter, this);
        }
        m_exportWindow->show();
        m_exportWindow->raise();
        m_exportWindow->activateWindow();
    });
    
    // Save button — converts CharacterData to Character and saves
    connect(m_saveButton, &QPushButton::clicked, this, [this]() {
        if (!m_lastCharacterData.basic.isValid() || !m_characterTracker) {
            return;
        }
        
        const auto& info = m_lastCharacterData.basic;
        Character character;
        character.name = info.name;
        character.server = info.server;
        character.characterClass = parseCharacterClass(info.className);
        character.race = parseCharacterRace(info.race);
        character.level = info.level;
        character.accountName = info.account;
        character.destinyPoints = info.destinyPoints;
        character.morale = info.morale;
        character.maxMorale = info.maxMorale;
        character.power = info.power;
        character.maxPower = info.maxPower;
        character.gold = info.gold;
        character.silver = info.silver;
        character.copper = info.copper;

        character.lastPlayed = std::chrono::system_clock::now();
        
        // Populate extended data from full extraction
        bool hasExtended = !m_lastCharacterData.virtues.empty() ||
                           !m_lastCharacterData.factions.empty() ||
                           !m_lastCharacterData.crafting.professions.empty();
        
        if (hasExtended) {
            character.hasExtendedData = true;
            
            // Virtues
            for (const auto& v : m_lastCharacterData.virtues) {
                SavedVirtue sv;
                sv.key = v.key;
                sv.name = v.name;
                sv.rank = v.rank;
                sv.xp = v.xp;
                character.virtues.push_back(sv);
            }
            
            // Factions
            for (const auto& f : m_lastCharacterData.factions) {
                SavedFaction sf;
                sf.factionId = f.factionId;
                sf.key = f.key;
                sf.name = f.name;
                sf.category = f.category;
                sf.tier = f.tier;
                sf.reputation = f.reputation;
                character.factions.push_back(sf);
            }
            
            // Crafting
            character.crafting.vocation = m_lastCharacterData.crafting.vocation;
            for (const auto& p : m_lastCharacterData.crafting.professions) {
                SavedCraftingProfession sp;
                sp.name = p.name;
                sp.tier = p.tier;
                sp.proficiency = p.proficiency;
                sp.mastery = p.mastery;
                sp.hasMastered = p.hasMastered;
                character.crafting.professions.push_back(sp);
            }
            
            // Gear, titles, emotes, skills, trait points
            character.equippedGear = m_lastCharacterData.equippedGear;
            character.titles = m_lastCharacterData.titles;
            character.emotes = m_lastCharacterData.emotes;
            character.skills = m_lastCharacterData.skills;
            character.traitPoints = m_lastCharacterData.traitPoints;
        }
        
        m_characterTracker->saveCharacter(character);
        setStatus(tr("Character saved: %1 (%2)").arg(character.name)
                  .arg(hasExtended ? tr("full data") : tr("basic")));
    });
}

// ============ Connect / Disconnect ============

void CharacterTrackerWindow::connectToGame() {
    setStatus(tr("Connecting..."));
    
    if (m_extractor->connect()) {
        m_connected = true;
        m_connectButton->setText(tr("Disconnect"));
        m_refreshButton->setEnabled(true);
        
        setStatus(tr("Connected to LOTRO client"));
        refresh();
        m_autoRefreshTimer->start();
        
        spdlog::info("Character tracker connected");
    } else {
        setStatus(m_extractor->lastError(), true);
    }
}

void CharacterTrackerWindow::disconnectFromGame() {
    m_autoRefreshTimer->stop();
    m_extractor->disconnect();
    m_connected = false;
    
    m_connectButton->setText(tr("Connect to Game"));
    m_refreshButton->setEnabled(false);
    m_saveButton->setEnabled(false);
    
    clearDisplay();
    setStatus(tr("Disconnected"));
    
    spdlog::info("Character tracker disconnected");
}

void CharacterTrackerWindow::refresh() {
    if (!m_connected) {
        return;
    }
    
    // Try full data extraction first
    auto fullData = m_extractor->extractFullData();
    if (fullData) {
        m_lastCharacterData = *fullData;
        m_saveButton->setEnabled(fullData->basic.isValid());
        
        updateOverview(fullData->basic);
        updateVirtues(fullData->virtues);
        updateReputation(fullData->factions);
        updateCrafting(fullData->crafting);
        updateGear(fullData->equippedGear);
        updateTitlesEmotes(fullData->titles, fullData->emotes);
        
        setStatus(tr("Full sync: %1").arg(QTime::currentTime().toString("hh:mm:ss")));
    } else {
        // Fallback to basic extraction
        auto info = m_extractor->extractCharacter();
        if (info) {
            m_lastCharacterData.basic = *info;
            m_saveButton->setEnabled(info->isValid());
            updateOverview(*info);
            setStatus(tr("Basic sync: %1").arg(QTime::currentTime().toString("hh:mm:ss")));
        } else {
            setStatus(m_extractor->lastError(), true);
        }
    }
}

void CharacterTrackerWindow::onAutoRefresh() {
    if (m_connected) {
        refresh();
    }
}

// ============ Display Updates ============

void CharacterTrackerWindow::updateOverview(const CharacterInfo& info) {
    if (!info.name.isEmpty()) {
        QString fullName = info.name;
        if (!info.surname.isEmpty()) {
            fullName += " " + info.surname;
        }
        m_nameLabel->setText(fullName);
    }
    
    m_classLabel->setText(info.className.isEmpty() ? "-" : info.className);
    m_levelLabel->setText(info.level > 0 ? QString::number(info.level) : "-");
    m_raceLabel->setText(info.race.isEmpty() ? "-" : info.race);
    m_serverLabel->setText(info.server.isEmpty() ? "-" : info.server);
    m_accountLabel->setText(info.account.isEmpty() ? "-" : info.account);
    
    QString typeStr = "-";
    switch(info.accountType) {
        case CharacterInfo::AccountType::FreeToPlay: typeStr = "Free to Play"; break;
        case CharacterInfo::AccountType::Premium: typeStr = "Premium"; break;
        case CharacterInfo::AccountType::VIP: typeStr = "VIP"; break;
        case CharacterInfo::AccountType::Lifetime: typeStr = "Lifetime"; break;
        default: break;
    }
    m_accountTypeLabel->setText(typeStr);
    
    // Vitals
    if (info.morale > 0) {
        m_moraleLabel->setText(QString::number(info.morale));
        m_moraleBar->setMaximum(info.morale);
        m_moraleBar->setValue(info.morale);
    }
    
    if (info.power > 0) {
        m_powerLabel->setText(QString::number(info.power));
        m_powerBar->setMaximum(info.power);
        m_powerBar->setValue(info.power);
    }
    
    // Currency
    m_goldLabel->setText(info.formattedMoney());
    m_destinyLabel->setText(QString::number(info.destinyPoints));
}

void CharacterTrackerWindow::updateVirtues(const std::vector<VirtueStatus>& virtues) {
    m_virtuesTable->setRowCount(0);
    
    for (const auto& v : virtues) {
        int row = m_virtuesTable->rowCount();
        m_virtuesTable->insertRow(row);
        
        QString displayName = v.name.isEmpty() ? v.key : v.name;
        m_virtuesTable->setItem(row, 0, new QTableWidgetItem(displayName));
        
        auto* rankItem = new QTableWidgetItem(QString::number(v.rank));
        rankItem->setTextAlignment(Qt::AlignCenter);
        m_virtuesTable->setItem(row, 1, rankItem);
        
        auto* xpItem = new QTableWidgetItem(QString::number(v.xp));
        xpItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_virtuesTable->setItem(row, 2, xpItem);
    }
    
    if (virtues.empty()) {
        m_virtuesTable->setRowCount(1);
        auto* emptyItem = new QTableWidgetItem(tr("No virtue data available"));
        emptyItem->setFlags(Qt::NoItemFlags);
        emptyItem->setForeground(QColor("#888"));
        m_virtuesTable->setItem(0, 0, emptyItem);
        m_virtuesTable->setSpan(0, 0, 1, 3);
    }
}

void CharacterTrackerWindow::updateReputation(const std::vector<FactionStatus>& factions) {
    m_reputationTree->clear();
    
    if (factions.empty()) {
        auto* emptyItem = new QTreeWidgetItem(m_reputationTree);
        emptyItem->setText(0, tr("No reputation data available"));
        emptyItem->setForeground(0, QColor("#888"));
        return;
    }
    
    // Tier name mapping
    auto tierName = [](int tier) -> QString {
        switch (tier) {
            case 1: return "Enemy";
            case 2: return "Outsider";
            case 3: return "Neutral";
            case 4: return "Acquaintance";
            case 5: return "Friend";
            case 6: return "Ally";
            case 7: return "Kindred";
            case 8: return "Respected";
            case 9: return "Honoured";
            case 10: return "Celebrated";
            default: return QString("Tier %1").arg(tier);
        }
    };
    
    // Group factions by category
    std::map<QString, std::vector<const FactionStatus*>> categorized;
    for (const auto& f : factions) {
        QString cat = f.category.isEmpty() ? tr("Other") : f.category;
        categorized[cat].push_back(&f);
    }
    
    for (const auto& [category, categoryFactions] : categorized) {
        auto* categoryItem = new QTreeWidgetItem(m_reputationTree);
        categoryItem->setText(0, category);
        categoryItem->setExpanded(true);
        QFont bold = categoryItem->font(0);
        bold.setBold(true);
        categoryItem->setFont(0, bold);
        
        for (const auto* f : categoryFactions) {
            auto* factionItem = new QTreeWidgetItem(categoryItem);
            factionItem->setText(0, f->name.isEmpty() ? f->key : f->name);
            factionItem->setText(1, tierName(f->tier));
            factionItem->setText(2, QString::number(f->reputation));
            
            // Color-code tier
            QColor tierColor;
            if (f->tier >= 7) tierColor = QColor("#4a8");      // Kindred = green
            else if (f->tier >= 6) tierColor = QColor("#48a");  // Ally = blue
            else if (f->tier >= 5) tierColor = QColor("#8a4");  // Friend = lime
            else if (f->tier <= 2) tierColor = QColor("#a44");  // Enemy = red
            else tierColor = QColor("#aaa");
            
            factionItem->setForeground(1, tierColor);
        }
    }
}

void CharacterTrackerWindow::updateCrafting(const CraftingStatus& crafting) {
    m_craftingTable->setRowCount(0);
    
    if (crafting.professions.empty()) {
        m_craftingTable->setRowCount(1);
        auto* emptyItem = new QTableWidgetItem(tr("No crafting data available"));
        emptyItem->setFlags(Qt::NoItemFlags);
        emptyItem->setForeground(QColor("#888"));
        m_craftingTable->setItem(0, 0, emptyItem);
        m_craftingTable->setSpan(0, 0, 1, 5);
        return;
    }
    
    for (const auto& prof : crafting.professions) {
        int row = m_craftingTable->rowCount();
        m_craftingTable->insertRow(row);
        
        m_craftingTable->setItem(row, 0, new QTableWidgetItem(prof.name));
        
        // Resolve tier name from GameDatabase
        auto* tierItem = new QTableWidgetItem(QString::number(prof.tier));
        tierItem->setTextAlignment(Qt::AlignCenter);
        m_craftingTable->setItem(row, 1, tierItem);
        
        auto* profItem = new QTableWidgetItem(QString::number(prof.proficiency));
        profItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_craftingTable->setItem(row, 2, profItem);
        
        auto* mastItem = new QTableWidgetItem(QString::number(prof.mastery));
        mastItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_craftingTable->setItem(row, 3, mastItem);
        
        auto* masteredItem = new QTableWidgetItem(prof.hasMastered ? "✓" : "✗");
        masteredItem->setTextAlignment(Qt::AlignCenter);
        masteredItem->setForeground(prof.hasMastered ? QColor("#4a8") : QColor("#888"));
        m_craftingTable->setItem(row, 4, masteredItem);
    }
}

void CharacterTrackerWindow::updateGear(const std::map<QString, int>& gear) {
    m_gearTable->setRowCount(0);
    
    if (gear.empty()) {
        m_gearTable->setRowCount(1);
        auto* emptyItem = new QTableWidgetItem(tr("No gear data available"));
        emptyItem->setFlags(Qt::NoItemFlags);
        emptyItem->setForeground(QColor("#888"));
        m_gearTable->setItem(0, 0, emptyItem);
        m_gearTable->setSpan(0, 0, 1, 3);
        return;
    }
    
    for (const auto& [slot, itemId] : gear) {
        int row = m_gearTable->rowCount();
        m_gearTable->insertRow(row);
        
        m_gearTable->setItem(row, 0, new QTableWidgetItem(slot));
        
        auto* idItem = new QTableWidgetItem(QString::number(itemId));
        idItem->setTextAlignment(Qt::AlignCenter);
        m_gearTable->setItem(row, 1, idItem);
        
        // Try to resolve item name from ItemDatabase
        auto item = ItemDatabase::instance().getItem(QString::number(itemId));
        QString itemName = item ? item->name : tr("(Unknown item)");
        m_gearTable->setItem(row, 2, new QTableWidgetItem(itemName));
    }
}

void CharacterTrackerWindow::updateTitlesEmotes(const std::vector<int>& titles, const std::vector<int>& emotes) {
    // Titles
    m_titlesTable->setRowCount(0);
    
    if (titles.empty()) {
        m_titlesTable->setRowCount(1);
        auto* emptyItem = new QTableWidgetItem(tr("No titles data available"));
        emptyItem->setFlags(Qt::NoItemFlags);
        emptyItem->setForeground(QColor("#888"));
        m_titlesTable->setItem(0, 0, emptyItem);
        m_titlesTable->setSpan(0, 0, 1, 2);
    } else {
        auto& db = GameDatabase::instance();
        for (int titleId : titles) {
            int row = m_titlesTable->rowCount();
            m_titlesTable->insertRow(row);
            
            auto* idItem = new QTableWidgetItem(QString::number(titleId));
            idItem->setTextAlignment(Qt::AlignCenter);
            m_titlesTable->setItem(row, 0, idItem);
            
            auto title = db.getTitle(QString::number(titleId));
            QString titleName = title ? title->name : tr("(Unknown title)");
            m_titlesTable->setItem(row, 1, new QTableWidgetItem(titleName));
        }
    }
    
    // Emotes
    m_emotesTable->setRowCount(0);
    
    if (emotes.empty()) {
        m_emotesTable->setRowCount(1);
        auto* emptyItem = new QTableWidgetItem(tr("No emote data available"));
        emptyItem->setFlags(Qt::NoItemFlags);
        emptyItem->setForeground(QColor("#888"));
        m_emotesTable->setItem(0, 0, emptyItem);
        m_emotesTable->setSpan(0, 0, 1, 2);
    } else {
        auto& db = GameDatabase::instance();
        for (int emoteId : emotes) {
            int row = m_emotesTable->rowCount();
            m_emotesTable->insertRow(row);
            
            auto* idItem = new QTableWidgetItem(QString::number(emoteId));
            idItem->setTextAlignment(Qt::AlignCenter);
            m_emotesTable->setItem(row, 0, idItem);
            
            auto emote = db.getEmote(QString::number(emoteId));
            QString emoteName = emote ? emote->command : tr("(Unknown emote)");
            m_emotesTable->setItem(row, 1, new QTableWidgetItem(emoteName));
        }
    }
}

void CharacterTrackerWindow::clearDisplay() {
    // Overview
    m_nameLabel->setText("-");
    m_classLabel->setText("-");
    m_levelLabel->setText("-");
    m_raceLabel->setText("-");
    m_serverLabel->setText("-");
    m_accountLabel->setText("-");
    m_accountTypeLabel->setText("-");
    m_moraleLabel->setText("-");
    m_moraleBar->setValue(0);
    m_powerLabel->setText("-");
    m_powerBar->setValue(0);
    m_goldLabel->setText("-");
    m_destinyLabel->setText("-");
    
    // Tables
    m_virtuesTable->setRowCount(0);
    m_reputationTree->clear();
    m_craftingTable->setRowCount(0);
    m_gearTable->setRowCount(0);
    m_titlesTable->setRowCount(0);
    m_emotesTable->setRowCount(0);
}

void CharacterTrackerWindow::setStatus(const QString& status, bool isError) {
    m_statusLabel->setText(status);
    if (isError) {
        m_statusLabel->setStyleSheet("color: #e55; font-style: italic;");
    } else {
        m_statusLabel->setStyleSheet("color: #888; font-style: italic;");
    }
}

} // namespace lotro
