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
    
    // Tables for installed addons
    QTableWidget* pluginsInstalledTable = nullptr;
    QTableWidget* skinsInstalledTable = nullptr;
    QTableWidget* musicInstalledTable = nullptr;
    
    // Tables for remote addons
    QTableWidget* pluginsRemoteTable = nullptr;
    QTableWidget* skinsRemoteTable = nullptr;
    QTableWidget* musicRemoteTable = nullptr;
    
    QLineEdit* searchEdit = nullptr;
    QProgressBar* progressBar = nullptr;
    QLabel* statusLabel = nullptr;
    
    QPushButton* installBtn = nullptr;
    QPushButton* updateBtn = nullptr;
    QPushButton* removeBtn = nullptr;
    QPushButton* refreshBtn = nullptr;
    QPushButton* browseBtn = nullptr;
    
    // Track current view
    bool showingRemote = false;
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
    
    m_impl->refreshBtn = new QPushButton("🔄 Refresh");
    toolbarLayout->addWidget(m_impl->refreshBtn);
    
    mainLayout->addLayout(toolbarLayout);
    
    // Main tab widget for addon types
    m_impl->typeTabWidget = new QTabWidget();
    
    // Create tabs for each addon type
    m_impl->typeTabWidget->addTab(createAddonTypeTab(AddonType::Plugin), "🔌 Plugins");
    m_impl->typeTabWidget->addTab(createAddonTypeTab(AddonType::Skin), "🎨 Skins");
    m_impl->typeTabWidget->addTab(createAddonTypeTab(AddonType::Music), "🎵 Music");
    
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
    
    m_impl->installBtn = new QPushButton("📥 Install from File...");
    m_impl->browseBtn = new QPushButton("🌐 Open on LotroInterface");
    m_impl->updateBtn = new QPushButton("⬆️ Update");
    m_impl->removeBtn = new QPushButton("🗑️ Remove");
    
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
    sourceBar->addTab("📦 Installed");
    sourceBar->addTab("🔍 Find More");
    layout->addWidget(sourceBar);
    
    // Stacked widget for switching views
    QStackedWidget* stack = new QStackedWidget();
    
    // Installed table
    QTableWidget* installedTable = createAddonTable();
    stack->addWidget(installedTable);
    
    // Remote table
    QTableWidget* remoteTable = createAddonTable();
    stack->addWidget(remoteTable);
    
    layout->addWidget(stack);
    
    // Store references
    switch (type) {
        case AddonType::Plugin:
            m_impl->pluginsSourceBar = sourceBar;
            m_impl->pluginsStack = stack;
            m_impl->pluginsInstalledTable = installedTable;
            m_impl->pluginsRemoteTable = remoteTable;
            break;
        case AddonType::Skin:
            m_impl->skinsSourceBar = sourceBar;
            m_impl->skinsStack = stack;
            m_impl->skinsInstalledTable = installedTable;
            m_impl->skinsRemoteTable = remoteTable;
            break;
        case AddonType::Music:
            m_impl->musicSourceBar = sourceBar;
            m_impl->musicStack = stack;
            m_impl->musicInstalledTable = installedTable;
            m_impl->musicRemoteTable = remoteTable;
            break;
    }
    
    // Connect source bar
    connect(sourceBar, &QTabBar::currentChanged, [this, stack, type](int index) {
        stack->setCurrentIndex(index);
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
    
    return table;
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
    auto connectTableDoubleClick = [this](QTableWidget* table, bool isRemote) {
        connect(table, &QTableWidget::cellDoubleClicked, [this, isRemote](int row, int) {
            if (isRemote) {
                installSelected();
            } else {
                openAddonPage();
            }
        });
    };
    
    connectTableDoubleClick(m_impl->pluginsInstalledTable, false);
    connectTableDoubleClick(m_impl->pluginsRemoteTable, true);
    connectTableDoubleClick(m_impl->skinsInstalledTable, false);
    connectTableDoubleClick(m_impl->skinsRemoteTable, true);
    connectTableDoubleClick(m_impl->musicInstalledTable, false);
    connectTableDoubleClick(m_impl->musicRemoteTable, true);
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
    
    QTableWidget* table = nullptr;
    switch (type) {
        case AddonType::Plugin: table = m_impl->pluginsInstalledTable; break;
        case AddonType::Skin:   table = m_impl->skinsInstalledTable; break;
        case AddonType::Music:  table = m_impl->musicInstalledTable; break;
    }
    
    if (!table) return;
    
    auto addons = m_impl->addonManager->getInstalledAddons(type);
    
    table->setSortingEnabled(false);
    table->setRowCount(static_cast<int>(addons.size()));
    
    for (int row = 0; row < static_cast<int>(addons.size()); ++row) {
        const auto& addon = addons[row];
        
        table->setItem(row, 0, new QTableWidgetItem(addon.name));
        table->setItem(row, 1, new QTableWidgetItem(addon.author));
        table->setItem(row, 2, new QTableWidgetItem(addon.installedVersion));
        table->setItem(row, 3, new QTableWidgetItem(addon.category));
        table->setItem(row, 4, new QTableWidgetItem(addon.releaseDate));
        
        // Downloads column - use formatted number
        QString downloadsStr = addon.downloadCount > 0 
            ? QString::number(addon.downloadCount) : "-";
        table->setItem(row, 5, new QTableWidgetItem(downloadsStr));
        
        QString statusText;
        if (addon.hasUpdate()) {
            statusText = "⬆️ Update Available";
        } else {
            statusText = "✅ Installed";
        }
        table->setItem(row, 6, new QTableWidgetItem(statusText));
        table->setItem(row, 7, new QTableWidgetItem(addon.fileSize));
        table->setItem(row, 8, new QTableWidgetItem(addon.id));
    }
    
    table->setSortingEnabled(true);
    m_impl->statusLabel->setText(QString("Found %1 installed %2")
        .arg(addons.size())
        .arg(addonTypeToString(type).toLower() + "s"));
}

void AddonManagerWindow::loadRemoteAddons(AddonType type) {
    if (!m_impl->addonManager) {
        return;
    }
    
    QTableWidget* table = nullptr;
    switch (type) {
        case AddonType::Plugin: table = m_impl->pluginsRemoteTable; break;
        case AddonType::Skin:   table = m_impl->skinsRemoteTable; break;
        case AddonType::Music:  table = m_impl->musicRemoteTable; break;
    }
    
    if (!table) return;
    
    m_impl->statusLabel->setText("Fetching addons from lotrointerface.com...");
    m_impl->progressBar->setVisible(true);
    m_impl->progressBar->setRange(0, 0);  // Indeterminate
    
    // Fetch asynchronously
    auto future = m_impl->addonManager->fetchRemoteAddonList(type);
    
    // Poll for completion
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, [this, future, table, type, timer]() mutable {
        if (future.isFinished()) {
            timer->stop();
            timer->deleteLater();
            
            auto addons = future.result();
            
            m_impl->progressBar->setVisible(false);
            
            table->setSortingEnabled(false);
            table->setRowCount(static_cast<int>(addons.size()));
            
            for (int row = 0; row < static_cast<int>(addons.size()); ++row) {
                const auto& addon = addons[row];
                
                table->setItem(row, 0, new QTableWidgetItem(addon.name));
                table->setItem(row, 1, new QTableWidgetItem(addon.author));
                table->setItem(row, 2, new QTableWidgetItem(addon.version));
                table->setItem(row, 3, new QTableWidgetItem(addon.category));
                table->setItem(row, 4, new QTableWidgetItem(addon.releaseDate));
                
                // Downloads column - format nicely
                QString downloadsStr = addon.downloadCount > 0 
                    ? QString::number(addon.downloadCount) : "-";
                table->setItem(row, 5, new QTableWidgetItem(downloadsStr));
                
                // Check if installed
                auto installed = m_impl->addonManager->getInstalledAddon(addon.id, type);
                QString statusText = installed ? "✅ Installed" : "📥 Available";
                table->setItem(row, 6, new QTableWidgetItem(statusText));
                table->setItem(row, 7, new QTableWidgetItem(addon.fileSize));
                table->setItem(row, 8, new QTableWidgetItem(addon.id));
            }
            
            table->setSortingEnabled(true);
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
    QTableWidget* table = nullptr;
    AddonType type = m_impl->currentType;
    
    switch (type) {
        case AddonType::Plugin: table = m_impl->pluginsRemoteTable; break;
        case AddonType::Skin:   table = m_impl->skinsRemoteTable; break;
        case AddonType::Music:  table = m_impl->musicRemoteTable; break;
    }
    
    if (!table) return;
    
    auto selectedItems = table->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "Install", "Please select an addon to install.");
        return;
    }
    
    int row = selectedItems[0]->row();
    QString addonId = table->item(row, 8)->text();  // ID column at index 8
    QString addonName = table->item(row, 0)->text();
    
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
                    loadInstalledAddons(m_impl->currentType);  // Refresh installed tab
                    loadRemoteAddons(m_impl->currentType);  // Refresh to show installed status
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
    QTableWidget* table = nullptr;
    AddonType type = m_impl->currentType;
    
    switch (type) {
        case AddonType::Plugin: table = m_impl->pluginsInstalledTable; break;
        case AddonType::Skin:   table = m_impl->skinsInstalledTable; break;
        case AddonType::Music:  table = m_impl->musicInstalledTable; break;
    }
    
    if (!table) return;
    
    auto selectedItems = table->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "Update", "Please select an addon to update.");
        return;
    }
    
    int row = selectedItems[0]->row();
    QString addonId = table->item(row, 8)->text();  // ID column at index 8
    QString addonName = table->item(row, 0)->text();
    
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
    QTableWidget* table = nullptr;
    AddonType type = m_impl->currentType;
    
    switch (type) {
        case AddonType::Plugin: table = m_impl->pluginsInstalledTable; break;
        case AddonType::Skin:   table = m_impl->skinsInstalledTable; break;
        case AddonType::Music:  table = m_impl->musicInstalledTable; break;
    }
    
    if (!table) return;
    
    auto selectedItems = table->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::warning(this, "Remove", "Please select an addon to remove.");
        return;
    }
    
    int row = selectedItems[0]->row();
    QString addonId = table->item(row, 8)->text();  // ID column is now at index 8
    QString addonName = table->item(row, 0)->text();
    
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
    QTableWidget* table = nullptr;
    AddonType type = m_impl->currentType;
    
    if (m_impl->showingRemote) {
        switch (type) {
            case AddonType::Plugin: table = m_impl->pluginsRemoteTable; break;
            case AddonType::Skin:   table = m_impl->skinsRemoteTable; break;
            case AddonType::Music:  table = m_impl->musicRemoteTable; break;
        }
    } else {
        switch (type) {
            case AddonType::Plugin: table = m_impl->pluginsInstalledTable; break;
            case AddonType::Skin:   table = m_impl->skinsInstalledTable; break;
            case AddonType::Music:  table = m_impl->musicInstalledTable; break;
        }
    }
    
    if (!table) return;
    
    auto selectedItems = table->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "Open Page", "Please select an addon.");
        return;
    }
    
    int row = selectedItems[0]->row();
    QString addonId = table->item(row, 8)->text();  // ID column at index 8
    
    QString url = QString("https://www.lotrointerface.com/downloads/info%1").arg(addonId);
    QDesktopServices::openUrl(QUrl(url));
}

void AddonManagerWindow::search(const QString& query) {
    QTableWidget* table = nullptr;
    
    if (m_impl->showingRemote) {
        switch (m_impl->currentType) {
            case AddonType::Plugin: table = m_impl->pluginsRemoteTable; break;
            case AddonType::Skin:   table = m_impl->skinsRemoteTable; break;
            case AddonType::Music:  table = m_impl->musicRemoteTable; break;
        }
    } else {
        switch (m_impl->currentType) {
            case AddonType::Plugin: table = m_impl->pluginsInstalledTable; break;
            case AddonType::Skin:   table = m_impl->skinsInstalledTable; break;
            case AddonType::Music:  table = m_impl->musicInstalledTable; break;
        }
    }
    
    if (!table) return;
    
    for (int row = 0; row < table->rowCount(); ++row) {
        bool match = query.isEmpty();
        
        if (!match) {
            for (int col = 0; col < 4; ++col) {  // Search name, author, version, category
                auto item = table->item(row, col);
                if (item && item->text().contains(query, Qt::CaseInsensitive)) {
                    match = true;
                    break;
                }
            }
        }
        
        table->setRowHidden(row, !match);
    }
}

} // namespace lotro
