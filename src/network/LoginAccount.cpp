/**
 * LOTRO Launcher - Login Account Implementation
 * 
 * Based on OneLauncher's network/login_account.py
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "LoginAccount.hpp"
#include "SoapClient.hpp"

#include <QXmlStreamReader>
#include <QtConcurrent>

#include <spdlog/spdlog.h>

namespace lotro {

namespace {

std::vector<GameSubscription> parseSubscriptions(QXmlStreamReader& reader) {
    std::vector<GameSubscription> subscriptions;
    GameSubscription current;
    bool inSubscription = false;
    bool inProductTokens = false;
    
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.isStartElement()) {
            QString name = reader.name().toString();
            
            if (name == "GameSubscription") {
                inSubscription = true;
                current = GameSubscription();
            } else if (inSubscription) {
                if (name == "Game") {
                    current.datacenterGameName = reader.readElementText();
                } else if (name == "Name") {
                    current.name = reader.readElementText();
                } else if (name == "Description") {
                    current.description = reader.readElementText();
                } else if (name == "Status") {
                    current.status = reader.readElementText();
                } else if (name == "ExpirationDate") {
                    current.expirationDate = reader.readElementText();
                } else if (name == "NextBillingDate") {
                    current.nextBillingDate = reader.readElementText();
                } else if (name == "ProductTokens") {
                    inProductTokens = true;
                } else if (name == "string" && inProductTokens) {
                    current.productTokens.push_back(reader.readElementText());
                }
            }
        } else if (reader.isEndElement()) {
            QString name = reader.name().toString();
            
            if (name == "GameSubscription") {
                subscriptions.push_back(current);
                inSubscription = false;
            } else if (name == "ProductTokens") {
                inProductTokens = false;
            } else if (name == "Subscriptions") {
                break; // Done parsing subscriptions
            }
        }
    }
    
    return subscriptions;
}

AccountLoginResponse parseLoginResponse(const QString& xml) {
    AccountLoginResponse response;
    QXmlStreamReader reader(xml);
    
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.isStartElement()) {
            QString name = reader.name().toString();
            
            if (name == "Ticket") {
                response.sessionTicket = reader.readElementText();
            } else if (name == "Subscriptions") {
                response.subscriptions = parseSubscriptions(reader);
            }
        }
    }
    
    if (reader.hasError()) {
        spdlog::warn("XML parsing error in login response: {}", 
                     reader.errorString().toStdString());
    }
    
    return response;
}

LoginError parseLoginError(const QString& errorMessage) {
    QString lower = errorMessage.toLower();
    
    if (lower.contains("no subscriber") || 
        lower.contains("password") ||
        lower.contains("username")) {
        return LoginError::WrongCredentials;
    }
    if (lower.contains("account") && lower.contains("lock")) {
        return LoginError::AccountLocked;
    }
    if (lower.contains("maintenance")) {
        return LoginError::MaintenanceMode;
    }
    
    return LoginError::ServiceError;
}

} // anonymous namespace

std::vector<GameSubscription> AccountLoginResponse::getGameSubscriptions(
    const QString& datacenterGameName
) const {
    std::vector<GameSubscription> result;
    for (const auto& sub : subscriptions) {
        if (sub.datacenterGameName == datacenterGameName) {
            result.push_back(sub);
        }
    }
    return result;
}

QString getDatacenterGameName(const QString& gameType) {
    QString type = gameType.toLower();
    
    if (type.startsWith("lotro")) {
        return "LOTRO";
    } else if (type.startsWith("ddo")) {
        return "DDO";
    }
    
    return gameType.toUpper();
}

QFuture<LoginResult> loginAccount(
    const QString& authServer,
    const QString& username,
    const QString& password
) {
    return QtConcurrent::run([authServer, username, password]() -> LoginResult {
        LoginResult result;
        
        try {
            spdlog::info("Logging in user: {}", username.toStdString());
            spdlog::debug("Auth server: {}", authServer.toStdString());
            
            SoapClient client(authServer);
            
            // LoginAccount SOAP operation
            QString params = QString(
                "<username>%1</username>"
                "<password>%2</password>"
                "<additionalInfo></additionalInfo>"
            ).arg(username, password);
            
            auto future = client.call("LoginAccount", params);
            future.waitForFinished();
            
            QString response = future.result();
            
            if (response.isEmpty()) {
                result.error = LoginError::ServiceError;
                result.errorMessage = "Empty response from authentication server";
                spdlog::error("Empty login response");
                return result;
            }
            
            // Check for SOAP fault
            if (response.contains("Fault") || response.contains("fault")) {
                // Extract fault message
                QXmlStreamReader reader(response);
                QString faultMessage;
                while (!reader.atEnd()) {
                    reader.readNext();
                    if (reader.isStartElement() && 
                        (reader.name().toString() == "faultstring" ||
                         reader.name().toString() == "Message")) {
                        faultMessage = reader.readElementText();
                        break;
                    }
                }
                
                result.error = parseLoginError(faultMessage);
                result.errorMessage = faultMessage.isEmpty() ? 
                    "Authentication failed" : faultMessage;
                spdlog::warn("Login failed: {}", result.errorMessage.toStdString());
                return result;
            }
            
            auto loginResponse = parseLoginResponse(response);
            
            if (loginResponse.sessionTicket.isEmpty()) {
                result.error = LoginError::ServiceError;
                result.errorMessage = "No session ticket in response";
                spdlog::error("Login response missing session ticket");
                return result;
            }
            
            result.response = loginResponse;
            spdlog::info("Login successful, got {} subscriptions", 
                        loginResponse.subscriptions.size());
            
            return result;
            
        } catch (const SoapError& e) {
            std::string errorStr = e.what();
            
            // Check for common server errors
            if (errorStr.find("503") != std::string::npos || 
                errorStr.find("Service Unavailable") != std::string::npos) {
                result.error = LoginError::ServiceError;
                result.errorMessage = "LOTRO authentication service is temporarily unavailable. Please try again later.";
            } else if (errorStr.find("500") != std::string::npos || 
                       errorStr.find("Internal Server Error") != std::string::npos) {
                result.error = LoginError::ServiceError;
                result.errorMessage = "LOTRO authentication service encountered an error. Please try again later.";
            } else {
                result.error = LoginError::NetworkError;
                result.errorMessage = QString::fromStdString(errorStr);
            }
            spdlog::error("SOAP error during login: {}", e.what());
            return result;
        } catch (const std::exception& e) {
            result.error = LoginError::NetworkError;
            result.errorMessage = QString::fromStdString(e.what());
            spdlog::error("Error during login: {}", e.what());
            return result;
        }
    });
}

} // namespace lotro
