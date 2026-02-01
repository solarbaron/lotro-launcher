/**
 * LOTRO Launcher - Credential Store Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "CredentialStore.hpp"

#include <spdlog/spdlog.h>

namespace lotro {

// Factory method - creates platform-appropriate implementation
std::unique_ptr<CredentialStore> CredentialStore::create() {
#ifdef PLATFORM_LINUX
    // Linux implementation using libsecret
    // Forward declaration - implemented in LibSecretStore.cpp
    extern std::unique_ptr<CredentialStore> createLibSecretStore();
    return createLibSecretStore();
#elif defined(PLATFORM_WINDOWS)
    // Windows implementation using Credential Manager
    // Forward declaration - implemented in WindowsCredentialStore.cpp
    extern std::unique_ptr<CredentialStore> createWindowsCredentialStore();
    return createWindowsCredentialStore();
#else
    spdlog::warn("No credential store implementation for this platform");
    return nullptr;
#endif
}

} // namespace lotro
