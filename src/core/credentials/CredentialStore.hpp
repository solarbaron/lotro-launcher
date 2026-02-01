/**
 * LOTRO Launcher - Credential Store
 * 
 * Secure credential storage abstraction.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <optional>
#include <string>

namespace lotro {

/**
 * Abstract credential storage interface
 * 
 * Implementations:
 * - LibSecretStore (Linux): Uses libsecret/gnome-keyring/kwallet
 * - WindowsCredentialStore (Windows): Uses Windows Credential Manager
 */
class CredentialStore {
public:
    virtual ~CredentialStore() = default;
    
    /**
     * Store a password for the given service and username
     * 
     * @param service The service identifier (e.g., "lotro-launcher")
     * @param username The account username
     * @param password The password to store
     * @return true if stored successfully
     */
    virtual bool storePassword(
        const std::string& service,
        const std::string& username,
        const std::string& password
    ) = 0;
    
    /**
     * Retrieve a password for the given service and username
     * 
     * @param service The service identifier
     * @param username The account username
     * @return The password if found, nullopt otherwise
     */
    virtual std::optional<std::string> getPassword(
        const std::string& service,
        const std::string& username
    ) = 0;
    
    /**
     * Delete a password for the given service and username
     * 
     * @param service The service identifier
     * @param username The account username
     * @return true if deleted successfully
     */
    virtual bool deletePassword(
        const std::string& service,
        const std::string& username
    ) = 0;
    
    /**
     * Check if the credential store is available
     */
    virtual bool isAvailable() const = 0;
    
    /**
     * Get the platform-appropriate credential store implementation
     */
    static std::unique_ptr<CredentialStore> create();
    
protected:
    CredentialStore() = default;
};

// Service identifier for LOTRO passwords
constexpr const char* LOTRO_CREDENTIAL_SERVICE = "lotro-launcher";

} // namespace lotro
