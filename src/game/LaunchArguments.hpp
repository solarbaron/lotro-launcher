/**
 * LOTRO Launcher - Launch Arguments
 * 
 * Builder for game client launch arguments.
 * Based on OneLauncher's get_launch_args function.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>

#include <QString>
#include <QStringList>

#include "core/config/GameConfig.hpp"

namespace lotro {

/**
 * Launch argument builder
 * 
 * Constructs the command line arguments for the LOTRO client.
 * 
 * The template format from the launcher config uses substitution:
 * - {SUBSCRIPTION} -> Account number
 * - {LOGIN} -> Login server URL
 * - {GLS} -> GLS ticket
 * - {CHAT} -> Chat server URL
 * - {LANGUAGE} -> Language code
 * - {PRODUCT} -> Product code
 * - etc.
 */
class LaunchArgumentBuilder {
public:
    LaunchArgumentBuilder() = default;
    
    /**
     * Set the argument template from launcher config
     */
    LaunchArgumentBuilder& setTemplate(const QString& argTemplate);
    
    /**
     * Set account subscription number
     */
    LaunchArgumentBuilder& setSubscription(const QString& subscription);
    
    /**
     * Set login server URL
     */
    LaunchArgumentBuilder& setLoginServer(const QString& loginServer);
    
    /**
     * Set GLS session ticket
     */
    LaunchArgumentBuilder& setTicket(const QString& ticket);
    
    /**
     * Set chat server URL
     */
    LaunchArgumentBuilder& setChatServer(const QString& chatServer);
    
    /**
     * Set language code (en, de, fr)
     */
    LaunchArgumentBuilder& setLanguage(const QString& language);
    
    /**
     * Set product code (LOTRO, DDO, etc.)
     */
    LaunchArgumentBuilder& setProduct(const QString& product);
    
    /**
     * Set auth server URL
     */
    LaunchArgumentBuilder& setAuthServer(const QString& authServer);
    
    /**
     * Set whether high-res textures are enabled
     */
    LaunchArgumentBuilder& setHighResEnabled(bool enabled);
    
    /**
     * Set GLS ticket lifetime in seconds
     */
    LaunchArgumentBuilder& setGlsTicketLifetime(const QString& lifetime);
    
    /**
     * Set custom game settings directory
     */
    LaunchArgumentBuilder& setUserDir(const std::filesystem::path& path);
    
    /**
     * Add extra arguments
     */
    LaunchArgumentBuilder& addExtraArgs(const QStringList& args);
    
    /**
     * Build the final argument list
     * 
     * @return List of arguments to pass to the client
     */
    QStringList build() const;
    
    /**
     * Build as a single string (for display/logging)
     */
    QString buildString() const;
    
private:
    QString substituteTemplate() const;
    
    QString m_template;
    QString m_subscription;
    QString m_loginServer;
    QString m_ticket;
    QString m_chatServer;
    QString m_language = "English";
    QString m_product = "LOTRO";
    QString m_authServer;
    QString m_glsTicketLifetime;
    bool m_highResEnabled = true;
    std::optional<std::filesystem::path> m_userDir;
    QStringList m_extraArgs;
};

/**
 * Default argument template for LOTRO
 * 
 * This matches the server-provided template from
 * gls.lotro.com/launcher/lotro/lotrolauncher.server.config.xml
 * (GameClient.WIN32.ArgTemplate)
 */
constexpr const char* DEFAULT_LOTRO_ARG_TEMPLATE = 
    "-a {SUBSCRIPTION} -h {LOGIN} --glsticketdirect {GLS} "
    "--chatserver {CHAT} --rodat on --language {LANGUAGE} "
    "--gametype {PRODUCT} --authserverurl {AUTHSERVERURL} "
    "--glsticketlifetime {GLSTICKETLIFETIME}";

} // namespace lotro
