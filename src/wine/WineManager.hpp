/**
 * LOTRO Launcher - Wine Manager (Linux only)
 *
 * Manages Wine/Proton via umu-launcher and DXVK setup.
 * Based on OneLauncher's wine_environment.py
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifdef PLATFORM_LINUX

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QProcessEnvironment>
#include <QStringList>

#include "core/config/WineConfig.hpp"

namespace lotro {

/**
 * Progress callback for download operations
 *
 * @param current Current bytes downloaded
 * @param total Total bytes to download
 */
using DownloadProgressCallback =
    std::function<void(size_t current, size_t total)>;

/**
 * Status callback for streaming setup progress messages to the UI
 *
 * @param message Human-readable status message
 */
using StatusCallback = std::function<void(const QString &)>;

/**
 * Wine manager singleton
 *
 * Handles:
 * - umu-launcher detection and invocation
 * - WINEPREFIX initialization
 * - DXVK installation
 * - Wine/Proton process argument building
 *
 * Workflow:
 * 1. Builtin mode: Uses umu-run which auto-downloads GE-Proton
 * 2. User mode: Uses user-specified Wine and prefix paths
 */
class WineManager {
public:
  static WineManager &instance();

  /**
   * Initialize the Wine manager with configuration
   */
  void setConfig(const WineConfig &config);
  const WineConfig &config() const { return m_config; }

  /**
   * Check if Wine environment is fully set up and ready
   */
  bool isSetup() const { return m_isSetup; }

  /**
   * Check if umu-run is available on the system
   */
  static bool isUmuAvailable();

  /**
   * Detect Steam/Proton prefix for a game directory
   *
   * Looks for compatdata in the Steam library containing the game.
   * LOTRO Steam App ID is 212500, DDO is 206480.
   *
   * @param gameDirectory Path to the game installation
   * @param appId Steam App ID (0 for auto-detect)
   * @return Path to the Proton prefix (pfx subdirectory), or empty if not found
   */
  static std::filesystem::path
  detectSteamPrefix(const std::filesystem::path &gameDirectory,
                    uint32_t appId = 0);

  /**
   * Set up the Wine environment
   *
   * In builtin mode:
   * - Verifies umu-run is available
   * - Creates WINEPREFIX if not present
   * - Installs DXVK if enabled
   *
   * In user mode:
   * - Validates user-provided paths
   * - Installs DXVK to user prefix if enabled
   *
   * @param progress Progress callback for downloads
   * @return true if setup completed successfully
   */
  bool setup(DownloadProgressCallback progress = nullptr,
             StatusCallback statusCb = nullptr);

  // ===================
  // Wine binary methods
  // ===================

  /**
   * Get path to Wine/umu-run executable
   */
  std::filesystem::path getWineExecutable() const;

  /**
   * Get path to Wine server executable (user mode only)
   */
  std::filesystem::path getWineServer() const;

  // ==================
  // WINEPREFIX methods
  // ==================

  /**
   * Get path to WINEPREFIX
   */
  std::filesystem::path getPrefixPath() const;

  /**
   * Initialize a new WINEPREFIX
   */
  bool initializePrefix(StatusCallback statusCb = nullptr);

  /**
   * Check if prefix exists and is valid
   */
  bool isPrefixValid() const;

  /**
   * Get path within prefix (e.g., drive_c/windows/system32)
   */
  std::filesystem::path getPrefixPath(const std::string &subpath) const;

  /**
   * Get the LOTRO settings path inside the Wine prefix
   *
   * Returns the path where LOTRO running under Wine will look for
   * plugins, skins, and user preferences. This is the Documents folder
   * inside the Wine prefix, not the native Linux Documents folder.
   *
   * @return Path like <prefix>/drive_c/users/<USER>/Documents/The Lord of the
   * Rings Online
   */
  std::filesystem::path getWineLotroSettingsPath() const;

  // =============
  // DXVK methods
  // =============

  /**
   * Download and install DXVK to the prefix
   */
  bool setupDxvk(DownloadProgressCallback progress = nullptr);

  /**
   * Check if DXVK is installed in the prefix
   */
  bool isDxvkInstalled() const;

  /**
   * Build Wine command line arguments
   *
   * @param executable Path to Windows executable
   * @param args Arguments to pass to executable
   * @return Full argument list for QProcess
   */
  QStringList buildWineArgs(const std::filesystem::path &executable,
                            const QStringList &args = {}) const;

  /**
   * Build Wine command line for console applications
   *
   * Uses plain Wine instead of umu-run/Proton because Proton
   * doesn't properly capture stdout/stderr from console apps.
   * This is needed for the patch client where we need to parse output.
   *
   * @param executable Path to Windows executable
   * @param args Arguments to pass to executable
   * @return Full argument list for QProcess
   */
  QStringList
  buildWineArgsForConsoleApp(const std::filesystem::path &executable,
                             const QStringList &args = {}) const;

  /**
   * Get path to plain Wine executable (not umu-run/Proton)
   *
   * This is needed for console apps that require stdout capture.
   * Falls back to "wine" in PATH if not found.
   */
  std::filesystem::path getPlainWineExecutable() const;

  /**
   * Get environment variables for Wine process
   */
  QProcessEnvironment getWineEnvironment() const;

  // =========================
  // System capability checks
  // =========================

  /**
   * Check if system supports esync (high open file limit)
   */
  static bool checkEsyncSupport();

  /**
   * Check if system supports fsync (kernel support)
   */
  static bool checkFsyncSupport();

  /**
   * Get system's open file limit
   */
  static std::optional<size_t> getOpenFileLimit();

private:
  WineManager();
  ~WineManager() = default;
  WineManager(const WineManager &) = delete;
  WineManager &operator=(const WineManager &) = delete;

  // Internal paths
  std::filesystem::path getBuiltinPrefixPath() const;
  std::filesystem::path getDxvkCachePath() const;

  // Download helpers
  bool downloadFile(const std::string &url,
                    const std::filesystem::path &destination,
                    DownloadProgressCallback progress);
  bool extractArchive(const std::filesystem::path &archive,
                      const std::filesystem::path &destination);

  WineConfig m_config;
  bool m_isSetup = false;

  // Cached paths
  std::filesystem::path m_dataPath;
  std::filesystem::path m_cachePath;
};

// UMU configuration
namespace UmuConfig {
// Game ID for LOTRO - use Steam App ID format for proper Proton fixes
// LOTRO Steam App ID: 212500
constexpr const char *LOTRO_GAME_ID = "umu-212500";

// Proton version to use (GE-Proton auto-downloads latest)
constexpr const char *PROTON_VERSION = "GE-Proton";
} // namespace UmuConfig

namespace DxvkVersions {
constexpr const char *DXVK_VERSION = "2.4";
constexpr const char *DXVK_URL =
    "https://github.com/doitsujin/dxvk/releases/download/v2.4/dxvk-2.4.tar.gz";
} // namespace DxvkVersions

// Minimum open file limit for esync
constexpr size_t ESYNC_MIN_OPEN_FILE_LIMIT = 524288;

} // namespace lotro

#endif // PLATFORM_LINUX
