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

#include <spdlog/spdlog.h>

namespace lotro {

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
    
    // Stacked widgets for switching between installed/remote
    QStackedWidget* pluginsStack = nullptr;
    QStackedWidget* skinsStack = nullptr;
    QStackedWidget* musicStack = nullptr;
    
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
    
    // Details panel (shown in grid mode)
    QWidget* detailsPanel = nullptr;
    QLabel* detailsName = nullptr;
    QLabel* detailsAuthor = nullptr;
    QLabel* detailsVersion = nullptr;
    QLabel* detailsCategory = nullptr;
    QLabel* detailsDownloads = nullptr;
    QLabel* detailsReleased = nullptr;
    QLabel* detailsDescription = nullptr;
    
    QLineEdit* searchEdit = nullptr;
    QProgressBar* progressBar = nullptr;
    QLabel* statusLabel = nullptr;
    
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
    setMinimumSize(800, 600);
    
    setupUi();
    setupConnections();
    
    // Load initial data
    refresh();
}

AddonManagerWindow::~AddonManagerWindow() = default;

void AddonManagerWindow::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Toolbar
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    
    m_impl->searchEdit = new QLineEdit();
    m_impl->searchEdit->setPlaceholderText("Search addons...");
    m_impl->searchEdit->setClearButtonEnabled(true);
    toolbarLayout->addWidget(m_impl->searchEdit, 1);
    
    m_impl->refreshBtn = new QPushButton("Refresh");
    toolbarLayout->addWidget(m_impl->refreshBtn);
    
    m_impl->viewToggleBtn = new QPushButton("List View");
    m_impl->viewToggleBtn->setCheckable(true);
    toolbarLayout->addWidget(m_impl->viewToggleBtn);
    
    mainLayout->addLayout(toolbarLayout);
    
    // Main tab widget for addon types
    m_impl->typeTabWidget = new QTabWidget();
    
    // Create tabs for each addon type
    m_impl->typeTabWidget->addTab(createAddonTypeTab(AddonType::Plugin), "Plugins");
    m_impl->typeTabWidget->addTab(createAddonTypeTab(AddonType::Skin), "Skins");
    m_impl->typeTabWidget->addTab(createAddonTypeTab(AddonType::Music), "Music");
    
    mainLayout->addWidget(m_impl->typeTabWidget);
    
    // Progress bar (hidden by default)
    m_impl->progressBar = new QProgressBar();
    m_impl->progressBar->setVisible(false);
    mainLayout->addWidget(m_impl->progressBar);
    
    // Status label
    m_impl->statusLabel = new QLabel();
    m_impl->statusLabel->setStyleSheet("color: #666;");
    mainLayout->addWidget(m_impl->statusLabel);
    
    // Action buttons
    QHBoxLayout* actionsLayout = new QHBoxLayout();
    
    m_impl->installBtn = new QPushButton("Install from File...");
    m_impl->browseBtn = new QPushButton("Open on LotroInterface");
    m_impl->updateBtn = new QPushButton("Update");
    m_impl->removeBtn = new QPushButton("Remove");
    
    actionsLayout->addWidget(m_impl->installBtn);
    actionsLayout->addWidget(m_impl->browseBtn);
    actionsLayout->addStretch();
    actionsLayout->addWidget(m_impl->updateBtn);
    actionsLayout->addWidget(m_impl->removeBtn);
    
    mainLayout->addLayout(actionsLayout);
    
    // Close button
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    QPushButton* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addStretch();
    bottomLayout->addWidget(closeBtn);
    mainLayout->addLayout(bottomLayout);
}

