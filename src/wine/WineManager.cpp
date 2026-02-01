/**
 * LOTRO Launcher - Wine Manager Implementation (Linux only)
 * 
 * Uses umu-launcher for Wine/Proton management.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef PLATFORM_LINUX

#include "WineManager.hpp"
#include "WinePrefixSetup.hpp"
#include "WineProcessBuilder.hpp"
#include "core/platform/Platform.hpp"

#include <QDir>
#include <QProcess>
#include <QFile>
#include <QStandardPaths>

#include <fstream>
#include <sys/resource.h>

#include <spdlog/spdlog.h>

namespace lotro {

WineManager::WineManager() {
    m_dataPath = Platform::getDataPath();
    m_cachePath = Platform::getCachePath();
    
    // Ensure directories exist
    std::filesystem::create_directories(m_dataPath);
    std::filesystem::create_directories(m_cachePath);
}

WineManager& WineManager::instance() {
    static WineManager instance;
    return instance;
}

void WineManager::setConfig(const WineConfig& config) {
    m_config = config;
    m_isSetup = false; // Need to re-validate
}

bool WineManager::isUmuAvailable() {
    // Check if umu-run is in PATH
    QString umuPath = QStandardPaths::findExecutable("umu-run");
    if (!umuPath.isEmpty()) {
        spdlog::debug("Found umu-run at: {}", umuPath.toStdString());
        return true;
    }
    
    // Check common installation paths
    std::vector<std::filesystem::path> commonPaths = {
        "/usr/bin/umu-run",
        "/usr/local/bin/umu-run",
        std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : "") / ".local/bin/umu-run"
    };
    
    for (const auto& path : commonPaths) {
        if (std::filesystem::exists(path)) {
            spdlog::debug("Found umu-run at: {}", path.string());
            return true;
        }
    }
    
    spdlog::warn("umu-run not found. Please install umu-launcher.");
    return false;
}

std::filesystem::path WineManager::detectSteamPrefix(
    const std::filesystem::path& gameDirectory,
    uint32_t appId
) {
    // Known Steam App IDs
    constexpr uint32_t LOTRO_APP_ID = 212500;
    constexpr uint32_t DDO_APP_ID = 206480;
    
    // Try to determine App ID from game directory name if not provided
    if (appId == 0) {
        std::string dirName = gameDirectory.filename().string();
        if (dirName.find("Lord of the Rings") != std::string::npos ||
            dirName.find("LOTRO") != std::string::npos) {
            appId = LOTRO_APP_ID;
        } else if (dirName.find("Dragons Online") != std::string::npos ||
                   dirName.find("DDO") != std::string::npos) {
            appId = DDO_APP_ID;
        }
    }
    
    if (appId == 0) {
        spdlog::debug("Could not determine Steam App ID for: {}", gameDirectory.string());
        return {};
    }
    
    // Find steamapps directory from game path
    // Game is typically at: /path/to/SteamLibrary/steamapps/common/Game Name
    std::filesystem::path current = gameDirectory;
    std::filesystem::path steamappsPath;
    
    // Walk up the directory tree looking for "steamapps"
    for (int i = 0; i < 5 && !current.empty(); ++i) {
        if (current.filename() == "steamapps") {
            steamappsPath = current;
            break;
        }
        current = current.parent_path();
    }
    
    if (steamappsPath.empty()) {
        spdlog::debug("Could not find steamapps directory from: {}", gameDirectory.string());
        return {};
    }
    
    // Check for Proton prefix
    std::filesystem::path compatdataPath = steamappsPath / "compatdata" / std::to_string(appId);
    
    // Proton uses pfx subdirectory
    std::filesystem::path prefixPath = compatdataPath / "pfx";
    if (std::filesystem::exists(prefixPath / "drive_c")) {
        spdlog::info("Found Steam/Proton prefix at: {}", prefixPath.string());
        return prefixPath;
    }
    
    // Some older setups might not have pfx subdirectory
    if (std::filesystem::exists(compatdataPath / "drive_c")) {
        spdlog::info("Found Steam prefix at: {}", compatdataPath.string());
        return compatdataPath;
    }
    
    spdlog::debug("Steam prefix not found at: {}", compatdataPath.string());
    return {};
}

bool WineManager::setup(DownloadProgressCallback progress) {
    spdlog::info("Setting up Wine environment...");
    
    if (m_config.prefixMode == WinePrefixMode::Builtin) {
        // Verify umu-run is available
        if (!isUmuAvailable()) {
            spdlog::error("umu-launcher is not installed. Please install it first.");
            spdlog::error("See: https://github.com/Open-Wine-Components/umu-launcher");
            return false;
        }
        
        // Initialize prefix if needed
        if (!isPrefixValid()) {
            spdlog::info("Initializing Wine prefix...");
            if (!initializePrefix()) {
                spdlog::error("Failed to initialize Wine prefix");
                return false;
            }
        }
        
        // Set up DXVK if enabled (umu-run usually handles this, but we can force it)
        if (m_config.dxvkEnabled && !isDxvkInstalled()) {
            spdlog::info("Setting up DXVK...");
            if (!setupDxvk(progress)) {
                spdlog::warn("Failed to set up DXVK, continuing without it");
            }
        }
    } else {
        // Validate user-provided Wine
        if (!std::filesystem::exists(m_config.userWineExecutable)) {
            spdlog::error("User Wine executable not found: {}", 
                         m_config.userWineExecutable.string());
            return false;
        }
        
        if (!std::filesystem::exists(m_config.userPrefixPath)) {
            spdlog::error("User Wine prefix not found: {}", 
                         m_config.userPrefixPath.string());
            return false;
        }
        
        // Optionally install DXVK to user prefix
        if (m_config.dxvkEnabled && !isDxvkInstalled()) {
            spdlog::info("Installing DXVK to user prefix...");
            setupDxvk(progress);
        }
    }
    
    m_isSetup = true;
    spdlog::info("Wine environment setup complete");
    return true;
}

std::filesystem::path WineManager::getWineExecutable() const {
    if (m_config.prefixMode == WinePrefixMode::User) {
        return m_config.userWineExecutable;
    }
    
    // For builtin mode, return umu-run
    QString umuPath = QStandardPaths::findExecutable("umu-run");
    if (!umuPath.isEmpty()) {
        return std::filesystem::path(umuPath.toStdString());
    }
    
    // Fallback paths
    if (std::filesystem::exists("/usr/bin/umu-run")) {
        return "/usr/bin/umu-run";
    }
    
    const char* home = std::getenv("HOME");
    if (home) {
        auto localPath = std::filesystem::path(home) / ".local/bin/umu-run";
        if (std::filesystem::exists(localPath)) {
            return localPath;
        }
    }
    
    return "umu-run"; // Hope it's in PATH
}

std::filesystem::path WineManager::getWineServer() const {
    if (m_config.prefixMode == WinePrefixMode::User) {
        auto wineDir = m_config.userWineExecutable.parent_path();
        return wineDir / "wineserver";
    }
    // umu-run handles wineserver internally
    return {};
}

std::filesystem::path WineManager::getPrefixPath() const {
    switch (m_config.prefixMode) {
        case WinePrefixMode::User:
            return m_config.userPrefixPath;
        case WinePrefixMode::Steam:
            if (!m_config.steamPrefixPath.empty()) {
                return m_config.steamPrefixPath;
            }
            // Fall through to builtin if no Steam prefix configured
            [[fallthrough]];
        case WinePrefixMode::Builtin:
        default:
            return getBuiltinPrefixPath();
    }
}

bool WineManager::isPrefixValid() const {
    auto prefixPath = getPrefixPath();
    return std::filesystem::exists(prefixPath / "system.reg") &&
           std::filesystem::exists(prefixPath / "drive_c");
}

std::filesystem::path WineManager::getPrefixPath(const std::string& subpath) const {
    return getPrefixPath() / subpath;
}

std::filesystem::path WineManager::getWineLotroSettingsPath() const {
    auto prefix = getPrefixPath();
    
    // Get the current user name
    const char* user = std::getenv("USER");
    if (!user || user[0] == '\0') {
        user = "user";
    }
    
    // Build path to Documents inside the Wine prefix
    // Wine maps Windows Documents to: <prefix>/drive_c/users/<USER>/Documents
    // or for newer Wine: <prefix>/drive_c/users/<USER>/My Documents
    auto docsPath = prefix / "drive_c/users" / user / "Documents";
    
    // Some Wine setups use "My Documents" instead of "Documents"
    if (!std::filesystem::exists(docsPath)) {
        auto altDocsPath = prefix / "drive_c/users" / user / "My Documents";
        if (std::filesystem::exists(altDocsPath)) {
            docsPath = altDocsPath;
        }
    }
    
    // LOTRO stores settings in Documents/The Lord of the Rings Online
    return docsPath / "The Lord of the Rings Online";
}

bool WineManager::initializePrefix() {
    auto prefixPath = getPrefixPath();
    auto wineExecutable = getWineExecutable();
    
    spdlog::info("Creating Wine prefix at: {}", prefixPath.string());
    
    // Create prefix directory
    std::filesystem::create_directories(prefixPath);
    
    // For umu-run, we just need to run any Windows command to initialize the prefix
    // The wineboot is handled by umu-run automatically
    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WINEPREFIX", QString::fromStdString(prefixPath.string()));
    env.insert("GAMEID", UmuConfig::LOTRO_GAME_ID);
    env.insert("PROTONPATH", UmuConfig::PROTON_VERSION);
    
    process.setProcessEnvironment(env);
    
    QStringList args;
    args << "wineboot" << "--init";
    
    process.start(QString::fromStdString(wineExecutable.string()), args);
    
    if (!process.waitForFinished(300000)) { // 5 minute timeout
        spdlog::error("Prefix initialization timed out");
        return false;
    }
    
    if (process.exitCode() != 0) {
        spdlog::error("Prefix initialization failed: {}", 
                     process.readAllStandardError().toStdString());
        return false;
    }
    
    return isPrefixValid();
}

bool WineManager::setupDxvk(DownloadProgressCallback progress) {
    auto cacheFile = m_cachePath / "dxvk.tar.gz";
    auto dxvkPath = getDxvkCachePath();
    
    spdlog::info("Downloading DXVK from: {}", DxvkVersions::DXVK_URL);
    
    if (!downloadFile(DxvkVersions::DXVK_URL, cacheFile, progress)) {
        return false;
    }
    
    // Extract DXVK
    std::filesystem::create_directories(dxvkPath.parent_path());
    if (!extractArchive(cacheFile, dxvkPath.parent_path())) {
        return false;
    }
    
    // Install DLLs to prefix
    auto prefixPath = getPrefixPath();
    auto system32 = WinePrefixSetup::getSystem32Path(prefixPath);
    auto syswow64 = WinePrefixSetup::getSysWow64Path(prefixPath);
    
    // DXVK DLLs
    std::vector<std::string> dxvkDlls = {
        "d3d9.dll", "d3d10core.dll", "d3d11.dll", "dxgi.dll"
    };
    
    for (const auto& dll : dxvkDlls) {
        // 64-bit
        auto src64 = dxvkPath / "x64" / dll;
        if (std::filesystem::exists(src64)) {
            std::filesystem::copy_file(src64, system32 / dll,
                std::filesystem::copy_options::overwrite_existing);
        }
        
        // 32-bit
        auto src32 = dxvkPath / "x32" / dll;
        if (std::filesystem::exists(src32) && std::filesystem::exists(syswow64)) {
            std::filesystem::copy_file(src32, syswow64 / dll,
                std::filesystem::copy_options::overwrite_existing);
        }
    }
    
    // Set DLL overrides (only needed for user mode, umu handles this)
    if (m_config.prefixMode == WinePrefixMode::User) {
        auto wineExe = getWineExecutable();
        for (const auto& dll : dxvkDlls) {
            std::string dllName = std::filesystem::path(dll).stem().string();
            WinePrefixSetup::setDllOverride(prefixPath, wineExe, dllName, "native");
        }
    }
    
    // Clean up
    std::filesystem::remove(cacheFile);
    
    spdlog::info("DXVK installed successfully");
    return true;
}

bool WineManager::isDxvkInstalled() const {
    auto system32 = WinePrefixSetup::getSystem32Path(getPrefixPath());
    return std::filesystem::exists(system32 / "d3d11.dll");
}

QStringList WineManager::buildWineArgs(
    const std::filesystem::path& executable,
    const QStringList& args
) const {
    WineProcessBuilder builder;
    builder.setWineExecutable(getWineExecutable())
           .setPrefix(getPrefixPath())
           .setExecutable(executable)
           .addArguments(args)
           .setEsync(m_config.esyncEnabled && checkEsyncSupport())
           .setFsync(m_config.fsyncEnabled && checkFsyncSupport());
    
    // Set umu-specific environment
    if (m_config.prefixMode == WinePrefixMode::Builtin) {
        builder.setEnvironment("GAMEID", UmuConfig::LOTRO_GAME_ID);
        builder.setEnvironment("PROTONPATH", UmuConfig::PROTON_VERSION);
    }
    
    if (!m_config.debugLevel.empty()) {
        builder.setDebugLevel(m_config.debugLevel);
    }
    
    return builder.buildCommandLine();
}

QProcessEnvironment WineManager::getWineEnvironment() const {
    WineProcessBuilder builder;
    builder.setWineExecutable(getWineExecutable())
           .setPrefix(getPrefixPath())
           .setEsync(m_config.esyncEnabled && checkEsyncSupport())
           .setFsync(m_config.fsyncEnabled && checkFsyncSupport());
    
    // Set umu-specific environment
    if (m_config.prefixMode == WinePrefixMode::Builtin) {
        builder.setEnvironment("GAMEID", UmuConfig::LOTRO_GAME_ID);
        builder.setEnvironment("PROTONPATH", UmuConfig::PROTON_VERSION);
    }
    
    if (!m_config.debugLevel.empty()) {
        builder.setDebugLevel(m_config.debugLevel);
    }
    
    return builder.buildEnvironment();
}

bool WineManager::checkEsyncSupport() {
    auto limit = getOpenFileLimit();
    if (!limit) {
        return false;
    }
    return *limit >= ESYNC_MIN_OPEN_FILE_LIMIT;
}

bool WineManager::checkFsyncSupport() {
    // Check for kernel futex_waitv support (Linux 5.16+)
    std::ifstream procVersion("/proc/version");
    if (!procVersion.is_open()) {
        return false;
    }
    
    std::string line;
    std::getline(procVersion, line);
    
    // Simple version check - look for kernel version
    size_t pos = line.find("Linux version ");
    if (pos == std::string::npos) {
        return false;
    }
    
    // Parse major.minor version
    pos += 14;
    int major = 0, minor = 0;
    if (sscanf(line.c_str() + pos, "%d.%d", &major, &minor) == 2) {
        // Fsync needs Linux 5.16+
        return major > 5 || (major == 5 && minor >= 16);
    }
    
    return false;
}

std::optional<size_t> WineManager::getOpenFileLimit() {
    struct rlimit limit;
    if (getrlimit(RLIMIT_NOFILE, &limit) == 0) {
        return static_cast<size_t>(limit.rlim_cur);
    }
    return std::nullopt;
}

std::filesystem::path WineManager::getBuiltinPrefixPath() const {
    // Follow umu-launcher convention: ~/.local/share/umu/GAMEID
    // But we use our own path for consistency
    return m_dataPath / "prefix";
}

std::filesystem::path WineManager::getDxvkCachePath() const {
    return m_cachePath / "dxvk" / DxvkVersions::DXVK_VERSION;
}

bool WineManager::downloadFile(
    const std::string& url,
    const std::filesystem::path& destination,
    DownloadProgressCallback progress
) {
    // Ensure parent directory exists
    std::filesystem::create_directories(destination.parent_path());
    
    // Use curl for downloading (more reliable for large files)
    QProcess process;
    QStringList args;
    
    args << "-L"  // Follow redirects
         << "-o" << QString::fromStdString(destination.string())
         << "--progress-bar"
         << "-f"  // Fail on HTTP errors
         << QString::fromStdString(url);
    
    process.start("curl", args);
    
    if (!process.waitForFinished(600000)) { // 10 minute timeout
        spdlog::error("Download timed out");
        return false;
    }
    
    if (process.exitCode() != 0) {
        spdlog::error("Download failed: {}", 
                     process.readAllStandardError().toStdString());
        return false;
    }
    
    return std::filesystem::exists(destination);
}

bool WineManager::extractArchive(
    const std::filesystem::path& archive,
    const std::filesystem::path& destination
) {
    // Ensure destination exists
    std::filesystem::create_directories(destination);
    
    QProcess process;
    QStringList args;
    
    // Detect archive type by extension
    std::string ext = archive.extension().string();
    
    if (ext == ".xz" || archive.string().find(".tar.xz") != std::string::npos) {
        args << "-xJf" << QString::fromStdString(archive.string())
             << "-C" << QString::fromStdString(destination.string());
    } else if (ext == ".gz" || archive.string().find(".tar.gz") != std::string::npos) {
        args << "-xzf" << QString::fromStdString(archive.string())
             << "-C" << QString::fromStdString(destination.string());
    } else {
        // Default to auto-detect
        args << "-xf" << QString::fromStdString(archive.string())
             << "-C" << QString::fromStdString(destination.string());
    }
    
    spdlog::debug("Extracting with: tar {}", args.join(" ").toStdString());
    
    process.start("tar", args);
    
    if (!process.waitForFinished(300000)) { // 5 minute timeout
        spdlog::error("Extraction timed out");
        return false;
    }
    
    if (process.exitCode() != 0) {
        spdlog::error("Extraction failed: {}", 
                     process.readAllStandardError().toStdString());
        return false;
    }
    
    return true;
}

} // namespace lotro

#endif // PLATFORM_LINUX
