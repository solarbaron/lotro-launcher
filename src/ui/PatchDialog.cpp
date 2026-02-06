/**
 * LOTRO Launcher - Patch Dialog Implementation
 * 
 * Based on OneLauncher's patch_game_window.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "PatchDialog.hpp"

#include <QApplication>
#include <QScrollBar>
#include <QDateTime>

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
    setWindowTitle("Game Update");
    setMinimumSize(720, 450);
    setModal(true);
    setObjectName("patchDialog");
    
    // Apply dark theme styling
    setStyleSheet(R"(
        #patchDialog {
            background-color: #1a1a2e;
        }
    )");
    
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 20, 20, 20);
    
    // Title
    m_titleLabel = new QLabel("Game Update");
    m_titleLabel->setStyleSheet(R"(
        font-size: 18px; 
        font-weight: bold; 
        color: #c9a227;
        padding-bottom: 5px;
    )");
    layout->addWidget(m_titleLabel);
    
    // Log view - main area showing detailed output
    m_logView = new QTextEdit();
    m_logView->setReadOnly(true);
    m_logView->setMinimumHeight(250);
    m_logView->setStyleSheet(R"(
        QTextEdit {
            background-color: #0d0d15;
            border: 2px solid #3a3a5c;
            border-radius: 6px;
            color: #ffffff;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 11px;
            padding: 10px;
        }
    )");
    layout->addWidget(m_logView, 1);
    
    // Progress section
    auto* progressSection = new QWidget();
    progressSection->setStyleSheet("background-color: transparent;");
    auto* progressLayout = new QVBoxLayout(progressSection);
    progressLayout->setContentsMargins(0, 10, 0, 10);
    progressLayout->setSpacing(8);
    
    // Phase indicator (Phase 1/3)
    m_phaseLabel = new QLabel("Phase 1/3");
    m_phaseLabel->setStyleSheet(R"(
        font-size: 12px;
        font-weight: bold;
        color: #2a9d8f;
    )");
    progressLayout->addWidget(m_phaseLabel);
    
    // Status label (what's happening now)
    m_statusLabel = new QLabel("Preparing...");
    m_statusLabel->setStyleSheet(R"(
        font-size: 14px; 
        font-weight: bold;
        color: #ffffff;
    )");
    progressLayout->addWidget(m_statusLabel);
    
    // Detail label (file being downloaded, etc.)
    m_detailLabel = new QLabel("");
    m_detailLabel->setStyleSheet(R"(
        color: #888888; 
        font-size: 11px;
    )");
    m_detailLabel->setWordWrap(true);
    progressLayout->addWidget(m_detailLabel);
    
    // Progress bar
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat("%p%");
    m_progressBar->setMinimumHeight(28);
    m_progressBar->setStyleSheet(R"(
        QProgressBar {
            border: 2px solid #3a3a5c;
            border-radius: 6px;
            background-color: #0d0d15;
            text-align: center;
            color: white;
            font-weight: bold;
            font-size: 12px;
        }
        QProgressBar::chunk {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #2a9d8f, stop:1 #4fc3b5);
            border-radius: 4px;
        }
    )");
    progressLayout->addWidget(m_progressBar);
    
    layout->addWidget(progressSection);
    
    // Button row
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_actionButton = new QPushButton("Abort");
    m_actionButton->setMinimumWidth(120);
    m_actionButton->setMinimumHeight(36);
    m_actionButton->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a5c;
            border: 2px solid #4a4a6c;
            border-radius: 6px;
            color: white;
            font-weight: bold;
            padding: 8px 20px;
        }
        QPushButton:hover {
            background-color: #4a4a6c;
            border-color: #c9a227;
        }
        QPushButton:pressed {
            background-color: #2a2a4c;
        }
    )");
    connect(m_actionButton, &QPushButton::clicked, this, &PatchDialog::onCancelClicked);
    buttonLayout->addWidget(m_actionButton);
    
    layout->addLayout(buttonLayout);
    
    setLayout(layout);
}

void PatchDialog::appendLog(const QString& message, const QString& color) {
    // Format with timestamp
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString formattedMsg = QString("<span style='color: #666666;'>[%1]</span> <span style='color: %2;'>%3</span>")
        .arg(timestamp)
        .arg(color)
        .arg(message.toHtmlEscaped());
    
    m_logView->append(formattedMsg);
    
    // Auto-scroll to bottom
    QScrollBar* scrollBar = m_logView->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void PatchDialog::updatePhaseDisplay(int currentPhase, int totalPhases) {
    m_currentPhase = currentPhase;
    m_totalPhases = totalPhases;
    m_phaseLabel->setText(QString("Phase %1/%2").arg(currentPhase).arg(totalPhases));
}

bool PatchDialog::startPatching() {
    // Run patch in a timer to allow dialog to show first
    QTimer::singleShot(100, this, &PatchDialog::runPatch);
    
    // Execute dialog modally
    exec();
    
    return m_success;
}

void PatchDialog::runPatch() {
    m_patching = true;
    appendLog("*** Started ***", "#2a9d8f");
    appendLog("Patch server: " + m_patchServerUrl, "#aaaaaa");
    appendLog("Game directory: " + QString::fromStdString(m_gameDirectory.string()), "#aaaaaa");
    
    m_statusLabel->setText("Connecting to patch server...");
    QApplication::processEvents();
    
    int phaseNum = 0;
    
    // Run patching with progress callback
    m_success = m_patchClient->patch(m_patchServerUrl, 
        [this, &phaseNum](const PatchProgress& progress) {
            // Track phase changes
            if (progress.phase == PatchPhase::FilesOnly) {
                if (progress.status.contains("Initializing")) {
                    phaseNum++;
                    QMetaObject::invokeMethod(this, [this, phaseNum]() {
                        updatePhaseDisplay(phaseNum, 3);
                        if (phaseNum == 1) {
                            appendLog("Starting file check (Phase 1)...", "#c9a227");
                        } else {
                            appendLog("Starting file check (Phase 2)...", "#c9a227");
                        }
                    }, Qt::QueuedConnection);
                }
            } else if (progress.phase == PatchPhase::DataOnly) {
                if (progress.status.contains("Initializing") || progress.status.contains("Checking")) {
                    QMetaObject::invokeMethod(this, [this]() {
                        updatePhaseDisplay(3, 3);
                        appendLog("Starting data patch (Phase 3)...", "#c9a227");
                    }, Qt::QueuedConnection);
                }
            }
            
            // Update UI on main thread
            QMetaObject::invokeMethod(this, [this, progress]() {
                updateProgress(progress);
            }, Qt::QueuedConnection);
        }
    );
    
    m_patching = false;
    
    if (m_success) {
        m_statusLabel->setText("Update complete!");
        m_progressBar->setValue(100);
        m_progressBar->setFormat("100%");
        m_actionButton->setText("Close");
        appendLog("*** Finished ***", "#2a9d8f");
        spdlog::info("Patching completed successfully");
    } else {
        m_lastError = m_patchClient->lastError();
        m_statusLabel->setText("Update failed!");
        m_detailLabel->setText(m_lastError);
        m_actionButton->setText("Close");
        appendLog("*** Failed: " + m_lastError + " ***", "#ff6b6b");
        spdlog::error("Patching failed: {}", m_lastError.toStdString());
    }
    
    emit patchingFinished(m_success);
}

void PatchDialog::updateProgress(const PatchProgress& progress) {
    // Update status
    m_statusLabel->setText(progress.status);
    
    // Update detail (current file being processed)
    if (!progress.currentFileName.isEmpty()) {
        m_detailLabel->setText(progress.currentFileName);
        appendLog("Downloading: " + progress.currentFileName, "#8888ff");
    }
    
    // Update progress bar
    if (progress.totalBytes > 0) {
        // Byte-based progress
        int percentage = static_cast<int>((progress.currentBytes * 100) / progress.totalBytes);
        m_progressBar->setValue(percentage);
        
        // Format: Downloaded / Total (percentage)
        QString bytesStr;
        if (progress.totalBytes > 1024 * 1024) {
            bytesStr = QString("%1 / %2 MB")
                .arg(progress.currentBytes / (1024 * 1024))
                .arg(progress.totalBytes / (1024 * 1024));
        } else if (progress.totalBytes > 1024) {
            bytesStr = QString("%1 / %2 KB")
                .arg(progress.currentBytes / 1024)
                .arg(progress.totalBytes / 1024);
        } else {
            bytesStr = QString("%1 / %2 bytes")
                .arg(progress.currentBytes)
                .arg(progress.totalBytes);
        }
        m_progressBar->setFormat(QString("%1 (%p%)").arg(bytesStr));
    } else if (progress.totalFiles > 0) {
        // File count-based progress
        int percentage = (progress.currentFile * 100) / progress.totalFiles;
        m_progressBar->setValue(percentage);
        m_progressBar->setFormat(QString("%1/%2 files (%p%)")
            .arg(progress.currentFile)
            .arg(progress.totalFiles));
    } else {
        // Indeterminate - show activity
        m_progressBar->setFormat(progress.status);
    }
    
    // Log important status changes
    if (progress.status.contains("Checking") && !progress.status.contains("...")) {
        appendLog(progress.status, "#aaaaaa");
    } else if (progress.status.contains("complete") || progress.status.contains("Complete")) {
        appendLog(progress.status, "#2a9d8f");
    } else if (progress.status.contains("Error") || progress.status.contains("failed")) {
        appendLog(progress.status, "#ff6b6b");
    } else if (progress.status.contains("patches:") || progress.status.contains("files to patch:")) {
        appendLog(progress.status, "#c9a227");
    }
}

void PatchDialog::onCancelClicked() {
    if (m_patching && m_patchClient && m_patchClient->isPatching()) {
        m_patchClient->cancel();
        m_statusLabel->setText("Aborting...");
        m_actionButton->setEnabled(false);
        appendLog("*** Aborted by user ***", "#ff6b6b");
    } else {
        accept();
    }
}

} // namespace lotro
