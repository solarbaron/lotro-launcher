/**
 * LOTRO Launcher - Login Widget Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "LoginWidget.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>

namespace lotro {

class LoginWidget::Impl {
public:
    QComboBox* accountSelector = nullptr;
    QPushButton* deleteButton = nullptr;
    QLineEdit* usernameEdit = nullptr;
    QLineEdit* passwordEdit = nullptr;
    QCheckBox* autoLoginCheck = nullptr;
    QPushButton* loginButton = nullptr;
    
    bool loggingIn = false;
};

LoginWidget::LoginWidget(QWidget* parent)
    : QWidget(parent)
    , m_impl(std::make_unique<Impl>())
{
    setupUi();
}

LoginWidget::~LoginWidget() = default;

void LoginWidget::setupUi() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // Account selector with delete button
    QHBoxLayout* accountLayout = new QHBoxLayout();
    m_impl->accountSelector = new QComboBox();
    m_impl->accountSelector->setPlaceholderText("Select saved account...");
    m_impl->accountSelector->setVisible(false); // Hidden until we have saved accounts
    accountLayout->addWidget(m_impl->accountSelector, 1);
    
    m_impl->deleteButton = new QPushButton("🗑");
    m_impl->deleteButton->setToolTip("Delete selected account");
    m_impl->deleteButton->setMaximumWidth(40);
    m_impl->deleteButton->setVisible(false);
    accountLayout->addWidget(m_impl->deleteButton);
    layout->addLayout(accountLayout);
    
    // Username
    QLabel* usernameLabel = new QLabel("Username:");
    m_impl->usernameEdit = new QLineEdit();
    m_impl->usernameEdit->setPlaceholderText("Enter username");
    layout->addWidget(usernameLabel);
    layout->addWidget(m_impl->usernameEdit);
    
    // Password
    QLabel* passwordLabel = new QLabel("Password:");
    m_impl->passwordEdit = new QLineEdit();
    m_impl->passwordEdit->setPlaceholderText("Enter password");
    m_impl->passwordEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(passwordLabel);
    layout->addWidget(m_impl->passwordEdit);

    // Auto login checkbox
    m_impl->autoLoginCheck = new QCheckBox("Log in automatically");
    layout->addWidget(m_impl->autoLoginCheck);
    
    // Login button
    m_impl->loginButton = new QPushButton("Log In");
    m_impl->loginButton->setMinimumHeight(40);
    layout->addWidget(m_impl->loginButton);
    
    // Connections
    connect(m_impl->accountSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int index) {
                if (index >= 0) {
                    QString username = m_impl->accountSelector->currentText();
                    emit accountSelected(username);
                }
            });
    
    connect(m_impl->deleteButton, &QPushButton::clicked, [this]() {
        QString username = m_impl->accountSelector->currentText();
        if (!username.isEmpty()) {
            emit deleteAccountRequested(username);
        }
    });
    
    connect(m_impl->loginButton, &QPushButton::clicked, [this]() {
        emit loginRequested(username(), password());
    });
    
    connect(m_impl->usernameEdit, &QLineEdit::textChanged,
            this, &LoginWidget::updateLoginButtonState);
    connect(m_impl->passwordEdit, &QLineEdit::textChanged,
            this, &LoginWidget::updateLoginButtonState);
            
    connect(m_impl->autoLoginCheck, &QCheckBox::toggled,
            this, &LoginWidget::autoLoginChanged);
    
    // Enter key triggers login
    connect(m_impl->passwordEdit, &QLineEdit::returnPressed,
            m_impl->loginButton, &QPushButton::click);
}

void LoginWidget::updateLoginButtonState() {
    bool canLogin = !m_impl->usernameEdit->text().isEmpty() &&
                   !m_impl->passwordEdit->text().isEmpty() &&
                   !m_impl->loggingIn;
    m_impl->loginButton->setEnabled(canLogin);
}

QString LoginWidget::username() const {
    return m_impl->usernameEdit->text();
}

void LoginWidget::setUsername(const QString& username) {
    m_impl->usernameEdit->setText(username);
}

QString LoginWidget::password() const {
    return m_impl->passwordEdit->text();
}

void LoginWidget::setPassword(const QString& password) {
    m_impl->passwordEdit->setText(password);
}

void LoginWidget::setSavedAccounts(const QStringList& accounts) {
    m_impl->accountSelector->clear();
    m_impl->accountSelector->addItems(accounts);
    m_impl->accountSelector->setVisible(!accounts.isEmpty());
    m_impl->deleteButton->setVisible(!accounts.isEmpty());
}

void LoginWidget::clear() {
    m_impl->usernameEdit->clear();
    m_impl->passwordEdit->clear();
    m_impl->autoLoginCheck->setChecked(false);
}

bool LoginWidget::isAutoLoginEnabled() const {
    return m_impl->autoLoginCheck->isChecked();
}

void LoginWidget::setAutoLoginEnabled(bool enabled) {
    m_impl->autoLoginCheck->setChecked(enabled);
}

void LoginWidget::setLoginEnabled(bool enabled) {
    m_impl->loginButton->setEnabled(enabled);
}

void LoginWidget::setLoggingIn(bool loggingIn) {
    m_impl->loggingIn = loggingIn;
    m_impl->loginButton->setEnabled(!loggingIn);
    m_impl->loginButton->setText(loggingIn ? "Logging in..." : "Log In");
    m_impl->usernameEdit->setEnabled(!loggingIn);
    m_impl->passwordEdit->setEnabled(!loggingIn);
    m_impl->autoLoginCheck->setEnabled(!loggingIn);
    m_impl->accountSelector->setEnabled(!loggingIn);
    m_impl->deleteButton->setEnabled(!loggingIn);
}

} // namespace lotro
