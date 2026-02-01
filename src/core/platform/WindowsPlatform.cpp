/**
 * LOTRO Launcher - Platform Implementation (Windows)
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef PLATFORM_WINDOWS

#include "Platform.hpp"

#include <QDesktopServices>
#include <QStandardPaths>
#include <QUrl>

#include <ShlObj.h>
#include <windows.h>

namespace lotro {

namespace {

std::filesystem::path getKnownFolderPath(REFKNOWNFOLDERID folderId) {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(folderId, 0, nullptr, &path))) {
        std::filesystem::path result(path);
        CoTaskMemFree(path);
        return result;
    }
    return {};
}

}  // namespace

std::filesystem::path Platform::getConfigPath() {
    auto appData = getKnownFolderPath(FOLDERID_RoamingAppData);
    if (!appData.empty()) {
        return appData / "lotro-launcher";
    }
    return std::filesystem::path("lotro-launcher");
}

std::filesystem::path Platform::getDataPath() {
    auto localAppData = getKnownFolderPath(FOLDERID_LocalAppData);
    if (!localAppData.empty()) {
        return localAppData / "lotro-launcher";
    }
    return std::filesystem::path("lotro-launcher");
}

std::filesystem::path Platform::getCachePath() {
    return getDataPath() / "cache";
}

std::filesystem::path Platform::getDocumentsPath() {
    auto docs = getKnownFolderPath(FOLDERID_Documents);
    if (!docs.empty()) {
        return docs;
    }
    QString docsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return std::filesystem::path(docsPath.toStdString());
}

std::vector<std::filesystem::path> Platform::detectGameInstallations() {
    std::vector<std::filesystem::path> installations;
    
    // Check common installation paths
    std::vector<std::filesystem::path> searchPaths = {
        "C:/Program Files/Standing Stone Games/Lord of the Rings Online",
        "C:/Program Files (x86)/Standing Stone Games/Lord of the Rings Online",
        "C:/Program Files/Turbine/Lord of the Rings Online",
        "C:/Program Files (x86)/Turbine/Lord of the Rings Online",
    };
    
    // Check Steam installation
    auto programFiles = getKnownFolderPath(FOLDERID_ProgramFilesX86);
    if (!programFiles.empty()) {
        searchPaths.push_back(programFiles / "Steam/steamapps/common/Lord of the Rings Online");
    }
    
    for (const auto& path : searchPaths) {
        if (std::filesystem::exists(path / "LotroLauncher.exe")) {
            installations.push_back(path);
        }
    }
    
    // TODO: Check Windows registry for installation path
    
    return installations;
}

std::filesystem::path Platform::getDefaultLotroSettingsPath() {
    return getDocumentsPath() / "The Lord of the Rings Online";
}

std::optional<size_t> Platform::getOpenFileLimit() {
    // Not applicable on Windows
    return std::nullopt;
}

bool Platform::openUrl(const std::string& url) {
    return QDesktopServices::openUrl(QUrl(QString::fromStdString(url)));
}

bool Platform::openFileManager(const std::filesystem::path& path) {
    return QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(path.string())));
}

} // namespace lotro

#endif // PLATFORM_WINDOWS
