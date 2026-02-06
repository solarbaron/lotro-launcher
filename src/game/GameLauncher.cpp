/**
 * LOTRO Launcher - Game Launcher Implementation
 * 
 * Based on OneLauncher's start_game.py
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "GameLauncher.hpp"
#include "LaunchArguments.hpp"
#include "UserPreferences.hpp"
#include "core/config/ConfigManager.hpp"

#ifdef PLATFORM_LINUX
#include "wine/WineManager.hpp"
#include "wine/WineProcessBuilder.hpp"
#include "core/config/WineConfig.hpp"
#endif

#include <QProcess>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QEventLoop>
#include <QRegularExpression>

#include <spdlog/spdlog.h>

// LOTRO public login queue URL (from OneLauncher's test data)
static const QString LOTRO_LOGIN_QUEUE_URL = "https://gls.lotro.com/GLS.AuthServer/LoginQueue.aspx";

namespace {
// Helper to safely quote strings for shell
QString shellQuote(const QString& s) {
    QString result = s;
    result.replace("'", "'\\''");
    return "'" + result + "'";
}

/**
 * Join the world login queue by POSTing TakeANumber command.
 * This is REQUIRED before connecting to a world - without it, the server rejects the connection.
 * 
 * Based on OneLauncher's WorldLoginQueue.join_queue()
 * 
 * @param accountNumber The subscription/account name
 * @param ticket The GLS session ticket
 * @param worldQueueUrl The world's queue URL (private IP)
 * @return true if queue join succeeded, false otherwise
 */
bool joinWorldQueue(const QString& accountNumber, const QString& ticket, const QString& worldQueueUrl) {
    if (worldQueueUrl.isEmpty()) {
        spdlog::debug("No queue URL provided, skipping queue join");
        return true; // Assume no queue needed
    }
    
    spdlog::info("Joining world login queue...");
    
    // Build POST parameters - based on OneLauncher's login_queue_params_template:
    // "command=TakeANumber&subscription={0}&ticket={1}&ticket_type=GLS&queue_url={2}"
    // 
    // IMPORTANT: Must properly percent-encode all values for form data!
    // QUrlQuery doesn't fully encode characters like + and / in values
    // ALSO: Cannot use QString::arg() because encoded values contain % which
    // gets interpreted as format specifiers (e.g. %2F looks like arg 2)
    QString encodedSubscription = QString::fromUtf8(QUrl::toPercentEncoding(accountNumber));
    QString encodedTicket = QString::fromUtf8(QUrl::toPercentEncoding(ticket));
    QString encodedQueueUrl = QString::fromUtf8(QUrl::toPercentEncoding(worldQueueUrl));
    
    QString postBody = "command=TakeANumber"
                       "&subscription=" + encodedSubscription +
                       "&ticket=" + encodedTicket +
                       "&ticket_type=GLS"
                       "&queue_url=" + encodedQueueUrl;
    
    QNetworkAccessManager manager;
    QUrl url{LOTRO_LOGIN_QUEUE_URL};
    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    
    spdlog::info("POSTing to: {}", LOTRO_LOGIN_QUEUE_URL.toStdString());
    spdlog::info("Subscription: {}", accountNumber.toStdString());
    spdlog::info("Queue URL param: {}", worldQueueUrl.toStdString());
    spdlog::info("Full POST body (first 200 chars): {}", postBody.left(200).toStdString());
    
    QEventLoop loop;
    QNetworkReply* reply = manager.post(request, postBody.toUtf8());
    
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    // Timeout after 15 seconds
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(15000);
    
    loop.exec();
    
    if (!timer.isActive()) {
        spdlog::error("Queue join timed out");
        reply->deleteLater();
        return false;
    }
    timer.stop();
    
    if (reply->error() != QNetworkReply::NoError) {
        spdlog::error("Queue join failed: {}", reply->errorString().toStdString());
        reply->deleteLater();
        return false;
    }
    
    QString response = QString::fromUtf8(reply->readAll());
    reply->deleteLater();
    
    spdlog::debug("Queue response: {}", response.toStdString());
    
    // Parse HResult from XML response
    // Check for error - HRESULT bit 31 set means error
    // See OneLauncher: if hresult >> 31 & 1: # error
    QRegularExpression hresultRegex("<HResult>(0x[0-9A-Fa-f]+)</HResult>");
    auto match = hresultRegex.match(response);
    
    if (match.hasMatch()) {
        QString hresultStr = match.captured(1);
        bool ok;
        quint32 hresult = hresultStr.toUInt(&ok, 16);
        
        if (ok) {
            // Check bit 31 (error indicator per HRESULT spec)
            if (hresult >> 31) {
                spdlog::error("Queue join failed with HResult: {} - This can happen if:", hresultStr.toStdString());
                spdlog::error("  - You've tried logging in too many times (rate limited)");
                spdlog::error("  - The server is down or in maintenance");
                spdlog::error("  - Your account is temporarily suspended");
                spdlog::error("  Please wait a few minutes and try again.");
                return false;
            }
            
            spdlog::info("World queue join successful (HResult: {})", hresultStr.toStdString());
            return true;
        }
    }
    
    // Check for QueueNumber/NowServingNumber as alternative success indicator
    if (response.contains("<QueueNumber>") && response.contains("<NowServingNumber>")) {
        spdlog::info("World queue join successful (got queue position)");
        return true;
    }
    
    // No clear success/failure - log and assume failure
    spdlog::warn("Could not parse queue response - treating as failure");
    return false;
}
}

