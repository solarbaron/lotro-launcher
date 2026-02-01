/**
 * LOTRO Launcher - Wine Prefix Setup Implementation (Linux only)
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef PLATFORM_LINUX

#include "WinePrefixSetup.hpp"
#include "core/platform/Platform.hpp"

#include <QProcess>
#include <QDir>

#include <spdlog/spdlog.h>

namespace lotro {

bool WinePrefixSetup::createPrefix(
    const std::filesystem::path& prefixPath,
    const std::filesystem::path& wineExecutable,
    bool is64bit
) {
    spdlog::info("Creating Wine prefix at: {}", prefixPath.string());
    
    // Create directory
    std::filesystem::create_directories(prefixPath);
    
    // Set up environment
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WINEPREFIX", QString::fromStdString(prefixPath.string()));
    env.insert("WINEARCH", is64bit ? "win64" : "win32");
    env.insert("WINEDEBUG", "-all");
    
    // Run wineboot to initialize the prefix
    QProcess process;
    process.setProcessEnvironment(env);
    process.setWorkingDirectory(QString::fromStdString(prefixPath.string()));
    
    QStringList args;
    args << "wineboot" << "--init";
    
    process.start(QString::fromStdString(wineExecutable.string()), args);
    
    if (!process.waitForFinished(120000)) { // 2 minute timeout
        spdlog::error("Prefix initialization timed out");
        return false;
    }
    
    if (process.exitCode() != 0) {
        spdlog::error("Prefix initialization failed: {}", 
                     process.readAllStandardError().toStdString());
        return false;
    }
    
    // Wait for wineserver to finish
    QProcess serverProcess;
    serverProcess.setProcessEnvironment(env);
    
    auto wineserverPath = wineExecutable.parent_path() / "wineserver";
    serverProcess.start(QString::fromStdString(wineserverPath.string()), 
                       QStringList() << "--wait");
    serverProcess.waitForFinished(60000);
    
    // Symlink documents
    auto realDocuments = Platform::getDocumentsPath();
    symlinkDocuments(prefixPath, realDocuments);
    
    spdlog::info("Wine prefix created successfully");
    return validatePrefix(prefixPath);
}

bool WinePrefixSetup::validatePrefix(const std::filesystem::path& prefixPath) {
    // Check for essential files
    if (!std::filesystem::exists(prefixPath / "system.reg")) {
        spdlog::warn("Prefix missing system.reg");
        return false;
    }
    
    if (!std::filesystem::exists(prefixPath / "drive_c")) {
        spdlog::warn("Prefix missing drive_c");
        return false;
    }
    
    if (!std::filesystem::exists(prefixPath / "drive_c/windows/system32")) {
        spdlog::warn("Prefix missing system32");
        return false;
    }
    
    return true;
}

std::filesystem::path WinePrefixSetup::getSystem32Path(
    const std::filesystem::path& prefixPath
) {
    return prefixPath / "drive_c/windows/system32";
}

std::filesystem::path WinePrefixSetup::getSysWow64Path(
    const std::filesystem::path& prefixPath
) {
    return prefixPath / "drive_c/windows/syswow64";
}

bool WinePrefixSetup::installDlls(
    const std::filesystem::path& prefixPath,
    const std::vector<std::pair<std::string, std::filesystem::path>>& dlls,
    const std::filesystem::path& targetDir
) {
    for (const auto& [name, source] : dlls) {
        auto target = targetDir / name;
        
        try {
            std::filesystem::copy_file(source, target,
                std::filesystem::copy_options::overwrite_existing);
            spdlog::debug("Installed DLL: {}", name);
        } catch (const std::exception& e) {
            spdlog::error("Failed to install DLL {}: {}", name, e.what());
            return false;
        }
    }
    
    return true;
}

bool WinePrefixSetup::setDllOverride(
    const std::filesystem::path& prefixPath,
    const std::filesystem::path& wineExecutable,
    const std::string& dllName,
    const std::string& mode
) {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WINEPREFIX", QString::fromStdString(prefixPath.string()));
    env.insert("WINEDEBUG", "-all");
    
    QProcess process;
    process.setProcessEnvironment(env);
    
    // Use wine reg to set the override
    QStringList args;
    args << "reg" << "add" 
         << "HKEY_CURRENT_USER\\Software\\Wine\\DllOverrides"
         << "/v" << QString::fromStdString(dllName)
         << "/d" << QString::fromStdString(mode)
         << "/f";
    
    process.start(QString::fromStdString(wineExecutable.string()), args);
    
    if (!process.waitForFinished(30000)) {
        spdlog::warn("DLL override setting timed out for: {}", dllName);
        return false;
    }
    
    return process.exitCode() == 0;
}

bool WinePrefixSetup::setRegistryValue(
    const std::filesystem::path& prefixPath,
    const std::filesystem::path& wineExecutable,
    const std::string& key,
    const std::string& valueName,
    const std::string& value,
    const std::string& type
) {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WINEPREFIX", QString::fromStdString(prefixPath.string()));
    env.insert("WINEDEBUG", "-all");
    
    QProcess process;
    process.setProcessEnvironment(env);
    
    QStringList args;
    args << "reg" << "add" 
         << QString::fromStdString(key)
         << "/v" << QString::fromStdString(valueName)
         << "/t" << QString::fromStdString(type)
         << "/d" << QString::fromStdString(value)
         << "/f";
    
    process.start(QString::fromStdString(wineExecutable.string()), args);
    
    if (!process.waitForFinished(30000)) {
        return false;
    }
    
    return process.exitCode() == 0;
}

bool WinePrefixSetup::symlinkDocuments(
    const std::filesystem::path& prefixPath,
    const std::filesystem::path& realDocuments
) {
    auto prefixDocs = prefixPath / "drive_c/users" / 
                      std::getenv("USER") / "Documents";
    
    // Remove existing documents folder if it's not a symlink
    if (std::filesystem::exists(prefixDocs) && 
        !std::filesystem::is_symlink(prefixDocs)) {
        try {
            std::filesystem::remove_all(prefixDocs);
        } catch (const std::exception& e) {
            spdlog::warn("Could not remove prefix Documents folder: {}", e.what());
            return false;
        }
    }
    
    // Create symlink
    try {
        if (!std::filesystem::exists(prefixDocs)) {
            std::filesystem::create_symlink(realDocuments, prefixDocs);
            spdlog::info("Symlinked prefix Documents to: {}", 
                        realDocuments.string());
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::warn("Could not create Documents symlink: {}", e.what());
        return false;
    }
}

} // namespace lotro

#endif // PLATFORM_LINUX
