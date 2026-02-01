/**
 * LOTRO Launcher - Journal Window Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "JournalWindow.hpp"
#include "core/JournalManager.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QMessageBox>
#include <QGroupBox>

namespace lotro {

JournalWindow::JournalWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Journal");
    setMinimumSize(800, 600);
    setupUi();
    refreshEntryList();
}

void JournalWindow::setupUi() {
    auto* mainLayout = new QHBoxLayout(this);
    
    // Create splitter for resizable panels
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    
    // Left panel - Entry list
    auto* leftWidget = new QWidget();
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    auto* listLabel = new QLabel("Entries");
    listLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    leftLayout->addWidget(listLabel);
    
    m_entryList = new QListWidget();
    m_entryList->setMinimumWidth(200);
    leftLayout->addWidget(m_entryList);
    
    // List buttons
    auto* listBtnLayout = new QHBoxLayout();
    m_newBtn = new QPushButton("New");
    m_newBtn->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    m_deleteBtn = new QPushButton("Delete");
    m_deleteBtn->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    listBtnLayout->addWidget(m_newBtn);
    listBtnLayout->addWidget(m_deleteBtn);
    leftLayout->addLayout(listBtnLayout);
    
    splitter->addWidget(leftWidget);
    
    // Right panel - Editor
    auto* rightWidget = new QWidget();
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    
    // Title
    auto* titleLayout = new QHBoxLayout();
    auto* titleLabel = new QLabel("Title:");
    titleLabel->setStyleSheet("font-weight: bold;");
    m_titleEdit = new QLineEdit();
    m_titleEdit->setPlaceholderText("Enter entry title...");
    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(m_titleEdit);
    rightLayout->addLayout(titleLayout);
    
    // Content
    auto* contentLabel = new QLabel("Content:");
    contentLabel->setStyleSheet("font-weight: bold;");
    rightLayout->addWidget(contentLabel);
    
    m_contentEdit = new QTextEdit();
    m_contentEdit->setPlaceholderText("Write your notes, plans, and goals here...");
    m_contentEdit->setMinimumWidth(400);
    rightLayout->addWidget(m_contentEdit, 1);
    
    // Save button
    auto* saveBtnLayout = new QHBoxLayout();
    saveBtnLayout->addStretch();
    m_saveBtn = new QPushButton("Save");
    m_saveBtn->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    saveBtnLayout->addWidget(m_saveBtn);
    rightLayout->addLayout(saveBtnLayout);
    
    splitter->addWidget(rightWidget);
    
    // Set initial splitter sizes
    splitter->setSizes({250, 550});
    
    mainLayout->addWidget(splitter);
    
    // Connect signals
    connect(m_newBtn, &QPushButton::clicked, this, &JournalWindow::onNewEntry);
    connect(m_deleteBtn, &QPushButton::clicked, this, &JournalWindow::onDeleteEntry);
    connect(m_saveBtn, &QPushButton::clicked, this, &JournalWindow::onSaveEntry);
    connect(m_entryList, &QListWidget::currentItemChanged, 
            this, &JournalWindow::onEntrySelected);
    
    // Track modifications
    connect(m_titleEdit, &QLineEdit::textChanged, [this]() { m_modified = true; });
    connect(m_contentEdit, &QTextEdit::textChanged, [this]() { m_modified = true; });
    
    // Disable editor until entry selected
    m_titleEdit->setEnabled(false);
    m_contentEdit->setEnabled(false);
    m_saveBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
}

void JournalWindow::refreshEntryList() {
    m_entryList->clear();
    
    auto& manager = JournalManager::instance();
    for (const auto& entry : manager.entries()) {
        auto* item = new QListWidgetItem(entry.title);
        item->setData(Qt::UserRole, entry.id);
        m_entryList->addItem(item);
    }
}

void JournalWindow::onNewEntry() {
    // Save current entry if modified
    if (m_modified && !m_currentEntryId.isEmpty()) {
        saveCurrentEntry();
    }
    
    auto& manager = JournalManager::instance();
    JournalEntry entry = manager.createEntry("New Entry");
    
    refreshEntryList();
    
    // Select the new entry
    for (int i = 0; i < m_entryList->count(); ++i) {
        if (m_entryList->item(i)->data(Qt::UserRole).toString() == entry.id) {
            m_entryList->setCurrentRow(i);
            break;
        }
    }
    
    m_titleEdit->selectAll();
    m_titleEdit->setFocus();
}

void JournalWindow::onDeleteEntry() {
    if (m_currentEntryId.isEmpty()) return;
    
    auto result = QMessageBox::question(this, "Delete Entry",
        "Are you sure you want to delete this entry?",
        QMessageBox::Yes | QMessageBox::No);
    
    if (result == QMessageBox::Yes) {
        auto& manager = JournalManager::instance();
        manager.deleteEntry(m_currentEntryId);
        
        m_currentEntryId.clear();
        m_titleEdit->clear();
        m_contentEdit->clear();
        m_titleEdit->setEnabled(false);
        m_contentEdit->setEnabled(false);
        m_saveBtn->setEnabled(false);
        m_deleteBtn->setEnabled(false);
        m_modified = false;
        
        refreshEntryList();
    }
}

void JournalWindow::onSaveEntry() {
    saveCurrentEntry();
}

void JournalWindow::saveCurrentEntry() {
    if (m_currentEntryId.isEmpty()) return;
    
    auto& manager = JournalManager::instance();
    JournalEntry* entry = manager.getEntry(m_currentEntryId);
    
    if (entry) {
        entry->title = m_titleEdit->text();
        entry->content = m_contentEdit->toPlainText();
        manager.updateEntry(*entry);
        
        // Update list item title
        for (int i = 0; i < m_entryList->count(); ++i) {
            auto* item = m_entryList->item(i);
            if (item->data(Qt::UserRole).toString() == m_currentEntryId) {
                item->setText(entry->title);
                break;
            }
        }
        
        m_modified = false;
    }
}

void JournalWindow::onEntrySelected(QListWidgetItem* current, QListWidgetItem* previous) {
    Q_UNUSED(previous);
    
    // Save previous entry if modified
    if (m_modified && !m_currentEntryId.isEmpty()) {
        saveCurrentEntry();
    }
    
    if (!current) {
        m_currentEntryId.clear();
        m_titleEdit->clear();
        m_contentEdit->clear();
        m_titleEdit->setEnabled(false);
        m_contentEdit->setEnabled(false);
        m_saveBtn->setEnabled(false);
        m_deleteBtn->setEnabled(false);
        return;
    }
    
    QString id = current->data(Qt::UserRole).toString();
    loadEntry(id);
}

void JournalWindow::loadEntry(const QString& id) {
    auto& manager = JournalManager::instance();
    JournalEntry* entry = manager.getEntry(id);
    
    if (entry) {
        m_currentEntryId = id;
        m_titleEdit->setText(entry->title);
        m_contentEdit->setPlainText(entry->content);
        
        m_titleEdit->setEnabled(true);
        m_contentEdit->setEnabled(true);
        m_saveBtn->setEnabled(true);
        m_deleteBtn->setEnabled(true);
        m_modified = false;
    }
}

} // namespace lotro
