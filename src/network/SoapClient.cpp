/**
 * LOTRO Launcher - SOAP Client Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "SoapClient.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtConcurrent>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <spdlog/spdlog.h>

namespace lotro {

namespace {
    // SOAP envelope templates
    constexpr const char* SOAP_ENVELOPE_START = R"(<?xml version="1.0" encoding="utf-8"?>
<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/" 
               xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" 
               xmlns:xsd="http://www.w3.org/2001/XMLSchema">
<soap:Body>
)";
    
    constexpr const char* SOAP_ENVELOPE_END = R"(
</soap:Body>
</soap:Envelope>)";
    
    // GLS namespace
    constexpr const char* GLS_NAMESPACE = "http://www.turbine.com/SE/GLS";
}

class SoapClient::Impl {
public:
    explicit Impl(const QString& serviceUrl) 
        : m_serviceUrl(serviceUrl)
        , m_networkManager(new QNetworkAccessManager())
        , m_timeout(30000) // 30 seconds default
    {
        spdlog::debug("SoapClient created for: {}", serviceUrl.toStdString());
    }
    
    ~Impl() = default;
    
    QFuture<QString> call(const QString& operation, const QString& params) {
        return QtConcurrent::run([this, operation, params]() -> QString {
            return executeCall(operation, params);
        });
    }
    
    void setTimeout(int milliseconds) {
        m_timeout = milliseconds;
    }
    
private:
    QString executeCall(const QString& operation, const QString& params) {
        // Build SOAP request
        QString soapBody = QString(SOAP_ENVELOPE_START);
        soapBody += QString("<%1 xmlns=\"%2\">").arg(operation, GLS_NAMESPACE);
        soapBody += params;
        soapBody += QString("</%1>").arg(operation);
        soapBody += SOAP_ENVELOPE_END;
        
        spdlog::debug("SOAP Request to {}: {}", m_serviceUrl.toStdString(), 
                      operation.toStdString());
        
        // Create request
        QNetworkRequest request(m_serviceUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, 
                         "text/xml; charset=utf-8");
        request.setRawHeader("SOAPAction", 
                            QString("\"%1/%2\"").arg(GLS_NAMESPACE, operation).toUtf8());
        
        // Execute request synchronously using event loop
        QEventLoop loop;
        QNetworkReply* reply = m_networkManager->post(request, soapBody.toUtf8());
        
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        
        // Timeout handling
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(m_timeout);
        
        loop.exec();
        
        if (!timer.isActive()) {
            spdlog::error("SOAP request timed out");
            reply->deleteLater();
            throw SoapError("Request timed out");
        }
        
        timer.stop();
        
        if (reply->error() != QNetworkReply::NoError) {
            QString errorMsg = reply->errorString();
            spdlog::error("SOAP request failed: {}", errorMsg.toStdString());
            reply->deleteLater();
            throw SoapError(errorMsg.toStdString());
        }
        
        QString response = QString::fromUtf8(reply->readAll());
        reply->deleteLater();
        
        spdlog::debug("SOAP Response received, length: {}", response.length());
        
        // Extract body from SOAP envelope
        return extractSoapBody(response);
    }
    
    QString extractSoapBody(const QString& soapResponse) {
        // Find the inner content of the SOAP Body
        // We need to extract everything inside <soap:Body>...</soap:Body>
        
        // Simple regex-like approach using indexOf
        int bodyStart = soapResponse.indexOf("<soap:Body>");
        if (bodyStart == -1) {
            bodyStart = soapResponse.indexOf("<Body>");
        }
        
        if (bodyStart == -1) {
            spdlog::warn("No SOAP Body found in response");
            return soapResponse;
        }
        
        // Move past the opening tag
        int contentStart = soapResponse.indexOf(">", bodyStart) + 1;
        
        int bodyEnd = soapResponse.indexOf("</soap:Body>");
        if (bodyEnd == -1) {
            bodyEnd = soapResponse.indexOf("</Body>");
        }
        
        if (bodyEnd == -1 || bodyEnd <= contentStart) {
            spdlog::warn("Malformed SOAP Body");
            return soapResponse;
        }
        
        QString bodyContent = soapResponse.mid(contentStart, bodyEnd - contentStart).trimmed();
        
        spdlog::debug("Extracted SOAP body content length: {}", bodyContent.length());
        
        return bodyContent;
    }
    
    QString m_serviceUrl;
    std::unique_ptr<QNetworkAccessManager> m_networkManager;
    int m_timeout;
};

SoapClient::SoapClient(const QString& serviceUrl)
    : m_impl(std::make_unique<Impl>(serviceUrl))
{
}

SoapClient::~SoapClient() = default;

QFuture<QString> SoapClient::call(const QString& operation, const QString& params) {
    return m_impl->call(operation, params);
}

void SoapClient::setTimeout(int milliseconds) {
    m_impl->setTimeout(milliseconds);
}

} // namespace lotro
