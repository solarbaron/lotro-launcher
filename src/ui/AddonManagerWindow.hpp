/**
 * LOTRO Launcher - Addon Manager Window
 * 
 * UI for managing plugins, skins, and music addons.
 * Based on OneLauncher's addon_manager_window.py
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>

#include <QDialog>
#include <QString>
#include <QTableWidget>
#include <QListWidget>

#include "addons/AddonManager.hpp"

namespace lotro {

/**
 * Addon manager window
 * 
 * Provides UI for:
 * - Viewing installed addons
 * - Installing new addons from lotrointerface.com
 * - Installing addons from local files
 * - Updating addons
 * - Removing addons
 * - Managing startup scripts
 */
class AddonManagerWindow : public QDialog {
    Q_OBJECT
    
public:
    /**
     * Create addon manager for a specific game
     * 
     * @param gameId Current game ID
     * @param parent Parent widget
     */
    explicit AddonManagerWindow(const QString& gameId, QWidget* parent = nullptr);
    ~AddonManagerWindow() override;
    
signals:
    /**
     * Emitted when addon installation starts
     */
    void addonInstallStarted(const QString& addonName);
    
    /**
     * Emitted when addon installation completes
     */
    void addonInstallCompleted(const QString& addonName, bool success);
    
public slots:
    /**
     * Switch to plugins tab
     */
    void showPlugins();
    
    /**
     * Switch to skins tab
     */
    void showSkins();
    
    /**
     * Switch to music tab
     */
    void showMusic();
    
    /**
     * Refresh the addon list
     */
    void refresh();
    
    /**
     * Install selected addon
     */
    void installSelected();
    
    /**
     * Install addon from local file
     */
    void installFromFile();
    
    /**
     * Update selected addon
     */
    void updateSelected();
    
    /**
     * Update all addons with available updates
     */
    void updateAll();
    
    /**
     * Remove selected addon
     */
    void removeSelected();
    
    /**
     * Open selected addon on lotrointerface.com
     */
    void openAddonPage();
    
    /**
     * Search for addons
     */
    void search(const QString& query);
    
private:
    void setupUi();
    void setupConnections();
    void loadInstalledAddons(AddonType type);
    void loadRemoteAddons(AddonType type);
    void updateAddonList();
    void updateButtonStates();
    QTableWidget* createAddonTable();
    QListWidget* createAddonGrid();
    QWidget* createAddonTypeTab(AddonType type);
    
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace lotro
