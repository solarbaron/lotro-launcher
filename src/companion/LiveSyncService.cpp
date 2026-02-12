/**
 * LOTRO Launcher - Live Sync Service Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "LiveSyncService.hpp"
#include "CharacterExtractor.hpp"
#include "CharacterTracker.hpp"

#include <spdlog/spdlog.h>

namespace lotro {

LiveSyncService::LiveSyncService(QObject* parent)
    : QObject(parent)
    , m_syncTimer(new QTimer(this))
    , m_connectionTimer(new QTimer(this))
{
    connect(m_syncTimer, &QTimer::timeout, this, &LiveSyncService::onSyncTimer);
    connect(m_connectionTimer, &QTimer::timeout, this, &LiveSyncService::onConnectionCheck);
    
    // Connection check is less frequent (every 10 seconds)
    m_connectionTimer->setInterval(10000);
}

LiveSyncService::~LiveSyncService() {
    stop();
}

void LiveSyncService::start(const QString& gamePath, CharacterTracker* tracker) {
    if (m_running) {
        return;
    }
    
    m_gamePath = gamePath;
    m_tracker = tracker;
    m_running = true;
    
    // Create extractor
    m_extractor = std::make_unique<CharacterExtractor>(gamePath);
    
    // Try to connect immediately
    tryConnect();
    
    // Start connection check timer
    m_connectionTimer->start();
    
    emit statusChanged(tr("Sync service started"));
    spdlog::info("LiveSyncService started");
}

void LiveSyncService::stop() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    m_connected = false;
    
    m_syncTimer->stop();
    m_connectionTimer->stop();
    
    if (m_extractor) {
        m_extractor->disconnect();
        m_extractor.reset();
    }
    
    emit connectionChanged(false);
    emit statusChanged(tr("Sync service stopped"));
    spdlog::info("LiveSyncService stopped");
}

void LiveSyncService::setSyncInterval(int ms) {
    m_syncInterval = ms;
    if (m_syncTimer->isActive()) {
        m_syncTimer->setInterval(ms);
    }
}

void LiveSyncService::tryConnect() {
    if (!m_extractor) {
        return;
    }
    
    emit statusChanged(tr("Connecting to game..."));
    
    if (m_extractor->connect()) {
        m_connected = true;
        emit connectionChanged(true);
        emit statusChanged(tr("Connected to LOTRO"));
        
        // Start sync timer
        m_syncTimer->setInterval(m_syncInterval);
        m_syncTimer->start();
        
        // Do immediate sync
        syncCharacter();
        
        spdlog::info("LiveSyncService connected to game");
    } else {
        m_connected = false;
        emit connectionChanged(false);
        emit statusChanged(tr("Game not found - waiting..."));
        
        m_syncTimer->stop();
    }
}

void LiveSyncService::onConnectionCheck() {
    if (!m_running) {
        return;
    }
    
    // If not connected, try to connect
    if (!m_connected) {
        tryConnect();
    } else if (m_extractor && !m_extractor->isConnected()) {
        // Lost connection
        m_connected = false;
        emit connectionChanged(false);
        emit statusChanged(tr("Connection lost - reconnecting..."));
        m_syncTimer->stop();
        
        spdlog::info("LiveSyncService lost connection");
    }
}

void LiveSyncService::onSyncTimer() {
    if (m_connected) {
        syncCharacter();
    }
}

void LiveSyncService::syncCharacter() {
    if (!m_extractor || !m_connected) {
        return;
    }
    
    auto info = m_extractor->extractCharacter();
    if (!info || !info->isValid()) {
        // No valid character data - might be at character select
        return;
    }
    
    emit characterUpdated(*info);
    
    // Check if character changed or leveled up
    bool characterChanged = (info->name != m_lastCharacterName || 
                            info->server != m_lastCharacterServer);
    bool leveledUp = (info->level > m_lastLevel && !m_lastCharacterName.isEmpty());
    
    if (characterChanged) {
        spdlog::info("Character changed: {} on {}", 
                     info->name.toStdString(), info->server.toStdString());
        
        // Auto-save on character change
        autoSaveCharacter(*info);
    } else if (leveledUp) {
        spdlog::info("Character {} leveled up to {}", 
                     info->name.toStdString(), info->level);
        
        // Auto-save on level up
        autoSaveCharacter(*info);
    }
    
    // Update tracking
    m_lastCharacterName = info->name;
    m_lastCharacterServer = info->server;
    m_lastLevel = info->level;
}

void LiveSyncService::autoSaveCharacter(const CharacterInfo& info) {
    if (!m_tracker) {
        return;
    }
    
    Character character;
    character.name = info.name;
    character.server = info.server;
    character.characterClass = parseCharacterClass(info.className);
    character.race = parseCharacterRace(info.race);
    character.level = info.level;
    character.accountName = info.account;
    character.destinyPoints = info.destinyPoints;
    character.lastPlayed = std::chrono::system_clock::now();
    
    m_tracker->saveCharacter(character);
    
    emit characterSaved(info.name, info.server);
    emit statusChanged(tr("Auto-saved: %1").arg(info.name));
}

} // namespace lotro
