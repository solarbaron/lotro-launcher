/**
 * LOTRO Launcher - Journal Manager
 * 
 * Manages journal entries with persistence.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "JournalEntry.hpp"

#include <QObject>
#include <QVector>
#include <QString>

namespace lotro {

/**
 * Manages player journal entries
 * 
 * Provides CRUD operations and persistence to JSON file.
 */
class JournalManager : public QObject {
    Q_OBJECT
    
public:
    static JournalManager& instance();
    
    /**
     * Get all journal entries
     */
    QVector<JournalEntry> entries() const { return m_entries; }
    
    /**
     * Get entry by ID
     */
    JournalEntry* getEntry(const QString& id);
    
    /**
     * Create a new entry
     */
    JournalEntry createEntry(const QString& title = "New Entry");
    
    /**
     * Update an existing entry
     */
    bool updateEntry(const JournalEntry& entry);
    
    /**
     * Delete an entry
     */
    bool deleteEntry(const QString& id);
    
    /**
     * Save entries to disk
     */
    void save();
    
    /**
     * Load entries from disk
     */
    void load();
    
signals:
    void entriesChanged();
    void entryAdded(const QString& id);
    void entryUpdated(const QString& id);
    void entryRemoved(const QString& id);
    
private:
    JournalManager();
    ~JournalManager() = default;
    
    QString getStoragePath() const;
    
    QVector<JournalEntry> m_entries;
};

} // namespace lotro
