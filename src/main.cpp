/**
 * LOTRO Launcher - Cross-platform launcher for Lord of the Rings Online
 * 
 * Copyright (C) 2024
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QIcon>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include "core/config/ConfigManager.hpp"
#include "core/platform/Platform.hpp"
#include "ui/MainWindow.hpp"
#include "ui/SetupWizard.hpp"

#ifdef PLATFORM_LINUX
#include "wine/WineManager.hpp"
#endif

namespace {

void setupLogging() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::info);
    
    auto configPath = lotro::Platform::getConfigPath();
    auto logPath = configPath / "logs" / "launcher.log";
    
    // Ensure log directory exists
    std::filesystem::create_directories(logPath.parent_path());
    
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logPath.string(), 1024 * 1024 * 5, 3);
    file_sink->set_level(spdlog::level::debug);
    
    auto logger = std::make_shared<spdlog::logger>(
        "lotro", spdlog::sinks_init_list{console_sink, file_sink});
    logger->set_level(spdlog::level::debug);
    
    spdlog::set_default_logger(logger);
    spdlog::info("LOTRO Launcher starting up...");
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("LOTRO Launcher");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("lotro-launcher");
    
    // Setup logging
    setupLogging();
    
    // Load and apply dark theme
    QFile styleFile(":/dark_theme.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = QString::fromUtf8(styleFile.readAll());
        app.setStyleSheet(styleSheet);
        spdlog::info("Dark theme loaded successfully");
    } else {
        spdlog::warn("Failed to load dark theme stylesheet");
    }
    
    // Set application icon
    app.setWindowIcon(QIcon(":/icon.png"));
    
    // Parse command line arguments
    QCommandLineParser parser;
    parser.setApplicationDescription("Cross-platform LOTRO Launcher");
    parser.addHelpOption();
    parser.addVersionOption();
    
    QCommandLineOption configDirOption(
        QStringList() << "c" << "config-directory",
        "Configuration directory path",
        "path"
    );
    parser.addOption(configDirOption);
    
    QCommandLineOption gameOption(
        QStringList() << "g" << "game",
        "Game to launch (lotro, lotro-preview)",
        "game",
        "lotro"
    );
    parser.addOption(gameOption);
    
    parser.process(app);
    
    // Initialize configuration
    std::filesystem::path configPath;
    if (parser.isSet(configDirOption)) {
        configPath = parser.value(configDirOption).toStdString();
    } else {
        configPath = lotro::Platform::getConfigPath();
    }
    
    auto& configManager = lotro::ConfigManager::instance();
    if (!configManager.initialize(configPath)) {
        spdlog::error("Failed to initialize configuration");
        return 1;
    }
    
    spdlog::info("Configuration loaded from: {}", configPath.string());
    
    // Check if first run - show setup wizard
    if (configManager.isFirstRun()) {
        spdlog::info("First run detected, showing setup wizard");
        lotro::SetupWizard wizard;
        if (wizard.exec() != QDialog::Accepted) {
            spdlog::info("Setup cancelled by user");
            return 0;
        }
    }
    
#ifdef PLATFORM_LINUX
    // Initialize Wine on Linux
    auto& wineManager = lotro::WineManager::instance();
    if (!wineManager.isSetup()) {
        spdlog::info("Wine environment needs setup");
        // This will be handled by the main window or wizard
    }
#endif
    
    // Show main window
    lotro::MainWindow mainWindow;
    mainWindow.show();
    
    spdlog::info("Main window displayed");
    
    return app.exec();
}
