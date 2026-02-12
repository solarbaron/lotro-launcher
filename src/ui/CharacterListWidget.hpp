/**
 * LOTRO Launcher - Character List Widget
 * 
 * Displays saved characters in a list/grid view.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QWidget>
#include <memory>

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;

namespace lotro {

class CharacterTracker;
struct Character;

/**
 * Widget for displaying saved characters
 */
class CharacterListWidget : public QWidget {
    Q_OBJECT

public:
    explicit CharacterListWidget(QWidget* parent = nullptr);
    ~CharacterListWidget() override;
    
    /**
     * Set the character tracker to use
     */
    void setCharacterTracker(CharacterTracker* tracker);
    
    /**
     * Refresh the character list
     */
    void refresh();

signals:
    /**
     * Emitted when a character is selected
     */
    void characterSelected(const QString& name, const QString& server);
    
    /**
     * Emitted when delete is requested
     */
    void deleteRequested(const QString& name, const QString& server);

private slots:
    void onItemClicked(QListWidgetItem* item);
    void onDeleteClicked();

private:
    void setupUi();
    void addCharacterItem(const Character& character);
    
    CharacterTracker* m_tracker = nullptr;
    QListWidget* m_listWidget = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QPushButton* m_refreshButton = nullptr;
};

} // namespace lotro
