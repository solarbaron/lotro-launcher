/**
 * LOTRO Launcher - LibSecret Credential Store (Linux)
 * 
 * Uses libsecret for secure credential storage on Linux.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef PLATFORM_LINUX

#include "CredentialStore.hpp"

#include <libsecret/secret.h>
#include <glib.h>

#include <spdlog/spdlog.h>

namespace lotro {

namespace {

// Schema for storing LOTRO credentials
const SecretSchema* getCredentialSchema() {
    static const SecretSchema schema = {
        "com.lotro.launcher.credentials",
        SECRET_SCHEMA_NONE,
        {
            {"service", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {"username", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING}
        }
    };
    return &schema;
}

} // anonymous namespace

class LibSecretStore : public CredentialStore {
public:
    LibSecretStore() {
        m_available = checkAvailability();
        if (m_available) {
            spdlog::info("LibSecret credential store initialized");
        } else {
            spdlog::warn("LibSecret not available");
        }
    }
    
    bool storePassword(
        const std::string& service,
        const std::string& username,
        const std::string& password
    ) override {
        if (!m_available) {
            return false;
        }
        
        GError* error = nullptr;
        
        gboolean result = secret_password_store_sync(
            getCredentialSchema(),
            SECRET_COLLECTION_DEFAULT,
            (service + " - " + username).c_str(),  // Label
            password.c_str(),
            nullptr,  // Cancellable
            &error,
            "service", service.c_str(),
            "username", username.c_str(),
            nullptr
        );
        
        if (error) {
            spdlog::error("Failed to store password: {}", error->message);
            g_error_free(error);
            return false;
        }
        
        spdlog::debug("Stored password for: {}", username);
        return result == TRUE;
    }
    
    std::optional<std::string> getPassword(
        const std::string& service,
        const std::string& username
    ) override {
        if (!m_available) {
            return std::nullopt;
        }
        
        GError* error = nullptr;
        
        gchar* password = secret_password_lookup_sync(
            getCredentialSchema(),
            nullptr,  // Cancellable
            &error,
            "service", service.c_str(),
            "username", username.c_str(),
            nullptr
        );
        
        if (error) {
            spdlog::warn("Failed to retrieve password: {}", error->message);
            g_error_free(error);
            return std::nullopt;
        }
        
        if (!password) {
            spdlog::debug("No password found for: {}", username);
            return std::nullopt;
        }
        
        std::string result(password);
        secret_password_free(password);
        
        spdlog::debug("Retrieved password for: {}", username);
        return result;
    }
    
    bool deletePassword(
        const std::string& service,
        const std::string& username
    ) override {
        if (!m_available) {
            return false;
        }
        
        GError* error = nullptr;
        
        gboolean result = secret_password_clear_sync(
            getCredentialSchema(),
            nullptr,  // Cancellable
            &error,
            "service", service.c_str(),
            "username", username.c_str(),
            nullptr
        );
        
        if (error) {
            spdlog::warn("Failed to delete password: {}", error->message);
            g_error_free(error);
            return false;
        }
        
        spdlog::debug("Deleted password for: {}", username);
        return result == TRUE;
    }
    
    bool isAvailable() const override {
        return m_available;
    }
    
private:
    bool checkAvailability() {
        // Try to access the default collection
        GError* error = nullptr;
        SecretService* service = secret_service_get_sync(
            SECRET_SERVICE_LOAD_COLLECTIONS,
            nullptr,
            &error
        );
        
        if (error) {
            spdlog::debug("LibSecret not available: {}", error->message);
            g_error_free(error);
            return false;
        }
        
        if (service) {
            g_object_unref(service);
        }
        
        return true;
    }
    
    bool m_available = false;
};

std::unique_ptr<CredentialStore> createLibSecretStore() {
    return std::make_unique<LibSecretStore>();
}

} // namespace lotro

#endif // PLATFORM_LINUX
