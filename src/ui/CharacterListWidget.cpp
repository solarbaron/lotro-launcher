/**
 * LOTRO Launcher - Character List Widget Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "CharacterListWidget.hpp"
#include "companion/CharacterTracker.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QDateTime>

#include <spdlog/spdlog.h>

namespace lotro {

CharacterListWidget::CharacterListWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

CharacterListWidget::~CharacterListWidget() = default;

void CharacterListWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);
    
    // Toolbar
    auto* toolbar = new QHBoxLayout();
    
    m_refreshButton = new QPushButton(tr("Refresh"));
    m_refreshButton->setMinimumHeight(30);
    connect(m_refreshButton, &QPushButton::clicked, this, &CharacterListWidget::refresh);
    toolbar->addWidget(m_refreshButton);
    
    toolbar->addStretch();
    
    m_deleteButton = new QPushButton(tr("Delete"));
    m_deleteButton->setMinimumHeight(30);
    m_deleteButton->setEnabled(false);
    connect(m_deleteButton, &QPushButton::clicked, this, &CharacterListWidget::onDeleteClicked);
    toolbar->addWidget(m_deleteButton);
    
    mainLayout->addLayout(toolbar);
    
    // Empty state label
    m_emptyLabel = new QLabel(
        tr("No saved characters.\n\n"
           "Connect to the game in the Character tab\n"
           "to automatically save your character data.")
    );
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet("color: #888; font-style: italic;");
    mainLayout->addWidget(m_emptyLabel);
    
    // Character list
    m_listWidget = new QListWidget();
    m_listWidget->setStyleSheet(R"(
        QListWidget {
            background-color: #1a1a2e;
            border: 1px solid #3a3a5c;
            border-radius: 4px;
        }
        QListWidget::item {
            padding: 10px;
            border-bottom: 1px solid #2a2a4c;
        }
        QListWidget::item:selected {
            background-color: #2a4a6a;
        }
        QListWidget::item:hover {
            background-color: #252542;
        }
    )");
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->hide();
    
    connect(m_listWidget, &QListWidget::itemClicked, 
            this, &CharacterListWidget::onItemClicked);
    
    mainLayout->addWidget(m_listWidget, 1);
}

void CharacterListWidget::setCharacterTracker(CharacterTracker* tracker) {
    m_tracker = tracker;
    refresh();
}

void CharacterListWidget::refresh() {
    m_listWidget->clear();
    m_deleteButton->setEnabled(false);
    
    if (!m_tracker) {
        m_emptyLabel->show();
        m_listWidget->hide();
        return;
    }
    
    auto characters = m_tracker->getCharacters();
    
    if (characters.empty()) {
        m_emptyLabel->show();
        m_listWidget->hide();
        return;
    }
    
    m_emptyLabel->hide();
    m_listWidget->show();
    
    // Sort by last played (most recent first)
    std::sort(characters.begin(), characters.end(), 
        [](const Character& a, const Character& b) {
            return a.lastPlayed > b.lastPlayed;
        });
    
    for (const auto& character : characters) {
        addCharacterItem(character);
    }
}

void CharacterListWidget::addCharacterItem(const Character& character) {
    // Format: "Name (Level Class) - Server"
    QString displayText = QString("%1 (Level %2 %3)\n%4 - %5")
        .arg(character.name)
        .arg(character.level)
        .arg(character.classString())
        .arg(character.raceString())
        .arg(character.server);
    
    // Add last played info
    auto lastPlayedSecs = std::chrono::duration_cast<std::chrono::seconds>(
        character.lastPlayed.time_since_epoch()).count();
    if (lastPlayedSecs > 0) {
        QDateTime lastPlayed = QDateTime::fromSecsSinceEpoch(lastPlayedSecs);
        displayText += QString("\nLast played: %1").arg(lastPlayed.toString("yyyy-MM-dd hh:mm"));
    }
    
    auto* item = new QListWidgetItem(displayText);
    item->setData(Qt::UserRole, character.name);
    item->setData(Qt::UserRole + 1, character.server);
    
    // Color based on class
    QColor classColor;
    switch (character.characterClass) {
        case CharacterClass::Beorning: classColor = QColor("#8B4513"); break;
        case CharacterClass::Brawler: classColor = QColor("#CD853F"); break;
        case CharacterClass::Burglar: classColor = QColor("#FFD700"); break;
        case CharacterClass::Captain: classColor = QColor("#4169E1"); break;
        case CharacterClass::Champion: classColor = QColor("#DC143C"); break;
        case CharacterClass::Guardian: classColor = QColor("#228B22"); break;
        case CharacterClass::Hunter: classColor = QColor("#32CD32"); break;
        case CharacterClass::LoreMaster: classColor = QColor("#9370DB"); break;
        case CharacterClass::Minstrel: classColor = QColor("#87CEEB"); break;
        case CharacterClass::RuneKeeper: classColor = QColor("#FF69B4"); break;
        case CharacterClass::Warden: classColor = QColor("#FFA500"); break;
        default: classColor = QColor("#AAAAAA"); break;
    }
    item->setForeground(classColor);
    
    m_listWidget->addItem(item);
}

void CharacterListWidget::onItemClicked(QListWidgetItem* item) {
    m_deleteButton->setEnabled(true);
    
    QString name = item->data(Qt::UserRole).toString();
    QString server = item->data(Qt::UserRole + 1).toString();
    emit characterSelected(name, server);
}

void CharacterListWidget::onDeleteClicked() {
    auto* item = m_listWidget->currentItem();
    if (!item) return;
    
    QString name = item->data(Qt::UserRole).toString();
    QString server = item->data(Qt::UserRole + 1).toString();
    
    if (m_tracker) {
        m_tracker->removeCharacter(name, server);
        refresh();
    }
    
    emit deleteRequested(name, server);
}

} // namespace lotro
