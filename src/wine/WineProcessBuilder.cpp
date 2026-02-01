/**
 * LOTRO Launcher - Wine Process Builder Implementation (Linux only)
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef PLATFORM_LINUX

#include "WineProcessBuilder.hpp"

#include <spdlog/spdlog.h>

namespace lotro {

WineProcessBuilder& WineProcessBuilder::setWineExecutable(const std::filesystem::path& path) {
    m_wineExecutable = path;
    return *this;
}

WineProcessBuilder& WineProcessBuilder::setPrefix(const std::filesystem::path& path) {
    m_prefix = path;
    return *this;
}

WineProcessBuilder& WineProcessBuilder::setExecutable(const std::filesystem::path& path) {
    m_executable = path;
    return *this;
}

WineProcessBuilder& WineProcessBuilder::addArgument(const QString& arg) {
    m_args.append(arg);
    return *this;
}

WineProcessBuilder& WineProcessBuilder::addArguments(const QStringList& args) {
    m_args.append(args);
    return *this;
}

WineProcessBuilder& WineProcessBuilder::setWorkingDirectory(const std::filesystem::path& path) {
    m_workingDirectory = path;
    return *this;
}

WineProcessBuilder& WineProcessBuilder::setEsync(bool enabled) {
    m_esyncEnabled = enabled;
    return *this;
}

WineProcessBuilder& WineProcessBuilder::setFsync(bool enabled) {
    m_fsyncEnabled = enabled;
    return *this;
}

WineProcessBuilder& WineProcessBuilder::setDebugLevel(const std::string& level) {
    m_debugLevel = level;
    return *this;
}

WineProcessBuilder& WineProcessBuilder::setEnvironment(const std::string& name, const std::string& value) {
    m_customEnv.emplace_back(name, value);
    return *this;
}

WineProcessBuilder& WineProcessBuilder::setDxvkHud(const std::string& config) {
    m_dxvkHud = config;
    return *this;
}

QStringList WineProcessBuilder::buildCommandLine() const {
    QStringList args;
    
    // Wine executable
    args << QString::fromStdString(m_wineExecutable.string());
    
    // Windows executable
    args << QString::fromStdString(m_executable.string());
    
    // Add passed arguments
    args.append(m_args);
    
    return args;
}

QProcessEnvironment WineProcessBuilder::buildEnvironment() const {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    
    // WINEPREFIX
    if (!m_prefix.empty()) {
        env.insert("WINEPREFIX", QString::fromStdString(m_prefix.string()));
    }
    
    // WINEDEBUG
    if (!m_debugLevel.empty()) {
        env.insert("WINEDEBUG", QString::fromStdString(m_debugLevel));
    }
    
    // Esync
    if (m_esyncEnabled) {
        env.insert("WINEESYNC", "1");
        env.insert("PROTON_NO_ESYNC", "0");
    } else {
        env.insert("WINEESYNC", "0");
        env.insert("PROTON_NO_ESYNC", "1");
    }
    
    // Fsync
    if (m_fsyncEnabled) {
        env.insert("WINEFSYNC", "1");
        env.insert("PROTON_NO_FSYNC", "0");
    } else {
        env.insert("WINEFSYNC", "0");
        env.insert("PROTON_NO_FSYNC", "1");
    }
    
    // DXVK HUD
    if (!m_dxvkHud.empty()) {
        env.insert("DXVK_HUD", QString::fromStdString(m_dxvkHud));
    }
    
    // Large address aware for 32-bit games
    env.insert("WINE_LARGE_ADDRESS_AWARE", "1");
    
    // WINEDLLOVERRIDES
    // Match OneLauncher's configuration
    QStringList dllOverrides;
    dllOverrides << "winemenubuilder.exe=d" << "mscoree=d" << "mshtml=d";
    
    // Add DXVK overrides if enabled (umu/Proton usually handles this but OneLauncher sets them explicitly)
    dllOverrides << "d3d11=n" << "dxgi=n" << "d3d10core=n" << "d3d9=n";
    
    env.insert("WINEDLLOVERRIDES", dllOverrides.join(";"));
    
    // Custom environment variables (includes GAMEID, PROTONPATH for umu)
    for (const auto& [name, value] : m_customEnv) {
        env.insert(QString::fromStdString(name), QString::fromStdString(value));
    }
    
    return env;
}

QString WineProcessBuilder::workingDirectory() const {
    if (m_workingDirectory.empty()) {
        // Default to executable's directory
        return QString::fromStdString(m_executable.parent_path().string());
    }
    return QString::fromStdString(m_workingDirectory.string());
}

} // namespace lotro

#endif // PLATFORM_LINUX
