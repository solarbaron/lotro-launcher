/**
 * LOTRO Launcher - Patch Dialog Implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "PatchDialog.hpp"

#include <QApplication>
#include <QThread>
#include <QTimer>

#include <spdlog/spdlog.h>

namespace lotro {

PatchDialog::PatchDialog(const std::filesystem::path& gameDirectory,
                         const QString& patchServerUrl,
                         QWidget* parent)
    : QDialog(parent)
    , m_gameDirectory(gameDirectory)
    , m_patchServerUrl(patchServerUrl)
{
    setupUi();
    
    // Create patch client
    m_patchClient = std::make_unique<PatchClient>(m_gameDirectory);
}

PatchDialog::~PatchDialog() {
    if (m_patchClient && m_patchClient->isPatching()) {
        m_patchClient->cancel();
    }
}

void PatchDialog::setupUi() {
    setWindowTitle("Updating Game");
    setMinimumWidth(450);
    setModal(true);
    
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 20, 20, 20);
    
    // Status label
    m_statusLabel = new QLabel("Initializing...");
    m_statusLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
    layout->addWidget(m_statusLabel);
    
    // File label
    m_fileLabel = new QLabel("");
    m_fileLabel->setStyleSheet("color: #888888; font-size: 11px;");
    m_fileLabel->setWordWrap(true);
    layout->addWidget(m_fileLabel);
    
    // Progress bar
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setStyleSheet(R"(
        QProgressBar {
            border: 1px solid #444;
            border-radius: 4px;
            background-color: #2a2a2a;
            height: 24px;
            text-align: center;
            color: white;
        }
        QProgressBar::chunk {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #4a90d9, stop:1 #5fa0e9);
            border-radius: 3px;
        }
    )");
    layout->addWidget(m_progressBar);
    
    layout->addStretch();
    
    // Cancel button
    m_cancelButton = new QPushButton("Cancel");
    m_cancelButton->setFixedWidth(100);
    connect(m_cancelButton, &QPushButton::clicked, this, &PatchDialog::onCancelClicked);
    
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_cancelButton);
    layout->addLayout(buttonLayout);
    
    setLayout(layout);
}

bool PatchDialog::startPatching() {
    // Run patch in a timer to allow dialog to show first
    QTimer::singleShot(100, this, &PatchDialog::runPatch);
    
    // Execute dialog modally
    exec();
    
    return m_success;
}

void PatchDialog::runPatch() {
    spdlog::info("Starting patch from dialog");
    
    m_statusLabel->setText("Checking for updates...");
    QApplication::processEvents();
    
    // Run patching with progress callback
    m_success = m_patchClient->patch(m_patchServerUrl, 
        [this](const PatchProgress& progress) {
            // Update UI on main thread
            QMetaObject::invokeMethod(this, [this, progress]() {
                updateProgress(progress);
            }, Qt::QueuedConnection);
        }
    );
    
    if (m_success) {
        m_statusLabel->setText("Update complete!");
        m_progressBar->setValue(100);
        m_cancelButton->setText("Close");
        spdlog::info("Patching completed successfully");
    } else {
        m_lastError = m_patchClient->lastError();
        m_statusLabel->setText("Update failed: " + m_lastError);
        m_cancelButton->setText("Close");
        spdlog::error("Patching failed: {}", m_lastError.toStdString());
    }
    
    emit patchingFinished(m_success);
}

void PatchDialog::updateProgress(const PatchProgress& progress) {
    m_statusLabel->setText(progress.status);
    m_progressBar->setValue(progress.percentage());
    
    if (!progress.currentFileName.isEmpty()) {
        m_fileLabel->setText(progress.currentFileName);
    }
    
    // Show file/total if available
    if (progress.totalFiles > 0) {
        m_progressBar->setFormat(QString("%1/%2 files (%p%)")
            .arg(progress.currentFile)
            .arg(progress.totalFiles));
    }
}

void PatchDialog::onCancelClicked() {
    if (m_patchClient && m_patchClient->isPatching()) {
        m_patchClient->cancel();
        m_statusLabel->setText("Cancelling...");
        m_cancelButton->setEnabled(false);
    } else {
        accept();
    }
}

} // namespace lotro
