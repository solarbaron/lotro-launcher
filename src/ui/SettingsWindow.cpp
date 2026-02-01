/**
 * LOTRO Launcher - Settings Window Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "SettingsWindow.hpp"
#include "PatchDialog.hpp"
#include "core/config/ConfigManager.hpp"
#include "core/platform/Platform.hpp"
#include "network/GameServicesInfo.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QTabWidget>
#include <QLabel>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QMessageBox>

#include <spdlog/spdlog.h>

namespace lotro {

class SettingsWindow::Impl {
public:
    QString gameId;
    
    // Game settings
    QLineEdit* gamePathEdit = nullptr;
    QLineEdit* settingsPathEdit = nullptr;
    QComboBox* clientTypeCombo = nullptr;
    QComboBox* localeCombo = nullptr;
    QCheckBox* highResCheck = nullptr;
    
#ifdef PLATFORM_LINUX
    // Wine settings
    QComboBox* wineModeCombo = nullptr;
    QLineEdit* winePathEdit = nullptr;
    QLineEdit* prefixPathEdit = nullptr;
    QCheckBox* dxvkCheck = nullptr;
    QCheckBox* esyncCheck = nullptr;
    QCheckBox* fsyncCheck = nullptr;
#endif
    
    QDialogButtonBox* buttonBox = nullptr;
};

SettingsWindow::SettingsWindow(const QString& gameId, QWidget* parent)
    : QDialog(parent)
    , m_impl(std::make_unique<Impl>())
{
    m_impl->gameId = gameId;
    
    setWindowTitle("Settings");
    setMinimumSize(500, 400);
    
    setupUi();
    loadSettings();
}

SettingsWindow::~SettingsWindow() = default;

void SettingsWindow::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    QTabWidget* tabs = new QTabWidget();
    
    // Game tab
    QWidget* gameTab = new QWidget();
    QVBoxLayout* gameLayout = new QVBoxLayout(gameTab);
    
    QGroupBox* pathsGroup = new QGroupBox("Game Paths");
    QFormLayout* pathsLayout = new QFormLayout(pathsGroup);
    
    // Game directory
    QHBoxLayout* gameDirLayout = new QHBoxLayout();
    m_impl->gamePathEdit = new QLineEdit();
    QPushButton* browseGameBtn = new QPushButton("Browse...");
    gameDirLayout->addWidget(m_impl->gamePathEdit);
    gameDirLayout->addWidget(browseGameBtn);
    pathsLayout->addRow("Game Directory:", gameDirLayout);
    
    connect(browseGameBtn, &QPushButton::clicked, this, &SettingsWindow::browseGameDirectory);
    
    // Settings directory
    QHBoxLayout* settingsDirLayout = new QHBoxLayout();
    m_impl->settingsPathEdit = new QLineEdit();
    QPushButton* browseSettingsBtn = new QPushButton("Browse...");
    settingsDirLayout->addWidget(m_impl->settingsPathEdit);
    settingsDirLayout->addWidget(browseSettingsBtn);
    pathsLayout->addRow("Settings Directory:", settingsDirLayout);
    
    connect(browseSettingsBtn, &QPushButton::clicked, this, &SettingsWindow::browseSettingsDirectory);
    
    gameLayout->addWidget(pathsGroup);
    
    // Client settings
    QGroupBox* clientGroup = new QGroupBox("Client Settings");
    QFormLayout* clientLayout = new QFormLayout(clientGroup);
    
    m_impl->clientTypeCombo = new QComboBox();
    m_impl->clientTypeCombo->addItem("64-bit (Recommended)", "Win64");
    m_impl->clientTypeCombo->addItem("32-bit", "Win32");
    m_impl->clientTypeCombo->addItem("32-bit Legacy", "Win32Legacy");
    clientLayout->addRow("Client Type:", m_impl->clientTypeCombo);
    
    m_impl->localeCombo = new QComboBox();
    m_impl->localeCombo->addItem("English", "en");
    m_impl->localeCombo->addItem("Deutsch", "de");
    m_impl->localeCombo->addItem("Français", "fr");
    clientLayout->addRow("Language:", m_impl->localeCombo);
    
    m_impl->highResCheck = new QCheckBox("Enable high-resolution textures");
    m_impl->highResCheck->setChecked(true);
    clientLayout->addRow("", m_impl->highResCheck);
    
    gameLayout->addWidget(clientGroup);
    gameLayout->addStretch();
    
    tabs->addTab(gameTab, "Game");
    
#ifdef PLATFORM_LINUX
    // Wine tab (Linux only)
    QWidget* wineTab = new QWidget();
    QVBoxLayout* wineLayout = new QVBoxLayout(wineTab);
    
    QGroupBox* wineModeGroup = new QGroupBox("Wine Mode");
    QFormLayout* wineModeLayout = new QFormLayout(wineModeGroup);
    
    m_impl->wineModeCombo = new QComboBox();
    m_impl->wineModeCombo->addItem("Built-in (Managed)", "Builtin");
    m_impl->wineModeCombo->addItem("Custom Wine Installation", "User");
    wineModeLayout->addRow("Mode:", m_impl->wineModeCombo);
    
    wineLayout->addWidget(wineModeGroup);
    
    QGroupBox* winePathsGroup = new QGroupBox("Custom Wine Paths");
    QFormLayout* winePathsLayout = new QFormLayout(winePathsGroup);
    
    QHBoxLayout* wineExeLayout = new QHBoxLayout();
    m_impl->winePathEdit = new QLineEdit();
    QPushButton* browseWineBtn = new QPushButton("Browse...");
    wineExeLayout->addWidget(m_impl->winePathEdit);
    wineExeLayout->addWidget(browseWineBtn);
    winePathsLayout->addRow("Wine Executable:", wineExeLayout);
    
    connect(browseWineBtn, &QPushButton::clicked, this, &SettingsWindow::browseWineExecutable);
    
    QHBoxLayout* prefixLayout = new QHBoxLayout();
    m_impl->prefixPathEdit = new QLineEdit();
    QPushButton* browsePrefixBtn = new QPushButton("Browse...");
    prefixLayout->addWidget(m_impl->prefixPathEdit);
    prefixLayout->addWidget(browsePrefixBtn);
    winePathsLayout->addRow("Wine Prefix:", prefixLayout);
    
    connect(browsePrefixBtn, &QPushButton::clicked, this, &SettingsWindow::browseWinePrefix);
    
    wineLayout->addWidget(winePathsGroup);
    
    QGroupBox* wineOptionsGroup = new QGroupBox("Wine Options");
    QVBoxLayout* wineOptionsLayout = new QVBoxLayout(wineOptionsGroup);
    
    m_impl->dxvkCheck = new QCheckBox("Enable DXVK (recommended for better performance)");
    m_impl->dxvkCheck->setChecked(true);
    wineOptionsLayout->addWidget(m_impl->dxvkCheck);
    
    m_impl->esyncCheck = new QCheckBox("Enable esync (requires high file limit)");
    m_impl->esyncCheck->setChecked(true);
    wineOptionsLayout->addWidget(m_impl->esyncCheck);
    
    m_impl->fsyncCheck = new QCheckBox("Enable fsync (requires Linux 5.16+)");
    m_impl->fsyncCheck->setChecked(true);
    wineOptionsLayout->addWidget(m_impl->fsyncCheck);
    
    wineLayout->addWidget(wineOptionsGroup);
    wineLayout->addStretch();
    
    tabs->addTab(wineTab, "Wine");
    
    // Update Wine path fields based on mode
    connect(m_impl->wineModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsWindow::updateWineSection);
#endif
    
    mainLayout->addWidget(tabs);
    
    // Maintenance tab
    QWidget* maintenanceTab = new QWidget();
    QVBoxLayout* maintenanceLayout = new QVBoxLayout(maintenanceTab);
    
    QGroupBox* updateGroup = new QGroupBox("Updates");
    QVBoxLayout* updateLayout = new QVBoxLayout(updateGroup);
    
    QLabel* updateLabel = new QLabel("Check for and download game updates");
    updateLayout->addWidget(updateLabel);
    
    QPushButton* checkUpdatesBtn = new QPushButton("Check for Updates");
    checkUpdatesBtn->setFixedWidth(180);
    connect(checkUpdatesBtn, &QPushButton::clicked, this, [this]() {
        // Get game config for path
        auto& configManager = ConfigManager::instance();
        auto gameConfig = configManager.getGameConfig(m_impl->gameId.toStdString());
        if (!gameConfig) {
            QMessageBox::warning(this, "Error", "Game not configured");
            return;
        }
        
        // Get patch server URL from services info
        QString datacenterUrl = getDatacenterUrl(m_impl->gameId);
        auto future = fetchGameServicesInfo(datacenterUrl, m_impl->gameId);
        future.waitForFinished();
        auto servicesInfo = future.result();
        if (!servicesInfo) {
            QMessageBox::warning(this, "Error", "Could not get patch server info");
            return;
        }
        
        PatchDialog dialog(gameConfig->gameDirectory, servicesInfo->patchServer, this);
        dialog.startPatching();
    });
    updateLayout->addWidget(checkUpdatesBtn);
    
    maintenanceLayout->addWidget(updateGroup);
    
    QGroupBox* repairGroup = new QGroupBox("Repair");
    QVBoxLayout* repairLayout = new QVBoxLayout(repairGroup);
    
    QLabel* repairLabel = new QLabel("Delete corrupted files and re-download them");
    repairLayout->addWidget(repairLabel);
    
    QPushButton* repairBtn = new QPushButton("Repair Game");
    repairBtn->setFixedWidth(180);
    connect(repairBtn, &QPushButton::clicked, this, [this]() {
        // Get game config for path
        auto& configManager = ConfigManager::instance();
        auto gameConfig = configManager.getGameConfig(m_impl->gameId.toStdString());
        if (!gameConfig) {
            QMessageBox::warning(this, "Error", "Game not configured");
            return;
        }
        
        int response = QMessageBox::question(this, "Repair Game",
            "This will delete cached files and re-download them.\n"
            "This may take a while and use significant bandwidth.\n\n"
            "Continue?",
            QMessageBox::Yes | QMessageBox::No);
            
        if (response != QMessageBox::Yes) {
            return;
        }
        
        // Delete known problem files
        std::vector<std::string> filesToDelete = {
            "patchcache.bin"
        };
        
        for (const auto& file : filesToDelete) {
            auto path = gameConfig->gameDirectory / file;
            if (std::filesystem::exists(path)) {
                std::filesystem::remove(path);
                spdlog::info("Deleted: {}", path.string());
            }
        }
        
        // Now run patcher
        QString datacenterUrl = getDatacenterUrl(m_impl->gameId);
        auto future = fetchGameServicesInfo(datacenterUrl, m_impl->gameId);
        future.waitForFinished();
        auto servicesInfo = future.result();
        if (!servicesInfo) {
            QMessageBox::warning(this, "Error", "Could not get patch server info");
            return;
        }
        
        PatchDialog dialog(gameConfig->gameDirectory, servicesInfo->patchServer, this);
        if (dialog.startPatching()) {
            QMessageBox::information(this, "Repair Complete", "Game files have been repaired");
        }
    });
    repairLayout->addWidget(repairBtn);
    
    maintenanceLayout->addWidget(repairGroup);
    maintenanceLayout->addStretch();
    
    tabs->addTab(maintenanceTab, "Maintenance");
    
    // Buttons
    m_impl->buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    
    connect(m_impl->buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveSettings();
        accept();
    });
    connect(m_impl->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_impl->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &SettingsWindow::apply);
    
    mainLayout->addWidget(m_impl->buttonBox);
}

void SettingsWindow::loadSettings() {
    auto& configManager = ConfigManager::instance();
    auto gameConfig = configManager.getGameConfig(m_impl->gameId.toStdString());
    
    if (gameConfig) {
        m_impl->gamePathEdit->setText(QString::fromStdString(gameConfig->gameDirectory.string()));
        m_impl->settingsPathEdit->setText(QString::fromStdString(gameConfig->settingsDirectory.string()));
        
        int clientIndex = m_impl->clientTypeCombo->findData(
            gameConfig->clientType == ClientType::Win64 ? "Win64" :
            gameConfig->clientType == ClientType::Win32 ? "Win32" : "Win32Legacy");
        if (clientIndex >= 0) {
            m_impl->clientTypeCombo->setCurrentIndex(clientIndex);
        }
        
        int localeIndex = m_impl->localeCombo->findData(QString::fromStdString(gameConfig->locale));
        if (localeIndex >= 0) {
            m_impl->localeCombo->setCurrentIndex(localeIndex);
        }
        
        m_impl->highResCheck->setChecked(gameConfig->highResEnabled);
    }
    
#ifdef PLATFORM_LINUX
    auto wineConfig = configManager.getWineConfig(m_impl->gameId.toStdString());
    if (wineConfig) {
        m_impl->wineModeCombo->setCurrentIndex(
            wineConfig->prefixMode == WinePrefixMode::Builtin ? 0 : 1);
        m_impl->winePathEdit->setText(QString::fromStdString(wineConfig->userWineExecutable.string()));
        m_impl->prefixPathEdit->setText(QString::fromStdString(wineConfig->userPrefixPath.string()));
        m_impl->dxvkCheck->setChecked(wineConfig->dxvkEnabled);
        m_impl->esyncCheck->setChecked(wineConfig->esyncEnabled);
        m_impl->fsyncCheck->setChecked(wineConfig->fsyncEnabled);
    }
    updateWineSection();
#endif
}

void SettingsWindow::saveSettings() {
    auto& configManager = ConfigManager::instance();
    
    GameConfig gameConfig;
    gameConfig.id = m_impl->gameId.toStdString();
    gameConfig.gameDirectory = m_impl->gamePathEdit->text().toStdString();
    gameConfig.settingsDirectory = m_impl->settingsPathEdit->text().toStdString();
    
    QString clientType = m_impl->clientTypeCombo->currentData().toString();
    if (clientType == "Win64") gameConfig.clientType = ClientType::Win64;
    else if (clientType == "Win32") gameConfig.clientType = ClientType::Win32;
    else gameConfig.clientType = ClientType::Win32Legacy;
    
    gameConfig.locale = m_impl->localeCombo->currentData().toString().toStdString();
    gameConfig.highResEnabled = m_impl->highResCheck->isChecked();
    
    configManager.setGameConfig(m_impl->gameId.toStdString(), gameConfig);
    
#ifdef PLATFORM_LINUX
    WineConfig wineConfig;
    wineConfig.prefixMode = m_impl->wineModeCombo->currentIndex() == 0 ? 
        WinePrefixMode::Builtin : WinePrefixMode::User;
    wineConfig.userWineExecutable = m_impl->winePathEdit->text().toStdString();
    wineConfig.userPrefixPath = m_impl->prefixPathEdit->text().toStdString();
    wineConfig.dxvkEnabled = m_impl->dxvkCheck->isChecked();
    wineConfig.esyncEnabled = m_impl->esyncCheck->isChecked();
    wineConfig.fsyncEnabled = m_impl->fsyncCheck->isChecked();
    
    configManager.setWineConfig(m_impl->gameId.toStdString(), wineConfig);
#endif
    
    configManager.save();
    emit settingsChanged();
    
    spdlog::info("Settings saved");
}

void SettingsWindow::browseGameDirectory() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Game Directory",
        m_impl->gamePathEdit->text());
    if (!dir.isEmpty()) {
        m_impl->gamePathEdit->setText(dir);
    }
}

void SettingsWindow::browseSettingsDirectory() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Settings Directory",
        m_impl->settingsPathEdit->text());
    if (!dir.isEmpty()) {
        m_impl->settingsPathEdit->setText(dir);
    }
}

#ifdef PLATFORM_LINUX
void SettingsWindow::browseWineExecutable() {
    QString file = QFileDialog::getOpenFileName(this, "Select Wine Executable",
        m_impl->winePathEdit->text(), "Wine (wine*)");
    if (!file.isEmpty()) {
        m_impl->winePathEdit->setText(file);
    }
}

void SettingsWindow::browseWinePrefix() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Wine Prefix",
        m_impl->prefixPathEdit->text());
    if (!dir.isEmpty()) {
        m_impl->prefixPathEdit->setText(dir);
    }
}

void SettingsWindow::resetWineToBuiltin() {
    m_impl->wineModeCombo->setCurrentIndex(0);
    m_impl->winePathEdit->clear();
    m_impl->prefixPathEdit->clear();
}

void SettingsWindow::updateWineSection() {
    bool isUser = m_impl->wineModeCombo->currentIndex() == 1;
    m_impl->winePathEdit->setEnabled(isUser);
    m_impl->prefixPathEdit->setEnabled(isUser);
}
#endif

void SettingsWindow::apply() {
    saveSettings();
}

void SettingsWindow::resetToDefaults() {
    // Set default paths based on platform detection
    auto installations = Platform::detectGameInstallations();
    if (!installations.empty()) {
        m_impl->gamePathEdit->setText(QString::fromStdString(installations[0].string()));
    }
    
    m_impl->settingsPathEdit->setText(
        QString::fromStdString(Platform::getDefaultLotroSettingsPath().string()));
    
    m_impl->clientTypeCombo->setCurrentIndex(0);
    m_impl->localeCombo->setCurrentIndex(0);
    m_impl->highResCheck->setChecked(true);
    
#ifdef PLATFORM_LINUX
    m_impl->wineModeCombo->setCurrentIndex(0);
    m_impl->winePathEdit->clear();
    m_impl->prefixPathEdit->clear();
    m_impl->dxvkCheck->setChecked(true);
    m_impl->esyncCheck->setChecked(true);
    m_impl->fsyncCheck->setChecked(true);
#endif
}

} // namespace lotro
