/**
 * LOTRO Launcher - Platform Implementation (Linux)
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef PLATFORM_LINUX

#include "Platform.hpp"

#include <cstdlib>
#include <fstream>
#include <regex>
#include <sys/resource.h>

#include <QDesktopServices>
#include <QStandardPaths>
#include <QUrl>

#include <spdlog/spdlog.h>

namespace lotro {

std::filesystem::path Platform::getConfigPath() {
    // Use XDG_CONFIG_HOME if set, otherwise ~/.config
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfig && xdgConfig[0] != '\0') {
        return std::filesystem::path(xdgConfig) / "lotro-launcher";
    }
    
    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / ".config" / "lotro-launcher";
    }
    
    return std::filesystem::path(".config") / "lotro-launcher";
}

std::filesystem::path Platform::getDataPath() {
    // Use XDG_DATA_HOME if set, otherwise ~/.local/share
    const char* xdgData = std::getenv("XDG_DATA_HOME");
    if (xdgData && xdgData[0] != '\0') {
        return std::filesystem::path(xdgData) / "lotro-launcher";
    }
    
    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / ".local" / "share" / "lotro-launcher";
    }
    
    return std::filesystem::path(".local/share") / "lotro-launcher";
}

std::filesystem::path Platform::getCachePath() {
    // Use XDG_CACHE_HOME if set, otherwise ~/.cache
    const char* xdgCache = std::getenv("XDG_CACHE_HOME");
    if (xdgCache && xdgCache[0] != '\0') {
        return std::filesystem::path(xdgCache) / "lotro-launcher";
    }
    
    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / ".cache" / "lotro-launcher";
    }
    
    return std::filesystem::path(".cache") / "lotro-launcher";
}

std::filesystem::path Platform::getDocumentsPath() {
    QString docsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return std::filesystem::path(docsPath.toStdString());
}

namespace {

/**
 * Parse Steam's libraryfolders.vdf to find all Steam library paths
 */
std::vector<std::filesystem::path> getSteamLibraryPaths() {
    std::vector<std::filesystem::path> libraries;
    
    const char* home = std::getenv("HOME");
    if (!home) return libraries;
    
    std::filesystem::path homePath(home);
    
    // Possible locations for libraryfolders.vdf
    std::vector<std::filesystem::path> vdfPaths = {
        homePath / ".steam/steam/steamapps/libraryfolders.vdf",
        homePath / ".local/share/Steam/steamapps/libraryfolders.vdf",
        homePath / ".steam/steamapps/libraryfolders.vdf",
        // Flatpak Steam
        homePath / ".var/app/com.valvesoftware.Steam/.steam/steam/steamapps/libraryfolders.vdf",
    };
    
    for (const auto& vdfPath : vdfPaths) {
        if (!std::filesystem::exists(vdfPath)) {
            continue;
        }
        
        try {
            std::ifstream file(vdfPath);
            std::string content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
            
            // Simple VDF parser - look for "path" entries
            // Format: "path"		"/path/to/library"
            std::regex pathRegex("\"path\"\\s+\"([^\"]+)\"");
            std::smatch match;
            std::string::const_iterator searchStart(content.cbegin());
            
            while (std::regex_search(searchStart, content.cend(), match, pathRegex)) {
                std::filesystem::path libPath = match[1].str();
                if (std::filesystem::exists(libPath / "steamapps")) {
                    libraries.push_back(libPath);
                    spdlog::debug("Found Steam library: {}", libPath.string());
                }
                searchStart = match.suffix().first;
            }
        } catch (const std::exception& e) {
            spdlog::warn("Error parsing {}: {}", vdfPath.string(), e.what());
        }
        
        // Only need to find one working vdf file
        if (!libraries.empty()) {
            break;
        }
    }
    
    // If no libraries found from VDF, add default paths
    if (libraries.empty()) {
        auto defaultSteam = homePath / ".steam/steam";
        if (std::filesystem::exists(defaultSteam / "steamapps")) {
            libraries.push_back(defaultSteam);
        }
        auto localSteam = homePath / ".local/share/Steam";
        if (std::filesystem::exists(localSteam / "steamapps")) {
            libraries.push_back(localSteam);
        }
    }
    
    return libraries;
}

/**
 * Check if a directory contains a valid LOTRO or DDO installation
 */
bool isValidGameInstall(const std::filesystem::path& path) {
    // Check for LOTRO
    if (std::filesystem::exists(path / "LotroLauncher.exe") ||
        std::filesystem::exists(path / "lotrolauncher.exe")) {
        return true;
    }
    // Check for DDO
    if (std::filesystem::exists(path / "DNDLauncher.exe") ||
        std::filesystem::exists(path / "dndlauncher.exe")) {
        return true;
    }
    return false;
}

/**
 * Recursively search for game installations
 */
