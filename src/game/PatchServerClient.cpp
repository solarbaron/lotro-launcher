/**
 * PatchServerClient - Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "PatchServerClient.hpp"

#include <spdlog/spdlog.h>

#include <QDataStream>
#include <QDir>
#include <QEventLoop>
#include <QTimer>

namespace lotro {

PatchServerClient::PatchServerClient(
    const std::filesystem::path& gameDirectory,
    QObject* parent
)
    : QObject(parent)
    , m_gameDirectory(gameDirectory)
{
    spdlog::info("PatchServerClient initialized for: {}", gameDirectory.string());
    
    // Pre-scan .dat versions
    auto versions = scanDatVersions(gameDirectory);
    for (const auto& v : versions) {
        m_datVersions[v.datName] = v;
    }
    spdlog::info("Found {} .dat files", m_datVersions.size());
}

PatchServerClient::~PatchServerClient() {
    disconnectFromServer();
}

void PatchServerClient::setPatchServer(const QString& host, quint16 port) {
    m_patchServer = host;
    m_patchPort = port;
}

std::map<QString, uint32_t> PatchServerClient::getCurrentVersions() const {
    std::map<QString, uint32_t> versions;
    for (const auto& [name, info] : m_datVersions) {
        versions[name] = info.version;
    }
    return versions;
}

bool PatchServerClient::connectToServer() {
    if (m_socket && m_socket->isOpen()) {
        return true;
    }

    m_socket = new QSslSocket(this);
    
    // Configure SSL
    m_socket->setPeerVerifyMode(QSslSocket::VerifyNone);  // LOTRO uses self-signed certs
    
    spdlog::info("Connecting to patch server: {}:{}", 
        m_patchServer.toStdString(), m_patchPort);

    m_socket->connectToHostEncrypted(m_patchServer, m_patchPort);

    // Wait for connection with timeout
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    
    connect(m_socket, &QSslSocket::encrypted, &loop, &QEventLoop::quit);
    connect(m_socket, &QSslSocket::errorOccurred, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timer.start(10000);  // 10 second timeout
    loop.exec();
    
    if (!timer.isActive()) {
        m_lastError = "Connection timeout";
        spdlog::error("PatchServerClient: {}", m_lastError.toStdString());
        return false;
    }
    
    timer.stop();
    
    if (!m_socket->isEncrypted()) {
        m_lastError = QString("Connection failed: %1").arg(m_socket->errorString());
        spdlog::error("PatchServerClient: {}", m_lastError.toStdString());
        return false;
    }
    
    spdlog::info("Connected to patch server");
    return true;
}

void PatchServerClient::disconnectFromServer() {
    if (m_socket) {
        if (m_socket->isOpen()) {
            m_socket->close();
        }
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}

QByteArray PatchServerClient::sendRequest(const QByteArray& request) {
    if (!m_socket || !m_socket->isEncrypted()) {
        m_lastError = "Not connected to server";
        return {};
    }

    // Send request
    m_socket->write(request);
    m_socket->flush();

    // Wait for response
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    
    connect(m_socket, &QSslSocket::readyRead, &loop, &QEventLoop::quit);
    connect(m_socket, &QSslSocket::errorOccurred, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timer.start(30000);  // 30 second timeout
    loop.exec();
    
    if (!timer.isActive()) {
        m_lastError = "Response timeout";
        return {};
    }
    
    timer.stop();
    
    // Read all available data
    QByteArray response = m_socket->readAll();
    
    // May need to read more
    while (m_socket->waitForReadyRead(100)) {
        response.append(m_socket->readAll());
    }
    
    return response;
}

PatchCheckResult PatchServerClient::checkForPatches() {
    PatchCheckResult result;
    
    // For now, just return current versions without contacting server
    // The actual protocol needs network capture to implement
    
    spdlog::info("Checking patch status...");
    
    // Log current .dat versions
    for (const auto& [name, info] : m_datVersions) {
        spdlog::debug("  {} version={} files={}", 
            name.toStdString(), info.version, info.fileCount);
    }
    
    // TODO: Implement actual patch server protocol
    // This requires capturing network traffic to understand the format
    
    result.success = true;
    return result;
}

bool PatchServerClient::applyPatches(PatchServerProgressCallback progress) {
    PatchServerProgress p;
    
    // Check what needs patching
    p.phase = PatchServerProgress::CheckingVersions;
    p.status = "Checking versions...";
    if (progress) progress(p);
    
    auto checkResult = checkForPatches();
    if (!checkResult.success) {
        p.phase = PatchServerProgress::Failed;
        p.status = checkResult.error;
        if (progress) progress(p);
        return false;
    }
    
    if (checkResult.isUpToDate()) {
        p.phase = PatchServerProgress::Complete;
        p.status = "Game is up to date";
        if (progress) progress(p);
        return true;
    }
    
    // TODO: Download and apply patches
    // This requires implementing the iteration download and apply logic
    
    p.phase = PatchServerProgress::Complete;
    p.status = "Patching complete";
    if (progress) progress(p);
    
    emit patchingComplete(true);
    return true;
}

QByteArray PatchServerClient::encryptRequest(const QByteArray& plaintext) {
    // TODO: Implement OEMinimalEnvelope encryption
    // This is proprietary and needs reverse engineering
    
    // For now, return plaintext
    return plaintext;
}

QByteArray PatchServerClient::decryptResponse(const QByteArray& ciphertext) {
    // TODO: Implement OEMinimalEnvelope decryption
    // This is proprietary and needs reverse engineering
    
    // For now, return ciphertext
    return ciphertext;
}

QByteArray PatchServerClient::buildVersionCheckRequest() {
    // TODO: Build the actual version check request
    // This needs protocol analysis
    
    return {};
}

bool PatchServerClient::parseVersionCheckResponse(
    const QByteArray& response, 
    PatchCheckResult& result
) {
    // TODO: Parse the actual response
    // This needs protocol analysis
    
    Q_UNUSED(response)
    Q_UNUSED(result)
    return false;
}

} // namespace lotro
