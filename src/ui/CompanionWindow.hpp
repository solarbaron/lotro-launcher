/**
 * LOTRO Launcher - Companion Window
 * 
 * Main window for LOTRO Companion-like features.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDialog>
#include <memory>

class QTabWidget;

namespace lotro {

class CharacterExtractor;
class CharacterTracker;
class CharacterListWidget;
class DeedBrowserWidget;
class RecipeBrowserWidget;
class LiveSyncService;
class SyncStatusWidget;
class GearSimulatorWidget;

/**
 * LOTRO Companion Window
 * 
 * Tabbed interface providing:
 * - Character tracking (live from game)
 * - Saved character history
 * - Deed browser
 * - Recipe browser
 * - Gear simulator
 * - Data export
 */
class CompanionWindow : public QDialog {
    Q_OBJECT

public:
    /**
     * Construct the companion window
     * @param gamePath Path to LOTRO installation directory
     * @param parent Parent widget
     */
    explicit CompanionWindow(const QString& gamePath = QString(), QWidget* parent = nullptr);
    ~CompanionWindow() override;

private slots:
    void onSyncToggled();
    void onCharacterSaved(const QString& name, const QString& server);

private:
    void setupUi();
    
    QString m_gamePath;
    QTabWidget* m_tabWidget = nullptr;
    std::unique_ptr<CharacterTracker> m_characterTracker;
    std::unique_ptr<LiveSyncService> m_syncService;
    CharacterListWidget* m_characterList = nullptr;
    DeedBrowserWidget* m_deedBrowser = nullptr;
    RecipeBrowserWidget* m_recipeBrowser = nullptr;
    GearSimulatorWidget* m_gearSimulator = nullptr;
    SyncStatusWidget* m_syncStatus = nullptr;
    QPushButton* m_syncButton = nullptr;
};

} // namespace lotro