QWidget* AddonManagerWindow::createAddonTypeTab(AddonType type) {
    QWidget* widget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // Source tab bar (Installed / Find More)
    QTabBar* sourceBar = new QTabBar();
    sourceBar->addTab("Installed");
    sourceBar->addTab("Find More");
    layout->addWidget(sourceBar);
    
    // View stack for grid vs list mode
    QStackedWidget* viewStack = new QStackedWidget();
    
    // === GRID VIEW (index 0) ===
    QWidget* gridContainer = new QWidget();
    QVBoxLayout* gridLayout = new QVBoxLayout(gridContainer);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    
    QStackedWidget* gridSourceStack = new QStackedWidget();
    QListWidget* installedGrid = createAddonGrid();
    QListWidget* remoteGrid = createAddonGrid();
    gridSourceStack->addWidget(installedGrid);
    gridSourceStack->addWidget(remoteGrid);
    gridLayout->addWidget(gridSourceStack);
    
    viewStack->addWidget(gridContainer);
    
    // === LIST VIEW (index 1) ===
    QWidget* listContainer = new QWidget();
    QVBoxLayout* listLayout = new QVBoxLayout(listContainer);
    listLayout->setContentsMargins(0, 0, 0, 0);
    
    QStackedWidget* listSourceStack = new QStackedWidget();
    QTableWidget* installedTable = createAddonTable();
    QTableWidget* remoteTable = createAddonTable();
    listSourceStack->addWidget(installedTable);
    listSourceStack->addWidget(remoteTable);
    listLayout->addWidget(listSourceStack);
    
    viewStack->addWidget(listContainer);
    
    layout->addWidget(viewStack);
    
    // Store references
    switch (type) {
        case AddonType::Plugin:
            m_impl->pluginsSourceBar = sourceBar;
            m_impl->pluginsStack = gridSourceStack;
            m_impl->pluginsViewStack = viewStack;
            m_impl->pluginsInstalledGrid = installedGrid;
            m_impl->pluginsRemoteGrid = remoteGrid;
            m_impl->pluginsInstalledTable = installedTable;
            m_impl->pluginsRemoteTable = remoteTable;
            break;
        case AddonType::Skin:
            m_impl->skinsSourceBar = sourceBar;
            m_impl->skinsStack = gridSourceStack;
            m_impl->skinsViewStack = viewStack;
            m_impl->skinsInstalledGrid = installedGrid;
            m_impl->skinsRemoteGrid = remoteGrid;
            m_impl->skinsInstalledTable = installedTable;
            m_impl->skinsRemoteTable = remoteTable;
            break;
        case AddonType::Music:
            m_impl->musicSourceBar = sourceBar;
            m_impl->musicStack = gridSourceStack;
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
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);  // Name
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);  // Author
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);  // Version
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);  // Category
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);  // Released
    table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);  // Downloads
    table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);  // Status
    table->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);  // Size
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
            border: 2px solid #3a3a5c;
            border-radius: 4px;
            color: #ffffff;
        }
        QTableWidget::item {
            padding: 8px;
            color: #e0e0e0;
            border-bottom: 1px solid #1a1a2e;
        }
        QTableWidget::item:hover {
            background-color: #252542;
        }
        QTableWidget::item:selected {
            background-color: #2a9d8f;
            color: #ffffff;
        }
        QTableWidget::item:alternate {
            background-color: #151525;
        }
        QHeaderView::section {
            background-color: #1a1a2e;
            color: #c9a227;
            padding: 10px 8px;
            border: none;
            border-right: 1px solid #3a3a5c;
            border-bottom: 2px solid #c9a227;
            font-weight: bold;
        }
    )");
    
    return table;
}

QListWidget* AddonManagerWindow::createAddonGrid() {
    QListWidget* grid = new QListWidget();
    grid->setViewMode(QListView::IconMode);
    grid->setIconSize(QSize(64, 64));
    grid->setGridSize(QSize(200, 130));
    grid->setSpacing(12);
    grid->setResizeMode(QListView::Adjust);
    grid->setWrapping(true);
    grid->setMovement(QListView::Static);
    grid->setSelectionMode(QAbstractItemView::SingleSelection);
    grid->setWordWrap(true);
    grid->setUniformItemSizes(true);
    grid->setStyleSheet(R"(
        QListWidget {
            background-color: #0d0d15;
            border: none;
        }
        QListWidget::item {
            background-color: #252542;
            border: 1px solid #3a3a5c;
            border-radius: 8px;
            color: #ffffff;
            padding: 10px;
        }
        QListWidget::item:hover {
            background-color: #3a3a5c;
            border-color: #c9a227;
        }
        QListWidget::item:selected {
            background-color: #2a9d8f;
            border-color: #4fc3b5;
            color: #ffffff;
        }
    )");
    
    return grid;
}

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
    
    // Double-click to install/view
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
    
    // Grid Selection for Details Panel
    auto connectGridSelection = [this](QListWidget* grid) {
        if (grid) {
            connect(grid, &QListWidget::itemSelectionChanged, [this, grid]() {
                auto selectedItems = grid->selectedItems();
                if (selectedItems.isEmpty()) return;
                
                QListWidgetItem* item = selectedItems.first();
                QString name = item->data(Qt::UserRole + 1).toString();
                QString author = item->data(Qt::UserRole + 2).toString();
                QString version = item->data(Qt::UserRole + 3).toString();
                QString category = item->data(Qt::UserRole + 4).toString();
                int downloads = item->data(Qt::UserRole + 5).toInt();
                QString released = item->data(Qt::UserRole + 6).toString();
                
                // Update details panel if it exists
                if (m_impl->detailsPanel) {
                    QList<QLabel*> labels = m_impl->detailsPanel->findChildren<QLabel*>();
                    for (QLabel* label : labels) {
                        if (label->objectName() == "detailsTitle") label->setText(name);
                        if (label->objectName() == "detailsAuthor") label->setText("Author: " + author);
                        if (label->objectName() == "detailsVersion") label->setText("Version: " + version);
                        if (label->objectName() == "detailsCategory") label->setText("Category: " + category);
                        if (label->objectName() == "detailsDownloads" && downloads > 0) label->setText("Downloads: " + QString::number(downloads));
                        if (label->objectName() == "detailsReleased") label->setText("Released: " + released);
                    }
                }
                
                // Update button states when selection changes
                updateButtonStates();
            });
        }
    };
    
    connectGridSelection(m_impl->pluginsInstalledGrid);
    connectGridSelection(m_impl->pluginsRemoteGrid);
    connectGridSelection(m_impl->skinsInstalledGrid);
    connectGridSelection(m_impl->skinsRemoteGrid);
    connectGridSelection(m_impl->musicInstalledGrid);
    connectGridSelection(m_impl->musicRemoteGrid);
    
    // View toggle button
    connect(m_impl->viewToggleBtn, &QPushButton::toggled, [this](bool listMode) {
        m_impl->isGridView = !listMode;
        m_impl->viewToggleBtn->setText(listMode ? "Grid View" : "List View");
        
        // Switch view stacks for current type
        int viewIndex = listMode ? 1 : 0;  // 0 = grid, 1 = list
        if (m_impl->pluginsViewStack) m_impl->pluginsViewStack->setCurrentIndex(viewIndex);
        if (m_impl->skinsViewStack) m_impl->skinsViewStack->setCurrentIndex(viewIndex);
        if (m_impl->musicViewStack) m_impl->musicViewStack->setCurrentIndex(viewIndex);
        
        // Show/hide details panel
        if (m_impl->detailsPanel) {
            m_impl->detailsPanel->setVisible(!listMode);
        }
        
        // Reload current view
        refresh();
    });
}

