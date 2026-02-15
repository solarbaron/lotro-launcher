/**
 * LOTRO Launcher - Addon Manager Window Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "AddonManagerWindow.hpp"
#include "core/config/ConfigManager.hpp"
#include "addons/AddonManager.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTabBar>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QProgressBar>
#include <QStackedWidget>
#include <QLabel>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QListWidget>
#include <QListWidgetItem>
#include <QComboBox>

#include <spdlog/spdlog.h>

namespace lotro {

// ============================================================================
// Custom QTableWidgetItem for numeric sorting
// ============================================================================
class NumericTableItem : public QTableWidgetItem {
public:
    using QTableWidgetItem::QTableWidgetItem;
    
    bool operator<(const QTableWidgetItem& other) const override {
        // Try numeric comparison first
        bool ok1, ok2;
        int v1 = text().replace(",", "").toInt(&ok1);
        int v2 = other.text().replace(",", "").toInt(&ok2);
        if (ok1 && ok2) return v1 < v2;
        if (ok1) return false;  // numbers sort after "-"
        if (ok2) return true;
        return text() < other.text();
    }
};

// ============================================================================
// Impl
// ============================================================================
class AddonManagerWindow::Impl {
public:
    QString gameId;
    std::unique_ptr<AddonManager> addonManager;
    
    // Main tab widget for addon types (Plugins, Skins, Music)
    QTabWidget* typeTabWidget = nullptr;
    
    // Source tab bars for each type (Installed / Find More)
    QTabBar* pluginsSourceBar = nullptr;
    QTabBar* skinsSourceBar = nullptr;
    QTabBar* musicSourceBar = nullptr;
    
    // Stacked widgets for switching between installed/remote (grid view)
    QStackedWidget* pluginsStack = nullptr;
    QStackedWidget* skinsStack = nullptr;
    QStackedWidget* musicStack = nullptr;
    
    // Stacked widgets for switching between installed/remote (list view)
    QStackedWidget* pluginsListStack = nullptr;
    QStackedWidget* skinsListStack = nullptr;
    QStackedWidget* musicListStack = nullptr;
    
    // Grid views for installed addons (using QListWidget in icon mode)
    QListWidget* pluginsInstalledGrid = nullptr;
    QListWidget* skinsInstalledGrid = nullptr;
    QListWidget* musicInstalledGrid = nullptr;
    
    // Grid views for remote addons
    QListWidget* pluginsRemoteGrid = nullptr;
    QListWidget* skinsRemoteGrid = nullptr;
    QListWidget* musicRemoteGrid = nullptr;
    
    // Table views for list mode
    QTableWidget* pluginsInstalledTable = nullptr;
    QTableWidget* skinsInstalledTable = nullptr;
    QTableWidget* musicInstalledTable = nullptr;
    QTableWidget* pluginsRemoteTable = nullptr;
    QTableWidget* skinsRemoteTable = nullptr;
    QTableWidget* musicRemoteTable = nullptr;
    
    // View mode stacks for each type (grid vs list)
    QStackedWidget* pluginsViewStack = nullptr;
    QStackedWidget* skinsViewStack = nullptr;
    QStackedWidget* musicViewStack = nullptr;
    
    QLineEdit* searchEdit = nullptr;
    QProgressBar* progressBar = nullptr;
    QLabel* statusLabel = nullptr;
    QComboBox* sortCombo = nullptr;
    
    QPushButton* installBtn = nullptr;
    QPushButton* updateBtn = nullptr;
    QPushButton* removeBtn = nullptr;
    QPushButton* refreshBtn = nullptr;
    QPushButton* browseBtn = nullptr;
    QPushButton* viewToggleBtn = nullptr;
    
    // Track current view
    bool showingRemote = false;
    bool isGridView = true;  // Default to grid view
    AddonType currentType = AddonType::Plugin;
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

AddonManagerWindow::AddonManagerWindow(const QString& gameId, QWidget* parent)
    : QDialog(parent)
    , m_impl(std::make_unique<Impl>())
{
    m_impl->gameId = gameId;
    
    // Get game settings path and create addon manager
    auto& configManager = ConfigManager::instance();
    auto gameConfig = configManager.getGameConfig(gameId.toStdString());
    
    if (gameConfig) {
        m_impl->addonManager = std::make_unique<AddonManager>(gameConfig->settingsDirectory);
    }
    
    setWindowTitle("Addon Manager");
    setMinimumSize(1050, 650);
    
    setupUi();
    setupConnections();
    
    // Load initial data
    refresh();
}

AddonManagerWindow::~AddonManagerWindow() = default;

// ============================================================================
// UI Setup
// ============================================================================

void AddonManagerWindow::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    
    // Window-level dark styling
    setStyleSheet(R"(
        AddonManagerWindow {
            background-color: #0d0d15;
            color: #e0e0e0;
        }
        QLabel {
            color: #e0e0e0;
        }
        QPushButton {
            background-color: #1a1a2e;
            color: #e0e0e0;
            border: 1px solid #3a3a5c;
            border-radius: 4px;
            padding: 6px 14px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #252542;
            border-color: #c9a227;
        }
        QPushButton:pressed {
            background-color: #2a9d8f;
        }
        QPushButton:disabled {
            background-color: #111122;
            color: #555;
            border-color: #222;
        }
        QLineEdit {
            background-color: #1a1a2e;
            color: #e0e0e0;
            border: 1px solid #3a3a5c;
            border-radius: 4px;
            padding: 6px 10px;
        }
        QLineEdit:focus {
            border-color: #c9a227;
        }
        QComboBox {
            background-color: #1a1a2e;
            color: #e0e0e0;
            border: 1px solid #3a3a5c;
            border-radius: 4px;
            padding: 5px 10px;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox QAbstractItemView {
            background-color: #1a1a2e;
            color: #e0e0e0;
            selection-background-color: #2a9d8f;
            border: 1px solid #3a3a5c;
        }
        QTabWidget::pane {
            border: 1px solid #3a3a5c;
            background-color: #0d0d15;
        }
        QTabBar::tab {
            background-color: #1a1a2e;
            color: #999;
            padding: 8px 20px;
            border: 1px solid #3a3a5c;
            border-bottom: none;
            margin-right: 2px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }
        QTabBar::tab:selected {
            background-color: #0d0d15;
            color: #c9a227;
            border-bottom: 2px solid #c9a227;
        }
        QTabBar::tab:hover:!selected {
            background-color: #252542;
            color: #e0e0e0;
        }
        QProgressBar {
            background-color: #1a1a2e;
            border: 1px solid #3a3a5c;
            border-radius: 4px;
            text-align: center;
            color: #e0e0e0;
            height: 18px;
        }
        QProgressBar::chunk {
            background-color: #2a9d8f;
            border-radius: 3px;
        }
    )");
    
    // ====== Toolbar Row ======
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(8);
    
    m_impl->searchEdit = new QLineEdit();
    m_impl->searchEdit->setPlaceholderText("🔍 Search addons...");
    m_impl->searchEdit->setClearButtonEnabled(true);
    m_impl->searchEdit->setMinimumWidth(200);
    toolbarLayout->addWidget(m_impl->searchEdit, 1);
    
    // Sort combo for grid view
    QLabel* sortLabel = new QLabel("Sort:");
    sortLabel->setStyleSheet("color: #999; font-size: 12px;");
    toolbarLayout->addWidget(sortLabel);
    
    m_impl->sortCombo = new QComboBox();
    m_impl->sortCombo->addItem("Name (A–Z)", 0);
    m_impl->sortCombo->addItem("Name (Z–A)", 1);
    m_impl->sortCombo->addItem("Author", 2);
    m_impl->sortCombo->addItem("Downloads ↓", 3);
    m_impl->sortCombo->addItem("Date ↓", 4);
    m_impl->sortCombo->setMinimumWidth(130);
    toolbarLayout->addWidget(m_impl->sortCombo);
    
    m_impl->viewToggleBtn = new QPushButton("📋 List View");
    m_impl->viewToggleBtn->setCheckable(true);
    toolbarLayout->addWidget(m_impl->viewToggleBtn);
    
    m_impl->refreshBtn = new QPushButton("🔄 Refresh");
    toolbarLayout->addWidget(m_impl->refreshBtn);
    
    mainLayout->addLayout(toolbarLayout);
    
    // ====== Main Tab Widget ======
    m_impl->typeTabWidget = new QTabWidget();
    m_impl->typeTabWidget->addTab(createAddonTypeTab(AddonType::Plugin), "⚙ Plugins");
    m_impl->typeTabWidget->addTab(createAddonTypeTab(AddonType::Skin), "🎨 Skins");
    m_impl->typeTabWidget->addTab(createAddonTypeTab(AddonType::Music), "🎵 Music");
    mainLayout->addWidget(m_impl->typeTabWidget, 1);
    
    // ====== Progress Bar (hidden) ======
    m_impl->progressBar = new QProgressBar();
    m_impl->progressBar->setVisible(false);
    mainLayout->addWidget(m_impl->progressBar);
    
    // ====== Bottom Bar: status + actions ======
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(8);
    
    m_impl->statusLabel = new QLabel();
    m_impl->statusLabel->setStyleSheet("color: #888; font-size: 12px;");
    bottomLayout->addWidget(m_impl->statusLabel, 1);
    
    m_impl->installBtn = new QPushButton("📥 Install from File...");
    m_impl->browseBtn = new QPushButton("🌐 Open on LotroInterface");
    m_impl->updateBtn = new QPushButton("⬆ Update");
    m_impl->removeBtn = new QPushButton("🗑 Remove");
    
    m_impl->removeBtn->setStyleSheet(
        "QPushButton { border-color: #5a2020; }"
        "QPushButton:hover { background-color: #4a1515; border-color: #992222; color: #ff6666; }"
    );
    
    bottomLayout->addWidget(m_impl->installBtn);
    bottomLayout->addWidget(m_impl->browseBtn);
    bottomLayout->addWidget(m_impl->updateBtn);
    bottomLayout->addWidget(m_impl->removeBtn);
    
    QPushButton* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(closeBtn);
    
    mainLayout->addLayout(bottomLayout);
}

QWidget* AddonManagerWindow::createAddonTypeTab(AddonType type) {
    QWidget* widget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(0);
    
    // Source tab bar (Installed / Find More)
    QTabBar* sourceBar = new QTabBar();
    sourceBar->addTab("📦 Installed");
    sourceBar->addTab("🔍 Find More");
    sourceBar->setStyleSheet(R"(
        QTabBar::tab {
            padding: 6px 16px;
            margin-right: 4px;
            border: 1px solid #3a3a5c;
            border-bottom: none;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            background-color: #151525;
            color: #999;
        }
        QTabBar::tab:selected {
            background-color: #0d0d15;
            color: #2a9d8f;
            border-bottom: 2px solid #2a9d8f;
        }
        QTabBar::tab:hover:!selected {
            color: #e0e0e0;
            background-color: #1a1a30;
        }
    )");
    layout->addWidget(sourceBar);
    
    // View stack for grid vs list mode
    QStackedWidget* viewStack = new QStackedWidget();
    
    // === GRID VIEW (index 0) ===
    QStackedWidget* gridSourceStack = new QStackedWidget();
    QListWidget* installedGrid = createAddonGrid();
    QListWidget* remoteGrid = createAddonGrid();
    gridSourceStack->addWidget(installedGrid);
    gridSourceStack->addWidget(remoteGrid);
    viewStack->addWidget(gridSourceStack);
    
    // === LIST VIEW (index 1) ===
    QStackedWidget* listSourceStack = new QStackedWidget();
    QTableWidget* installedTable = createAddonTable();
    QTableWidget* remoteTable = createAddonTable();
    listSourceStack->addWidget(installedTable);
    listSourceStack->addWidget(remoteTable);
    viewStack->addWidget(listSourceStack);
    
    layout->addWidget(viewStack, 1);
    
    // Store references
    switch (type) {
        case AddonType::Plugin:
            m_impl->pluginsSourceBar = sourceBar;
            m_impl->pluginsStack = gridSourceStack;
            m_impl->pluginsListStack = listSourceStack;
            m_impl->pluginsViewStack = viewStack;
            m_impl->pluginsInstalledGrid = installedGrid;
            m_impl->pluginsRemoteGrid = remoteGrid;
            m_impl->pluginsInstalledTable = installedTable;
            m_impl->pluginsRemoteTable = remoteTable;
            break;
        case AddonType::Skin:
            m_impl->skinsSourceBar = sourceBar;
            m_impl->skinsStack = gridSourceStack;
            m_impl->skinsListStack = listSourceStack;
            m_impl->skinsViewStack = viewStack;
            m_impl->skinsInstalledGrid = installedGrid;
            m_impl->skinsRemoteGrid = remoteGrid;
            m_impl->skinsInstalledTable = installedTable;
            m_impl->skinsRemoteTable = remoteTable;
            break;
        case AddonType::Music:
            m_impl->musicSourceBar = sourceBar;
            m_impl->musicStack = gridSourceStack;
            m_impl->musicListStack = listSourceStack;
            m_impl->musicViewStack = viewStack;
            m_impl->musicInstalledGrid = installedGrid;
            m_impl->musicRemoteGrid = remoteGrid;
            m_impl->musicInstalledTable = installedTable;
            m_impl->musicRemoteTable = remoteTable;
            break;
    }
    
    // Connect source bar to switch both grid and list source stacks
    connect(sourceBar, &QTabBar::currentChanged, [this, gridSourceStack, listSourceStack, type](int index) {
        gridSourceStack->setCurrentIndex(index);
        listSourceStack->setCurrentIndex(index);
        m_impl->showingRemote = (index == 1);
        if (index == 1) {
            loadRemoteAddons(type);
        }
        updateButtonStates();
    });
    
    return widget;
}

QTableWidget* AddonManagerWindow::createAddonTable() {
    QTableWidget* table = new QTableWidget();
    table->setColumnCount(9);
    table->setHorizontalHeaderLabels({
        "Name", "Author", "Version", "Category", 
        "Released", "Downloads", "Status", "Size", "ID"
    });
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);   // Name
    table->horizontalHeader()->setMinimumSectionSize(80);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents); // Author
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents); // Version
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents); // Category
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);      // Released
    table->horizontalHeader()->resizeSection(4, 90);
    table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents); // Downloads
    table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Interactive);      // Status
    table->horizontalHeader()->resizeSection(6, 85);
    table->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Interactive);      // Size
    table->horizontalHeader()->resizeSection(7, 65);
    table->setColumnHidden(8, true);  // Hide ID column
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSortingEnabled(true);
    table->verticalHeader()->setVisible(false);
    table->setShowGrid(false);
    table->setStyleSheet(R"(
        QTableWidget {
            background-color: #0d0d15;
            border: 1px solid #3a3a5c;
            border-radius: 4px;
            color: #e0e0e0;
            gridline-color: transparent;
        }
        QTableWidget::item {
            padding: 8px 6px;
            color: #e0e0e0;
            border-bottom: 1px solid #1a1a2e;
        }
        QTableWidget::item:hover {
            background-color: #1e1e38;
        }
        QTableWidget::item:selected {
            background-color: #1a6d63;
            color: #ffffff;
        }
        QTableWidget::item:alternate {
            background-color: #111120;
        }
        QTableWidget::item:alternate:selected {
            background-color: #1a6d63;
            color: #ffffff;
        }
        QTableWidget::item:alternate:hover {
            background-color: #1e1e38;
        }
        QTableWidget::item:alternate:selected:hover {
            background-color: #1f7d72;
            color: #ffffff;
        }
        QHeaderView::section {
            background-color: #1a1a2e;
            color: #c9a227;
            padding: 8px 6px;
            border: none;
            border-right: 1px solid #252542;
            border-bottom: 2px solid #c9a227;
            font-weight: bold;
            font-size: 12px;
        }
        QHeaderView::section:hover {
            background-color: #252542;
        }
    )");
    
    return table;
}

QListWidget* AddonManagerWindow::createAddonGrid() {
    QListWidget* grid = new QListWidget();
    grid->setViewMode(QListView::IconMode);
    grid->setIconSize(QSize(0, 0));  // No icon — we use rich text
    grid->setGridSize(QSize(260, 120));
    grid->setSpacing(8);
    grid->setResizeMode(QListView::Adjust);
    grid->setWrapping(true);
    grid->setMovement(QListView::Static);
    grid->setSelectionMode(QAbstractItemView::SingleSelection);
    grid->setWordWrap(true);
    grid->setStyleSheet(R"(
        QListWidget {
            background-color: #0d0d15;
            border: none;
            outline: none;
        }
        QListWidget::item {
            background-color: #151528;
            border: 2px solid #252545;
            border-radius: 8px;
            color: #e0e0e0;
            padding: 10px;
            margin: 2px;
        }
        QListWidget::item:hover {
            background-color: #1e1e38;
            border-color: #454570;
        }
        QListWidget::item:selected {
            background-color: #152e2b;
            border-color: #2a9d8f;
            border-width: 2px;
        }
        QListWidget::item:selected:hover {
            background-color: #1a3835;
            border-color: #34c4b3;
        }
    )");
    
    return grid;
}

// ============================================================================
// Connections
// ============================================================================

void AddonManagerWindow::setupConnections() {
    connect(m_impl->searchEdit, &QLineEdit::textChanged, [this](const QString& text) {
        search(text);
    });
    
    connect(m_impl->refreshBtn, &QPushButton::clicked, this, &AddonManagerWindow::refresh);
    connect(m_impl->installBtn, &QPushButton::clicked, this, &AddonManagerWindow::installFromFile);
    connect(m_impl->browseBtn, &QPushButton::clicked, this, &AddonManagerWindow::openAddonPage);
    connect(m_impl->updateBtn, &QPushButton::clicked, this, &AddonManagerWindow::updateSelected);
    connect(m_impl->removeBtn, &QPushButton::clicked, this, &AddonManagerWindow::removeSelected);
    
    connect(m_impl->typeTabWidget, &QTabWidget::currentChanged, [this](int index) {
        switch (index) {
            case 0: m_impl->currentType = AddonType::Plugin; showPlugins(); break;
            case 1: m_impl->currentType = AddonType::Skin; showSkins(); break;
            case 2: m_impl->currentType = AddonType::Music; showMusic(); break;
        }
        updateButtonStates();
    });
    
    // Double-click to install (remote) or open page (installed)
    auto connectGridDoubleClick = [this](QListWidget* grid, bool isRemote) {
        if (grid) {
            connect(grid, &QListWidget::itemDoubleClicked, [this, isRemote](QListWidgetItem*) {
                if (isRemote) {
                    installSelected();
                } else {
                    openAddonPage();
                }
            });
        }
    };
    
    connectGridDoubleClick(m_impl->pluginsInstalledGrid, false);
    connectGridDoubleClick(m_impl->pluginsRemoteGrid, true);
    connectGridDoubleClick(m_impl->skinsInstalledGrid, false);
    connectGridDoubleClick(m_impl->skinsRemoteGrid, true);
    connectGridDoubleClick(m_impl->musicInstalledGrid, false);
    connectGridDoubleClick(m_impl->musicRemoteGrid, true);
    
    // Double-click on table rows
    auto connectTableDoubleClick = [this](QTableWidget* table, bool isRemote) {
        if (table) {
            connect(table, &QTableWidget::itemDoubleClicked, [this, isRemote](QTableWidgetItem*) {
                if (isRemote) {
                    installSelected();
                } else {
                    openAddonPage();
                }
            });
        }
    };
    
    connectTableDoubleClick(m_impl->pluginsInstalledTable, false);
    connectTableDoubleClick(m_impl->pluginsRemoteTable, true);
    connectTableDoubleClick(m_impl->skinsInstalledTable, false);
    connectTableDoubleClick(m_impl->skinsRemoteTable, true);
    connectTableDoubleClick(m_impl->musicInstalledTable, false);
    connectTableDoubleClick(m_impl->musicRemoteTable, true);
    
    // Selection change on grids and tables => update button states
    auto connectSelection = [this](QListWidget* grid) {
        if (grid) {
            connect(grid, &QListWidget::itemSelectionChanged, [this]() {
                updateButtonStates();
            });
        }
    };
    connectSelection(m_impl->pluginsInstalledGrid);
    connectSelection(m_impl->pluginsRemoteGrid);
    connectSelection(m_impl->skinsInstalledGrid);
    connectSelection(m_impl->skinsRemoteGrid);
    connectSelection(m_impl->musicInstalledGrid);
    connectSelection(m_impl->musicRemoteGrid);
    
    auto connectTableSelection = [this](QTableWidget* table) {
        if (table) {
            connect(table, &QTableWidget::itemSelectionChanged, [this]() {
                updateButtonStates();
            });
        }
    };
    connectTableSelection(m_impl->pluginsInstalledTable);
    connectTableSelection(m_impl->pluginsRemoteTable);
    connectTableSelection(m_impl->skinsInstalledTable);
    connectTableSelection(m_impl->skinsRemoteTable);
    connectTableSelection(m_impl->musicInstalledTable);
    connectTableSelection(m_impl->musicRemoteTable);
    
    // View toggle button
    connect(m_impl->viewToggleBtn, &QPushButton::toggled, [this](bool listMode) {
        m_impl->isGridView = !listMode;
        m_impl->viewToggleBtn->setText(listMode ? "⊞ Grid View" : "📋 List View");
        
        // Switch view stacks for all types: 0 = grid stack, 1 = list stack
        int viewIndex = listMode ? 1 : 0;
        if (m_impl->pluginsViewStack) m_impl->pluginsViewStack->setCurrentIndex(viewIndex);
        if (m_impl->skinsViewStack) m_impl->skinsViewStack->setCurrentIndex(viewIndex);
        if (m_impl->musicViewStack) m_impl->musicViewStack->setCurrentIndex(viewIndex);
        
        updateButtonStates();
    });
    
    // Sort combo
    connect(m_impl->sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        QListWidget* grid = getCurrentGrid();
        if (grid) {
            sortGrid(grid, index);
        }
        
        // Also sort table view
        QTableWidget* table = getCurrentTable();
        if (table) {
            switch (index) {
                case 0: table->sortByColumn(0, Qt::AscendingOrder); break;  // Name A-Z
                case 1: table->sortByColumn(0, Qt::DescendingOrder); break; // Name Z-A
                case 2: table->sortByColumn(1, Qt::AscendingOrder); break;  // Author
                case 3: table->sortByColumn(5, Qt::DescendingOrder); break; // Downloads
                case 4: table->sortByColumn(4, Qt::DescendingOrder); break; // Date
            }
        }
    });
}

// ============================================================================
// Selection Helpers — work for both grid and table views
// ============================================================================

QListWidget* AddonManagerWindow::getCurrentGrid() const {
    if (m_impl->showingRemote) {
        switch (m_impl->currentType) {
            case AddonType::Plugin: return m_impl->pluginsRemoteGrid;
            case AddonType::Skin:   return m_impl->skinsRemoteGrid;
            case AddonType::Music:  return m_impl->musicRemoteGrid;
        }
    } else {
        switch (m_impl->currentType) {
            case AddonType::Plugin: return m_impl->pluginsInstalledGrid;
            case AddonType::Skin:   return m_impl->skinsInstalledGrid;
            case AddonType::Music:  return m_impl->musicInstalledGrid;
        }
    }
    return nullptr;
}

QTableWidget* AddonManagerWindow::getCurrentTable() const {
    if (m_impl->showingRemote) {
        switch (m_impl->currentType) {
            case AddonType::Plugin: return m_impl->pluginsRemoteTable;
            case AddonType::Skin:   return m_impl->skinsRemoteTable;
            case AddonType::Music:  return m_impl->musicRemoteTable;
        }
    } else {
        switch (m_impl->currentType) {
            case AddonType::Plugin: return m_impl->pluginsInstalledTable;
            case AddonType::Skin:   return m_impl->skinsInstalledTable;
            case AddonType::Music:  return m_impl->musicInstalledTable;
        }
    }
    return nullptr;
}

QString AddonManagerWindow::getSelectedAddonId() const {
    if (m_impl->isGridView) {
        QListWidget* grid = getCurrentGrid();
        if (grid) {
            auto items = grid->selectedItems();
            if (!items.isEmpty()) {
                return items.first()->data(Qt::UserRole).toString();
            }
        }
    } else {
        QTableWidget* table = getCurrentTable();
        if (table) {
            int row = table->currentRow();
            if (row >= 0) {
                QTableWidgetItem* idItem = table->item(row, 8); // ID column
                if (idItem) return idItem->text();
            }
        }
    }
    return {};
}

QString AddonManagerWindow::getSelectedAddonName() const {
    if (m_impl->isGridView) {
        QListWidget* grid = getCurrentGrid();
        if (grid) {
            auto items = grid->selectedItems();
            if (!items.isEmpty()) {
                return items.first()->data(Qt::UserRole + 1).toString();
            }
        }
    } else {
        QTableWidget* table = getCurrentTable();
        if (table) {
            int row = table->currentRow();
            if (row >= 0) {
                QTableWidgetItem* nameItem = table->item(row, 0); // Name column
                if (nameItem) return nameItem->text();
            }
        }
    }
    return {};
}

// ============================================================================
// Sorting
// ============================================================================

void AddonManagerWindow::sortGrid(QListWidget* grid, int sortIndex) {
    if (!grid || grid->count() == 0) return;
    
    // Extract all items into a sortable list
    struct SortableItem {
        QString name;
        QString author;
        int downloads;
        QString date;
        // Store all UserRole data to reconstruct
        QMap<int, QVariant> data;
        QString text;
        QSize sizeHint;
        Qt::Alignment alignment;
    };
    
    std::vector<SortableItem> items;
    for (int i = 0; i < grid->count(); ++i) {
        QListWidgetItem* item = grid->item(i);
        SortableItem si;
        si.name = item->data(Qt::UserRole + 1).toString();
        si.author = item->data(Qt::UserRole + 2).toString();
        si.downloads = item->data(Qt::UserRole + 5).toInt();
        si.date = item->data(Qt::UserRole + 6).toString();
        si.text = item->text();
        si.sizeHint = item->sizeHint();
        si.alignment = Qt::Alignment(item->textAlignment());
        // Copy all user data roles
        for (int r = Qt::UserRole; r <= Qt::UserRole + 10; ++r) {
            QVariant v = item->data(r);
            if (v.isValid()) si.data[r] = v;
        }
        items.push_back(si);
    }
    
    // Sort
    switch (sortIndex) {
        case 0: // Name A-Z
            std::sort(items.begin(), items.end(), [](const SortableItem& a, const SortableItem& b) {
                return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
            });
            break;
        case 1: // Name Z-A
            std::sort(items.begin(), items.end(), [](const SortableItem& a, const SortableItem& b) {
                return a.name.compare(b.name, Qt::CaseInsensitive) > 0;
            });
            break;
        case 2: // Author
            std::sort(items.begin(), items.end(), [](const SortableItem& a, const SortableItem& b) {
                return a.author.compare(b.author, Qt::CaseInsensitive) < 0;
            });
            break;
        case 3: // Downloads descending
            std::sort(items.begin(), items.end(), [](const SortableItem& a, const SortableItem& b) {
                return a.downloads > b.downloads;
            });
            break;
        case 4: // Date descending
            std::sort(items.begin(), items.end(), [](const SortableItem& a, const SortableItem& b) {
                return a.date > b.date;
            });
            break;
    }
    
    // Rebuild the grid
    grid->clear();
    for (const auto& si : items) {
        QListWidgetItem* item = new QListWidgetItem(si.text);
        for (auto it = si.data.begin(); it != si.data.end(); ++it) {
            item->setData(it.key(), it.value());
        }
        item->setSizeHint(si.sizeHint);
        item->setTextAlignment(si.alignment);
        grid->addItem(item);
    }
}

// ============================================================================
// Button States
// ============================================================================

void AddonManagerWindow::updateButtonStates() {
    bool showingRemote = m_impl->showingRemote;
    bool hasSelection = !getSelectedAddonId().isEmpty();
    
    // Install from file only for installed view
    m_impl->installBtn->setVisible(!showingRemote);
    
    // Remove only for installed view
    m_impl->removeBtn->setVisible(!showingRemote);
    m_impl->removeBtn->setEnabled(hasSelection);
    
    // Browse always available if something is selected
    m_impl->browseBtn->setEnabled(hasSelection);
    
    // Update text based on view
    if (showingRemote) {
        m_impl->updateBtn->setText("📥 Install Selected");
        m_impl->updateBtn->setVisible(true);
        m_impl->updateBtn->setEnabled(hasSelection);
        disconnect(m_impl->updateBtn, &QPushButton::clicked, this, &AddonManagerWindow::updateSelected);
        connect(m_impl->updateBtn, &QPushButton::clicked, this, &AddonManagerWindow::installSelected);
    } else {
        m_impl->updateBtn->setText("⬆ Update");
        m_impl->updateBtn->setEnabled(hasSelection);
        disconnect(m_impl->updateBtn, &QPushButton::clicked, this, &AddonManagerWindow::installSelected);
        connect(m_impl->updateBtn, &QPushButton::clicked, this, &AddonManagerWindow::updateSelected);
    }
}

// ============================================================================
// Tab Switching
// ============================================================================

void AddonManagerWindow::showPlugins() {
    loadInstalledAddons(AddonType::Plugin);
}

void AddonManagerWindow::showSkins() {
    loadInstalledAddons(AddonType::Skin);
}

void AddonManagerWindow::showMusic() {
    loadInstalledAddons(AddonType::Music);
}

void AddonManagerWindow::refresh() {
    if (m_impl->addonManager) {
        m_impl->addonManager->refreshInstalledAddons();
    }
    
    int currentTab = m_impl->typeTabWidget->currentIndex();
    switch (currentTab) {
        case 0: showPlugins(); break;
        case 1: showSkins(); break;
        case 2: showMusic(); break;
    }
    
    m_impl->statusLabel->setText("Addons refreshed");
}

// ============================================================================
// Helper: format a grid card's text
// ============================================================================
namespace {

QString formatGridCardText(const QString& name, const QString& author, const QString& version,
                           const QString& category, int downloads, const QString& status) {
    // Build a multi-line card with visual hierarchy
    QString text = name;
    
    if (!author.isEmpty()) {
        text += "\n  by " + author;
    }
    
    QStringList details;
    if (!version.isEmpty()) details << "v" + version;
    if (!category.isEmpty()) details << category;
    if (!details.isEmpty()) {
        text += "\n" + details.join("  •  ");
    }
    
    QStringList meta;
    if (downloads > 0) {
        // Format with K/M suffixes
        if (downloads >= 1000000)
            meta << QString("%1M ↓").arg(downloads / 1000000.0, 0, 'f', 1);
        else if (downloads >= 1000)
            meta << QString("%1K ↓").arg(downloads / 1000.0, 0, 'f', 1);
        else
            meta << QString("%1 ↓").arg(downloads);
    }
    if (!status.isEmpty()) meta << status;
    
    if (!meta.isEmpty()) {
        text += "\n" + meta.join("  •  ");
    }
    
    return text;
}

} // anonymous namespace

// ============================================================================
// Load Addons
// ============================================================================

void AddonManagerWindow::loadInstalledAddons(AddonType type) {
    if (!m_impl->addonManager) {
        return;
    }
    
    QListWidget* grid = nullptr;
    QTableWidget* table = nullptr;
    switch (type) {
        case AddonType::Plugin: 
            grid = m_impl->pluginsInstalledGrid; 
            table = m_impl->pluginsInstalledTable;
            break;
        case AddonType::Skin:   
            grid = m_impl->skinsInstalledGrid; 
            table = m_impl->skinsInstalledTable;
            break;
        case AddonType::Music:  
            grid = m_impl->musicInstalledGrid; 
            table = m_impl->musicInstalledTable;
            break;
    }
    
    auto addons = m_impl->addonManager->getInstalledAddons(type);
    
    // Populate grid view
    if (grid) {
        grid->clear();
        for (const auto& addon : addons) {
            QString statusText;
            if (addon.hasUpdate()) statusText = "⬆ Update Available";
            
            QString itemText = formatGridCardText(
                addon.name, addon.author, addon.installedVersion,
                addon.category, addon.downloadCount, statusText
            );
            
            QListWidgetItem* item = new QListWidgetItem(itemText);
            item->setData(Qt::UserRole, addon.id);
            item->setData(Qt::UserRole + 1, addon.name);
            item->setData(Qt::UserRole + 2, addon.author);
            item->setData(Qt::UserRole + 3, addon.installedVersion);
            item->setData(Qt::UserRole + 4, addon.category);
            item->setData(Qt::UserRole + 5, addon.downloadCount);
            item->setData(Qt::UserRole + 6, addon.releaseDate);
            
            item->setSizeHint(QSize(250, 110));
            
            grid->addItem(item);
        }
        
        // Apply current sort
        sortGrid(grid, m_impl->sortCombo->currentIndex());
    }
    
    // Populate table view
    if (table) {
        table->setSortingEnabled(false);
        table->setRowCount(static_cast<int>(addons.size()));
        
        for (int row = 0; row < static_cast<int>(addons.size()); ++row) {
            const auto& addon = addons[row];
            
            table->setItem(row, 0, new QTableWidgetItem(addon.name));
            table->setItem(row, 1, new QTableWidgetItem(addon.author));
            table->setItem(row, 2, new QTableWidgetItem(addon.installedVersion));
            table->setItem(row, 3, new QTableWidgetItem(addon.category));
            table->setItem(row, 4, new QTableWidgetItem(addon.releaseDate));
            
            // Use NumericTableItem for downloads so sorting works correctly
            auto* dlItem = new NumericTableItem(
                addon.downloadCount > 0 ? QString::number(addon.downloadCount) : "-"
            );
            table->setItem(row, 5, dlItem);
            
            QString statusText = addon.hasUpdate() ? "Update Available" : "Installed";
            table->setItem(row, 6, new QTableWidgetItem(statusText));
            table->setItem(row, 7, new QTableWidgetItem(addon.fileSize));
            table->setItem(row, 8, new QTableWidgetItem(addon.id));
        }
        
        table->setSortingEnabled(true);
    }
    
    m_impl->statusLabel->setText(QString("Found %1 installed %2")
        .arg(addons.size())
        .arg(addonTypeToString(type).toLower() + "s"));
}

void AddonManagerWindow::loadRemoteAddons(AddonType type) {
    if (!m_impl->addonManager) {
        return;
    }
    
    QListWidget* grid = nullptr;
    QTableWidget* table = nullptr;
    switch (type) {
        case AddonType::Plugin: 
            grid = m_impl->pluginsRemoteGrid; 
            table = m_impl->pluginsRemoteTable;
            break;
        case AddonType::Skin:   
            grid = m_impl->skinsRemoteGrid; 
            table = m_impl->skinsRemoteTable;
            break;
        case AddonType::Music:  
            grid = m_impl->musicRemoteGrid; 
            table = m_impl->musicRemoteTable;
            break;
    }
    
    m_impl->statusLabel->setText("Fetching addons from lotrointerface.com...");
    m_impl->progressBar->setVisible(true);
    m_impl->progressBar->setRange(0, 0);  // Indeterminate
    
    // Fetch asynchronously
    auto future = m_impl->addonManager->fetchRemoteAddonList(type);
    
    // Poll for completion
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, [this, future, grid, table, type, timer]() mutable {
        if (future.isFinished()) {
            timer->stop();
            timer->deleteLater();
            
            auto addons = future.result();
            
            m_impl->progressBar->setVisible(false);
            
            // Populate grid view
            if (grid) {
                grid->clear();
                for (const auto& addon : addons) {
                    auto installed = m_impl->addonManager->getInstalledAddon(addon.id, type);
                    QString statusText = installed ? "✓ Installed" : "";
                    
                    QString itemText = formatGridCardText(
                        addon.name, addon.author, addon.version,
                        addon.category, addon.downloadCount, statusText
                    );
                    
                    QListWidgetItem* item = new QListWidgetItem(itemText);
                    item->setData(Qt::UserRole, addon.id);
                    item->setData(Qt::UserRole + 1, addon.name);
                    item->setData(Qt::UserRole + 2, addon.author);
                    item->setData(Qt::UserRole + 3, addon.version);
                    item->setData(Qt::UserRole + 4, addon.category);
                    item->setData(Qt::UserRole + 5, addon.downloadCount);
                    item->setData(Qt::UserRole + 6, addon.releaseDate);
                    
                    item->setSizeHint(QSize(250, 110));
                    
                    grid->addItem(item);
                }
                
                // Apply current sort
                sortGrid(grid, m_impl->sortCombo->currentIndex());
            }
            
            // Populate table view
            if (table) {
                table->setSortingEnabled(false);
                table->setRowCount(static_cast<int>(addons.size()));
                
                for (int row = 0; row < static_cast<int>(addons.size()); ++row) {
                    const auto& addon = addons[row];
                    
                    table->setItem(row, 0, new QTableWidgetItem(addon.name));
                    table->setItem(row, 1, new QTableWidgetItem(addon.author));
                    table->setItem(row, 2, new QTableWidgetItem(addon.version));
                    table->setItem(row, 3, new QTableWidgetItem(addon.category));
                    table->setItem(row, 4, new QTableWidgetItem(addon.releaseDate));
                    
                    // Numeric sort for downloads
                    auto* dlItem = new NumericTableItem(
                        addon.downloadCount > 0 ? QString::number(addon.downloadCount) : "-"
                    );
                    table->setItem(row, 5, dlItem);
                    
                    auto installed = m_impl->addonManager->getInstalledAddon(addon.id, type);
                    QString statusText = installed ? "Installed" : "Available";
                    table->setItem(row, 6, new QTableWidgetItem(statusText));
                    table->setItem(row, 7, new QTableWidgetItem(addon.fileSize));
                    table->setItem(row, 8, new QTableWidgetItem(addon.id));
                }
                
                table->setSortingEnabled(true);
            }
            
            m_impl->statusLabel->setText(QString("Found %1 available %2")
                .arg(addons.size())
                .arg(addonTypeToString(type).toLower() + "s"));
        }
    });
    timer->start(100);
}

// ============================================================================
// Actions
// ============================================================================

void AddonManagerWindow::installFromFile() {
    QString filter = "Addon Archives (*.zip);;All Files (*)";
    QString file = QFileDialog::getOpenFileName(this, "Select Addon File", "", filter);
    
    if (file.isEmpty()) {
        return;
    }
    
    AddonType type = m_impl->currentType;
    
    if (m_impl->addonManager) {
        m_impl->statusLabel->setText("Installing addon...");
        bool success = m_impl->addonManager->installFromFile(
            std::filesystem::path(file.toStdString()), type);
        
        if (success) {
            m_impl->statusLabel->setText("Addon installed successfully!");
            refresh();
        } else {
            m_impl->statusLabel->setText("Failed to install addon");
            QMessageBox::warning(this, "Error", "Failed to install addon.");
        }
    }
}

void AddonManagerWindow::installSelected() {
    QString addonId = getSelectedAddonId();
    QString addonName = getSelectedAddonName();
    AddonType type = m_impl->currentType;
    
    if (addonId.isEmpty()) {
        QMessageBox::information(this, "Install", "Please select an addon to install.");
        return;
    }
    
    m_impl->statusLabel->setText(QString("Installing %1...").arg(addonName));
    m_impl->progressBar->setVisible(true);
    m_impl->progressBar->setRange(0, 100);
    m_impl->progressBar->setValue(0);
    
    if (m_impl->addonManager) {
        auto future = m_impl->addonManager->installAddon(addonId, type,
            [this](int current, int total, const QString& status) {
                QMetaObject::invokeMethod(this, [this, current, total, status]() {
                    m_impl->progressBar->setValue(current);
                    m_impl->statusLabel->setText(status);
                }, Qt::QueuedConnection);
            });
        
        // Poll for completion
        QTimer* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, [this, future, addonName, timer]() mutable {
            if (future.isFinished()) {
                timer->stop();
                timer->deleteLater();
                
                m_impl->progressBar->setVisible(false);
                
                if (future.result()) {
                    m_impl->statusLabel->setText(QString("%1 installed successfully!").arg(addonName));
                    m_impl->addonManager->refreshInstalledAddons();
                    loadInstalledAddons(m_impl->currentType);
                    loadRemoteAddons(m_impl->currentType);
                } else {
                    m_impl->statusLabel->setText(QString("Failed to install %1").arg(addonName));
                    QMessageBox::warning(this, "Error", "Failed to install addon.");
                }
            }
        });
        timer->start(100);
    }
}

void AddonManagerWindow::updateSelected() {
    QString addonId = getSelectedAddonId();
    QString addonName = getSelectedAddonName();
    AddonType type = m_impl->currentType;
    
    if (addonId.isEmpty()) {
        QMessageBox::information(this, "Update", "Please select an addon to update.");
        return;
    }
    
    m_impl->statusLabel->setText(QString("Updating %1...").arg(addonName));
    m_impl->progressBar->setVisible(true);
    m_impl->progressBar->setRange(0, 0);
    
    if (m_impl->addonManager) {
        auto future = m_impl->addonManager->updateAddon(addonId, type, nullptr);
        
        QTimer* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, [this, future, addonName, timer]() mutable {
            if (future.isFinished()) {
                timer->stop();
                timer->deleteLater();
                
                m_impl->progressBar->setVisible(false);
                
                if (future.result()) {
                    m_impl->statusLabel->setText(QString("%1 updated successfully!").arg(addonName));
                    refresh();
                } else {
                    m_impl->statusLabel->setText(QString("Failed to update %1").arg(addonName));
                }
            }
        });
        timer->start(100);
    }
}

void AddonManagerWindow::updateAll() {
    AddonType type = m_impl->currentType;
    
    m_impl->statusLabel->setText("Updating all addons with available updates...");
    m_impl->progressBar->setVisible(true);
    m_impl->progressBar->setRange(0, 0);
    
    if (m_impl->addonManager) {
        auto future = m_impl->addonManager->updateAllAddons(type, nullptr);
        
        QTimer* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, [this, future, timer]() mutable {
            if (future.isFinished()) {
                timer->stop();
                timer->deleteLater();
                
                m_impl->progressBar->setVisible(false);
                int count = future.result();
                m_impl->statusLabel->setText(QString("Updated %1 addons").arg(count));
                refresh();
            }
        });
        timer->start(100);
    }
}

void AddonManagerWindow::removeSelected() {
    QString addonId = getSelectedAddonId();
    QString addonName = getSelectedAddonName();
    AddonType type = m_impl->currentType;
    
    if (addonId.isEmpty()) {
        QMessageBox::warning(this, "Remove", "Please select an addon to remove.");
        return;
    }
    
    auto result = QMessageBox::question(this, "Confirm Remove",
        QString("Are you sure you want to remove '%1'?").arg(addonName));
    
    if (result != QMessageBox::Yes) {
        return;
    }
    
    if (m_impl->addonManager) {
        bool success = m_impl->addonManager->removeAddon(addonId, type);
        
        if (success) {
            m_impl->statusLabel->setText(QString("%1 removed").arg(addonName));
            refresh();
        } else {
            m_impl->statusLabel->setText(QString("Failed to remove %1").arg(addonName));
            QMessageBox::warning(this, "Error", "Failed to remove addon.");
        }
    }
}

void AddonManagerWindow::openAddonPage() {
    QString addonId = getSelectedAddonId();
    
    if (addonId.isEmpty()) {
        QMessageBox::information(this, "Open Page", "Please select an addon.");
        return;
    }
    
    QString url = QString("https://www.lotrointerface.com/downloads/info%1").arg(addonId);
    QDesktopServices::openUrl(QUrl(url));
}

// ============================================================================
// Search — filters BOTH grid and table views
// ============================================================================

void AddonManagerWindow::search(const QString& query) {
    // Filter grid
    QListWidget* grid = getCurrentGrid();
    if (grid) {
        for (int i = 0; i < grid->count(); ++i) {
            QListWidgetItem* item = grid->item(i);
            bool match = query.isEmpty() || 
                item->text().contains(query, Qt::CaseInsensitive);
            item->setHidden(!match);
        }
    }
    
    // Filter table
    QTableWidget* table = getCurrentTable();
    if (table) {
        for (int row = 0; row < table->rowCount(); ++row) {
            bool match = false;
            if (query.isEmpty()) {
                match = true;
            } else {
                // Search across name, author, category columns
                for (int col : {0, 1, 3}) {
                    QTableWidgetItem* item = table->item(row, col);
                    if (item && item->text().contains(query, Qt::CaseInsensitive)) {
                        match = true;
                        break;
                    }
                }
            }
            table->setRowHidden(row, !match);
        }
    }
}

} // namespace lotro
