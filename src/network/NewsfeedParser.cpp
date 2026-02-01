/**
 * LOTRO Launcher - Newsfeed Parser Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "NewsfeedParser.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QXmlStreamReader>
#include <QDateTime>
#include <QRegularExpression>
#include <QtConcurrent>
#include <QEventLoop>
#include <QTimer>

#include <spdlog/spdlog.h>

namespace lotro {

namespace {

std::chrono::system_clock::time_point parseDate(const QString& dateStr) {
    // Try various date formats
    QDateTime dt;
    
    // RFC 2822 (common in RSS)
    dt = QDateTime::fromString(dateStr, Qt::RFC2822Date);
    if (dt.isValid()) {
        return std::chrono::system_clock::from_time_t(dt.toSecsSinceEpoch());
    }
    
    // ISO 8601 (common in Atom)
    dt = QDateTime::fromString(dateStr, Qt::ISODate);
    if (dt.isValid()) {
        return std::chrono::system_clock::from_time_t(dt.toSecsSinceEpoch());
    }
    
    // Custom formats
    QStringList formats = {
        "yyyy-MM-ddTHH:mm:ss",
        "yyyy-MM-dd HH:mm:ss",
        "ddd, dd MMM yyyy HH:mm:ss",
        "dd MMM yyyy HH:mm:ss"
    };
    
    for (const auto& format : formats) {
        dt = QDateTime::fromString(dateStr, format);
        if (dt.isValid()) {
            return std::chrono::system_clock::from_time_t(dt.toSecsSinceEpoch());
        }
    }
    
    // Return current time if parsing fails
    return std::chrono::system_clock::now();
}

QString stripHtml(const QString& html) {
    QString result = html;
    
    // Remove HTML tags
    result.remove(QRegularExpression("<[^>]*>"));
    
    // Decode common HTML entities
    result.replace("&nbsp;", " ");
    result.replace("&amp;", "&");
    result.replace("&lt;", "<");
    result.replace("&gt;", ">");
    result.replace("&quot;", "\"");
    result.replace("&#39;", "'");
    
    // Collapse whitespace
    result = result.simplified();
    
    return result;
}

} // anonymous namespace

QString NewsItem::publishedDateString() const {
    auto time_t = std::chrono::system_clock::to_time_t(publishedDate);
    QDateTime dt = QDateTime::fromSecsSinceEpoch(time_t);
    return dt.toString("MMM d, yyyy");
}

QString NewsItem::plainDescription() const {
    return stripHtml(description);
}

std::vector<NewsItem> parseNewsfeed(const QString& content, int maxItems) {
    std::vector<NewsItem> items;
    QXmlStreamReader reader(content);
    
    NewsItem current;
    bool inItem = false;
    bool isAtom = false;
    
    while (!reader.atEnd() && (maxItems == 0 || static_cast<int>(items.size()) < maxItems)) {
        reader.readNext();
        
        if (reader.isStartElement()) {
            QString name = reader.name().toString().toLower();
            
            // Detect feed type
            if (name == "feed") {
                isAtom = true;
            }
            
            // Item/entry start
            if (name == "item" || name == "entry") {
                inItem = true;
                current = NewsItem();
            }
            
            if (inItem) {
                if (name == "title") {
                    current.title = reader.readElementText();
                } else if (name == "description" || name == "summary" || name == "content") {
                    current.description = reader.readElementText();
                } else if (name == "link") {
                    if (isAtom) {
                        // Atom uses href attribute
                        current.link = reader.attributes().value("href").toString();
                        if (current.link.isEmpty()) {
                            current.link = reader.readElementText();
                        }
                    } else {
                        current.link = reader.readElementText();
                    }
                } else if (name == "author" || name == "dc:creator") {
                    // Author might be a complex element in Atom
                    QString authorText = reader.readElementText();
                    if (authorText.isEmpty()) {
                        // Try to find name subelement
                        while (!reader.atEnd()) {
                            reader.readNext();
                            if (reader.isStartElement() && 
                                reader.name().toString().toLower() == "name") {
                                authorText = reader.readElementText();
                                break;
                            }
                            if (reader.isEndElement() && 
                                reader.name().toString().toLower() == "author") {
                                break;
                            }
                        }
                    }
                    current.author = authorText;
                } else if (name == "pubdate" || name == "published" || name == "updated") {
                    current.publishedDate = parseDate(reader.readElementText());
                } else if (name == "enclosure" || name == "media:thumbnail") {
                    current.imageUrl = reader.attributes().value("url").toString();
                }
            }
        } else if (reader.isEndElement()) {
            QString name = reader.name().toString().toLower();
            if (name == "item" || name == "entry") {
                if (!current.title.isEmpty()) {
                    items.push_back(current);
                }
                inItem = false;
            }
        }
    }
    
    if (reader.hasError()) {
        spdlog::warn("XML parsing error in newsfeed: {}", 
                     reader.errorString().toStdString());
    }
    
    return items;
}

QFuture<std::vector<NewsItem>> fetchNewsfeed(const QString& feedUrl, int maxItems) {
    return QtConcurrent::run([feedUrl, maxItems]() -> std::vector<NewsItem> {
        try {
            spdlog::info("Fetching newsfeed from: {}", feedUrl.toStdString());
            
            QNetworkAccessManager manager;
            QNetworkRequest request(feedUrl);
            request.setHeader(QNetworkRequest::UserAgentHeader, "LOTRO-Launcher/1.0");
            
            // Configure SSL to ignore certificate errors (common on Linux)
            QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
            sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
            request.setSslConfiguration(sslConfig);
            
            // Allow all redirects unconditionally
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, 
                                 QNetworkRequest::UserVerifiedRedirectPolicy);
            
            QEventLoop loop;
            QNetworkReply* reply = manager.get(request);
            
            // Handle redirects manually (allows any redirect type)
            QObject::connect(reply, &QNetworkReply::redirected,
                [](const QUrl&) { /* Accept all redirects */ });
            
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            
            QTimer timer;
            timer.setSingleShot(true);
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            timer.start(15000);
            
            loop.exec();
            
            if (!timer.isActive()) {
                spdlog::warn("Newsfeed request timed out");
                reply->deleteLater();
                return {};
            }
            timer.stop();
            
            if (reply->error() != QNetworkReply::NoError) {
                spdlog::warn("Newsfeed request failed: {}", 
                            reply->errorString().toStdString());
                reply->deleteLater();
                return {};
            }
            
            QString content = QString::fromUtf8(reply->readAll());
            reply->deleteLater();
            
            auto items = parseNewsfeed(content, maxItems);
            spdlog::info("Parsed {} news items", items.size());
            
            return items;
            
        } catch (const std::exception& e) {
            spdlog::error("Error fetching newsfeed: {}", e.what());
            return {};
        }
    });
}

} // namespace lotro
