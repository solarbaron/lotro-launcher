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
 */
class PatchDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit PatchDialog(const std::filesystem::path& gameDirectory, 
                        const QString& patchServerUrl,
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
    void appendLog(const QString& message, const QString& color = "#aaaaaa");
    void updatePhaseDisplay(int currentPhase, int totalPhases);
    
    std::filesystem::path m_gameDirectory;
    QString m_patchServerUrl;
    std::unique_ptr<PatchClient> m_patchClient;
    
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
    int m_totalPhases = 3;  // FilesOnly, FilesOnly, DataOnly
};

} // namespace lotro
