/**
 * LOTRO Launcher - Patch Client Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "PatchClient.hpp"
#include "core/config/ConfigManager.hpp"
#include "core/platform/Platform.hpp"

#ifdef PLATFORM_LINUX
#include "wine/WineManager.hpp"
#endif

#include <QCoreApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>

#include <spdlog/spdlog.h>

namespace lotro {

class PatchClient::Impl {
public:
    Impl(const std::filesystem::path& gameDirectory, const QString& patchClientFilename)
        : m_gameDirectory(gameDirectory)
        , m_patchClientFilename(patchClientFilename)
        , m_process(new QProcess())
    {
    }
    
    ~Impl() {
        if (m_process->state() != QProcess::NotRunning) {
            cancel();
        }
    }
    
    bool patch(const QString& patchServerUrl, bool highResEnabled, const QString& language, PatchProgressCallback progress) {
        m_patching = true;
        m_cancelled = false;
        m_lastError.clear();
        
        spdlog::info("Starting patching from: {} (highres={}, lang={})", 
            patchServerUrl.toStdString(), highResEnabled, language.toStdString());
        
        // Find patchclient.dll
        auto patchClientPath = m_gameDirectory / m_patchClientFilename.toStdString();
        if (!std::filesystem::exists(patchClientPath)) {
            m_lastError = "patchclient.dll not found";
            spdlog::error(m_lastError.toStdString());
            m_patching = false;
            return false;
        }
        
        // Run patching phases
        std::vector<PatchPhase> phases = {
            PatchPhase::FilesOnly,
            PatchPhase::FilesOnly,  // Run twice for self-patching
            PatchPhase::DataOnly
        };
        
        for (auto phase : phases) {
            if (m_cancelled) {
                m_lastError = "Patching cancelled";
                m_patching = false;
                return false;
            }
            
            if (!runPatchPhase(phase, patchServerUrl, highResEnabled, language, progress)) {
                m_patching = false;
                return false;
            }
        }
        
        spdlog::info("Patching completed successfully");
        m_patching = false;
        return true;
    }
    
    bool isPatching() const { return m_patching; }
    
    void cancel() {
        m_cancelled = true;
        if (m_process->state() != QProcess::NotRunning) {
            m_process->terminate();
            if (!m_process->waitForFinished(5000)) {
                m_process->kill();
            }
        }
    }
    
    QString lastError() const { return m_lastError; }
    
    bool needsPatching(const QString& patchServerUrl) {
        // Quick check by running patchclient with check-only flag
        // This is a simplified implementation
        return true; // Default to needing patching
    }
private:
    /**
     * Extract host:port from patch server URL
     * Handles various formats:
     *   "http://patch.lotro.com:6015/path" -> "patch.lotro.com:6015"
     *   "patch.lotro.com:6015" -> "patch.lotro.com:6015"
     *   "patch.lotro.com" -> "patch.lotro.com:6015"
     */
    QString extractPatchServer(const QString& patchServerUrl) {
        QString url = patchServerUrl.trimmed();
        
        // If URL has a scheme, use QUrl
        if (url.startsWith("http://") || url.startsWith("https://")) {
            QUrl parsedUrl(url);
            QString host = parsedUrl.host();
            int port = parsedUrl.port(6015);
            return QString("%1:%2").arg(host).arg(port);
        }
        
        // No scheme - check if it already has host:port format
        if (url.contains(':')) {
            // Already in host:port format, return as-is
            return url;
        }
        
        // Just hostname, add default port
        return QString("%1:6015").arg(url);
    }
    
    bool runPatchPhase(PatchPhase phase, const QString& patchServerUrl,
                       bool highResEnabled, const QString& language,
                       PatchProgressCallback progress) {
        QString phaseStr = (phase == PatchPhase::FilesOnly) ? "filesonly" : "dataonly";
        spdlog::info("Running patch phase: {}", phaseStr.toStdString());
        
        PatchProgress currentProgress;
        currentProgress.phase = phase;
        currentProgress.status = "Initializing...";
        
        if (progress) {
            progress(currentProgress);
        }
        
        // Extract server address (host:port format, no http://)
        QString patchServer = extractPatchServer(patchServerUrl);
        
        // Build full path to PatchClient.dll - OneLauncher passes the full path
        auto patchClientPath = m_gameDirectory / m_patchClientFilename.toStdString();
        QString patchClientFullPath = QString::fromStdString(patchClientPath.string());
        
        // Build arguments for run_patch_client.exe wrapper
        // OneLauncher joins all patchclient args into a single string as the second argument
        // Format: run_patch_client.exe "full/path/to/PatchClient.dll" "server:port --language English --highres --filesonly"
        QString patchArgs = QString("%1 --language %2")
            .arg(patchServer)
            .arg(language);
        
        // Add --highres flag when high-res textures are enabled
        // This tells patchclient.dll to also update the high-res texture dat file
        if (highResEnabled) {
            patchArgs += " --highres";
        }
        
        patchArgs += QString(" --%1").arg(phaseStr);
        
        spdlog::info("Patch client: {}", patchClientFullPath.toStdString());
        spdlog::info("Patch args: {}", patchArgs.toStdString());
        
        // Set working directory to game directory (critical for patchclient.dll to find other DLLs)
        m_process->setWorkingDirectory(QString::fromStdString(m_gameDirectory.string()));
        
        // Build command: run_patch_client.exe "dll_path" "args_string"
        QStringList args;
        args << patchClientFullPath << patchArgs;
        
#ifdef PLATFORM_LINUX
        auto& wineManager = WineManager::instance();
        // Use buildWineArgsForConsoleApp because Proton doesn't capture stdout
        // We need plain Wine to get the patch client output for progress parsing
        QStringList wineArgs = wineManager.buildWineArgsForConsoleApp(
            getPatchClientRunnerPath(), args);
        QProcessEnvironment env = wineManager.getWineEnvironment();
        
        // Use separate channels to properly capture stdout (where progress info comes from)
        m_process->setProcessChannelMode(QProcess::SeparateChannels);
        m_process->setProcessEnvironment(env);
        QString executable = wineArgs.takeFirst();
        spdlog::info("Running: {} {}", executable.toStdString(), wineArgs.join(" ").toStdString());
        m_process->start(executable, wineArgs);
#else
        m_process->start(QString::fromStdString(getPatchClientRunnerPath().string()), args);
#endif
        
        if (!m_process->waitForStarted(30000)) {
            m_lastError = "Failed to start patch process";
            spdlog::error(m_lastError.toStdString());
            return false;
        }
        
        spdlog::info("Patch process started with PID: {}", m_process->processId());
        
        // Read output and parse progress
        QString statusMsg = (phase == PatchPhase::FilesOnly) 
            ? "Checking files..." 
            : "Checking data...";
        currentProgress.status = statusMsg;
        currentProgress.totalBytes = 0;
        currentProgress.currentBytes = 0;
        currentProgress.totalFiles = 0;
        currentProgress.currentFile = 0;
        
        if (progress) {
            progress(currentProgress);
        }
        
        // Wait for process with periodic output reading
        while (m_process->state() == QProcess::Running && !m_cancelled) {
            // Wait for output or timeout
            m_process->waitForReadyRead(200);
            
            // Read stdout for progress info
            while (m_process->canReadLine()) {
                QByteArray rawLine = m_process->readLine();
                QString line = QString::fromUtf8(rawLine).trimmed();
                if (!line.isEmpty()) {
                    spdlog::info("Patch output: {}", line.toStdString());
                    parsePatchLine(line, currentProgress);
                    if (progress) {
                        progress(currentProgress);
                    }
                }
            }
            
            // Also read stderr for error messages
            QByteArray stderrData = m_process->readAllStandardError();
            if (!stderrData.isEmpty()) {
                QString stderrStr = QString::fromUtf8(stderrData).trimmed();
                if (!stderrStr.isEmpty()) {
                    spdlog::warn("Patch stderr: {}", stderrStr.toStdString());
                }
            }
            
            // Process Qt events so UI updates
            QCoreApplication::processEvents();
        }
        
        if (m_cancelled) {
            spdlog::info("Patching cancelled by user");
            return false;
        }
        
        // Read any remaining output
        m_process->waitForFinished(30000);
        
        // Read any remaining stdout
        while (m_process->canReadLine()) {
            QString line = QString::fromUtf8(m_process->readLine()).trimmed();
            if (!line.isEmpty()) {
                spdlog::info("Patch output (final): {}", line.toStdString());
                parsePatchLine(line, currentProgress);
            }
        }
        
        // Exit code 0 means success
        int exitCode = m_process->exitCode();
        spdlog::info("Patch process exited with code: {}", exitCode);
        
        if (exitCode != 0) {
            // Read any error output
            QString stderrStr = QString::fromUtf8(m_process->readAllStandardError());
            m_lastError = QString("Patch phase %1 failed with exit code %2. %3")
                .arg(phaseStr).arg(exitCode).arg(stderrStr);
            spdlog::error(m_lastError.toStdString());
            currentProgress.status = "Error: " + m_lastError;
            if (progress) {
                progress(currentProgress);
            }
            return false;
        }
        
        // Mark phase complete
        currentProgress.status = (phase == PatchPhase::FilesOnly) 
            ? "Files up to date" 
            : "Data patching complete";
        if (currentProgress.totalBytes > 0) {
            currentProgress.currentBytes = currentProgress.totalBytes;
        }
        if (currentProgress.totalFiles > 0) {
            currentProgress.currentFile = currentProgress.totalFiles;
        }
        
        if (progress) {
            progress(currentProgress);
        }
        
        spdlog::info("Patch phase {} completed successfully", phaseStr.toStdString());
        return true;
    }
    
    void parsePatchLine(const QString& line, PatchProgress& progress) {
        // Parse PatchClient.dll output
        // Format examples:
        //   "Connecting to patch.lotro.com:6015"
        //   "Checking files...files to patch: 0 bytes to download: 0"
        //   "checking data...data patches: 897 bytes to download: 86977801"
        //   "Downloading client_cell_1.dat-33186.........."
        //   "Applying 899 forward iterations..."
        //   "File patching complete"
        //   "Data patching complete."
        
        if (line.startsWith("Connecting")) {
            progress.status = "Connecting to patch server...";
        }
        else if (line.contains("Checking files") || line.contains("checking data")) {
            progress.status = "Checking for updates...";
            
            // Parse "bytes to download: NNNN"
            QRegularExpression bytesRe(R"(bytes to download:\s*(\d+))");
            auto match = bytesRe.match(line);
            if (match.hasMatch()) {
                progress.totalBytes = match.captured(1).toInt();
            }
            
            // Parse "patches: NNN" or "files to patch: NNN"
            QRegularExpression patchesRe(R"((?:patches|files to patch):\s*(\d+))");
            match = patchesRe.match(line);
            if (match.hasMatch()) {
                progress.totalFiles = match.captured(1).toInt();
            }
        }
        else if (line.startsWith("Downloading")) {
            progress.status = "Downloading...";
            
            // Extract filename: "Downloading client_cell_1.dat-33186"
            QRegularExpression fileRe(R"(Downloading\s+([\w_.-]+))");
            auto match = fileRe.match(line);
            if (match.hasMatch()) {
                progress.currentFileName = match.captured(1);
                progress.currentFile++;
            }
        }
        else if (line.startsWith("Applying")) {
            progress.status = "Applying patches...";
        }
        else if (line.contains("patching complete") || line.contains("Patching complete")) {
            progress.status = "Complete";
            progress.currentFile = progress.totalFiles;
            progress.currentBytes = progress.totalBytes;
        }
    }
    
    std::filesystem::path m_gameDirectory;
    QString m_patchClientFilename;
    std::unique_ptr<QProcess> m_process;
    bool m_patching = false;
    bool m_cancelled = false;
    QString m_lastError;
};

