/**
 * LOTRO Launcher - Newsfeed Parser
 * 
 * RSS/Atom feed parsing for launcher news display.
 * Based on OneLauncher's network/game_newsfeed.py
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <QFuture>
#include <QString>

namespace lotro {

/**
 * News item from feed
 */
struct NewsItem {
    QString title;
    QString description;         // HTML content
    QString link;               // URL to full article
    QString author;
    std::chrono::system_clock::time_point publishedDate;
    QString imageUrl;           // Optional thumbnail
    
    /**
     * Get published date as formatted string
     */
    QString publishedDateString() const;
    
    /**
     * Get description with HTML stripped
     */
    QString plainDescription() const;
};

/**
 * Parse a news feed from URL
 * 
 * Supports RSS 2.0 and Atom feeds.
 * 
 * @param feedUrl URL of the news feed
 * @param maxItems Maximum items to return (0 = all)
 * @return List of news items
 */
QFuture<std::vector<NewsItem>> fetchNewsfeed(
    const QString& feedUrl,
    int maxItems = 10
);

/**
 * Parse feed content directly
 * 
 * @param content Feed XML content
 * @param maxItems Maximum items to return
 * @return List of news items
 */
std::vector<NewsItem> parseNewsfeed(
    const QString& content,
    int maxItems = 10
);

} // namespace lotro
