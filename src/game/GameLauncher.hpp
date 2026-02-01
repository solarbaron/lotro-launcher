/**
 * LOTRO Launcher - Game Launcher
 * 
 * Main game launch logic.
 * Based on OneLauncher's start_game.py
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include <QProcess>
#include <QString>

#include "core/config/GameConfig.hpp"
#include "network/WorldList.hpp"

namespace lotro {

/**
 * Launch result
 */
struct LaunchResult {
    bool success = false;
    QString errorMessage;
    qint64 processId = 0;  // PID of launched game
};

/**
 * Game launch callback
 */
using LaunchCallback = std::function<void(const LaunchResult&)>;

/**
 * Game launcher
 * 
 * Handles the complete game launch process:
 * 1. Build launch arguments
 * 2. Set up environment (Wine on Linux)
 * 3. Run startup scripts
 * 4. Launch the game client
 */
class GameLauncher {
public:
    /**
     * Create launcher for the specified game
     */
    explicit GameLauncher(const GameConfig& gameConfig);
    ~GameLauncher();
    
    /**
     * Launch the game
     * 
     * @param world World to connect to
     * @param ticket Session ticket from login
     * @param accountNumber Account number/subscription
     * @param loginServer Login server URL
     * @param callback Called when launch completes/fails
     */
    void launch(
        const World& world,
        const QString& ticket,
        const QString& accountNumber,
        const QString& loginServer,
        LaunchCallback callback = nullptr
    );
    
    /**
     * Check if launch is in progress
     */
    bool isLaunching() const;
    
    /**
     * Get the game process (if running)
     */
    QProcess* process() const;
    
    /**
     * Set whether to run startup scripts
     */
    void setRunStartupScripts(bool enabled);
    
    /**
     * Set whether to update UserPreferences.ini
     */
    void setUpdateUserPreferences(bool enabled);
    
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace lotro
