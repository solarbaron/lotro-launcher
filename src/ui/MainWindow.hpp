/**
 * LOTRO Launcher - Main Window
 * 
 * Primary application window.
 * Based on OneLauncher's main_window.py
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>

#include <QMainWindow>
#include <QString>

#include "core/config/ConfigManager.hpp"
#include "network/WorldList.hpp"

namespace lotro {

/**
 * Main application window
 * 
 * Contains:
 * - Login/account selection
 * - World/server selection
 * - News feed display
 * - Launch button
 * - Access to settings and addon manager
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    
    /**
     * Set the current game
     */
    void setGame(const QString& gameId);
    
    /**
     * Get the current game ID
     */
    QString currentGame() const;
    
signals:
    /**
     * Emitted when game launch is requested
     */
    void launchRequested(const World& world);
    
    /**
     * Emitted when game is started
     */
    void gameStarted(qint64 processId);
    
    /**
     * Emitted when game has exited
     */
    void gameExited(int exitCode);
    
    /**
     * Emitted when user requests to switch games
     */
    void gameSwitchRequested(const QString& gameId);
    
    /**
     * Emitted when settings have changed
     */
    void settingsChanged();
    
public slots:
    /**
     * Attempt to log in with current credentials
     */
    void login();
    
    /**
     * Launch the game with selected world
     */
    void launchGame();
    
    /**
     * Open the settings window
     */
    void openSettings();
    
    /**
     * Open the addon manager
     */
    void openAddonManager();
    
    /**
     * Open the character tracker
     */
    void openCharacterTracker();
    
    /**
     * Force refresh of news feed
     */
    void refreshNewsfeed();
    
    /**
     * Force refresh of world list
     */
    void refreshWorldList();
    
protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    
private slots:
    void onLoginComplete();
    void onLoginFailed(const QString& error);
    void onWorldsLoaded();
    
private:
    void setupUi();
    void setupConnections();
    void loadSavedAccounts();
    void loadAccount(const QString& username);
    void saveCurrentAccount();
    void autoLogin();
    void updateWorldList(const std::vector<World>& worlds);
    void updateNewsfeed();
    void deleteAccount(const QString& username);
    
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace lotro
