/**
 * LOTRO Launcher - Patch Client
 * 
 * Interface for game patching via patchclient.dll.
 * Based on OneLauncher's patch_game.py
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

namespace lotro {

/**
 * Patching phase
 */
enum class PatchPhase {
    FilesOnly,    // Patch game files
    DataOnly      // Patch data files (.dat)
};

/**
 * Patching progress information
 */
struct PatchProgress {
    PatchPhase phase;
    int currentFile = 0;
    int totalFiles = 0;
    int currentBytes = 0;
    int totalBytes = 0;
    QString currentFileName;
    QString status;
    
    /**
     * Get progress as percentage (0-100)
     */
    int percentage() const {
        if (totalBytes > 0) {
            return static_cast<int>((currentBytes * 100) / totalBytes);
        }
        if (totalFiles > 0) {
            return static_cast<int>((currentFile * 100) / totalFiles);
        }
        return 0;
    }
};

/**
 * Progress callback type
 */
using PatchProgressCallback = std::function<void(const PatchProgress&)>;

/**
 * Patch client interface
 * 
 * Runs the game's patchclient.dll to update game files.
 * On Linux, this is run through Wine.
 */
class PatchClient {
public:
    /**
     * Create patch client for the specified game
     * 
     * @param gameDirectory Game installation directory
     * @param patchClientFilename Name of patchclient.dll
     */
    PatchClient(
        const std::filesystem::path& gameDirectory,
        const QString& patchClientFilename = "PatchClient.dll"
    );
    ~PatchClient();
    
    /**
     * Run the patching process
     * 
     * @param patchServerUrl Patch server URL from GameServicesInfo
     * @param progress Progress callback
     * @return true if patching completed successfully
     */
    bool patch(
        const QString& patchServerUrl,
        bool highResEnabled = true,
        const QString& language = "English",
        PatchProgressCallback progress = nullptr
    );
    
    /**
     * Check if patching is currently in progress
     */
    bool isPatching() const;
    
    /**
     * Cancel the current patching operation
     */
    void cancel();
    
    /**
     * Get the last error message
     */
    QString lastError() const;
    
    /**
     * Check if game needs patching
     * 
     * This is a quick check that doesn't download anything.
     */
    bool needsPatching(const QString& patchServerUrl);
    
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * Path to the patch client runner executable
 * 
 * This is a helper executable that runs patchclient.dll and
 * captures its output.
 */
std::filesystem::path getPatchClientRunnerPath();

} // namespace lotro
