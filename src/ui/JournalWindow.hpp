/**
 * LOTRO Launcher - Journal Window
 * 
 * UI for managing player journal entries.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>

namespace lotro {

/**
 * Journal window for managing player notes and plans
 * 
 * Features a list of entries on the left and an editor on the right.
 */
class JournalWindow : public QDialog {
    Q_OBJECT
    
public:
    explicit JournalWindow(QWidget* parent = nullptr);
    ~JournalWindow() = default;
    
private slots:
    void onNewEntry();
    void onDeleteEntry();
    void onSaveEntry();
    void onEntrySelected(QListWidgetItem* current, QListWidgetItem* previous);
    void refreshEntryList();
    
private:
    void setupUi();
    void loadEntry(const QString& id);
    void saveCurrentEntry();
    
    QListWidget* m_entryList;
    QLineEdit* m_titleEdit;
    QTextEdit* m_contentEdit;
    QPushButton* m_newBtn;
    QPushButton* m_deleteBtn;
    QPushButton* m_saveBtn;
    
    QString m_currentEntryId;
    bool m_modified = false;
};

} // namespace lotro
