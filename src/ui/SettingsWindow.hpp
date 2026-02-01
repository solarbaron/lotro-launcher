/**
 * LOTRO Launcher - Settings Window
 * 
 * Application and game settings UI.
 * Based on OneLauncher's settings_window.py
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>

#include <QDialog>
#include <QString>

#include "core/config/GameConfig.hpp"

namespace lotro {

/**
 * Settings window
 * 
 * Provides UI for configuring:
 * - Game installation path
 * - Client type (64-bit/32-bit)
 * - Language/locale
 * - Wine settings (Linux)
 * - Account management
 * - General launcher options
 */
class SettingsWindow : public QDialog {
    Q_OBJECT
    
public:
    /**
     * Create settings window for a specific game
     * 
     * @param gameId Current game ID
     * @param parent Parent widget
     */
    explicit SettingsWindow(const QString& gameId, QWidget* parent = nullptr);
    ~SettingsWindow() override;
    
signals:
    /**
     * Emitted when settings have been changed and saved
     */
    void settingsChanged();
    
    /**
     * Emitted when game path has changed
     */
    void gamePathChanged(const QString& newPath);
    
public slots:
    /**
     * Browse for game installation directory
     */
    void browseGameDirectory();
    
    /**
     * Browse for custom settings directory
     */
    void browseSettingsDirectory();
    
#ifdef PLATFORM_LINUX
    /**
     * Browse for custom Wine executable
     */
    void browseWineExecutable();
    
    /**
     * Browse for custom Wine prefix
     */
    void browseWinePrefix();
    
    /**
     * Reset Wine to built-in management
     */
    void resetWineToBuiltin();
#endif
    
    /**
     * Apply settings without closing
     */
    void apply();
    
    /**
     * Reset settings to defaults
     */
    void resetToDefaults();

private:
    void setupUi();
    void loadSettings();
    void saveSettings();
#ifdef PLATFORM_LINUX
    void updateWineSection();
#endif
    
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace lotro
