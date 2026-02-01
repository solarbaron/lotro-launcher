/**
 * LOTRO Launcher - Loading Spinner Widget
 * 
 * Animated loading indicator for async operations.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QHBoxLayout>
#include <QLabel>
#include <cmath>

namespace lotro {

/**
 * Animated circular loading spinner
 * 
 * Note: This is a header-only implementation to avoid MOC complexity.
 * No signals/slots are used, just basic QWidget functionality.
 */
class LoadingSpinner : public QWidget {
public:
    explicit LoadingSpinner(QWidget* parent = nullptr, int size = 32)
        : QWidget(parent)
        , m_size(size)
        , m_angle(0)
        , m_color(QColor("#d4af37"))  // LOTRO gold
        , m_timer(new QTimer(this))
    {
        setFixedSize(m_size, m_size);
        
        connect(m_timer, &QTimer::timeout, this, [this]() {
            m_angle = (m_angle + 30) % 360;
            update();
        });
    }
    
    void start() {
        m_timer->start(50);  // 20 FPS
        show();
    }
    
    void stop() {
        m_timer->stop();
        hide();
    }
    
    bool isAnimating() const {
        return m_timer->isActive();
    }
    
    void setColor(const QColor& color) {
        m_color = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        
        int centerX = width() / 2;
        int centerY = height() / 2;
        int radius = (m_size - 6) / 2;
        int dotRadius = 3;
        int numDots = 8;
        
        for (int i = 0; i < numDots; ++i) {
            double angle = (m_angle + i * (360 / numDots)) * M_PI / 180.0;
            int x = centerX + static_cast<int>(radius * std::cos(angle));
            int y = centerY + static_cast<int>(radius * std::sin(angle));
            
            // Fade dots based on position (creates tail effect)
            int alpha = 255 - (i * 255 / numDots);
            QColor dotColor = m_color;
            dotColor.setAlpha(alpha);
            
            painter.setBrush(dotColor);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(QPoint(x, y), dotRadius, dotRadius);
        }
    }

private:
    int m_size;
    int m_angle;
    QColor m_color;
    QTimer* m_timer;
};

/**
 * Inline loading text with spinner
 */
class LoadingLabel : public QWidget {
public:
    explicit LoadingLabel(const QString& text = "Loading...", QWidget* parent = nullptr)
        : QWidget(parent)
        , m_text(text)
    {
        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        
        m_spinner = new LoadingSpinner(this, 20);
        layout->addWidget(m_spinner);
        
        m_label = new QLabel(m_text);
        m_label->setStyleSheet("color: #a0a0a0;");
        layout->addWidget(m_label);
        
        layout->addStretch();
    }
    
    void start(const QString& text = QString()) {
        if (!text.isEmpty()) {
            m_label->setText(text);
        }
        m_spinner->start();
        show();
    }
    
    void stop() {
        m_spinner->stop();
        hide();
    }
    
    void setText(const QString& text) {
        m_label->setText(text);
    }

private:
    QString m_text;
    LoadingSpinner* m_spinner;
    QLabel* m_label;
};

} // namespace lotro
