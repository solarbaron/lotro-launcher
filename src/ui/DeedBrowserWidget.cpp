/**
 * LOTRO Launcher - Deed Browser Widget Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "DeedBrowserWidget.hpp"
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
#include <QPushButton>

#include <spdlog/spdlog.h>

namespace lotro {

// Category name helper
static QString categoryName(DeedCategory cat) {
    switch (cat) {
        case DeedCategory::Class: return "Class";
        case DeedCategory::Race: return "Race";
        case DeedCategory::Social: return "Social";
        case DeedCategory::Exploration: return "Exploration";
        case DeedCategory::Quest: return "Quest";
        case DeedCategory::Reputation: return "Reputation";
        case DeedCategory::Slayer: return "Slayer";
        case DeedCategory::Lore: return "Lore";
        default: return "Unknown";
    }
}

DeedBrowserWidget::DeedBrowserWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    refresh();
}

DeedBrowserWidget::~DeedBrowserWidget() = default;

void DeedBrowserWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);
    
    // Search bar
    auto* searchLayout = new QHBoxLayout();
    
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText(tr("Search deeds..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &DeedBrowserWidget::onSearchChanged);
    searchLayout->addWidget(m_searchEdit, 1);
    
    m_categoryCombo = new QComboBox();
    m_categoryCombo->addItem(tr("All Categories"), -1);
    m_categoryCombo->addItem(tr("Class"), static_cast<int>(DeedCategory::Class));
    m_categoryCombo->addItem(tr("Race"), static_cast<int>(DeedCategory::Race));
    m_categoryCombo->addItem(tr("Social"), static_cast<int>(DeedCategory::Social));
    m_categoryCombo->addItem(tr("Exploration"), static_cast<int>(DeedCategory::Exploration));
    m_categoryCombo->addItem(tr("Quest"), static_cast<int>(DeedCategory::Quest));
    m_categoryCombo->addItem(tr("Reputation"), static_cast<int>(DeedCategory::Reputation));
    m_categoryCombo->addItem(tr("Slayer"), static_cast<int>(DeedCategory::Slayer));
    m_categoryCombo->addItem(tr("Lore"), static_cast<int>(DeedCategory::Lore));
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DeedBrowserWidget::onCategoryChanged);
    searchLayout->addWidget(m_categoryCombo);
    
    mainLayout->addLayout(searchLayout);
    
    // Count label
    m_countLabel = new QLabel(tr("0 deeds"));
    m_countLabel->setStyleSheet("color: #888;");
    mainLayout->addWidget(m_countLabel);
    
    // Splitter with table and details
    auto* splitter = new QSplitter(Qt::Vertical);
    
    // Deed table
    m_deedTable = new QTableWidget();
    m_deedTable->setColumnCount(5);
    m_deedTable->setHorizontalHeaderLabels({tr("Name"), tr("Category"), tr("Region"), tr("Level"), tr("LP")});
    m_deedTable->horizontalHeader()->setStretchLastSection(true);
    m_deedTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_deedTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_deedTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_deedTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_deedTable->setAlternatingRowColors(true);
    m_deedTable->verticalHeader()->setVisible(false);
    connect(m_deedTable, &QTableWidget::itemSelectionChanged,
            this, &DeedBrowserWidget::onDeedSelected);
    splitter->addWidget(m_deedTable);
    
    // Details view
    m_detailsView = new QTextEdit();
    m_detailsView->setReadOnly(true);
    m_detailsView->setMaximumHeight(150);
    m_detailsView->setStyleSheet("background-color: #1a1a2e; border: 1px solid #3a3a5c;");
    splitter->addWidget(m_detailsView);
    
    splitter->setSizes({300, 100});
    mainLayout->addWidget(splitter, 1);
}

void DeedBrowserWidget::refresh() {
    auto& db = GameDatabase::instance();
    
    if (!db.isLoaded()) {
        m_countLabel->setText(tr("Database not loaded"));
        m_deedTable->setRowCount(0);
        return;
    }
    
    // Get deeds based on current filter
    int categoryIndex = m_categoryCombo->currentData().toInt();
    QString searchText = m_searchEdit->text();
    
    if (categoryIndex >= 0 && searchText.isEmpty()) {
        // Filter by category only
        m_currentDeeds = db.getDeedsByCategory(static_cast<DeedCategory>(categoryIndex));
    } else if (!searchText.isEmpty()) {
        // Search 
        auto results = db.searchDeeds(searchText);
        
        // Also filter by category if selected
        if (categoryIndex >= 0) {
            DeedCategory cat = static_cast<DeedCategory>(categoryIndex);
            m_currentDeeds.clear();
            for (const auto& deed : results) {
                if (deed.category == cat) {
                    m_currentDeeds.push_back(deed);
                }
            }
        } else {
            m_currentDeeds = results;
        }
    } else {
        // Show all deeds (use search with empty for all)
        m_currentDeeds = db.searchDeeds("");
    }
    
    populateDeeds(m_currentDeeds);
}

void DeedBrowserWidget::populateDeeds(const std::vector<Deed>& deeds) {
    m_deedTable->setRowCount(static_cast<int>(deeds.size()));
    m_countLabel->setText(tr("%1 deeds").arg(deeds.size()));
    
    int row = 0;
    for (const auto& deed : deeds) {
        m_deedTable->setItem(row, 0, new QTableWidgetItem(deed.name));
        m_deedTable->setItem(row, 1, new QTableWidgetItem(categoryName(deed.category)));
        m_deedTable->setItem(row, 2, new QTableWidgetItem(deed.region));
        m_deedTable->setItem(row, 3, new QTableWidgetItem(deed.level > 0 ? QString::number(deed.level) : "-"));
        m_deedTable->setItem(row, 4, new QTableWidgetItem(deed.lotroPoints > 0 ? QString::number(deed.lotroPoints) : "-"));
        row++;
    }
}

void DeedBrowserWidget::onSearchChanged(const QString& /*text*/) {
    refresh();
}