void AddonManagerWindow::updateButtonStates() {
    bool showingRemote = m_impl->showingRemote;
    
    // Install from file only for installed view
    m_impl->installBtn->setVisible(!showingRemote);
    m_impl->installBtn->setText(showingRemote ? "📥 Install Selected" : "📥 Install from File...");
    
    // Remove only for installed view
    m_impl->removeBtn->setVisible(!showingRemote);
    
    // Update text based on view
    if (showingRemote) {
        m_impl->updateBtn->setText("📥 Install Selected");
        m_impl->updateBtn->setVisible(true);
        disconnect(m_impl->updateBtn, &QPushButton::clicked, this, &AddonManagerWindow::updateSelected);
        connect(m_impl->updateBtn, &QPushButton::clicked, this, &AddonManagerWindow::installSelected);
    } else {
        m_impl->updateBtn->setText("⬆️ Update");
        disconnect(m_impl->updateBtn, &QPushButton::clicked, this, &AddonManagerWindow::installSelected);
        connect(m_impl->updateBtn, &QPushButton::clicked, this, &AddonManagerWindow::updateSelected);
    }
}

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
    
    // Reload current tab
    int currentTab = m_impl->typeTabWidget->currentIndex();
    switch (currentTab) {
        case 0: showPlugins(); break;
        case 1: showSkins(); break;
        case 2: showMusic(); break;
    }
    
    m_impl->statusLabel->setText("Addons refreshed");
}

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
            QString itemText = addon.name + "\n" + addon.author + "\nv" + addon.installedVersion;
            if (addon.hasUpdate()) {
                itemText += "\n[Update Available]";
            }
            
            QListWidgetItem* item = new QListWidgetItem(itemText);
            item->setData(Qt::UserRole, addon.id);
            item->setData(Qt::UserRole + 1, addon.name);
            item->setData(Qt::UserRole + 2, addon.author);
            item->setData(Qt::UserRole + 3, addon.installedVersion);
            item->setData(Qt::UserRole + 4, addon.category);
            item->setData(Qt::UserRole + 5, 0); // Downloads not stored locally usually, or irrelevant
            item->setData(Qt::UserRole + 6, addon.releaseDate);
            
            item->setSizeHint(QSize(180, 100));
            item->setTextAlignment(Qt::AlignCenter);
            
            grid->addItem(item);
        }
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
            
            QString downloadsStr = addon.downloadCount > 0 
                ? QString::number(addon.downloadCount) : "-";
            table->setItem(row, 5, new QTableWidgetItem(downloadsStr));
            
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
                    QString statusText = installed ? "[Installed]" : "";
                    
                    QString itemText = addon.name + "\n" + addon.author + "\nv" + addon.version;
                    if (!statusText.isEmpty()) {
                        itemText += "\n" + statusText;
                    }
                    
                    QListWidgetItem* item = new QListWidgetItem(itemText);
                item->setData(Qt::UserRole, addon.id);
                item->setData(Qt::UserRole + 1, addon.name);
                item->setData(Qt::UserRole + 2, addon.author);
                item->setData(Qt::UserRole + 3, addon.version);
                item->setData(Qt::UserRole + 4, addon.category);
                item->setData(Qt::UserRole + 5, addon.downloadCount);
                item->setData(Qt::UserRole + 6, addon.releaseDate);
                
                item->setSizeHint(QSize(180, 100));
                item->setTextAlignment(Qt::AlignCenter);
                
                grid->addItem(item);
                }
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
                    
                    QString downloadsStr = addon.downloadCount > 0 
                        ? QString::number(addon.downloadCount) : "-";
                    table->setItem(row, 5, new QTableWidgetItem(downloadsStr));
                    
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
    QListWidget* grid = nullptr;
    AddonType type = m_impl->currentType;
    
    switch (type) {
        case AddonType::Plugin: grid = m_impl->pluginsRemoteGrid; break;
        case AddonType::Skin:   grid = m_impl->skinsRemoteGrid; break;
        case AddonType::Music:  grid = m_impl->musicRemoteGrid; break;
    }
    
    if (!grid) return;
    
    auto selectedItems = grid->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "Install", "Please select an addon to install.");
        return;
    }
    
    QListWidgetItem* item = selectedItems[0];
    QString addonId = item->data(Qt::UserRole).toString();
    QString addonName = item->data(Qt::UserRole + 1).toString();
    
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
    QListWidget* grid = nullptr;
    AddonType type = m_impl->currentType;
    
    switch (type) {
        case AddonType::Plugin: grid = m_impl->pluginsInstalledGrid; break;
        case AddonType::Skin:   grid = m_impl->skinsInstalledGrid; break;
        case AddonType::Music:  grid = m_impl->musicInstalledGrid; break;
    }
    
    if (!grid) return;
    
    auto selectedItems = grid->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "Update", "Please select an addon to update.");
        return;
    }
    
    QListWidgetItem* item = selectedItems[0];
    QString addonId = item->data(Qt::UserRole).toString();
    QString addonName = item->data(Qt::UserRole + 1).toString();
    
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
    QListWidget* grid = nullptr;
    AddonType type = m_impl->currentType;
    
    switch (type) {
        case AddonType::Plugin: grid = m_impl->pluginsInstalledGrid; break;
        case AddonType::Skin:   grid = m_impl->skinsInstalledGrid; break;
        case AddonType::Music:  grid = m_impl->musicInstalledGrid; break;
    }
    
    if (!grid) return;
    
    auto selectedItems = grid->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::warning(this, "Remove", "Please select an addon to remove.");
        return;
    }
    
    QListWidgetItem* item = selectedItems[0];
    QString addonId = item->data(Qt::UserRole).toString();
    QString addonName = item->data(Qt::UserRole + 1).toString();
    
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
    QListWidget* grid = nullptr;
    AddonType type = m_impl->currentType;
    
    if (m_impl->showingRemote) {
        switch (type) {
            case AddonType::Plugin: grid = m_impl->pluginsRemoteGrid; break;
            case AddonType::Skin:   grid = m_impl->skinsRemoteGrid; break;
            case AddonType::Music:  grid = m_impl->musicRemoteGrid; break;
        }
    } else {
        switch (type) {
            case AddonType::Plugin: grid = m_impl->pluginsInstalledGrid; break;
            case AddonType::Skin:   grid = m_impl->skinsInstalledGrid; break;
            case AddonType::Music:  grid = m_impl->musicInstalledGrid; break;
        }
    }
    
    if (!grid) return;
    
    auto selectedItems = grid->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "Open Page", "Please select an addon.");
        return;
    }
    
    QListWidgetItem* item = selectedItems[0];
    QString addonId = item->data(Qt::UserRole).toString();
    
    QString url = QString("https://www.lotrointerface.com/downloads/info%1").arg(addonId);
    QDesktopServices::openUrl(QUrl(url));
}

void AddonManagerWindow::search(const QString& query) {
    QListWidget* grid = nullptr;
    
    if (m_impl->showingRemote) {
        switch (m_impl->currentType) {
            case AddonType::Plugin: grid = m_impl->pluginsRemoteGrid; break;
            case AddonType::Skin:   grid = m_impl->skinsRemoteGrid; break;
            case AddonType::Music:  grid = m_impl->musicRemoteGrid; break;
        }
    } else {
        switch (m_impl->currentType) {
            case AddonType::Plugin: grid = m_impl->pluginsInstalledGrid; break;
            case AddonType::Skin:   grid = m_impl->skinsInstalledGrid; break;
            case AddonType::Music:  grid = m_impl->musicInstalledGrid; break;
        }
    }
    
    if (!grid) return;
    
    for (int i = 0; i < grid->count(); ++i) {
        QListWidgetItem* item = grid->item(i);
        bool match = query.isEmpty() || 
            item->text().contains(query, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

} // namespace lotro