namespace lotro {

class GameLauncher::Impl {
public:
    explicit Impl(const GameConfig& gameConfig)
        : m_gameConfig(gameConfig)
        , m_process(new QProcess())
        , m_runStartupScripts(true)
        , m_updateUserPreferences(true)
    {
    }
    
    ~Impl() {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->terminate();
            m_process->waitForFinished(3000);
        }
    }
    
    void launch(
        const World& world,
        const QString& ticket,
        const QString& accountNumber,
        const QString& loginServer,
        LaunchCallback callback
    ) {
        m_launching = true;
        
        LaunchResult result;
        
        try {
            spdlog::info("Launching game for world: {}", world.name.toStdString());
            
            // Update user preferences if enabled
            if (m_updateUserPreferences) {
                updateUserPreferences(world);
            }
            
            // Build launch arguments
            auto clientPath = m_gameConfig.getClientExecutable();
            if (!std::filesystem::exists(clientPath)) {
                result.success = false;
                result.errorMessage = "Game client not found: " + 
                    QString::fromStdString(clientPath.string());
                spdlog::error(result.errorMessage.toStdString());
                m_launching = false;
                if (callback) callback(result);
                return;
            }
            
            // Join the world login queue (REQUIRED - server rejects connections without this)
            if (!world.queueUrl.isEmpty()) {
                if (!joinWorldQueue(accountNumber, ticket, world.queueUrl)) {
                    result.success = false;
                    result.errorMessage = "Failed to join world login queue. Please try again.";
                    spdlog::error(result.errorMessage.toStdString());
                    m_launching = false;
                    if (callback) callback(result);
                    return;
                }
            }
            
            LaunchArgumentBuilder argBuilder;
            // TODO: Get template from launcher config
            argBuilder.setTemplate(DEFAULT_LOTRO_ARG_TEMPLATE)
                      .setSubscription(accountNumber)
                      .setLoginServer(loginServer)
                      .setTicket(ticket)
                      .setLanguage(QString::fromStdString(m_gameConfig.locale))
                      .setHighResEnabled(m_gameConfig.highResEnabled);
            
            QStringList args = argBuilder.build();
            
            spdlog::debug("Launch args: {}", argBuilder.buildString().toStdString());
            
#ifdef PLATFORM_LINUX
            // Launch via Wine using launcher's own prefix
            auto& wineManager = WineManager::instance();
            
            // Always use builtin prefix mode (managed by umu-run)
            WineConfig wineConfig = wineManager.config();
            if (wineConfig.prefixMode != WinePrefixMode::User) {
                wineConfig.prefixMode = WinePrefixMode::Builtin;
                wineManager.setConfig(wineConfig);
            }
            
            if (!wineManager.isSetup()) {
                spdlog::info("Wine not set up, initializing...");
                if (!wineManager.setup()) {
                    result.success = false;
                    result.errorMessage = "Failed to set up Wine environment";
                    m_launching = false;
                    if (callback) callback(result);
                    return;
                }
            }
            
            // Create helper batch file to ensure correct working directory
            // This is the standard way to set working directory in Wine - QProcess.setWorkingDirectory
            // only affects the Linux side, not the Wine internal working directory.
            // The batch file runs INSIDE Wine and uses cd /d to set the correct path.
            std::filesystem::path batPath = m_gameConfig.gameDirectory / "lotro-launcher-helper.bat";
            
            {
                QFile batFile(QString::fromStdString(batPath.string()));
                if (batFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream out(&batFile);
                    // Use CRLF for Windows compatibility
                    out << "@echo off\r\n";
                    out << "cd /d \"%~dp0\"\r\n";
                    
                    std::filesystem::path clientRel = std::filesystem::relative(clientPath, m_gameConfig.gameDirectory);
                    QString clientRelWin = QString::fromStdString(clientRel.string()).replace("/", "\\");
                    
                    // Run the game executable with all arguments passed to batch file
                    // Use "start /b" to run without showing a console window
                    out << "start /b \"\" \"" << clientRelWin << "\" %*\r\n";
                    batFile.close();
                    
                    // Ensure it's executable
                    batFile.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | 
                                         QFile::ReadGroup | QFile::ExeGroup);
                } else {
                     spdlog::error("Failed to write batch helper: {}", batPath.string());
                }
            }

            // Launch the batch file via umu-run
            QStringList wineArgs = wineManager.buildWineArgs(batPath, args);
            QProcessEnvironment env = wineManager.getWineEnvironment();
            
            // Set Steam App ID for Proton compatibility (LOTRO = 212500)
            env.insert("SteamAppId", "212500");
            env.insert("SteamGameId", "212500");
            
            m_process->setProcessEnvironment(env);
            
            QString wineExe = wineArgs.takeFirst();
            
            // Log launch details for debugging
            spdlog::info("Wine executable: {}", wineExe.toStdString());
            spdlog::info("Game client: {}", clientPath.string());
            spdlog::info("Launch helper: {}", batPath.string());
            spdlog::info("WINEPREFIX: {}", env.value("WINEPREFIX", "not set").toStdString());
            
            m_process->start(wineExe, wineArgs);
            
#else
            // Windows native launch
            m_process->setWorkingDirectory(
                QString::fromStdString(m_gameConfig.gameDirectory.string()));
            m_process->start(QString::fromStdString(clientPath.string()), args);
#endif
            
            if (!m_process->waitForStarted(10000)) {
                result.success = false;
                result.errorMessage = "Failed to start game process: " + 
                    m_process->errorString();
                spdlog::error(result.errorMessage.toStdString());
            } else {
                result.success = true;
                result.processId = m_process->processId();
                spdlog::info("Game process started with PID: {}", result.processId);
            }
            
        } catch (const std::exception& e) {
            result.success = false;
            result.errorMessage = QString::fromStdString(e.what());
            spdlog::error("Exception launching game: {}", e.what());
        }
        
