/**
 * LOTRO Launcher - Setup Wizard Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "SetupWizard.hpp"
#include "core/config/ConfigManager.hpp"
#include "core/platform/Platform.hpp"

#ifdef PLATFORM_LINUX
#include "wine/WineManager.hpp"
#endif

#include <QButtonGroup>
#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include <spdlog/spdlog.h>

namespace lotro {

class SetupWizard::Impl {
public:
  QString gameId = "lotro";
  QString gamePath;
  QString settingsPath;
  QString locale = "en";

  // Widget references for validation
  QLineEdit *gamePathEdit = nullptr;
  QLineEdit *settingsPathEdit = nullptr;
  QComboBox *localeCombo = nullptr;
  QProgressBar *wineProgress = nullptr;
  QLabel *wineStatusLabel = nullptr;
};

// Welcome page
class WelcomePage : public QWizardPage {
public:
  WelcomePage(QWidget *parent = nullptr) : QWizardPage(parent) {
    setTitle("Welcome to LOTRO Launcher");
    setSubTitle(
        "This wizard will help you set up the launcher for the first time.");

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *welcomeText =
        new QLabel("LOTRO Launcher is a cross-platform launcher for Lord of "
                   "the Rings Online "
                   "that provides:\n\n"
                   "• Linux support with Wine management\n"
                   "• Addon management (plugins, skins, music)\n"
                   "• Multi-account support\n"
                   "• Secure credential storage\n\n"
                   "Click 'Next' to begin setup.");
    welcomeText->setWordWrap(true);
    layout->addWidget(welcomeText);
    layout->addStretch();
  }
};

// Game selection page
class GameSelectionPage : public QWizardPage {
public:
  GameSelectionPage(SetupWizard::Impl *impl, QWidget *parent = nullptr)
      : QWizardPage(parent), m_impl(impl) {
    setTitle("Select Game");
    setSubTitle("Choose which game you want to play.");

    QVBoxLayout *layout = new QVBoxLayout(this);

    QButtonGroup *gameGroup = new QButtonGroup(this);

    QRadioButton *lotroBtn = new QRadioButton("Lord of the Rings Online");
    lotroBtn->setChecked(true);
    gameGroup->addButton(lotroBtn, 0);
    layout->addWidget(lotroBtn);

    QRadioButton *lotroPreviewBtn =
        new QRadioButton("LOTRO Preview (Bullroarer)");
    gameGroup->addButton(lotroPreviewBtn, 1);
    layout->addWidget(lotroPreviewBtn);

    QRadioButton *ddoBtn = new QRadioButton("Dungeons & Dragons Online");
    gameGroup->addButton(ddoBtn, 2);
    layout->addWidget(ddoBtn);

    connect(gameGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            [this](int id) {
              switch (id) {
              case 0:
                m_impl->gameId = "lotro";
                break;
              case 1:
                m_impl->gameId = "lotro-preview";
                break;
              case 2:
                m_impl->gameId = "ddo";
                break;
              }
            });

    layout->addStretch();
  }

private:
  SetupWizard::Impl *m_impl;
};

// Game path page
class GamePathPage : public QWizardPage {
public:
  GamePathPage(SetupWizard::Impl *impl, QWidget *parent = nullptr)
      : QWizardPage(parent), m_impl(impl) {
    setTitle("Game Location");
    setSubTitle("Select your game installation directory.");

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *infoLabel = new QLabel(
        "If you haven't installed LOTRO yet, please install it first using "
        "Steam, the standalone launcher, or another method.");
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    layout->addSpacing(20);

    QHBoxLayout *pathLayout = new QHBoxLayout();
    m_impl->gamePathEdit = new QLineEdit();
    QPushButton *browseBtn = new QPushButton("Browse...");
    pathLayout->addWidget(m_impl->gamePathEdit);
    pathLayout->addWidget(browseBtn);

    layout->addWidget(new QLabel("Game Directory:"));
    layout->addLayout(pathLayout);

    connect(browseBtn, &QPushButton::clicked, [this]() {
      QString dir = QFileDialog::getExistingDirectory(
          this, "Select Game Directory", m_impl->gamePathEdit->text());
      if (!dir.isEmpty()) {
        m_impl->gamePathEdit->setText(dir);
      }
    });

#ifndef PLATFORM_LINUX
    // Settings path - only shown on Windows/macOS since on Linux
    // the Wine prefix is auto-managed and the path is determined automatically
    layout->addSpacing(10);

    QHBoxLayout *settingsLayout = new QHBoxLayout();
    m_impl->settingsPathEdit = new QLineEdit();
    m_impl->settingsPathEdit->setText(QString::fromStdString(
        Platform::getDefaultLotroSettingsPath().string()));
    QPushButton *browseSettingsBtn = new QPushButton("Browse...");
    settingsLayout->addWidget(m_impl->settingsPathEdit);
    settingsLayout->addWidget(browseSettingsBtn);

    layout->addWidget(new QLabel("Game Documents Directory:"));
    layout->addLayout(settingsLayout);

    connect(browseSettingsBtn, &QPushButton::clicked, [this]() {
      QString dir = QFileDialog::getExistingDirectory(
          this, "Select Game Documents Directory",
          m_impl->settingsPathEdit->text());
      if (!dir.isEmpty()) {
        m_impl->settingsPathEdit->setText(dir);
      }
    });
#endif

    // Try to auto-detect
    QPushButton *detectBtn = new QPushButton("Auto-Detect");
    layout->addWidget(detectBtn);

    connect(detectBtn, &QPushButton::clicked, [this]() {
      auto installations = Platform::detectGameInstallations();
      if (!installations.empty()) {
        m_impl->gamePathEdit->setText(
            QString::fromStdString(installations[0].string()));
      }
    });

    layout->addStretch();

    registerField("gamePath*", m_impl->gamePathEdit);
#ifndef PLATFORM_LINUX
    registerField("settingsPath*", m_impl->settingsPathEdit);
#endif
  }

  void initializePage() override {
    auto installations = Platform::detectGameInstallations();
    if (!installations.empty() && m_impl->gamePathEdit->text().isEmpty()) {
      m_impl->gamePathEdit->setText(
          QString::fromStdString(installations[0].string()));
    }
  }

  bool validatePage() override {
    QString path = m_impl->gamePathEdit->text();
    if (path.isEmpty()) {
      return false;
    }

    // Check for launcher executable
    std::filesystem::path gamePath(path.toStdString());
    if (!std::filesystem::exists(gamePath / "LotroLauncher.exe")) {
      // Warning but allow to continue
      spdlog::warn("LotroLauncher.exe not found at: {}", path.toStdString());
    }

    m_impl->gamePath = path;
    if (m_impl->settingsPathEdit) {
      m_impl->settingsPath = m_impl->settingsPathEdit->text();
    }
    return true;
  }

private:
  SetupWizard::Impl *m_impl;
};

// Language page
class LanguagePage : public QWizardPage {
public:
  LanguagePage(SetupWizard::Impl *impl, QWidget *parent = nullptr)
      : QWizardPage(parent), m_impl(impl) {
    setTitle("Language");
    setSubTitle("Select your preferred language.");

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_impl->localeCombo = new QComboBox();
    m_impl->localeCombo->addItem("English", "en");
    m_impl->localeCombo->addItem("Deutsch", "de");
    m_impl->localeCombo->addItem("Français", "fr");

    layout->addWidget(new QLabel("Game Language:"));
    layout->addWidget(m_impl->localeCombo);

    layout->addStretch();
  }

  bool validatePage() override {
    m_impl->locale = m_impl->localeCombo->currentData().toString();
    return true;
  }

private:
  SetupWizard::Impl *m_impl;
};

#ifdef PLATFORM_LINUX
// Wine setup page (Linux only)
class WineSetupPage : public QWizardPage {
public:
  WineSetupPage(SetupWizard::Impl *impl, QWidget *parent = nullptr)
      : QWizardPage(parent), m_impl(impl) {
    setTitle("Wine Setup");
    setSubTitle("Set up Wine for running LOTRO on Linux.");

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *infoLabel = new QLabel(
        "LOTRO requires Wine to run on Linux. The launcher can automatically "
        "download and configure Wine for you, or you can use your own "
        "installation.");
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    layout->addSpacing(20);

    QRadioButton *builtinBtn = new QRadioButton("Automatic (recommended)");
    builtinBtn->setChecked(true);
    layout->addWidget(builtinBtn);

    QLabel *builtinInfo = new QLabel(
        "  Downloads Wine-GE and creates a managed prefix with DXVK.");
    builtinInfo->setStyleSheet("color: gray;");
    layout->addWidget(builtinInfo);

    QRadioButton *customBtn = new QRadioButton("Use custom Wine installation");
    layout->addWidget(customBtn);

    layout->addSpacing(20);

    m_impl->wineProgress = new QProgressBar();
    m_impl->wineProgress->setVisible(false);
    layout->addWidget(m_impl->wineProgress);

    m_impl->wineStatusLabel = new QLabel();
    m_impl->wineStatusLabel->setVisible(false);
    layout->addWidget(m_impl->wineStatusLabel);

    layout->addStretch();
  }

  bool validatePage() override {
    // Set up Wine if needed
    auto &wineManager = WineManager::instance();

    if (!wineManager.isSetup()) {
      m_impl->wineProgress->setVisible(true);
      m_impl->wineProgress->setRange(0, 0); // Indeterminate/pulsing mode
      m_impl->wineStatusLabel->setVisible(true);
      m_impl->wineStatusLabel->setWordWrap(true);
      m_impl->wineStatusLabel->setText("Setting up Wine...");

      // Status callback: stream umu-run output to the label
      auto statusCb = [this](const QString &message) {
        m_impl->wineStatusLabel->setText(message);
        QCoreApplication::processEvents();
      };

      // Download progress callback
      auto progressCb = [this](size_t current, size_t total) {
        if (total > 0) {
          m_impl->wineProgress->setRange(0, 100);
          int percent = static_cast<int>((current * 100) / total);
          m_impl->wineProgress->setValue(percent);
        }
      };

      bool success = wineManager.setup(progressCb, statusCb);

      if (!success) {
        m_impl->wineProgress->setRange(0, 100);
        m_impl->wineProgress->setValue(0);
        return false;
      }

      m_impl->wineProgress->setRange(0, 100);
      m_impl->wineProgress->setValue(100);
      m_impl->wineStatusLabel->setText("Wine setup complete!");
    }

    return true;
  }

private:
  SetupWizard::Impl *m_impl;
};
#endif

// Complete page
class CompletePage : public QWizardPage {
public:
  CompletePage(SetupWizard::Impl *impl, QWidget *parent = nullptr)
      : QWizardPage(parent), m_impl(impl) {
    setTitle("Setup Complete");
    setSubTitle("The launcher is ready to use.");

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *completeLabel =
        new QLabel("Setup is complete! You can now use LOTRO Launcher to:\n\n"
                   "• Log in to your account\n"
                   "• Select a server and play\n"
                   "• Manage your addons\n\n"
                   "Click 'Finish' to start the launcher.");
    completeLabel->setWordWrap(true);
    layout->addWidget(completeLabel);

    layout->addStretch();
  }

  void initializePage() override {
    // Save configuration
    auto &configManager = ConfigManager::instance();

    GameConfig gameConfig;
    gameConfig.id = m_impl->gameId.toStdString();
    gameConfig.gameType =
        m_impl->gameId.contains("ddo") ? GameType::DDO : GameType::LOTRO;
    gameConfig.gameDirectory = m_impl->gamePath.toStdString();

    // On Linux, use the Wine prefix settings path for addons
    // This ensures plugins are installed where LOTRO (running under Wine) can
    // find them
#ifdef PLATFORM_LINUX
    auto &wineManager = WineManager::instance();
    if (wineManager.isSetup()) {
      auto wineSettingsPath = wineManager.getWineLotroSettingsPath();
      gameConfig.settingsDirectory = wineSettingsPath;
      spdlog::info("Using Wine prefix settings path: {}",
                   wineSettingsPath.string());
    } else {
      gameConfig.settingsDirectory = m_impl->settingsPath.toStdString();
    }
#else
    gameConfig.settingsDirectory = m_impl->settingsPath.toStdString();
#endif

    gameConfig.locale = m_impl->locale.toStdString();
    gameConfig.clientType = ClientType::Win64;
    gameConfig.highResEnabled = true;

    configManager.setGameConfig(m_impl->gameId.toStdString(), gameConfig);
    configManager.save();

    spdlog::info("Setup complete, configuration saved");
  }

private:
  SetupWizard::Impl *m_impl;
};

SetupWizard::SetupWizard(QWidget *parent)
    : QWizard(parent), m_impl(std::make_unique<Impl>()) {
  setWindowTitle("LOTRO Launcher Setup");
  setMinimumSize(600, 450);

  setupPages();
}

SetupWizard::~SetupWizard() = default;

void SetupWizard::setupPages() {
  addPage(new WelcomePage(this));
  addPage(new GameSelectionPage(m_impl.get(), this));
  addPage(new GamePathPage(m_impl.get(), this));
  addPage(new LanguagePage(m_impl.get(), this));
#ifdef PLATFORM_LINUX
  addPage(new WineSetupPage(m_impl.get(), this));
#endif
  addPage(new CompletePage(m_impl.get(), this));
}

QString SetupWizard::gameId() const { return m_impl->gameId; }

QString SetupWizard::gamePath() const { return m_impl->gamePath; }

void SetupWizard::initializePage(int id) { QWizard::initializePage(id); }

bool SetupWizard::validateCurrentPage() {
  return QWizard::validateCurrentPage();
}

} // namespace lotro
