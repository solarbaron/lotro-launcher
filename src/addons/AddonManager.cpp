/**
 * LOTRO Launcher - Addon Manager Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "AddonManager.hpp"
#include "CompendiumParser.hpp"
#include "network/LotroInterfaceClient.hpp"

#include <QDir>
#include <QProcess>
#include <QtConcurrent>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>

namespace lotro {

namespace {

// Invalid folder names that should be stripped from addon archives
// (OneLauncher-style behavior)
const std::vector<std::string> INVALID_FOLDER_NAMES = {
    "ui", "skins", "plugins", "music",
    "my documents", "documents",
    "the lord of the rings online",
    "dungeons and dragons online",
    "dungeons & dragons online"
};

// Case-insensitive string comparison
bool iequals(const std::string& a, const std::string& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
        [](char a, char b) { return std::tolower(a) == std::tolower(b); });
}

// Check if a folder name is an invalid wrapper folder
bool isInvalidFolderName(const std::string& name) {
    for (const auto& invalid : INVALID_FOLDER_NAMES) {
        if (iequals(name, invalid)) {
            return true;
        }
    }
    return false;
}

// Clean invalid wrapper folders from extracted addon
// This mimics OneLauncher's clean_temp_addon_folder behavior
void cleanTempAddonFolder(const std::filesystem::path& addonDir) {
    while (true) {
        std::filesystem::path invalidDir;
        
        for (const auto& entry : std::filesystem::directory_iterator(addonDir)) {
            if (entry.is_directory() && 
                isInvalidFolderName(entry.path().filename().string())) {
                invalidDir = entry.path();
                break;
            }
        }
        
        if (invalidDir.empty()) {
            break;
        }
        
        spdlog::info("Removing invalid wrapper folder: {}", invalidDir.filename().string());
        
        // Move contents of invalid folder up one level
        for (const auto& child : std::filesystem::directory_iterator(invalidDir)) {
            auto dest = addonDir / child.path().filename();
            try {
                std::filesystem::rename(child.path(), dest);
            } catch (const std::exception& e) {
                // If rename fails (cross-device), copy and delete
                try {
                    if (child.is_directory()) {
                        std::filesystem::copy(child.path(), dest, 
                            std::filesystem::copy_options::recursive | 
                            std::filesystem::copy_options::overwrite_existing);
                    } else {
                        std::filesystem::copy_file(child.path(), dest,
                            std::filesystem::copy_options::overwrite_existing);
                    }
                } catch (const std::exception& e2) {
                    spdlog::warn("Failed to move {}: {}", child.path().string(), e2.what());
                }
            }
        }
        
        // Remove the now-empty invalid folder
        try {
            std::filesystem::remove_all(invalidDir);
        } catch (const std::exception& e) {
            spdlog::warn("Failed to remove invalid folder: {}", e.what());
        }
    }
}

// Fix improper root directory for music/skins
// If the extraction contains loose files instead of a single folder,
// wrap them in a folder with the addon name (like OneLauncher does)
std::filesystem::path fixImproperRootDir(
    const std::filesystem::path& tempDir,
    const std::string& addonName
) {
    std::vector<std::filesystem::path> entries;
    int fileCount = 0;
    int dirCount = 0;
    
    for (const auto& entry : std::filesystem::directory_iterator(tempDir)) {
        entries.push_back(entry.path());
        if (entry.is_directory()) {
            dirCount++;
        } else {
            fileCount++;
        }
    }
    
    // If there's exactly one directory and nothing else, use it as root
    if (dirCount == 1 && fileCount == 0) {
        return entries[0];
    }
    
    // If there are loose files, wrap everything in a folder with addon name
    if (fileCount > 0 || dirCount > 1 || dirCount == 0) {
        auto newRootDir = tempDir / addonName;
        std::filesystem::create_directories(newRootDir);
        
        for (const auto& entry : entries) {
            auto dest = newRootDir / entry.filename();
            try {
                std::filesystem::rename(entry, dest);
            } catch (const std::exception& e) {
                // If rename fails, copy and delete
                try {
                    if (std::filesystem::is_directory(entry)) {
                        std::filesystem::copy(entry, dest,
                            std::filesystem::copy_options::recursive);
                        std::filesystem::remove_all(entry);
                    } else {
                        std::filesystem::copy_file(entry, dest);
                        std::filesystem::remove(entry);
                    }
                } catch (const std::exception& e2) {
                    spdlog::warn("Failed to move {}: {}", entry.string(), e2.what());
                }
            }
        }
        
        spdlog::info("Wrapped loose files in folder: {}", addonName);
        return newRootDir;
    }
    
    return tempDir;
}

} // anonymous namespace

class AddonManager::Impl {
public:
    std::filesystem::path settingsDir;
    
    std::vector<AddonInfo> installedPlugins;
    std::vector<AddonInfo> installedSkins;
    std::vector<AddonInfo> installedMusic;
    
    // Remote addon cache
    std::vector<RemoteAddonInfo> remotePlugins;
    std::vector<RemoteAddonInfo> remoteSkins;
    std::vector<RemoteAddonInfo> remoteMusic;
    
    LotroInterfaceClient client;
};

AddonManager::AddonManager(const std::filesystem::path& settingsDir)
    : m_impl(std::make_unique<Impl>())
{
    m_impl->settingsDir = settingsDir;
    
    spdlog::info("=== AddonManager initialized ===");
    spdlog::info("Settings directory: {}", settingsDir.string());
    spdlog::info("Plugin directory: {}", getAddonDirectory(AddonType::Plugin).string());
    spdlog::info("Skins directory: {}", getAddonDirectory(AddonType::Skin).string());
    spdlog::info("Music directory: {}", getAddonDirectory(AddonType::Music).string());
    
    // Create addon directories if they don't exist
    std::filesystem::create_directories(getAddonDirectory(AddonType::Plugin));
    std::filesystem::create_directories(getAddonDirectory(AddonType::Skin));
    std::filesystem::create_directories(getAddonDirectory(AddonType::Music));
    
    refreshInstalledAddons();
}

AddonManager::~AddonManager() = default;

std::filesystem::path AddonManager::getAddonDirectory(AddonType type) const {
    switch (type) {
        case AddonType::Plugin:
            return m_impl->settingsDir / "Plugins";
        case AddonType::Skin:
            return m_impl->settingsDir / "ui" / "skins";
        case AddonType::Music:
            return m_impl->settingsDir / "Music";
    }
    return m_impl->settingsDir;
}

std::vector<AddonInfo> AddonManager::getInstalledAddons(AddonType type) const {
    switch (type) {
        case AddonType::Plugin: return m_impl->installedPlugins;
        case AddonType::Skin:   return m_impl->installedSkins;
        case AddonType::Music:  return m_impl->installedMusic;
    }
    return {};
}

std::optional<AddonInfo> AddonManager::getInstalledAddon(
    const QString& id, AddonType type
) const {
    const auto& addons = getInstalledAddons(type);
    for (const auto& addon : addons) {
        if (addon.id == id) {
            return addon;
        }
    }
    return std::nullopt;
}

void AddonManager::refreshInstalledAddons() {
    m_impl->installedPlugins.clear();
    m_impl->installedSkins.clear();
    m_impl->installedMusic.clear();
    
    // Scan for compendium files
    auto pluginFiles = CompendiumParser::findCompendiumFiles(
        getAddonDirectory(AddonType::Plugin));
    for (const auto& file : pluginFiles) {
        if (auto addon = CompendiumParser::parse(file)) {
            addon->status = AddonStatus::Installed;
            addon->compendiumFilePath = file;
            m_impl->installedPlugins.push_back(*addon);
        }
    }
    
    auto skinFiles = CompendiumParser::findCompendiumFiles(
        getAddonDirectory(AddonType::Skin));
    for (const auto& file : skinFiles) {
        if (auto addon = CompendiumParser::parse(file)) {
            addon->status = AddonStatus::Installed;
            addon->compendiumFilePath = file;
            m_impl->installedSkins.push_back(*addon);
        }
    }
    
    // Music: scan for compendium files
    auto musicFiles = CompendiumParser::findCompendiumFiles(
        getAddonDirectory(AddonType::Music));
    for (const auto& file : musicFiles) {
        if (auto addon = CompendiumParser::parse(file)) {
            addon->status = AddonStatus::Installed;
            addon->compendiumFilePath = file;
            m_impl->installedMusic.push_back(*addon);
        }
    }
    
    // Also scan for music folders without compendium files (like OneLauncher does)
    auto musicDir = getAddonDirectory(AddonType::Music);
    if (std::filesystem::exists(musicDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(musicDir)) {
            if (entry.is_directory()) {
                // Check if this folder already has a compendium file (already added)
                bool hasCompendium = false;
                for (const auto& addon : m_impl->installedMusic) {
                    if (addon.compendiumFilePath.parent_path() == entry.path()) {
                        hasCompendium = true;
                        break;
                    }
                }
                
                if (!hasCompendium) {
                    // Add as unmanaged music folder
                    AddonInfo addon;
                    addon.id = QString::fromStdString(entry.path().filename().string());
                    addon.name = QString::fromStdString(entry.path().filename().string());
                    addon.type = AddonType::Music;
                    addon.status = AddonStatus::Installed;
                    addon.compendiumFilePath = entry.path();  // Use folder path
                    addon.category = "Unmanaged";
                    m_impl->installedMusic.push_back(addon);
                }
            }
        }
    }
    
    spdlog::info("Found {} plugins, {} skins, {} music addons",
                m_impl->installedPlugins.size(),
                m_impl->installedSkins.size(),
                m_impl->installedMusic.size());
}
bool AddonManager::installFromFile(
    const std::filesystem::path& zipPath,
    AddonType type
) {
    auto destDir = getAddonDirectory(type);
    
    spdlog::info("Installing addon from: {}", zipPath.string());
    
    // Extract zip to destination
    // Using unzip command for simplicity
    QProcess process;
    QStringList args;
    args << "-o"  // Overwrite
         << QString::fromStdString(zipPath.string())
         << "-d"
         << QString::fromStdString(destDir.string());
    
    process.start("unzip", args);
    
    if (!process.waitForFinished(60000)) {
        spdlog::error("Extraction timed out");
        return false;
    }
    
    if (process.exitCode() != 0) {
        spdlog::error("Extraction failed: {}", 
                     process.readAllStandardError().toStdString());
        return false;
    }
    
    refreshInstalledAddons();
    spdlog::info("Addon installed successfully");
    return true;
}

bool AddonManager::removeAddon(const QString& id, AddonType type) {
    auto addon = getInstalledAddon(id, type);
    if (!addon) {
        spdlog::warn("Addon not found: {}", id.toStdString());
        return false;
    }
    
    bool removed = false;
    
    // First, try to use the compendiumFilePath (works for all addon types)
    if (!addon->compendiumFilePath.empty() && std::filesystem::exists(addon->compendiumFilePath)) {
        std::filesystem::path dirToRemove;
        
        if (std::filesystem::is_directory(addon->compendiumFilePath)) {
            // compendiumFilePath is a folder (for unmanaged music)
            dirToRemove = addon->compendiumFilePath;
        } else {
            // compendiumFilePath is a file - remove its parent directory
            dirToRemove = addon->compendiumFilePath.parent_path();
        }
        
        if (std::filesystem::exists(dirToRemove)) {
            try {
                std::filesystem::remove_all(dirToRemove);
                spdlog::info("Removed addon directory: {}", dirToRemove.string());
                removed = true;
            } catch (const std::exception& e) {
                spdlog::error("Failed to remove directory {}: {}", dirToRemove.string(), e.what());
            }
        }
    }
    
    // For plugins, also try using descriptors if compendiumFilePath didn't work
    if (!removed && !addon->descriptors.empty()) {
        auto addonDir = getAddonDirectory(type);
        
        for (const auto& descriptor : addon->descriptors) {
            auto descPath = addonDir / descriptor.toStdString();
            auto parentDir = std::filesystem::path(descPath).parent_path();
            
            if (std::filesystem::exists(parentDir)) {
                try {
                    std::filesystem::remove_all(parentDir);
                    spdlog::info("Removed addon directory: {}", parentDir.string());
                    removed = true;
                } catch (const std::exception& e) {
                    spdlog::error("Failed to remove directory: {}", e.what());
                }
            }
        }
    }
    
    if (!removed) {
        spdlog::warn("Could not find directory to remove for addon: {}", id.toStdString());
    }
    
    refreshInstalledAddons();
    return removed;
}

QFuture<std::vector<AddonInfo>> AddonManager::searchRemoteAddons(
    const QString& query,
    AddonType type
) {
    // TODO: Implement lotrointerface.com search
    return QtConcurrent::run([]() -> std::vector<AddonInfo> {
        return {};
    });
}

QFuture<std::optional<AddonInfo>> AddonManager::getRemoteAddon(
    const QString& id,
    AddonType type
) {
    // TODO: Implement lotrointerface.com lookup
    return QtConcurrent::run([]() -> std::optional<AddonInfo> {
        return std::nullopt;
    });
}

QFuture<std::vector<AddonInfo>> AddonManager::fetchRemoteAddonList(AddonType type) {
    auto* impl = m_impl.get();
    return QtConcurrent::run([impl, type]() -> std::vector<AddonInfo> {
        // Fetch from lotrointerface.com
        auto future = impl->client.fetchAddonList(type);
        future.waitForFinished();
        auto remoteAddons = future.result();
        
        // Convert RemoteAddonInfo to AddonInfo
        std::vector<AddonInfo> result;
        for (const auto& remote : remoteAddons) {
            AddonInfo info;
            info.id = remote.interfaceId;
            info.name = remote.name;
            info.type = type;
            info.version = remote.version;
            info.author = remote.author;
            info.category = remote.category;
            info.latestVersion = remote.latestRelease;
            info.downloadUrl = remote.downloadUrl;
            info.infoUrl = remote.infoUrl;
            info.status = AddonStatus::NotInstalled;
            
            // New metadata fields
            info.releaseDate = remote.latestRelease;
            info.downloadCount = remote.downloads;
            info.fileSize = remote.fileSize;
            info.favourites = remote.favourites;
            
            result.push_back(info);
        }
        
        spdlog::info("Fetched {} remote addons", result.size());
        return result;
    });
}

QFuture<bool> AddonManager::installAddon(
    const QString& id,
    AddonType type,
    AddonProgressCallback progress
) {
    auto* impl = m_impl.get();
    auto destDir = getAddonDirectory(type);
    
    return QtConcurrent::run([impl, id, type, destDir, progress]() -> bool {
        try {
            // First, fetch addon list to get download URL
            auto listFuture = impl->client.fetchAddonList(type);
            listFuture.waitForFinished();
            auto remoteAddons = listFuture.result();
            
            // Find the addon by ID
            QString downloadUrl;
            QString addonName;
            for (const auto& addon : remoteAddons) {
                if (addon.interfaceId == id) {
                    downloadUrl = addon.downloadUrl;
                    addonName = addon.name;
                    break;
                }
            }
            
            if (downloadUrl.isEmpty()) {
                spdlog::error("Addon not found: {}", id.toStdString());
                return false;
            }
            
            if (progress) {
                progress(0, 1, QString("Downloading %1...").arg(addonName));
            }
            
            // Download the addon
            auto dlFuture = impl->client.downloadAddon(downloadUrl, 
                [&progress, &addonName](qint64 received, qint64 total) {
                    if (progress && total > 0) {
                        int percent = static_cast<int>((received * 100) / total);
                        progress(percent, 100, QString("Downloading %1... %2%").arg(addonName).arg(percent));
                    }
                });
            dlFuture.waitForFinished();
            QString zipPath = dlFuture.result();
            
            if (zipPath.isEmpty()) {
                spdlog::error("Failed to download addon");
                if (progress) {
                    progress(-1, 100, QString("Download failed for %1").arg(addonName));
                }
                return false;
            }
            
            // Verify the downloaded file is actually a ZIP
            // (lotrointerface.com may return HTML error pages)
            QFile downloadedFile(zipPath);
            if (downloadedFile.open(QIODevice::ReadOnly)) {
                QByteArray header = downloadedFile.read(4);
                downloadedFile.close();
                
                // ZIP files start with "PK\x03\x04" signature
                if (header.size() < 4 || 
                    header[0] != 'P' || header[1] != 'K' ||
                    header[2] != 0x03 || header[3] != 0x04) {
                    
                    // Not a ZIP - likely an HTML error page
                    QString errorMsg;
                    if (downloadedFile.open(QIODevice::ReadOnly)) {
                        QString content = QString::fromUtf8(downloadedFile.readAll());
                        downloadedFile.close();
                        
                        if (content.contains("not yet been approved", Qt::CaseInsensitive)) {
                            errorMsg = QString("%1: File not yet approved for download on lotrointerface.com").arg(addonName);
                        } else if (content.contains("file not found", Qt::CaseInsensitive)) {
                            errorMsg = QString("%1: File not found on lotrointerface.com").arg(addonName);
                        } else {
                            errorMsg = QString("%1: Download failed - server returned invalid file").arg(addonName);
                        }
                    } else {
                        errorMsg = QString("%1: Download failed - file is not a valid archive").arg(addonName);
                    }
                    
                    spdlog::error("{}", errorMsg.toStdString());
                    if (progress) {
                        progress(-1, 100, errorMsg);
                    }
                    QFile::remove(zipPath);
                    return false;
                }
            }
            
            if (progress) {
                progress(100, 100, QString("Installing %1...").arg(addonName));
            }
            
            // Create a temporary directory for extraction
            QString tempDirPath = QDir::tempPath() + "/lotro-launcher-addon-" + 
                QString::number(QDateTime::currentMSecsSinceEpoch());
            QDir tempDir(tempDirPath);
            tempDir.mkpath(".");
            
            spdlog::info("Extracting addon to temp directory: {}", tempDirPath.toStdString());
            
            // Extract zip to temporary directory using unzip command
            QProcess process;
            QStringList args;
            args << "-o"  // Overwrite
                 << zipPath
                 << "-d"
                 << tempDirPath;
            
            process.start("unzip", args);
            
            if (!process.waitForFinished(60000)) {
                spdlog::error("Extraction timed out");
                if (progress) {
                    progress(-1, 100, QString("Extraction timed out for %1").arg(addonName));
                }
                QFile::remove(zipPath);
                tempDir.removeRecursively();
                return false;
            }
            
            if (process.exitCode() != 0) {
                QString errorOutput = QString::fromUtf8(process.readAllStandardError());
                spdlog::error("Extraction failed: {}", errorOutput.toStdString());
                if (progress) {
                    progress(-1, 100, QString("Extraction failed for %1").arg(addonName));
                }
                QFile::remove(zipPath);
                tempDir.removeRecursively();
                return false;
            }
            
            // Clean up zip file
            QFile::remove(zipPath);
            
            // Clean invalid wrapper folders (OneLauncher-style)
            cleanTempAddonFolder(tempDirPath.toStdString());
            
            // For music and skins, ensure files are wrapped in a named folder
            // This makes them manageable instead of being loose files
            std::filesystem::path copySource = tempDirPath.toStdString();
            if (type == AddonType::Music || type == AddonType::Skin) {
                auto fixedRoot = fixImproperRootDir(
                    tempDirPath.toStdString(), 
                    addonName.toStdString()
                );
                // If fixImproperRootDir returned a subfolder, copy just that folder
                if (fixedRoot != copySource) {
                    // The files are now in a named folder, copy that folder to destination
                    copySource = fixedRoot;
                }
            }
            
            // Copy contents to final destination
            spdlog::info("Copying addon to final destination: {}", destDir.string());
            
            // Copy each top-level item from the temp/source directory to the destination
            // This avoids copying the temp directory folder itself (which would create
            // .../Plugins/lotro-launcher-addon-123/ instead of .../Plugins/AddonName/)
            for (const auto& entry : std::filesystem::directory_iterator(copySource)) {
                QProcess copyProcess;
                QStringList copyArgs;
                copyArgs << "-r" << "-f"
                         << QString::fromStdString(entry.path().string())
                         << QString::fromStdString(destDir.string());
                
                copyProcess.start("cp", copyArgs);
                if (!copyProcess.waitForFinished(60000)) {
                    spdlog::error("Copy to destination failed for: {}", entry.path().string());
                    if (progress) {
                        progress(-1, 100, QString("Failed to install %1").arg(addonName));
                    }
                    tempDir.removeRecursively();
                    return false;
                }
            }
            
            // Clean up temp directory
            tempDir.removeRecursively();
            
            spdlog::info("Addon {} installed successfully to {}", addonName.toStdString(), destDir.string());
            return true;
            
        } catch (const std::exception& e) {
            spdlog::error("Exception installing addon: {}", e.what());
            return false;
        }
    });
}

QFuture<bool> AddonManager::updateAddon(
    const QString& id,
    AddonType type,
    AddonProgressCallback progress
) {
    // Update is just reinstall - download new version and overwrite
    return installAddon(id, type, progress);
}

QFuture<int> AddonManager::updateAllAddons(
    AddonType type,
    AddonProgressCallback progress
) {
    auto* impl = m_impl.get();
    auto installedAddons = getInstalledAddons(type);
    
    return QtConcurrent::run([this, impl, type, installedAddons, progress]() -> int {
        int updatedCount = 0;
        int total = static_cast<int>(installedAddons.size());
        int current = 0;
        
        for (const auto& addon : installedAddons) {
            if (addon.hasUpdate()) {
                if (progress) {
                    progress(current, total, QString("Updating %1...").arg(addon.name));
                }
                
                auto updateFuture = this->updateAddon(addon.id, type, nullptr);
                updateFuture.waitForFinished();
                if (updateFuture.result()) {
                    updatedCount++;
                }
            }
            current++;
        }
        
        if (progress) {
            progress(total, total, QString("Updated %1 addons").arg(updatedCount));
        }
        
        spdlog::info("Updated {} addons", updatedCount);
        return updatedCount;
    });
}

std::optional<AddonInfo> AddonManager::parseCompendiumFile(
    const std::filesystem::path& path
) const {
    return CompendiumParser::parse(path);
}

std::vector<AddonInfo> AddonManager::getAddonsWithStartupScripts() const {
    std::vector<AddonInfo> result;
    
    for (const auto& addon : m_impl->installedPlugins) {
        if (!addon.startupScript.isEmpty()) {
            result.push_back(addon);
        }
    }
    
    return result;
}

// Helper functions

QString getLotroInterfaceUrl(AddonType type) {
    switch (type) {
        case AddonType::Plugin:
            return "https://www.lotrointerface.com/addons/plugins/";
        case AddonType::Skin:
            return "https://www.lotrointerface.com/addons/skins/";
        case AddonType::Music:
            return "https://www.lotrointerface.com/addons/music/";
    }
    return "";
}

QString addonTypeToString(AddonType type) {
    switch (type) {
        case AddonType::Plugin: return "Plugin";
        case AddonType::Skin:   return "Skin";
        case AddonType::Music:  return "Music";
    }
    return "Unknown";
}

AddonType stringToAddonType(const QString& str) {
    if (str.toLower() == "plugin") return AddonType::Plugin;
    if (str.toLower() == "skin") return AddonType::Skin;
    if (str.toLower() == "music") return AddonType::Music;
    return AddonType::Plugin;
}

} // namespace lotro
