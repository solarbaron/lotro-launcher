/**
 * LOTRO Launcher - Login Account
 * 
 * Account authentication and subscription handling.
 * Based on OneLauncher's network/login_account.py
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <QFuture>
#include <QString>

namespace lotro {

/**
 * Game subscription information
 */
struct GameSubscription {
    QString datacenterGameName;  // e.g., "LOTRO"
    QString name;
    QString description;
    std::vector<QString> productTokens;
    QString status;              // e.g., "Active"
    QString expirationDate;
    QString nextBillingDate;
};

/**
 * Account login response
 */
struct AccountLoginResponse {
    QString sessionTicket;
    std::vector<GameSubscription> subscriptions;
    
    /**
     * Get subscriptions for a specific game type
     */
    std::vector<GameSubscription> getGameSubscriptions(
        const QString& datacenterGameName
    ) const;
};

/**
 * Login error types
 */
enum class LoginError {
    None,
    WrongCredentials,      // Username or password incorrect
    NetworkError,          // Connection failed
    ServiceError,          // GLS service error
    AccountLocked,         // Account is locked
    MaintenanceMode        // Servers under maintenance
};

/**
 * Login result
 */
struct LoginResult {
    LoginError error = LoginError::None;
    QString errorMessage;
    std::optional<AccountLoginResponse> response;
    
    bool isSuccess() const { return error == LoginError::None && response.has_value(); }
};

/**
 * Login to game account
 * 
 * @param authServer Authentication server URL (from GameServicesInfo)
 * @param username Account username
 * @param password Account password
 * @return LoginResult with session ticket and subscriptions on success
 */
QFuture<LoginResult> loginAccount(
    const QString& authServer,
    const QString& username,
    const QString& password
);

/**
 * Get the datacenter game name for a game type
 */
QString getDatacenterGameName(const QString& gameType);

} // namespace lotro
