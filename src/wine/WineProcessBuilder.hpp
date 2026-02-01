/**
 * LOTRO Launcher - Wine Process Builder (Linux only)
 * 
 * Constructs Wine command lines and environment for game execution.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifdef PLATFORM_LINUX

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <QProcessEnvironment>
#include <QStringList>

#include "core/config/WineConfig.hpp"

namespace lotro {

/**
 * Wine process builder
 * 
 * Constructs command line and environment for running
 * Windows executables through Wine.
 * 
 * Based on OneLauncher's get_wine_process_args function.
 */
class WineProcessBuilder {
public:
    WineProcessBuilder() = default;
    
    /**
     * Set the Wine executable path
     */
    WineProcessBuilder& setWineExecutable(const std::filesystem::path& path);
    
    /**
     * Set the WINEPREFIX path
     */
    WineProcessBuilder& setPrefix(const std::filesystem::path& path);
    
    /**
     * Set the Windows executable to run
     */
    WineProcessBuilder& setExecutable(const std::filesystem::path& path);
    
    /**
     * Add an argument to pass to the Windows executable
     */
    WineProcessBuilder& addArgument(const QString& arg);
    
    /**
     * Add multiple arguments
     */
    WineProcessBuilder& addArguments(const QStringList& args);
    
    /**
     * Set the working directory
     */
    WineProcessBuilder& setWorkingDirectory(const std::filesystem::path& path);
    
    /**
     * Enable/disable esync
     */
    WineProcessBuilder& setEsync(bool enabled);
    
    /**
     * Enable/disable fsync
     */
    WineProcessBuilder& setFsync(bool enabled);
    
    /**
     * Set WINEDEBUG level
     */
    WineProcessBuilder& setDebugLevel(const std::string& level);
    
    /**
     * Add a custom environment variable
     */
    WineProcessBuilder& setEnvironment(const std::string& name, const std::string& value);
    
    /**
     * Set DXVK HUD configuration
     * 
     * @param config HUD config string (e.g., "fps,memory")
     */
    WineProcessBuilder& setDxvkHud(const std::string& config);
    
    /**
     * Build the command line arguments for QProcess
     * 
     * The first element is the Wine executable, followed by
     * the Windows executable and its arguments.
     */
    QStringList buildCommandLine() const;
    
    /**
     * Build the process environment
     */
    QProcessEnvironment buildEnvironment() const;
    
    /**
     * Get the working directory (or empty if not set)
     */
    QString workingDirectory() const;
    
private:
    std::filesystem::path m_wineExecutable;
    std::filesystem::path m_prefix;
    std::filesystem::path m_executable;
    std::filesystem::path m_workingDirectory;
    QStringList m_args;
    
    bool m_esyncEnabled = true;
    bool m_fsyncEnabled = true;
    std::string m_debugLevel = "-all";
    std::string m_dxvkHud;
    
    std::vector<std::pair<std::string, std::string>> m_customEnv;
};

} // namespace lotro

#endif // PLATFORM_LINUX
