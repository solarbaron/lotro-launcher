/**
 * LOTRO Launcher - Setup Wizard
 * 
 * First-run setup wizard.
 * Based on OneLauncher's setup_wizard.py
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>

#include <QWizard>
#include <QString>

namespace lotro {

/**
 * Setup wizard for first-run configuration
 * 
 * Guides users through:
 * 1. Game selection (LOTRO/DDO)
 * 2. Game installation path detection/selection
 * 3. Language/locale selection
 * 4. Wine configuration (Linux)
 * 5. Account setup (optional)
 */
class SetupWizard : public QWizard {
    Q_OBJECT
    
public:
    explicit SetupWizard(QWidget* parent = nullptr);
    ~SetupWizard() override;
    
    /**
     * Get the configured game ID after wizard completes
     */
    QString gameId() const;
    
    /**
     * Get the configured game path after wizard completes
     */
    QString gamePath() const;
    
signals:
    /**
     * Emitted when Wine download starts (Linux)
     */
    void wineDownloadStarted();
    
    /**
     * Emitted during Wine download with progress
     */
    void wineDownloadProgress(int percent);
    
    /**
     * Emitted when Wine download completes
     */
    void wineDownloadCompleted();
    
protected:
    void initializePage(int id) override;
    bool validateCurrentPage() override;
    
public:
    class Impl;

private:
    void setupPages();
    
    std::unique_ptr<Impl> m_impl;
};

/**
 * Wizard page IDs
 */
enum class SetupWizardPage {
    Welcome,
    GameSelection,
    GamePath,
    Language,
#ifdef PLATFORM_LINUX
    WineSetup,
#endif
    AccountSetup,
    Complete
};

} // namespace lotro
