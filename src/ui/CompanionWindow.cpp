/**
 * LOTRO Launcher - Companion Window Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "CompanionWindow.hpp"
#include "CharacterTrackerWindow.hpp"
#include "CharacterListWidget.hpp"
#include "DeedBrowserWidget.hpp"
#include "RecipeBrowserWidget.hpp"
#include "SyncStatusWidget.hpp"
#include "GearSimulatorWidget.hpp"
#include "DataExportWindow.hpp"
#include "companion/CharacterTracker.hpp"
#include "companion/GameDatabase.hpp"
#include "companion/ItemDatabase.hpp"
#include "companion/LiveSyncService.hpp"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QGridLayout>
#include <QStandardPaths>
#include <QCoreApplication>

#include <spdlog/spdlog.h>

namespace lotro {

CompanionWindow::CompanionWindow(const QString& gamePath, QWidget* parent)
    : QDialog(parent)
    , m_gamePath(gamePath)
{
    setWindowTitle(tr("LOTRO Companion"));
    setMinimumSize(600, 500);
    resize(700, 600);
    
    // Initialize character tracker with app data directory
    auto dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_characterTracker = std::make_unique<CharacterTracker>(
        std::filesystem::path(dataDir.toStdString()) / "companion"
    );
    
    // Initialize live sync service
    m_syncService = std::make_unique<LiveSyncService>(this);
    connect(m_syncService.get(), &LiveSyncService::characterSaved,
            this, &CompanionWindow::onCharacterSaved);
    
    // Initialize game database with bundled data
    auto appDir = QCoreApplication::applicationDirPath();
    GameDatabase::instance().initialize(
        std::filesystem::path(appDir.toStdString()) / "data"
    );
    
    // Initialize item database
    ItemDatabase::instance().initialize(
        std::filesystem::path(appDir.toStdString()) / "data"
    );
    
    setupUi();
}

CompanionWindow::~CompanionWindow() = default;

void CompanionWindow::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // Create tab widget
    m_tabWidget = new QTabWidget();
    
    // === TAB 1: Character Tracker ===
    auto* trackerWidget = new QWidget();
    auto* trackerLayout = new QVBoxLayout(trackerWidget);
    trackerLayout->setContentsMargins(16, 16, 16, 16);
    
    auto* infoLabel = new QLabel(
        tr("<b>Character Tracker</b><br>"
           "Connect to the running game to view live character data. "
           "The game must be running for this feature to work.")
    );
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #aaa; margin-bottom: 10px;");
    trackerLayout->addWidget(infoLabel);
    
    // Real-time sync status widget
    auto* syncGroup = new QGroupBox(tr("Real-Time Sync"));
    auto* syncLayout = new QVBoxLayout(syncGroup);
    m_syncStatus = new SyncStatusWidget();
    m_syncStatus->setSyncService(m_syncService.get());
    connect(m_syncStatus, &SyncStatusWidget::destroyed, this, [this]() {
        m_syncStatus = nullptr;
    });
    syncLayout->addWidget(m_syncStatus);
    trackerLayout->addWidget(syncGroup);
    
    // Start sync button - use member or capture-safe approach
    m_syncButton = new QPushButton(tr("Start Background Sync"));
    m_syncButton->setMinimumHeight(36);
    connect(m_syncButton, &QPushButton::clicked, this, &CompanionWindow::onSyncToggled);
    trackerLayout->addWidget(m_syncButton);
    
    auto* openTrackerBtn = new QPushButton(tr("Open Advanced Tracker"));
    openTrackerBtn->setMinimumHeight(36);
    connect(openTrackerBtn, &QPushButton::clicked, this, [this]() {
        CharacterTrackerWindow tracker(m_gamePath, this);
        tracker.exec();
        // After tracker closes, refresh saved characters list
        if (m_characterList) {
            m_characterList->refresh();
        }
    });
    trackerLayout->addWidget(openTrackerBtn);
    
    trackerLayout->addStretch();
    m_tabWidget->addTab(trackerWidget, tr("Character"));
    
    // === TAB 2: Saved Characters ===
    auto* savedWidget = new QWidget();
    auto* savedLayout = new QVBoxLayout(savedWidget);
    savedLayout->setContentsMargins(16, 16, 16, 16);
    
    auto* savedLabel = new QLabel(
        tr("<b>Saved Characters</b><br>"
           "View and manage previously tracked characters.")
    );
    savedLabel->setWordWrap(true);
    savedLabel->setStyleSheet("color: #aaa; margin-bottom: 10px;");
    savedLayout->addWidget(savedLabel);
    
    m_characterList = new CharacterListWidget();
    m_characterList->setCharacterTracker(m_characterTracker.get());
    savedLayout->addWidget(m_characterList, 1);
    
    m_tabWidget->addTab(savedWidget, tr("Saved"));
    
    // === TAB 3: Deeds Browser ===
    auto* deedsWidget = new QWidget();
    auto* deedsLayout = new QVBoxLayout(deedsWidget);
    deedsLayout->setContentsMargins(16, 16, 16, 16);
    
    m_deedBrowser = new DeedBrowserWidget();
    deedsLayout->addWidget(m_deedBrowser, 1);
    
    m_tabWidget->addTab(deedsWidget, tr("Deeds"));
    
    // === TAB 4: Recipes Browser ===
    auto* recipesWidget = new QWidget();
    auto* recipesLayout = new QVBoxLayout(recipesWidget);
    recipesLayout->setContentsMargins(16, 16, 16, 16);
    
    m_recipeBrowser = new RecipeBrowserWidget();
    recipesLayout->addWidget(m_recipeBrowser, 1);
    
    m_tabWidget->addTab(recipesWidget, tr("Recipes"));
    
    // === TAB 5: Gear Simulator ===
    auto* gearWidget = new QWidget();
    auto* gearLayout = new QVBoxLayout(gearWidget);
    gearLayout->setContentsMargins(8, 8, 8, 8);
    
    m_gearSimulator = new GearSimulatorWidget();
    gearLayout->addWidget(m_gearSimulator, 1);
    
    m_tabWidget->addTab(gearWidget, tr("Gear Sim"));
    
    // === TAB 6: Export ===
    auto* exportWidget = new QWidget();
    auto* exportLayout = new QVBoxLayout(exportWidget);
    exportLayout->setContentsMargins(16, 16, 16, 16);
    
    auto* exportLabel = new QLabel(
        tr("<b>Data Export</b><br>"
           "Export character data in LOTRO Companion-compatible format.<br>"
           "Connect to the game first in the Character tab.")
    );
    exportLabel->setWordWrap(true);
    exportLabel->setStyleSheet("color: #aaa; margin-bottom: 10px;");
    exportLayout->addWidget(exportLabel);
    
    auto* openExportBtn = new QPushButton(tr("Open Export Dialog"));
    openExportBtn->setMinimumHeight(40);
    connect(openExportBtn, &QPushButton::clicked, this, [this]() {
        CharacterTrackerWindow tracker(m_gamePath, this);
        tracker.exec();
    });
    exportLayout->addWidget(openExportBtn);
    
    exportLayout->addStretch();
    m_tabWidget->addTab(exportWidget, tr("Export"));
    
    mainLayout->addWidget(m_tabWidget);
    
    spdlog::info("Companion window initialized");
}

void CompanionWindow::onSyncToggled() {
    if (!m_syncService) {
        return;
    }
    
    if (m_syncService->isRunning()) {
        m_syncService->stop();
        if (m_syncButton) {
            m_syncButton->setText(tr("Start Background Sync"));
        }
        if (m_syncStatus) {
            m_syncStatus->onSyncStopped();
        }
    } else {
        m_syncService->start(m_gamePath, m_characterTracker.get());
        if (m_syncButton) {
            m_syncButton->setText(tr("Stop Sync"));
        }
    }
}

void CompanionWindow::onCharacterSaved(const QString& /*name*/, const QString& /*server*/) {
    // Refresh the saved characters list when auto-save occurs
    if (m_characterList) {
        m_characterList->refresh();
    }
}

} // namespace lotro
