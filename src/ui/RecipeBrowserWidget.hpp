/**
 * LOTRO Launcher - Recipe Browser Widget
 * 
 * Browse and search LOTRO crafting recipes.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QWidget>
#include "companion/GameDatabase.hpp"

class QLineEdit;
class QComboBox;
class QTableWidget;
class QLabel;
class QTextEdit;

namespace lotro {

/**
 * Widget for browsing recipes
 */
class RecipeBrowserWidget : public QWidget {
    Q_OBJECT

public:
    explicit RecipeBrowserWidget(QWidget* parent = nullptr);
    ~RecipeBrowserWidget() override;
    
    void refresh();

private slots:
    void onSearchChanged(const QString& text);
    void onProfessionChanged(int index);
    void onRecipeSelected();

private:
    void setupUi();
    void populateRecipes(const std::vector<Recipe>& recipes);
    void showRecipeDetails(const Recipe& recipe);
    
    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_professionCombo = nullptr;
    QTableWidget* m_recipeTable = nullptr;
    QTextEdit* m_detailsView = nullptr;
    QLabel* m_countLabel = nullptr;
    
    std::vector<Recipe> m_currentRecipes;
};

} // namespace lotro
