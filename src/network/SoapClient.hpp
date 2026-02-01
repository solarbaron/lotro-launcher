/**
 * LOTRO Launcher - SOAP Client
 * 
 * SOAP client for LOTRO authentication and services.
 * Based on OneLauncher's network/soap.py
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
 * SOAP service error
 */
class SoapError : public std::runtime_error {
public:
    explicit SoapError(const std::string& message) 
        : std::runtime_error(message) {}
};

/**
 * SOAP client for LOTRO/DDO authentication
 * 
 * Handles communication with Standing Stone Games' GLS (Game Launcher Service)
 */
class SoapClient {
public:
    /**
     * Create a SOAP client for the specified service URL
     * 
     * @param serviceUrl Base URL for the SOAP service
     */
    explicit SoapClient(const QString& serviceUrl);
    ~SoapClient();
    
    /**
     * Perform a SOAP call
     * 
     * @param operation SOAP operation name
     * @param params Parameters as XML string
     * @return Response XML string
     */
    QFuture<QString> call(const QString& operation, const QString& params);
    
    /**
     * Set timeout for requests
     */
    void setTimeout(int milliseconds);
    
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace lotro