void DeedBrowserWidget::onCategoryChanged(int /*index*/) {
    refresh();
}

void DeedBrowserWidget::onDeedSelected() {
    int row = m_deedTable->currentRow();
    if (row < 0 || row >= static_cast<int>(m_currentDeeds.size())) {
        m_detailsView->clear();
        return;
    }
    
    showDeedDetails(m_currentDeeds[row]);
}

void DeedBrowserWidget::showDeedDetails(const Deed& deed) {
    QString html = QString("<h3>%1</h3>").arg(deed.name.toHtmlEscaped());
    
    html += QString("<p><b>Category:</b> %1</p>").arg(categoryName(deed.category));
    
    if (!deed.region.isEmpty()) {
        html += QString("<p><b>Region:</b> %1</p>").arg(deed.region.toHtmlEscaped());
    }
    
    if (deed.level > 0) {
        html += QString("<p><b>Level:</b> %1</p>").arg(deed.level);
    }
    
    if (!deed.description.isEmpty()) {
        html += QString("<p>%1</p>").arg(deed.description.toHtmlEscaped());
    }
    
    // Rewards
    QStringList rewards;
    if (deed.lotroPoints > 0) {
        rewards << QString("%1 LOTRO Points").arg(deed.lotroPoints);
    }
    if (deed.virtueXP > 0) {
        rewards << QString("%1 Virtue XP").arg(deed.virtueXP);
    }
    if (!deed.titleReward.isEmpty()) {
        rewards << QString("Title: %1").arg(deed.titleReward);
    }
    if (!deed.traitReward.isEmpty()) {
        rewards << QString("Trait: %1").arg(deed.traitReward);
    }
    
    if (!rewards.isEmpty()) {
        html += QString("<p><b>Rewards:</b> %1</p>").arg(rewards.join(", "));
    }
    
    m_detailsView->setHtml(html);
}

} // namespace lotro
