/**
 * LOTRO Launcher - Windows Credential Store
 * 
 * Uses Windows Credential Manager for secure credential storage.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef PLATFORM_WINDOWS

#include "CredentialStore.hpp"

#include <windows.h>
#include <wincred.h>

#include <spdlog/spdlog.h>

namespace lotro {

class WindowsCredentialStore : public CredentialStore {
public:
    WindowsCredentialStore() {
        spdlog::info("Windows credential store initialized");
    }
    
    bool storePassword(
        const std::string& service,
        const std::string& username,
        const std::string& password
    ) override {
        std::wstring targetName = buildTargetName(service, username);
        std::wstring wPassword(password.begin(), password.end());
        
        CREDENTIALW cred = {};
        cred.Type = CRED_TYPE_GENERIC;
        cred.TargetName = const_cast<LPWSTR>(targetName.c_str());
        cred.CredentialBlobSize = static_cast<DWORD>(password.size());
        cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(password.c_str()));
        cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
        cred.UserName = const_cast<LPWSTR>(std::wstring(username.begin(), username.end()).c_str());
        
        if (!CredWriteW(&cred, 0)) {
            DWORD error = GetLastError();
            spdlog::error("Failed to store password, error: {}", error);
            return false;
        }
        
        spdlog::debug("Stored password for: {}", username);
        return true;
    }
    
    std::optional<std::string> getPassword(
        const std::string& service,
        const std::string& username
    ) override {
        std::wstring targetName = buildTargetName(service, username);
        
        PCREDENTIALW pCred = nullptr;
        
        if (!CredReadW(targetName.c_str(), CRED_TYPE_GENERIC, 0, &pCred)) {
            DWORD error = GetLastError();
            if (error != ERROR_NOT_FOUND) {
                spdlog::warn("Failed to retrieve password, error: {}", error);
            }
            return std::nullopt;
        }
        
        std::string password(
            reinterpret_cast<char*>(pCred->CredentialBlob),
            pCred->CredentialBlobSize
        );
        
        CredFree(pCred);
        
        spdlog::debug("Retrieved password for: {}", username);
        return password;
    }
    
    bool deletePassword(
        const std::string& service,
        const std::string& username
    ) override {
        std::wstring targetName = buildTargetName(service, username);
        
        if (!CredDeleteW(targetName.c_str(), CRED_TYPE_GENERIC, 0)) {
            DWORD error = GetLastError();
            if (error != ERROR_NOT_FOUND) {
                spdlog::warn("Failed to delete password, error: {}", error);
            }
            return false;
        }
        
        spdlog::debug("Deleted password for: {}", username);
        return true;
    }
    
    bool isAvailable() const override {
        // Windows Credential Manager is always available on Windows
        return true;
    }
    
private:
    std::wstring buildTargetName(const std::string& service, const std::string& username) {
        std::string combined = service + ":" + username;
        return std::wstring(combined.begin(), combined.end());
    }
};

std::unique_ptr<CredentialStore> createWindowsCredentialStore() {
    return std::make_unique<WindowsCredentialStore>();
}

} // namespace lotro

#endif // PLATFORM_WINDOWS
