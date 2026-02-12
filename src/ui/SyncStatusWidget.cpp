/**
 * LOTRO Launcher - Sync Status Widget Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "SyncStatusWidget.hpp"
#include "companion/LiveSyncService.hpp"
#include "companion/CharacterExtractor.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>

namespace lotro {

SyncStatusWidget::SyncStatusWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

SyncStatusWidget::~SyncStatusWidget() = default;

void SyncStatusWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);
    
    // Status row
    auto* statusRow = new QHBoxLayout();
    
    // Status indicator (green/red circle)
    m_statusIndicator = new QLabel("●");
    m_statusIndicator->setStyleSheet("color: #888; font-size: 16px;");
    statusRow->addWidget(m_statusIndicator);
    
    m_statusLabel = new QLabel(tr("Sync stopped"));
    m_statusLabel->setStyleSheet("color: #aaa;");
    statusRow->addWidget(m_statusLabel, 1);
    
    mainLayout->addLayout(statusRow);
    
    // Character info
    m_characterLabel = new QLabel(tr("No character"));
    m_characterLabel->setStyleSheet("color: #666; font-style: italic;");
    mainLayout->addWidget(m_characterLabel);
    
    // Auto-save checkbox
    m_autoSaveCheck = new QCheckBox(tr("Auto-save on character change/level up"));
    m_autoSaveCheck->setChecked(true);
    m_autoSaveCheck->setEnabled(false);  // Always enabled for now
    mainLayout->addWidget(m_autoSaveCheck);
}

void SyncStatusWidget::setSyncService(LiveSyncService* service) {
    if (m_syncService) {
        disconnect(m_syncService, nullptr, this, nullptr);
    }
    
    m_syncService = service;
    
    if (m_syncService) {
        connect(m_syncService, &LiveSyncService::connectionChanged,
                this, &SyncStatusWidget::onConnectionChanged);
        connect(m_syncService, &LiveSyncService::characterUpdated,
                this, &SyncStatusWidget::onCharacterUpdated);
        connect(m_syncService, &LiveSyncService::statusChanged,
                this, &SyncStatusWidget::onStatusChanged);
        
        // Update initial state
        updateIndicator(m_syncService->isConnected());
    }
}

void SyncStatusWidget::onConnectionChanged(bool connected) {
    updateIndicator(connected);
}

void SyncStatusWidget::onCharacterUpdated(const CharacterInfo& info) {
    QString text = QString("%1 - Level %2 %3")
        .arg(info.name)
        .arg(info.level)
        .arg(info.className);
    
    if (!info.server.isEmpty()) {
        text += QString(" (%1)").arg(info.server);
    }
    
    m_characterLabel->setText(text);
    m_characterLabel->setStyleSheet("color: #fff;");
}

void SyncStatusWidget::onStatusChanged(const QString& status) {
    m_statusLabel->setText(status);
}

void SyncStatusWidget::onSyncStopped() {
    m_characterLabel->setText(tr("No character"));
    m_characterLabel->setStyleSheet("color: #666; font-style: italic;");
    updateIndicator(false);
}

void SyncStatusWidget::updateIndicator(bool connected) {
    if (connected) {
        m_statusIndicator->setStyleSheet("color: #4CAF50; font-size: 16px;");  // Green
        m_statusIndicator->setToolTip(tr("Connected to game"));
    } else if (m_syncService && m_syncService->isRunning()) {
        m_statusIndicator->setStyleSheet("color: #FFC107; font-size: 16px;");  // Yellow
        m_statusIndicator->setToolTip(tr("Searching for game..."));
    } else {
        m_statusIndicator->setStyleSheet("color: #888; font-size: 16px;");  // Gray
        m_statusIndicator->setToolTip(tr("Sync stopped"));
    }
}

} // namespace lotro

