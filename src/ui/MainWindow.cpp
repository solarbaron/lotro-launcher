/**
 * LOTRO Launcher - Main Window Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "MainWindow.hpp"
#include "LoginWidget.hpp"
#include "SettingsWindow.hpp"
#include "AddonManagerWindow.hpp"
#include "CompanionWindow.hpp"
#include "JournalWindow.hpp"
#include "LoadingSpinner.hpp"
#include "core/config/ConfigManager.hpp"
#include "core/credentials/CredentialStore.hpp"
#include "network/GameServicesInfo.hpp"
#include "network/LoginAccount.hpp"
#include "network/WorldList.hpp"
#include "network/NewsfeedParser.hpp"
#include "game/GameLauncher.hpp"

#ifdef PLATFORM_LINUX
#include "wine/WineManager.hpp"
#endif

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QStatusBar>
#include <QProgressBar>
#include <QFrame>
#include <QSplitter>
#include <QGroupBox>
#include <QScrollArea>
#include <QDesktopServices>
#include <QTimer>
#include <QUrl>
#include <QEvent>
#include <QDateTime>

#include <spdlog/spdlog.h>

namespace lotro {

class MainWindow::Impl {
public:
    QString currentGameId;
    std::optional<GameServicesInfo> servicesInfo;
    std::optional<AccountLoginResponse> loginResponse;
    std::vector<World> worlds;
    
    // UI Components
    LoginWidget* loginWidget = nullptr;
    QComboBox* worldSelector = nullptr;
    QPushButton* launchButton = nullptr;
    QPushButton* logoutButton = nullptr;
    QPushButton* settingsButton = nullptr;
    QPushButton* addonsButton = nullptr;
    QPushButton* journalButton = nullptr;
    QLabel* statusLabel = nullptr;
    QProgressBar* progressBar = nullptr;
    LoadingLabel* loadingLabel = nullptr;
    QScrollArea* newsfeedScrollArea = nullptr;
    QWidget* newsfeedContainer = nullptr;
    QVBoxLayout* newsfeedLayout = nullptr;
    
    std::unique_ptr<CredentialStore> credentialStore;
    std::unique_ptr<GameLauncher> gameLauncher;
    
    bool isLoggedIn = false;
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_impl(std::make_unique<Impl>())
{
    m_impl->credentialStore = CredentialStore::create();
    
    setupUi();
    setupConnections();
    
    // Default to LOTRO
    setGame("lotro");
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
    setWindowTitle("LOTRO Launcher");
    setMinimumSize(900, 700);
    resize(1000, 750);  // Default size larger than minimum
    
    // Create central widget
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    // Menu bar removed - features accessible via navigation buttons
    
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // ========== TOP NAVIGATION BAR ==========
    QFrame* topNavBar = new QFrame();
    topNavBar->setObjectName("topNavBar");
    topNavBar->setFixedHeight(50);
    
    QHBoxLayout* navLayout = new QHBoxLayout(topNavBar);
    navLayout->setContentsMargins(20, 0, 20, 0);
    navLayout->setSpacing(10);
    
    // Navigation buttons
    m_impl->addonsButton = new QPushButton("Add-ons");
    m_impl->addonsButton->setObjectName("navButton");
    m_impl->journalButton = new QPushButton("Journal");
    m_impl->journalButton->setObjectName("navButton");
    
    QPushButton* trackerButton = new QPushButton("Tracker");
    trackerButton->setObjectName("navButton");
    connect(trackerButton, &QPushButton::clicked, this, &MainWindow::openCharacterTracker);
    
    m_impl->settingsButton = new QPushButton("Settings");
    m_impl->settingsButton->setObjectName("navButton");
    
    navLayout->addWidget(m_impl->addonsButton);
    navLayout->addWidget(m_impl->journalButton);
    navLayout->addWidget(trackerButton);
    navLayout->addWidget(m_impl->settingsButton);
    
    navLayout->addStretch();
    
    mainLayout->addWidget(topNavBar);
    
    // ========== MAIN CONTENT AREA ==========
    QWidget* contentArea = new QWidget();
    QHBoxLayout* contentLayout = new QHBoxLayout(contentArea);
    contentLayout->setContentsMargins(15, 15, 15, 15);
    contentLayout->setSpacing(15);
    
    // --- LEFT: LOGIN PANEL ---
    QFrame* loginPanel = new QFrame();
    loginPanel->setObjectName("loginPanel");
    loginPanel->setFixedWidth(300);
    
    QVBoxLayout* loginPanelLayout = new QVBoxLayout(loginPanel);
    loginPanelLayout->setContentsMargins(20, 20, 20, 20);
    loginPanelLayout->setSpacing(12);
    
    // Account section
    QLabel* accountLabel = new QLabel("Account");
    accountLabel->setObjectName("sectionLabel");
    loginPanelLayout->addWidget(accountLabel);
    
    m_impl->loginWidget = new LoginWidget();
    loginPanelLayout->addWidget(m_impl->loginWidget);
    
    // Decorative separator
    QFrame* sep1 = new QFrame();
    sep1->setObjectName("separator");
    sep1->setFrameShape(QFrame::HLine);
    sep1->setFixedHeight(2);
    loginPanelLayout->addWidget(sep1);
    
    // Server section
    QLabel* serverLabel = new QLabel("Server");
    serverLabel->setObjectName("sectionLabel");
    loginPanelLayout->addWidget(serverLabel);
    
    m_impl->worldSelector = new QComboBox();
    m_impl->worldSelector->setEnabled(false);
    m_impl->worldSelector->addItem("Select a server...");
    loginPanelLayout->addWidget(m_impl->worldSelector);
    
    m_impl->loadingLabel = new LoadingLabel("Fetching servers...", loginPanel);
    m_impl->loadingLabel->hide();
    loginPanelLayout->addWidget(m_impl->loadingLabel);
    
    // Logout button (hidden by default)
    m_impl->logoutButton = new QPushButton("Logout");
    m_impl->logoutButton->setVisible(false);
    loginPanelLayout->addWidget(m_impl->logoutButton);
    
    loginPanelLayout->addStretch();
    
    contentLayout->addWidget(loginPanel);
    
    // --- RIGHT: NEWS SECTION (Main Focus) ---
    QFrame* newsPanel = new QFrame();
    newsPanel->setObjectName("newsPanel");
    
    QVBoxLayout* newsPanelLayout = new QVBoxLayout(newsPanel);
    newsPanelLayout->setContentsMargins(20, 20, 20, 20);
    newsPanelLayout->setSpacing(15);
    
    // News header
    QLabel* newsHeader = new QLabel("Latest News & Updates");
    newsHeader->setObjectName("sectionLabel");
    newsHeader->setStyleSheet("font-size: 16px;");
    newsPanelLayout->addWidget(newsHeader);
    
    // News scroll area
    m_impl->newsfeedScrollArea = new QScrollArea();
    m_impl->newsfeedScrollArea->setWidgetResizable(true);
    m_impl->newsfeedScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_impl->newsfeedScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    m_impl->newsfeedContainer = new QWidget();
    m_impl->newsfeedLayout = new QVBoxLayout(m_impl->newsfeedContainer);
    m_impl->newsfeedLayout->setSpacing(12);
    m_impl->newsfeedLayout->setContentsMargins(5, 5, 5, 5);
    m_impl->newsfeedLayout->addStretch();
    
    m_impl->newsfeedScrollArea->setWidget(m_impl->newsfeedContainer);
    newsPanelLayout->addWidget(m_impl->newsfeedScrollArea);
    
    contentLayout->addWidget(newsPanel, 1);  // Stretch to fill remaining space
    
    mainLayout->addWidget(contentArea, 1);
    
    // ========== FOOTER BAR (Play Button) ==========
    QFrame* footerBar = new QFrame();
    footerBar->setObjectName("footerBar");
    footerBar->setFixedHeight(65);
    footerBar->setStyleSheet("QFrame#footerBar { background-color: #0d0d15; border-top: 2px solid #c9a227; }");
    
    QHBoxLayout* footerLayout = new QHBoxLayout(footerBar);
    footerLayout->setContentsMargins(20, 8, 20, 8);
    
    // Status on left
    m_impl->statusLabel = new QLabel("Ready");
    m_impl->statusLabel->setStyleSheet("color: #b0b0b0; font-size: 12px;");
    footerLayout->addWidget(m_impl->statusLabel);
    
    m_impl->progressBar = new QProgressBar();
    m_impl->progressBar->setVisible(false);
    m_impl->progressBar->setFixedWidth(200);
    footerLayout->addWidget(m_impl->progressBar);
    
    footerLayout->addStretch();
    
    // Play button on right - prominent position
    m_impl->launchButton = new QPushButton("PLAY");
    m_impl->launchButton->setObjectName("playButton");
    m_impl->launchButton->setFixedSize(180, 45);
    m_impl->launchButton->setCursor(Qt::PointingHandCursor);
    m_impl->launchButton->setEnabled(false);
    
    footerLayout->addWidget(m_impl->launchButton);
    
    mainLayout->addWidget(footerBar);
}

void MainWindow::setupConnections() {
    connect(m_impl->loginWidget, &LoginWidget::loginRequested,
            this, &MainWindow::login);
            
    connect(m_impl->loginWidget, &LoginWidget::accountSelected,
            this, &MainWindow::loadAccount);
            
    connect(m_impl->loginWidget, &LoginWidget::deleteAccountRequested,
            [this](const QString& username) {
                deleteAccount(username);
            });
    
    connect(m_impl->launchButton, &QPushButton::clicked,
            this, &MainWindow::launchGame);
    
    connect(m_impl->settingsButton, &QPushButton::clicked,
            this, &MainWindow::openSettings);
    
    connect(m_impl->addonsButton, &QPushButton::clicked,
            this, &MainWindow::openAddonManager);
    
    connect(m_impl->journalButton, &QPushButton::clicked,
            this, [this]() {
                JournalWindow journal(this);
                journal.exec();
            });
    
    connect(m_impl->logoutButton, &QPushButton::clicked,
            this, [this]() {
                // Clear login state
                m_impl->isLoggedIn = false;
                m_impl->loginResponse.reset();
                m_impl->worlds.clear();
                
                // Reset UI
                m_impl->loginWidget->setLoggingIn(false);
                m_impl->loginWidget->setPassword("");
                m_impl->worldSelector->clear();
                m_impl->worldSelector->addItem("Select a server...");
                m_impl->worldSelector->setEnabled(false);
                m_impl->launchButton->setEnabled(false);
                m_impl->logoutButton->setVisible(false);
                m_impl->statusLabel->setText("Logged out");
                
                spdlog::info("User logged out");
            });
}

void MainWindow::setGame(const QString& gameId) {
    m_impl->currentGameId = gameId;
    m_impl->isLoggedIn = false;
    m_impl->worlds.clear();
    m_impl->worldSelector->clear();
    m_impl->worldSelector->addItem("Select a server...");
    m_impl->worldSelector->setEnabled(false);
    m_impl->launchButton->setEnabled(false);
    
    // Fetch game services info
    m_impl->statusLabel->setText("Connecting to game services...");
    
    QString datacenterUrl = getDatacenterUrl(gameId);
    auto future = fetchGameServicesInfo(datacenterUrl, gameId);
    
    // TODO: Handle async properly with QFutureWatcher
    future.waitForFinished();
    auto result = future.result();
    
    if (result) {
        m_impl->servicesInfo = *result;
        m_impl->statusLabel->setText("Connected. Please log in.");
        spdlog::info("Game services loaded for: {}", gameId.toStdString());
    } else {
        m_impl->statusLabel->setText("Failed to connect to game services");
        spdlog::error("Failed to load game services for: {}", gameId.toStdString());
    }
    
    refreshNewsfeed();
    loadSavedAccounts();
    autoLogin();
}

QString MainWindow::currentGame() const {
    return m_impl->currentGameId;
}

void MainWindow::login() {
    QString username = m_impl->loginWidget->username();
    QString password = m_impl->loginWidget->password();
    
    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "Login Error", "Please enter username and password");
        return;
    }
    
    if (!m_impl->servicesInfo) {
        QMessageBox::warning(this, "Login Error", "Game services not available");
        return;
    }
    
    m_impl->loginWidget->setLoggingIn(true);
    m_impl->statusLabel->setText("Logging in...");
    
    auto future = loginAccount(
        m_impl->servicesInfo->authServer,
        username,
        password
    );
    
    // TODO: Use QFutureWatcher for proper async
    future.waitForFinished();
    auto result = future.result();
    
    if (result.isSuccess()) {
        m_impl->loginResponse = result.response;
        m_impl->isLoggedIn = true;
        
        // Always save password on successful login
        if (m_impl->credentialStore) {
            m_impl->credentialStore->storePassword(
                LOTRO_CREDENTIAL_SERVICE,
                username.toStdString(),
                password.toStdString()
            );
        }
        
        saveCurrentAccount();
        onLoginComplete();
    } else {
        m_impl->isLoggedIn = false;
        onLoginFailed(result.errorMessage);
    }
    
    m_impl->loginWidget->setLoggingIn(false);
}

void MainWindow::onLoginComplete() {
    m_impl->statusLabel->setText("Login successful. Loading servers...");
    m_impl->logoutButton->setVisible(true);
    spdlog::info("Login successful");
    
    // Fetch world list
    refreshWorldList();
}

void MainWindow::onLoginFailed(const QString& error) {
    m_impl->statusLabel->setText("Login failed");
    QMessageBox::warning(this, "Login Failed", error);
    spdlog::warn("Login failed: {}", error.toStdString());
}

void MainWindow::refreshWorldList() {
    if (!m_impl->servicesInfo) {
        return;
    }
    
    // Show loading indicator
    m_impl->loadingLabel->start("Fetching server status...");
    m_impl->worldSelector->setEnabled(false);
    
    // Use the new API that gets worlds from GameServicesInfo and fetches status
    auto future = fetchWorldsWithStatus(*m_impl->servicesInfo);
    future.waitForFinished();
    
    // Hide loading indicator
    m_impl->loadingLabel->stop();
    
    m_impl->worlds = future.result();
    updateWorldList(m_impl->worlds);
    onWorldsLoaded();
}

void MainWindow::onWorldsLoaded() {
    m_impl->worldSelector->setEnabled(!m_impl->worlds.empty());
    
    if (m_impl->isLoggedIn && !m_impl->worlds.empty()) {
        m_impl->launchButton->setEnabled(true);
        
        // Try to select last used world
        QString currentUsername = m_impl->loginWidget->username();
        auto& config = ConfigManager::instance();
        auto accounts = config.getAccounts(m_impl->currentGameId.toStdString());
        
        spdlog::debug("Looking for last world for user: {}", currentUsername.toStdString());
        
        for (const auto& account : accounts) {
            if (account.username == currentUsername.toStdString()) {
                spdlog::debug("Found account, lastUsedWorld: '{}'", account.lastUsedWorld);
                
                if (!account.lastUsedWorld.empty()) {
                    QString lastWorld = QString::fromStdString(account.lastUsedWorld);
                    int index = m_impl->worldSelector->findData(lastWorld);
                    spdlog::debug("findData('{}') returned index: {}", lastWorld.toStdString(), index);
                    
                    if (index >= 0) {
                        m_impl->worldSelector->setCurrentIndex(index);
                        spdlog::info("Selected last used world: {}", account.lastUsedWorld);
                    } else {
                        spdlog::warn("Could not find world '{}' in selector", account.lastUsedWorld);
                    }
                }
                break;
            }
        }
    }
    
    m_impl->statusLabel->setText("Ready to play");
}

void MainWindow::updateWorldList(const std::vector<World>& worlds) {
    m_impl->worldSelector->clear();
    
    // Get current account's play times
    std::map<std::string, int64_t> playTimes;
    QString currentUsername = m_impl->loginWidget->username();
    if (!currentUsername.isEmpty()) {
        auto& config = ConfigManager::instance();
        auto accounts = config.getAccounts(m_impl->currentGameId.toStdString());
        for (const auto& account : accounts) {
            if (account.username == currentUsername.toStdString()) {
                playTimes = account.worldPlayTimes;
                break;
            }
        }
    }
    
    for (const auto& world : worlds) {
        QString displayText = world.displayName.isEmpty() ? world.name : world.displayName;
        
        // Add status indicator using Unicode circles with color styling
        QString statusIcon;
        switch (world.status) {
            case WorldStatus::Online:
                statusIcon = "🟢";  // Green circle
                break;
            case WorldStatus::Busy:
                statusIcon = "🟡";  // Yellow circle
                break;
            case WorldStatus::Full:
                statusIcon = "🟠";  // Orange circle
                break;
            case WorldStatus::Locked:
            case WorldStatus::Offline:
                statusIcon = "🔴";  // Red circle
                break;
            default:
                statusIcon = "⚪";  // White circle
                break;
        }
        
        // Check if this world was played before
        QString lastPlayedInfo;
        auto it = playTimes.find(world.name.toStdString());
        if (it != playTimes.end() && it->second > 0) {
            QDateTime playTime = QDateTime::fromSecsSinceEpoch(it->second);
            QDateTime now = QDateTime::currentDateTime();
            qint64 secsAgo = playTime.secsTo(now);
            
            if (secsAgo < 60) {
                lastPlayedInfo = " - Last played: just now";
            } else if (secsAgo < 3600) {
                int mins = secsAgo / 60;
                lastPlayedInfo = QString(" - Last played: %1 min ago").arg(mins);
            } else if (secsAgo < 86400) {
                int hours = secsAgo / 3600;
                lastPlayedInfo = QString(" - Last played: %1h ago").arg(hours);
            } else {
                int days = secsAgo / 86400;
                lastPlayedInfo = QString(" - Last played: %1d ago").arg(days);
            }
        }
        
        QString itemText = QString("%1 %2 (%3)%4")
            .arg(statusIcon)
            .arg(displayText)
            .arg(world.statusString())
            .arg(lastPlayedInfo);
        
        m_impl->worldSelector->addItem(itemText, world.name);
        
        // Disable if not online
        if (!world.canLogin()) {
            m_impl->worldSelector->setItemData(
                m_impl->worldSelector->count() - 1,
                false,
                Qt::UserRole - 1  // Enable role
            );
        }
    }
}

void MainWindow::launchGame() {
    if (!m_impl->isLoggedIn || !m_impl->loginResponse) {
        QMessageBox::warning(this, "Launch Error", "Please log in first");
        return;
    }
    
    int worldIndex = m_impl->worldSelector->currentIndex();
    if (worldIndex < 0 || worldIndex >= static_cast<int>(m_impl->worlds.size())) {
        QMessageBox::warning(this, "Launch Error", "Please select a server");
        return;
    }
    
    const World& selectedWorld = m_impl->worlds[worldIndex];
    
    // Get game config
    auto& configManager = ConfigManager::instance();
    auto gameConfig = configManager.getGameConfig(m_impl->currentGameId.toStdString());
    
    if (!gameConfig) {
        QMessageBox::warning(this, "Launch Error", 
            "Game not configured. Please run setup.");
        return;
    }
    
    m_impl->statusLabel->setText("Launching game...");
    m_impl->launchButton->setEnabled(false);
    
    // Save selected world for next session
    saveCurrentAccount();
    
    // Get subscription info
    QString accountNumber;
    
    // Use loginServer (from loginservers XML) for the -h argument
    // This is different from queueUrl which is used for world queue joining
    QString loginServer = selectedWorld.loginServer;
    
    auto subs = m_impl->loginResponse->getGameSubscriptions(
        getDatacenterGameName(m_impl->currentGameId));
    if (!subs.empty()) {
        accountNumber = subs[0].name; // Use first subscription
    }
    
    // Create and run launcher
    m_impl->gameLauncher = std::make_unique<GameLauncher>(*gameConfig);
    
    m_impl->gameLauncher->launch(
        selectedWorld,
        m_impl->loginResponse->sessionTicket,
        accountNumber,
        loginServer,
        [this](const LaunchResult& result) {
            if (result.success) {
                m_impl->statusLabel->setText("Game launched");
                emit gameStarted(result.processId);
                spdlog::info("Game launched with PID: {}", result.processId);
            } else {
                m_impl->statusLabel->setText("Launch failed");
                QMessageBox::critical(this, "Launch Failed", result.errorMessage);
            }
            m_impl->launchButton->setEnabled(true);
        }
    );
}

void MainWindow::openSettings() {
    SettingsWindow settings(m_impl->currentGameId, this);
    if (settings.exec() == QDialog::Accepted) {
        // Refresh if settings changed
        emit settingsChanged();
    }
}

void MainWindow::openAddonManager() {
    AddonManagerWindow addons(m_impl->currentGameId, this);
    addons.exec();
}

void MainWindow::openCharacterTracker() {
    // Get game path if available
    QString gamePath;
    auto& configManager = ConfigManager::instance();
    auto gameConfig = configManager.getGameConfig(m_impl->currentGameId.toStdString());
    if (gameConfig) {
        gamePath = QString::fromStdString(gameConfig->gameDirectory.string());
    }
    
    CompanionWindow companion(gamePath, this);
    companion.exec();
}

void MainWindow::refreshNewsfeed() {
    // Determine news URL - use configured URL or fallback to known LOTRO feed
    QString newsUrl;
    if (m_impl->servicesInfo && !m_impl->servicesInfo->newsUrl.isEmpty()) {
        newsUrl = m_impl->servicesInfo->newsUrl;
    } else {
        // Fallback to known news feed URLs (from official launcher config)
        QString gameType = m_impl->currentGameId.toLower();
        if (gameType.startsWith("lotro")) {
            newsUrl = "https://www.lotro.com/en/launcher-feed.xml";
        } else if (gameType.startsWith("ddo")) {
            newsUrl = "https://www.ddo.com/en/launcher-feed.xml";
        } else {
            spdlog::warn("No news URL available for game: {}", gameType.toStdString());
            return;
        }
    }
    
    // Clear existing news items (but keep the stretch at the end)
    while (m_impl->newsfeedLayout->count() > 1) {
        QLayoutItem* item = m_impl->newsfeedLayout->takeAt(0);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    
    // Add loading indicator
    QLabel* loadingLabel = new QLabel("Loading news...");
    loadingLabel->setAlignment(Qt::AlignCenter);
    loadingLabel->setStyleSheet("color: #6a6a8a; font-style: italic;");
    m_impl->newsfeedLayout->insertWidget(0, loadingLabel);
    
    // Fetch news asynchronously
    auto future = fetchNewsfeed(newsUrl, 10);
    
    // Use QFutureWatcher for proper async handling
    // For simplicity, we'll do a blocking call in a timer to avoid blocking UI
    QTimer::singleShot(100, [this, future]() mutable {
        future.waitForFinished();
        auto items = future.result();
        
        // Clear loading indicator
        while (m_impl->newsfeedLayout->count() > 1) {
            QLayoutItem* item = m_impl->newsfeedLayout->takeAt(0);
            if (item->widget()) {
                delete item->widget();
            }
            delete item;
        }
        
        if (items.empty()) {
            QLabel* noNewsLabel = new QLabel("No news available");
            noNewsLabel->setAlignment(Qt::AlignCenter);
            noNewsLabel->setStyleSheet("color: #6a6a8a;");
            m_impl->newsfeedLayout->insertWidget(0, noNewsLabel);
            return;
        }
        
        // Create news cards
        int index = 0;
        for (const auto& newsItem : items) {
            QFrame* card = new QFrame();
            card->setFrameShape(QFrame::StyledPanel);
            card->setStyleSheet(R"(
                QFrame {
                    background-color: #252542;
                    border: 1px solid #3a3a5c;
                    border-left: 3px solid #c9a227;
                    border-radius: 4px;
                    padding: 8px;
                }
                QFrame:hover {
                    background-color: #2d2d50;
                    border-color: #4a4a6c;
                    border-left-color: #e6c96a;
                }
            )");
            card->setCursor(Qt::PointingHandCursor);
            
            QVBoxLayout* cardLayout = new QVBoxLayout(card);
            cardLayout->setSpacing(6);
            cardLayout->setContentsMargins(12, 10, 12, 10);
            
            // Title
            QLabel* titleLabel = new QLabel(newsItem.title);
            titleLabel->setWordWrap(true);
            titleLabel->setStyleSheet("font-weight: bold; font-size: 13px; color: #c9a227;");
            cardLayout->addWidget(titleLabel);
            
            // Date
            QLabel* dateLabel = new QLabel(newsItem.publishedDateString());
            dateLabel->setStyleSheet("font-size: 11px; color: #6a6a8a; margin-bottom: 4px;");
            cardLayout->addWidget(dateLabel);
            
            // Description - store both full and truncated text
            QString fullDesc = newsItem.plainDescription();
            QString truncatedDesc = fullDesc;
            bool isTruncated = fullDesc.length() > 150;
            if (isTruncated) {
                truncatedDesc = fullDesc.left(147) + "...";
            }
            
            if (!fullDesc.isEmpty()) {
                QLabel* descLabel = new QLabel(truncatedDesc);
                descLabel->setWordWrap(true);
                descLabel->setStyleSheet("font-size: 12px; color: #b0b0c0;");
                descLabel->setObjectName("descLabel");
                cardLayout->addWidget(descLabel);
                
                // Store full description for expansion
                card->setProperty("fullDescription", fullDesc);
                card->setProperty("truncatedDescription", truncatedDesc);
                card->setProperty("isExpanded", false);
                card->setProperty("isTruncated", isTruncated);
            }
            
            // Store link for click handling
            QString link = newsItem.link;
            card->setProperty("link", link);
            card->installEventFilter(this);
            
            m_impl->newsfeedLayout->insertWidget(index++, card);
        }
        
        spdlog::info("Displayed {} news items", items.size());
    });
}

void MainWindow::loadSavedAccounts() {
    auto& config = ConfigManager::instance();
    auto accounts = config.getAccounts(m_impl->currentGameId.toStdString());
    
    QStringList usernames;
    for (const auto& account : accounts) {
        usernames << QString::fromStdString(account.username);
    }
    
    m_impl->loginWidget->setSavedAccounts(usernames);
    
    // Attempt to select the last added/modified account or just the first
    if (!accounts.empty()) {
        // Just load the first one by default if nothing else is selected
        // Ideally we'd track "lastActiveAccount" in AccountConfig or similar
        // For now, let's load the first one
        loadAccount(QString::fromStdString(accounts[0].username));
    }
}

void MainWindow::loadAccount(const QString& username) {
    auto& config = ConfigManager::instance();
    auto accounts = config.getAccounts(m_impl->currentGameId.toStdString());
    
    for (const auto& account : accounts) {
        if (account.username == username.toStdString()) {
            m_impl->loginWidget->setUsername(QString::fromStdString(account.username));
            m_impl->loginWidget->setAutoLoginEnabled(account.autoLogin);
            
            // Always try to get password from keyring (passwords are always saved)
            if (m_impl->credentialStore) {
                auto password = m_impl->credentialStore->getPassword(
                    LOTRO_CREDENTIAL_SERVICE,
                    account.username
                );
                
                if (password) {
                    m_impl->loginWidget->setPassword(QString::fromStdString(*password));
                } else {
                    m_impl->loginWidget->setPassword("");
                }
            }
            break;
        }
    }
}

void MainWindow::saveCurrentAccount() {
    AccountConfig account;
    account.username = m_impl->loginWidget->username().toStdString();
    account.autoLogin = m_impl->loginWidget->isAutoLoginEnabled();
    
    // Keep existing config options if possible (like display name, last world)
    auto& configMgr = ConfigManager::instance();
    auto existingAccounts = configMgr.getAccounts(m_impl->currentGameId.toStdString());
    
    // Default display name
    account.displayName = account.username;
    
    for (const auto& existing : existingAccounts) {
        if (existing.username == account.username) {
            account.displayName = existing.displayName;
            account.lastUsedWorld = existing.lastUsedWorld;
            account.worldPlayTimes = existing.worldPlayTimes;  // Preserve play times
            break;
        }
    }
    
    if (m_impl->worldSelector->currentIndex() >= 0 && !m_impl->worlds.empty()) {
        int index = m_impl->worldSelector->currentIndex();
        if (index >= 0 && index < static_cast<int>(m_impl->worlds.size())) {
            QString worldData = m_impl->worlds[index].name;
            if (!worldData.isEmpty()) {
                account.lastUsedWorld = worldData.toStdString();
                // Save current timestamp for this world
                int64_t now = QDateTime::currentSecsSinceEpoch();
                account.setWorldPlayTime(worldData.toStdString(), now);
                spdlog::info("Saving lastUsedWorld: '{}' (played at {}) for user: {}", 
                            account.lastUsedWorld, now, account.username);
            }
        }
    } else {
        spdlog::debug("Worlds not loaded yet, preserving existing lastUsedWorld: '{}'", account.lastUsedWorld);
    }
    
    configMgr.addAccount(m_impl->currentGameId.toStdString(), account);
    
    // Refresh list in case it's a new account
    auto accounts = configMgr.getAccounts(m_impl->currentGameId.toStdString());
    QStringList usernames;
    for (const auto& acc : accounts) {
        usernames << QString::fromStdString(acc.username);
    }
    m_impl->loginWidget->setSavedAccounts(usernames);
}

void MainWindow::autoLogin() {
    auto& config = ConfigManager::instance();
    auto accounts = config.getAccounts(m_impl->currentGameId.toStdString());
    
    for (const auto& account : accounts) {
        if (account.autoLogin) {
            // Found auto-login account. Load it and login.
            loadAccount(QString::fromStdString(account.username));
            
            // Login relies on UI state (username/password getters)
            // loadAccount sets the UI state, so calling login() immediately should work.
            if (!m_impl->loginWidget->username().isEmpty() && 
                !m_impl->loginWidget->password().isEmpty()) {
                login();
            }
            break;
        }
    }
}

void MainWindow::deleteAccount(const QString& username) {
    // Delete from credential store
    if (m_impl->credentialStore) {
        m_impl->credentialStore->deletePassword(
            LOTRO_CREDENTIAL_SERVICE,
            username.toStdString()
        );
    }
    
    // Delete from config
    auto& configMgr = ConfigManager::instance();
    configMgr.removeAccount(m_impl->currentGameId.toStdString(), username.toStdString());
    
    // Refresh UI
    loadSavedAccounts();
    m_impl->loginWidget->clear();
    
    spdlog::info("Deleted account: {}", username.toStdString());
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        QFrame* card = qobject_cast<QFrame*>(obj);
        if (card) {
            // Check if this card has expandable text
            bool isTruncated = card->property("isTruncated").toBool();
            bool isExpanded = card->property("isExpanded").toBool();
            
            if (isTruncated && !isExpanded) {
                // Expand the description
                QLabel* descLabel = card->findChild<QLabel*>("descLabel");
                if (descLabel) {
                    QString fullDesc = card->property("fullDescription").toString();
                    descLabel->setText(fullDesc);
                    card->setProperty("isExpanded", true);
                    return true;
                }
            } else if (isTruncated && isExpanded) {
                // Collapse the description back
                QLabel* descLabel = card->findChild<QLabel*>("descLabel");
                if (descLabel) {
                    QString truncatedDesc = card->property("truncatedDescription").toString();
                    descLabel->setText(truncatedDesc);
                    card->setProperty("isExpanded", false);
                    return true;
                }
            }
            
            // If not truncatable or some issue, open link
            QString link = card->property("link").toString();
            if (!link.isEmpty()) {
                QDesktopServices::openUrl(QUrl(link));
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Save state before closing
    auto& configManager = ConfigManager::instance();
    configManager.save();
    
    QMainWindow::closeEvent(event);
}

} // namespace lotro
