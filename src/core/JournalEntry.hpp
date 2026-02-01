/**
 * LOTRO Launcher - Journal Entry
 * 
 * Data model for player journal entries.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QUuid>

namespace lotro {

/**
 * A single journal entry with title and content
 */
struct JournalEntry {
    QString id;           // Unique ID (UUID)
    QString title;        // Entry title
    QString content;      // Entry content (plain text or HTML)
    QDateTime createdAt;  // When entry was created
    QDateTime modifiedAt; // When entry was last modified
    
    /**
     * Create a new entry with default values
     */
    static JournalEntry create(const QString& title = "New Entry") {
        JournalEntry entry;
        entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        entry.title = title;
        entry.content = "";
        entry.createdAt = QDateTime::currentDateTime();
        entry.modifiedAt = entry.createdAt;
        return entry;
    }
    
    /**
     * Serialize to JSON
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["title"] = title;
        obj["content"] = content;
        obj["createdAt"] = createdAt.toString(Qt::ISODate);
        obj["modifiedAt"] = modifiedAt.toString(Qt::ISODate);
        return obj;
    }
    
    /**
     * Deserialize from JSON
     */
    static JournalEntry fromJson(const QJsonObject& obj) {
        JournalEntry entry;
        entry.id = obj["id"].toString();
        entry.title = obj["title"].toString();
        entry.content = obj["content"].toString();
        entry.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
        entry.modifiedAt = QDateTime::fromString(obj["modifiedAt"].toString(), Qt::ISODate);
        return entry;
    }
};

} // namespace lotro
