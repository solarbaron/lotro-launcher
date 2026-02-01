/**
 * LOTRO Launcher - Patch Dialog
 * 
 * Dialog for displaying patching/update progress.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "game/PatchClient.hpp"

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace lotro {

/**
 * Dialog for patching/updating game files
 * 
 * Shows progress of the patching operation with cancel support.
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
    
    std::filesystem::path m_gameDirectory;
    QString m_patchServerUrl;
    std::unique_ptr<PatchClient> m_patchClient;
    
    QLabel* m_statusLabel = nullptr;
    QLabel* m_fileLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QPushButton* m_cancelButton = nullptr;
    
    bool m_success = false;
    QString m_lastError;
};

} // namespace lotro