        m_launching = false;
        
        if (callback) {
            callback(result);
        }
    }
    
    bool isLaunching() const { return m_launching; }
    QProcess* process() const { return m_process.get(); }
    
    void setRunStartupScripts(bool enabled) { m_runStartupScripts = enabled; }
    void setUpdateUserPreferences(bool enabled) { m_updateUserPreferences = enabled; }
    
private:
    void updateUserPreferences(const World& world) {
        auto prefsPath = findUserPreferences(
            m_gameConfig.settingsDirectory,
            m_gameConfig.clientType == ClientType::Win64
        );
        
        if (!prefsPath) {
            spdlog::debug("UserPreferences.ini not found, skipping update");
            return;
        }
        
        UserPreferences prefs(*prefsPath);
        if (!prefs.isValid()) {
            spdlog::warn("Failed to load UserPreferences.ini");
            return;
        }
        
        // Set last world
        prefs.setLastWorld(world.name);
        
        // Save
        if (!prefs.save()) {
            spdlog::warn("Failed to save UserPreferences.ini");
        }
    }
    
    GameConfig m_gameConfig;
    std::unique_ptr<QProcess> m_process;
    bool m_launching = false;
    bool m_runStartupScripts;
    bool m_updateUserPreferences;
};

GameLauncher::GameLauncher(const GameConfig& gameConfig)
    : m_impl(std::make_unique<Impl>(gameConfig))
{
}

GameLauncher::~GameLauncher() = default;

void GameLauncher::launch(
    const World& world,
    const QString& ticket,
    const QString& accountNumber,
    const QString& loginServer,
    LaunchCallback callback
) {
    m_impl->launch(world, ticket, accountNumber, loginServer, callback);
}

bool GameLauncher::isLaunching() const {
    return m_impl->isLaunching();
}

QProcess* GameLauncher::process() const {
    return m_impl->process();
}

void GameLauncher::setRunStartupScripts(bool enabled) {
    m_impl->setRunStartupScripts(enabled);
}

void GameLauncher::setUpdateUserPreferences(bool enabled) {
    m_impl->setUpdateUserPreferences(enabled);
}

} // namespace lotro
