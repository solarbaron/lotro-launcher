/**
 * LOTRO Launcher - Patch Dialog
 * 
 * Dialog for displaying patching/update progress.
 * Based on OneLauncher's patch_game_window.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "game/PatchClient.hpp"
#include "game/NativePatcher.hpp"

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QTimer>

namespace lotro {

/**
 * Dialog for patching/updating game files
 * 
 * Shows progress of the patching operation with cancel support.
 * Features a detailed log view and precise progress tracking.
 * 
 * Patching phases (matching OneLauncher):
 *   Phase 1: Akamai CDN - download missing game files and splashscreens
 *   Phase 2: FilesOnly (1st pass) - patchclient.dll file check
 *   Phase 3: FilesOnly (2nd pass) - patchclient.dll self-patch handling
 *   Phase 4: DataOnly - patchclient.dll data patching
 */
class PatchDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit PatchDialog(const std::filesystem::path& gameDirectory, 
                        const QString& patchServerUrl,
                        const QString& launcherConfigUrl = QString(),
                        bool highResEnabled = true,
                        const QString& locale = "en",
                        QWidget* parent = nullptr);
    ~PatchDialog();
    
    /**
     * Start the patching process
     * @return true if patching completed successfully
     */
    bool startPatching();
    
    /**
     * Check if patching was successful
     */
    bool wasSuccessful() const { return m_success; }
    
    /**
     * Get the last error message
     */
    QString lastError() const { return m_lastError; }
    
signals:
    void patchingFinished(bool success);
    
private slots:
    void onCancelClicked();
    void updateProgress(const PatchProgress& progress);
    
private:
    void setupUi();
    void runPatch();
    void runAkamaiPhase();
    void appendLog(const QString& message, const QString& color = "#aaaaaa");
    void updatePhaseDisplay(int currentPhase, int totalPhases);
    
    std::filesystem::path m_gameDirectory;
    QString m_patchServerUrl;
    QString m_launcherConfigUrl;
    bool m_highResEnabled;
    QString m_locale;
    std::unique_ptr<PatchClient> m_patchClient;
    std::unique_ptr<NativePatcher> m_nativePatcher;
    
    // UI elements
    QLabel* m_titleLabel = nullptr;
    QLabel* m_phaseLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_detailLabel = nullptr;
    QTextEdit* m_logView = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QPushButton* m_actionButton = nullptr;
    
    // State
    bool m_success = false;
    bool m_patching = false;
    QString m_lastError;
    int m_currentPhase = 0;
    int m_totalPhases = 4;  // Akamai, FilesOnly, FilesOnly, DataOnly
};

} // namespace lotro
