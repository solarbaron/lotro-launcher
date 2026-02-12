/**
 * LOTRO Launcher - Character Tracker Window Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "CharacterTrackerWindow.hpp"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTime>
#include <QVBoxLayout>

#include <QVBoxLayout>
#include <QHBoxLayout>

#include "DataExportWindow.hpp"
#include "companion/export/DataExporter.hpp"
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
    setMinimumSize(400, 500);
    resize(450, 550);
    
    // Initialize character tracker
    auto dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_characterTracker = std::make_unique<CharacterTracker>(
        std::filesystem::path(dataDir.toStdString()) / "companion"
    );
    
    setupUi();
    setupConnections();
    
    // Auto-refresh every 5 seconds when connected
    m_autoRefreshTimer->setInterval(5000);
}

CharacterTrackerWindow::~CharacterTrackerWindow() {
    disconnectFromGame();
}

void CharacterTrackerWindow::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    
    // Connection controls
    auto* controlLayout = new QHBoxLayout();
    
    m_connectButton = new QPushButton(tr("Connect to Game"));
    m_connectButton->setMinimumHeight(36);
    controlLayout->addWidget(m_connectButton);
    
    m_refreshButton = new QPushButton(tr("Refresh"));
    m_refreshButton->setEnabled(false);
    m_refreshButton->setMinimumHeight(36);
    controlLayout->addWidget(m_refreshButton);
    
    // Export Data Button
    m_exportButton = new QPushButton(tr("Export Data..."));
    m_exportButton->setMinimumHeight(36);
    controlLayout->addWidget(m_exportButton);
    
    // Save Character Button
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
    
    // Character info group
    m_characterGroup = new QGroupBox(tr("Character"));
    auto* charLayout = new QGridLayout(m_characterGroup);
    charLayout->setSpacing(8);
    
    charLayout->addWidget(new QLabel(tr("Name:")), 0, 0);
    m_nameLabel = new QLabel("-");
    m_nameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    charLayout->addWidget(m_nameLabel, 0, 1);
    
    charLayout->addWidget(new QLabel(tr("Class:")), 1, 0);
    m_classLabel = new QLabel("-");
    charLayout->addWidget(m_classLabel, 1, 1);
    
    charLayout->addWidget(new QLabel(tr("Level:")), 2, 0);
    m_levelLabel = new QLabel("-");
    charLayout->addWidget(m_levelLabel, 2, 1);
    
    charLayout->addWidget(new QLabel(tr("Race:")), 3, 0);
    m_raceLabel = new QLabel("-");
    charLayout->addWidget(m_raceLabel, 3, 1);
    
    charLayout->addWidget(new QLabel(tr("Server:")), 4, 0);
    m_serverLabel = new QLabel("-");
    charLayout->addWidget(m_serverLabel, 4, 1);
    
    charLayout->addWidget(new QLabel(tr("Account:")), 5, 0);
    m_accountLabel = new QLabel("-");
    charLayout->addWidget(m_accountLabel, 5, 1);
    
    charLayout->addWidget(new QLabel(tr("Acct Type:")), 6, 0);
    m_accountTypeLabel = new QLabel("-");
    charLayout->addWidget(m_accountTypeLabel, 6, 1);
    
    charLayout->setColumnStretch(1, 1);
    mainLayout->addWidget(m_characterGroup);
    
    // Stats group
    m_statsGroup = new QGroupBox(tr("Stats"));
    auto* statsLayout = new QGridLayout(m_statsGroup);
    statsLayout->setSpacing(8);
    
    statsLayout->addWidget(new QLabel(tr("Morale:")), 0, 0);
    m_moraleLabel = new QLabel("-");
    statsLayout->addWidget(m_moraleLabel, 0, 1);
    
    m_moraleBar = new QProgressBar();
    m_moraleBar->setRange(0, 100);
    m_moraleBar->setValue(0);
    m_moraleBar->setStyleSheet(
        "QProgressBar { border: 1px solid #444; border-radius: 3px; background: #222; }"
        "QProgressBar::chunk { background: #4a8; border-radius: 2px; }"
    );
    statsLayout->addWidget(m_moraleBar, 1, 0, 1, 2);
    
    statsLayout->addWidget(new QLabel(tr("Power:")), 2, 0);
    m_powerLabel = new QLabel("-");
    statsLayout->addWidget(m_powerLabel, 2, 1);
    
    m_powerBar = new QProgressBar();
    m_powerBar->setRange(0, 100);
    m_powerBar->setValue(0);
    m_powerBar->setStyleSheet(
        "QProgressBar { border: 1px solid #444; border-radius: 3px; background: #222; }"
        "QProgressBar::chunk { background: #48a; border-radius: 2px; }"
    );
    statsLayout->addWidget(m_powerBar, 3, 0, 1, 2);
    
    statsLayout->setColumnStretch(1, 1);
    mainLayout->addWidget(m_statsGroup);
    
    // Currency group
    m_currencyGroup = new QGroupBox(tr("Currency"));
    auto* currencyLayout = new QGridLayout(m_currencyGroup);
    currencyLayout->setSpacing(8);
    
    currencyLayout->addWidget(new QLabel(tr("Gold:")), 0, 0);
    m_goldLabel = new QLabel("-");
    m_goldLabel->setStyleSheet("color: #fc0;");
    currencyLayout->addWidget(m_goldLabel, 0, 1);
    
    currencyLayout->addWidget(new QLabel(tr("Destiny Points:")), 1, 0);
    m_destinyLabel = new QLabel("-");
    currencyLayout->addWidget(m_destinyLabel, 1, 1);
    
    currencyLayout->addWidget(new QLabel(tr("LOTRO Points:")), 2, 0);
    m_lotroPointsLabel = new QLabel("-");
    currencyLayout->addWidget(m_lotroPointsLabel, 2, 1);
    
    currencyLayout->setColumnStretch(1, 1);
    mainLayout->addWidget(m_currencyGroup);
    
    mainLayout->addStretch();
    
    // Initially disable info groups
    m_characterGroup->setEnabled(false);
    m_statsGroup->setEnabled(false);
    m_currencyGroup->setEnabled(false);
}

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
            // Ensure memory reader connects to the same process if possible
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
    
    // Save button - converts CharacterInfo to Character and saves
    connect(m_saveButton, &QPushButton::clicked, this, [this]() {
        if (!m_lastCharacterInfo.isValid() || !m_characterTracker) {
            return;
        }
        
        Character character;
        character.name = m_lastCharacterInfo.name;
        character.server = m_lastCharacterInfo.server;
        character.characterClass = parseCharacterClass(m_lastCharacterInfo.className);
        character.race = parseCharacterRace(m_lastCharacterInfo.race);
        character.level = m_lastCharacterInfo.level;
        character.accountName = m_lastCharacterInfo.account;
        character.destinyPoints = m_lastCharacterInfo.destinyPoints;
        character.lastPlayed = std::chrono::system_clock::now();
        
        m_characterTracker->saveCharacter(character);
        setStatus(tr("Character saved: %1").arg(character.name));
    });
}

void CharacterTrackerWindow::connectToGame() {
    setStatus(tr("Connecting..."));
    
    if (m_extractor->connect()) {
        m_connected = true;
        m_connectButton->setText(tr("Disconnect"));
        m_refreshButton->setEnabled(true);
        m_characterGroup->setEnabled(true);
        m_statsGroup->setEnabled(true);
        m_currencyGroup->setEnabled(true);
        
        setStatus(tr("Connected to LOTRO client"));
        
        // Do initial refresh
        refresh();
        
        // Start auto-refresh
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
    m_characterGroup->setEnabled(false);
    m_statsGroup->setEnabled(false);
    m_currencyGroup->setEnabled(false);
    
    clearDisplay();
    setStatus(tr("Disconnected"));
    
    spdlog::info("Character tracker disconnected");
}

void CharacterTrackerWindow::refresh() {
    if (!m_connected) {
        return;
    }
    
    auto info = m_extractor->extractCharacter();
    if (info) {
        m_lastCharacterInfo = *info;  // Store for saving
        m_saveButton->setEnabled(info->isValid());
        updateDisplay(*info);
        setStatus(tr("Updated: %1").arg(QTime::currentTime().toString("hh:mm:ss")));
    } else {
        setStatus(m_extractor->lastError(), true);
    }
}

void CharacterTrackerWindow::onAutoRefresh() {
    if (m_connected) {
        refresh();
    }
}

void CharacterTrackerWindow::updateDisplay(const CharacterInfo& info) {
    // Character info
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
    
    // Stats
    if (info.maxMorale > 0) {
        m_moraleLabel->setText(QString("%1 / %2").arg(info.morale).arg(info.maxMorale));
        m_moraleBar->setMaximum(info.maxMorale);
        m_moraleBar->setValue(info.morale);
    }
    
    if (info.maxPower > 0) {
        m_powerLabel->setText(QString("%1 / %2").arg(info.power).arg(info.maxPower));
        m_powerBar->setMaximum(info.maxPower);
        m_powerBar->setValue(info.power);
    }
    
    // Currency
    m_goldLabel->setText(info.formattedMoney());
    m_destinyLabel->setText(QString::number(info.destinyPoints));
    m_lotroPointsLabel->setText(QString::number(info.lotroPoints));
}

void CharacterTrackerWindow::clearDisplay() {
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
    m_lotroPointsLabel->setText("-");
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
