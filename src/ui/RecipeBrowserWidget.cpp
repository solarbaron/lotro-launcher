/**
 * LOTRO Launcher - Recipe Browser Widget Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "RecipeBrowserWidget.hpp"
#include "companion/GameDatabase.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QTextEdit>
#include <QSplitter>

#include <spdlog/spdlog.h>

namespace lotro {

RecipeBrowserWidget::RecipeBrowserWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    refresh();
}

RecipeBrowserWidget::~RecipeBrowserWidget() = default;

void RecipeBrowserWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);
    
    // Search bar
    auto* searchLayout = new QHBoxLayout();
    
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText(tr("Search recipes..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &RecipeBrowserWidget::onSearchChanged);
    searchLayout->addWidget(m_searchEdit, 1);
    
    m_professionCombo = new QComboBox();
    m_professionCombo->addItem(tr("All Professions"), QString());
    m_professionCombo->addItem(tr("Cook"), "Cook");
    m_professionCombo->addItem(tr("Jeweller"), "Jeweller");
    m_professionCombo->addItem(tr("Metalsmith"), "Metalsmith");
    m_professionCombo->addItem(tr("Scholar"), "Scholar");
    m_professionCombo->addItem(tr("Tailor"), "Tailor");
    m_professionCombo->addItem(tr("Weaponsmith"), "Weaponsmith");
    m_professionCombo->addItem(tr("Woodworker"), "Woodworker");
    m_professionCombo->addItem(tr("Farmer"), "Farmer");
    m_professionCombo->addItem(tr("Forester"), "Forester");
    m_professionCombo->addItem(tr("Prospector"), "Prospector");
    connect(m_professionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RecipeBrowserWidget::onProfessionChanged);
    searchLayout->addWidget(m_professionCombo);
    
    mainLayout->addLayout(searchLayout);
    
    // Count label
    m_countLabel = new QLabel(tr("0 recipes"));
    m_countLabel->setStyleSheet("color: #888;");
    mainLayout->addWidget(m_countLabel);
    
    // Splitter with table and details
    auto* splitter = new QSplitter(Qt::Vertical);
    
    // Recipe table
    m_recipeTable = new QTableWidget();
    m_recipeTable->setColumnCount(4);
    m_recipeTable->setHorizontalHeaderLabels({tr("Name"), tr("Profession"), tr("Tier"), tr("Output")});
    m_recipeTable->horizontalHeader()->setStretchLastSection(true);
    m_recipeTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_recipeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_recipeTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_recipeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_recipeTable->setAlternatingRowColors(true);
    m_recipeTable->verticalHeader()->setVisible(false);
    connect(m_recipeTable, &QTableWidget::itemSelectionChanged,
            this, &RecipeBrowserWidget::onRecipeSelected);
    splitter->addWidget(m_recipeTable);
    
    // Details view
    m_detailsView = new QTextEdit();
    m_detailsView->setReadOnly(true);
    m_detailsView->setMaximumHeight(150);
    m_detailsView->setStyleSheet("background-color: #1a1a2e; border: 1px solid #3a3a5c;");
    splitter->addWidget(m_detailsView);
    
    splitter->setSizes({300, 100});
    mainLayout->addWidget(splitter, 1);
}

void RecipeBrowserWidget::refresh() {
    auto& db = GameDatabase::instance();
    
    if (!db.isLoaded()) {
        m_countLabel->setText(tr("Database not loaded"));
        m_recipeTable->setRowCount(0);
        return;
    }
    
    QString profession = m_professionCombo->currentData().toString();
    QString searchText = m_searchEdit->text();
    
    if (!profession.isEmpty() && searchText.isEmpty()) {
        m_currentRecipes = db.getRecipesByProfession(profession);
    } else if (!searchText.isEmpty()) {
        auto results = db.searchRecipes(searchText);
        
        if (!profession.isEmpty()) {
            m_currentRecipes.clear();
            for (const auto& recipe : results) {
                if (recipe.profession == profession) {
                    m_currentRecipes.push_back(recipe);
                }
            }
        } else {
            m_currentRecipes = results;
        }
    } else {
        m_currentRecipes = db.searchRecipes("");
    }
    
    populateRecipes(m_currentRecipes);
}

void RecipeBrowserWidget::populateRecipes(const std::vector<Recipe>& recipes) {
    m_recipeTable->setRowCount(static_cast<int>(recipes.size()));
    m_countLabel->setText(tr("%1 recipes").arg(recipes.size()));
    
    int row = 0;
    for (const auto& recipe : recipes) {
        m_recipeTable->setItem(row, 0, new QTableWidgetItem(recipe.name));
        m_recipeTable->setItem(row, 1, new QTableWidgetItem(recipe.profession));
        m_recipeTable->setItem(row, 2, new QTableWidgetItem(QString("Tier %1").arg(recipe.tier)));
        m_recipeTable->setItem(row, 3, new QTableWidgetItem(recipe.outputItemName));
        row++;
    }
}

void RecipeBrowserWidget::onSearchChanged(const QString& /*text*/) {
    refresh();
}

void RecipeBrowserWidget::onProfessionChanged(int /*index*/) {
    refresh();
}

void RecipeBrowserWidget::onRecipeSelected() {
    int row = m_recipeTable->currentRow();
    if (row < 0 || row >= static_cast<int>(m_currentRecipes.size())) {
        m_detailsView->clear();
        return;
    }
    
    showRecipeDetails(m_currentRecipes[row]);
}

void RecipeBrowserWidget::showRecipeDetails(const Recipe& recipe) {
    QString html = QString("<h3>%1</h3>").arg(recipe.name.toHtmlEscaped());
    
    html += QString("<p><b>Profession:</b> %1 (Tier %2)</p>")
        .arg(recipe.profession.toHtmlEscaped())
        .arg(recipe.tier);
    
    if (!recipe.category.isEmpty()) {
        html += QString("<p><b>Category:</b> %1</p>").arg(recipe.category.toHtmlEscaped());
    }
    
    // Output
    html += QString("<p><b>Creates:</b> %1")
        .arg(recipe.outputItemName.toHtmlEscaped());
    if (recipe.outputQuantity > 1) {
        html += QString(" x%1").arg(recipe.outputQuantity);
    }
    html += "</p>";
    
    // Ingredients
    if (!recipe.ingredients.empty()) {
        html += "<p><b>Ingredients:</b></p><ul>";
        for (const auto& ing : recipe.ingredients) {
            html += QString("<li>%1 x%2</li>")
                .arg(ing.name.toHtmlEscaped())
                .arg(ing.quantity);
        }
        html += "</ul>";
    }
    
    m_detailsView->setHtml(html);
}

} // namespace lotro
