/**
 * LOTRO Launcher - Status Indicator Widget
 * 
 * Colored status indicator for server status display.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QWidget>
#include <QPainter>
#include <QPainterPath>

#include "network/WorldList.hpp"

namespace lotro {

/**
 * Small colored circle indicating status
 * 
 * Note: Header-only implementation, no Q_OBJECT to avoid MOC complexity.
 */
class StatusIndicator : public QWidget {
public:
    explicit StatusIndicator(QWidget* parent = nullptr, int size = 12)
        : QWidget(parent)
        , m_size(size)
        , m_status(WorldStatus::Unknown)
    {
        setFixedSize(m_size, m_size);
        setAttribute(Qt::WA_TranslucentBackground);
    }
    
    void setStatus(WorldStatus status) {
        m_status = status;
        update();
    }
    
    WorldStatus status() const {
        return m_status;
    }
    
    static QColor colorForStatus(WorldStatus status) {
        switch (status) {
            case WorldStatus::Online:
                return QColor("#4CAF50");  // Green
            case WorldStatus::Busy:
                return QColor("#FFC107");  // Amber
            case WorldStatus::Full:
                return QColor("#FF9800");  // Orange
            case WorldStatus::Locked:
                return QColor("#f44336");  // Red
            case WorldStatus::Offline:
                return QColor("#666666");  // Gray
            case WorldStatus::Unknown:
            default:
                return QColor("#888888");  // Light gray
        }
    }
    
    static QString tooltipForStatus(WorldStatus status) {
        switch (status) {
            case WorldStatus::Online:
                return "Online";
            case WorldStatus::Busy:
                return "High Population";
            case WorldStatus::Full:
                return "Server Full";
            case WorldStatus::Locked:
                return "Locked (VIP Only)";
            case WorldStatus::Offline:
                return "Offline";
            case WorldStatus::Unknown:
            default:
                return "Status Unknown";
        }
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        
        QColor color = colorForStatus(m_status);
        
        // Draw filled circle
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        
        int margin = 1;
        painter.drawEllipse(margin, margin, m_size - 2 * margin, m_size - 2 * margin);
        
        // Draw subtle glow for online/busy
        if (m_status == WorldStatus::Online || m_status == WorldStatus::Busy) {
            QColor glowColor = color;
            glowColor.setAlpha(80);
            painter.setBrush(glowColor);
            painter.drawEllipse(-1, -1, m_size + 2, m_size + 2);
        }
    }

private:
    int m_size;
    WorldStatus m_status;
};

} // namespace lotro