PatchClient::PatchClient(
    const std::filesystem::path& gameDirectory,
    const QString& patchClientFilename
) : m_impl(std::make_unique<Impl>(gameDirectory, patchClientFilename))
{
}

PatchClient::~PatchClient() = default;

bool PatchClient::patch(const QString& patchServerUrl, bool highResEnabled, const QString& language, PatchProgressCallback progress) {
    return m_impl->patch(patchServerUrl, highResEnabled, language, progress);
}

bool PatchClient::isPatching() const {
    return m_impl->isPatching();
}

void PatchClient::cancel() {
    m_impl->cancel();
}

QString PatchClient::lastError() const {
    return m_impl->lastError();
}

bool PatchClient::needsPatching(const QString& patchServerUrl) {
    return m_impl->needsPatching(patchServerUrl);
}

std::filesystem::path getPatchClientRunnerPath() {
    // The run_patch_client.exe wrapper that invokes PatchClient.dll
    // First check data path (bundled with launcher)
    auto dataPath = Platform::getDataPath();
    auto bundledPath = dataPath / "bin" / "run_patch_client.exe";
    if (std::filesystem::exists(bundledPath)) {
        return bundledPath;
    }
    
    // Fallback: look in tools directory (development)
    auto devPath = std::filesystem::current_path() / "tools" / "run_patch_client.exe";
    if (std::filesystem::exists(devPath)) {
        return devPath;
    }
    
    // Default path (if neither found)
    return bundledPath;
}

} // namespace lotro