void findGameDirsRecursive(
    const std::filesystem::path& searchDir,
    std::vector<std::filesystem::path>& results,
    int maxDepth = 5
) {
    if (maxDepth <= 0) return;
    
    try {
        if (!std::filesystem::exists(searchDir) || !std::filesystem::is_directory(searchDir)) {
            return;
        }
        
        // Skip hidden directories and common non-game directories
        std::string dirName = searchDir.filename().string();
        if (dirName.empty() || dirName[0] == '.' || 
            dirName == "dosdevices" || 
            dirName == "BACKUP" || dirName == "backup") {
            return;
        }
        
        // Check if this directory is a game install
        if (isValidGameInstall(searchDir)) {
            results.push_back(searchDir);
            return; // Don't recurse into game directories
        }
        
        // Recurse into subdirectories
        for (const auto& entry : std::filesystem::directory_iterator(searchDir)) {
            if (entry.is_directory()) {
                findGameDirsRecursive(entry.path(), results, maxDepth - 1);
            }
        }
    } catch (const std::exception& e) {
        // Permission denied or other errors - skip
        spdlog::debug("Error searching {}: {}", searchDir.string(), e.what());
    }
}

} // anonymous namespace

std::vector<std::filesystem::path> Platform::detectGameInstallations() {
    std::vector<std::filesystem::path> installations;
    
    const char* home = std::getenv("HOME");
    if (!home) {
        spdlog::warn("HOME environment variable not set");
        return installations;
    }
    
    std::filesystem::path homePath(home);
    spdlog::info("Searching for game installations...");
    
    // 1. Search Steam library locations
    auto steamLibraries = getSteamLibraryPaths();
    for (const auto& lib : steamLibraries) {
        auto commonPath = lib / "steamapps/common";
        
        // Check known game folder names
        std::vector<std::string> gameNames = {
            "Lord of the Rings Online",
            "Dungeons & Dragons Online",
            "DDO",
            "LOTRO"
        };
        
        for (const auto& name : gameNames) {
            auto gamePath = commonPath / name;
            if (isValidGameInstall(gamePath)) {
                installations.push_back(gamePath);
                spdlog::info("Found game in Steam library: {}", gamePath.string());
            }
        }
        
        // Also search compatdata for Proton prefixes
        auto compatData = lib / "steamapps/compatdata";
        if (std::filesystem::exists(compatData)) {
            for (const auto& entry : std::filesystem::directory_iterator(compatData)) {
                if (!entry.is_directory()) continue;
                
                // Check both pfx/drive_c and drive_c paths
                std::vector<std::filesystem::path> prefixPaths = {
                    entry.path() / "pfx/drive_c/Program Files/Standing Stone Games",
                    entry.path() / "pfx/drive_c/Program Files (x86)/Standing Stone Games",
                    entry.path() / "drive_c/Program Files/Standing Stone Games",
                    entry.path() / "drive_c/Program Files (x86)/Standing Stone Games",
                };
                
                for (const auto& prefix : prefixPaths) {
                    findGameDirsRecursive(prefix, installations, 3);
                }
            }
        }
    }
    
    // 2. Search Wine prefixes
    std::vector<std::filesystem::path> winePrefixRoots = {
        homePath / ".wine",
        homePath / ".wine32",
        homePath / ".wine64",
    };
    
    // Also search for any directory containing "wine" in home
    try {
        for (const auto& entry : std::filesystem::directory_iterator(homePath)) {
            if (entry.is_directory()) {
                std::string name = entry.path().filename().string();
                if (name.find("wine") != std::string::npos || 
                    name.find("Wine") != std::string::npos) {
                    winePrefixRoots.push_back(entry.path());
                }
            }
        }
    } catch (...) {}
    
    for (const auto& prefix : winePrefixRoots) {
        std::vector<std::filesystem::path> searchPaths = {
            prefix / "drive_c/Program Files/Standing Stone Games",
            prefix / "drive_c/Program Files (x86)/Standing Stone Games",
            prefix / "drive_c/Program Files/Turbine",
            prefix / "drive_c/Program Files (x86)/Turbine",
        };
        
        for (const auto& path : searchPaths) {
            findGameDirsRecursive(path, installations, 3);
        }
    }
    
    // 3. Search ~/games
    auto gamesDir = homePath / "games";
    if (std::filesystem::exists(gamesDir)) {
        findGameDirsRecursive(gamesDir, installations, 5);
    }
    
    // 4. Search ~/Games (case variation)
    auto GamesDir = homePath / "Games";
    if (std::filesystem::exists(GamesDir)) {
        findGameDirsRecursive(GamesDir, installations, 5);
    }
    
    // Remove duplicates (preserve order)
    std::vector<std::filesystem::path> unique;
    for (const auto& path : installations) {
        auto canonical = std::filesystem::canonical(path);
        bool found = false;
        for (const auto& existing : unique) {
            if (std::filesystem::canonical(existing) == canonical) {
                found = true;
                break;
            }
        }
        if (!found) {
            unique.push_back(path);
        }
    }
    
    spdlog::info("Found {} game installation(s)", unique.size());
    return unique;
}

std::filesystem::path Platform::getDefaultLotroSettingsPath() {
    return getDocumentsPath() / "The Lord of the Rings Online";
}

std::optional<size_t> Platform::getOpenFileLimit() {
    struct rlimit limit;
    if (getrlimit(RLIMIT_NOFILE, &limit) == 0) {
        return static_cast<size_t>(limit.rlim_cur);
    }
    return std::nullopt;
}

bool Platform::openUrl(const std::string& url) {
    return QDesktopServices::openUrl(QUrl(QString::fromStdString(url)));
}

bool Platform::openFileManager(const std::filesystem::path& path) {
    return QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(path.string())));
}

} // namespace lotro

#endif // PLATFORM_LINUX
