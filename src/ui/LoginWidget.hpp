/**
 * LOTRO Launcher - Login Widget
 * 
 * Account login and selection UI component.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>

#include <QWidget>
#include <QString>

namespace lotro {

/**
 * Login widget
 * 
 * UI component for:
 * - Account selection dropdown
 * - Username/password entry
 * - Save password checkbox
 * - Login button
 */
class LoginWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit LoginWidget(QWidget* parent = nullptr);
    ~LoginWidget() override;
    
    /**
     * Get the current username
     */
    QString username() const;
    
    /**
     * Set the username
     */
    void setUsername(const QString& username);
    
    /**
     * Get the current password
     */
    QString password() const;
    
    /**
     * Set the password (for auto-fill)
     */
    void setPassword(const QString& password);

    /**
     * Check if "auto login" is enabled
     */
    bool isAutoLoginEnabled() const;

    /**
     * Set "auto login" checkbox state
     */
    void setAutoLoginEnabled(bool enabled);
    
    /**
     * Set the list of saved accounts
     */
    void setSavedAccounts(const QStringList& accounts);
    
    /**
     * Clear the form
     */
    void clear();
    
    /**
     * Enable/disable the login button
     */
    void setLoginEnabled(bool enabled);
    
    /**
     * Show login in progress state
     */
    void setLoggingIn(bool loggingIn);
    
signals:
    /**
     * Emitted when login is requested
     */
    void loginRequested(const QString& username, const QString& password);
    
    /**
     * Emitted when a saved account is selected
     */
    void accountSelected(const QString& username);

    /**
     * Emitted when delete account is requested
     */
    void deleteAccountRequested(const QString& username);

    /**
     * Emitted when auto login state changes
     */
    void autoLoginChanged(bool enabled);
    
private:
    void setupUi();
    void updateLoginButtonState();
    
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace lotro
