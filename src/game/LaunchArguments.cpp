/**
 * LOTRO Launcher - Launch Arguments Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "LaunchArguments.hpp"

#include <QRegularExpression>

#include <spdlog/spdlog.h>

namespace lotro {

LaunchArgumentBuilder& LaunchArgumentBuilder::setTemplate(const QString& argTemplate) {
    m_template = argTemplate;
    return *this;
}

LaunchArgumentBuilder& LaunchArgumentBuilder::setSubscription(const QString& subscription) {
    m_subscription = subscription;
    return *this;
}

LaunchArgumentBuilder& LaunchArgumentBuilder::setLoginServer(const QString& loginServer) {
    m_loginServer = loginServer;
    return *this;
}

LaunchArgumentBuilder& LaunchArgumentBuilder::setTicket(const QString& ticket) {
    m_ticket = ticket;
    return *this;
}

LaunchArgumentBuilder& LaunchArgumentBuilder::setChatServer(const QString& chatServer) {
    m_chatServer = chatServer;
    return *this;
}

LaunchArgumentBuilder& LaunchArgumentBuilder::setLanguage(const QString& language) {
    m_language = language;
    return *this;
}

LaunchArgumentBuilder& LaunchArgumentBuilder::setProduct(const QString& product) {
    m_product = product;
    return *this;
}

LaunchArgumentBuilder& LaunchArgumentBuilder::setAuthServer(const QString& authServer) {
    m_authServer = authServer;
    return *this;
}

LaunchArgumentBuilder& LaunchArgumentBuilder::setHighResEnabled(bool enabled) {
    m_highResEnabled = enabled;
    return *this;
}

LaunchArgumentBuilder& LaunchArgumentBuilder::setGlsTicketLifetime(const QString& lifetime) {
    m_glsTicketLifetime = lifetime;
    return *this;
}

LaunchArgumentBuilder& LaunchArgumentBuilder::setUserDir(const std::filesystem::path& path) {
    m_userDir = path;
    return *this;
}

LaunchArgumentBuilder& LaunchArgumentBuilder::addExtraArgs(const QStringList& args) {
    m_extraArgs.append(args);
    return *this;
}

QString LaunchArgumentBuilder::substituteTemplate() const {
    QString result = m_template;
    
    // Substitute placeholders
    result.replace("{SUBSCRIPTION}", m_subscription);
    result.replace("{LOGIN}", m_loginServer);
    result.replace("{GLS}", m_ticket);
    result.replace("{CHAT}", m_chatServer);
    result.replace("{LANGUAGE}", m_language);
    // Support both {LANG} (server template) and {LANGUAGE} (our template)
    result.replace("{LANG}", m_language);
    result.replace("{PRODUCT}", m_product);
    result.replace("{AUTHSERVERURL}", m_authServer);
    result.replace("{AUTHSERVER}", m_authServer);
    result.replace("{GLSTICKETLIFETIME}", m_glsTicketLifetime);
    
    // Handle user directory
    if (m_userDir) {
        result.replace("{USERDIR}", QString::fromStdString(m_userDir->string()));
    }
    
    return result;
}

QStringList LaunchArgumentBuilder::build() const {
    QStringList args;
    
    // Parse the template
    QString substituted = substituteTemplate();
    
    // Split by whitespace, respecting quoted strings
    QRegularExpression re(R"((?:[^\s"]+|"[^"]*")+)");
    QRegularExpressionMatchIterator i = re.globalMatch(substituted);
    
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString arg = match.captured(0);
        
        // Remove surrounding quotes if present
        if (arg.startsWith('"') && arg.endsWith('"')) {
            arg = arg.mid(1, arg.length() - 2);
        }
        
        // Skip empty placeholders
        if (!arg.isEmpty() && !arg.contains("{")) {
            args << arg;
        }
    }
    
    // Add extra arguments
    args.append(m_extraArgs);
    
    // When high-res is disabled, tell the client the high-res dat file
    // was not updated. This matches OneLauncher's behavior and prevents
    // the client from detecting a mismatch in texture data state.
    if (!m_highResEnabled) {
        args << "--HighResOutOfDate";
    }
    
    return args;
}

QString LaunchArgumentBuilder::buildString() const {
    return build().join(" ");
}

} // namespace lotro
