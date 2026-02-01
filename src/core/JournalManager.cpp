/**
 * LOTRO Launcher - Journal Manager Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "JournalManager.hpp"
#include "platform/Platform.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>

#include <spdlog/spdlog.h>

namespace lotro {

JournalManager& JournalManager::instance() {
    static JournalManager instance;
    return instance;
}

JournalManager::JournalManager() {
    load();
}

QString JournalManager::getStoragePath() const {
    auto dataPath = Platform::getDataPath();
    return QString::fromStdString((dataPath / "journals.json").string());
}

JournalEntry* JournalManager::getEntry(const QString& id) {
    for (auto& entry : m_entries) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

JournalEntry JournalManager::createEntry(const QString& title) {
    JournalEntry entry = JournalEntry::create(title);
    m_entries.append(entry);
    save();
    
    emit entryAdded(entry.id);
    emit entriesChanged();
    
    spdlog::info("Created journal entry: {}", title.toStdString());
    return entry;
}

bool JournalManager::updateEntry(const JournalEntry& entry) {
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id == entry.id) {
            m_entries[i] = entry;
            m_entries[i].modifiedAt = QDateTime::currentDateTime();
            save();
            
            emit entryUpdated(entry.id);
            emit entriesChanged();
            return true;
        }
    }
    return false;
}

bool JournalManager::deleteEntry(const QString& id) {
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id == id) {
            QString title = m_entries[i].title;
            m_entries.removeAt(i);
            save();
            
            emit entryRemoved(id);
            emit entriesChanged();
            
            spdlog::info("Deleted journal entry: {}", title.toStdString());
            return true;
        }
    }
    return false;
}

void JournalManager::save() {
    QJsonArray array;
    for (const auto& entry : m_entries) {
        array.append(entry.toJson());
    }
    
    QJsonDocument doc(array);
    QString path = getStoragePath();
    
    // Ensure directory exists
    QFileInfo fileInfo(path);
    QDir().mkpath(fileInfo.absolutePath());
    
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        spdlog::debug("Saved {} journal entries", m_entries.size());
    } else {
        spdlog::error("Failed to save journals to: {}", path.toStdString());
    }
}

void JournalManager::load() {
    QString path = getStoragePath();
    QFile file(path);
    
    if (!file.exists()) {
        spdlog::debug("No journal file found at: {}", path.toStdString());
        return;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        spdlog::error("Failed to open journals file: {}", path.toStdString());
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isArray()) {
        spdlog::error("Invalid journals file format");
        return;
    }
    
    m_entries.clear();
    QJsonArray array = doc.array();
    for (const auto& value : array) {
        if (value.isObject()) {
            m_entries.append(JournalEntry::fromJson(value.toObject()));
        }
    }
    
    spdlog::info("Loaded {} journal entries", m_entries.size());
}

} // namespace lotro
