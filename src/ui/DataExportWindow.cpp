#include "DataExportWindow.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QCheckBox>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QScrollArea>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QDir>

namespace lotro {
    
DataExportWindow::DataExportWindow(DataExporter* exporter, QWidget* parent)
    : QDialog(parent), m_exporter(exporter)
{
    setupUi();
    
    connect(m_exporter, &DataExporter::logMessage, this, &DataExportWindow::onLogMessage);
    connect(m_exporter, &DataExporter::extractionFinished, this, &DataExportWindow::onExtractionFinished);
}

void DataExportWindow::setupUi() {
    setWindowTitle(tr("Export Character Data"));
    resize(950, 650);
    
    auto mainLayout = new QHBoxLayout(this);
    
    // Left: Configuration
    auto configGroup = new QGroupBox(tr("Data to Export"));
    auto configLayout = new QVBoxLayout(configGroup);
    
    // Info label
    auto infoLabel = new QLabel(tr("Select the data you want to extract from the game.\n"
                                   "The game must be running and you must be logged in."));
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #666; font-style: italic; padding: 5px;");
    configLayout->addWidget(infoLabel);
    
    auto scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    auto scrollContent = new QWidget();
    auto scrollLayout = new QVBoxLayout(scrollContent);
    
    // Checkboxes
    auto elements = DataExporter::getSupportedElements();
    for(const auto& def : elements) {
        auto chk = new QCheckBox(def.name);
        chk->setChecked(def.enabledByDefault);
        m_checkBoxes[def.id] = chk;
        scrollLayout->addWidget(chk);
    }
    scrollLayout->addStretch();
    scrollContent->setLayout(scrollLayout);
    scrollArea->setWidget(scrollContent);
    
    configLayout->addWidget(scrollArea);
    
    // Selection buttons
    auto btnLayout = new QHBoxLayout();
    auto btnAll = new QPushButton(tr("Select All"));
    connect(btnAll, &QPushButton::clicked, this, &DataExportWindow::onSelectAllClicked);
    auto btnNone = new QPushButton(tr("Select None"));
    connect(btnNone, &QPushButton::clicked, this, &DataExportWindow::onUnselectAllClicked);
    btnLayout->addWidget(btnAll);
    btnLayout->addWidget(btnNone);
    configLayout->addLayout(btnLayout);
    
    mainLayout->addWidget(configGroup, 1);
    
    // Right: Execution & Results
    auto rightLayout = new QVBoxLayout();
    
    // Action buttons row
    auto actionContainer = new QWidget();
    auto actionLayout = new QHBoxLayout(actionContainer);
    actionLayout->setContentsMargins(0,0,0,0);
    
    m_startButton = new QPushButton(tr("Start Export"));
    m_startButton->setMinimumHeight(40);
    m_startButton->setStyleSheet("font-weight: bold; font-size: 14px;");
    connect(m_startButton, &QPushButton::clicked, this, &DataExportWindow::onStartClicked);
    actionLayout->addWidget(m_startButton);
    
    auto openFolderBtn = new QPushButton(tr("Open Export Folder"));
    openFolderBtn->setMinimumHeight(40);
    connect(openFolderBtn, &QPushButton::clicked, this, []() {
        QString path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) 
                       + "/lotro-launcher/exports";
        QDir().mkpath(path);
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    actionLayout->addWidget(openFolderBtn);
    
    actionLayout->addStretch();
    
    rightLayout->addWidget(actionContainer);
    
    // Results Group
    auto resultsGroup = new QGroupBox(tr("Extraction Log"));
    auto resultsLayout = new QVBoxLayout(resultsGroup);
    m_logView = new QTextEdit();
    m_logView->setReadOnly(true);
    m_logView->setStyleSheet("font-family: monospace; font-size: 11px;");
    m_logView->setPlaceholderText(tr("Export results will appear here..."));
    resultsLayout->addWidget(m_logView);
    rightLayout->addWidget(resultsGroup, 1);
    
    mainLayout->addLayout(rightLayout, 2);
}

void DataExportWindow::onStartClicked() {
    m_startButton->setEnabled(false);
    m_logView->clear();
    
    std::vector<ExtractableElement> toExport;
    for(auto const& [id, chk] : m_checkBoxes) {
        if(chk->isChecked()) toExport.push_back(id);
    }
    
    if (toExport.empty()) {
        m_logView->append("No elements selected! Please select at least one data type to export.");
        m_startButton->setEnabled(true);
        return;
    }

    m_exporter->extract(toExport);
}

void DataExportWindow::onSelectAllClicked() {
    for(auto const& [id, chk] : m_checkBoxes) chk->setChecked(true);
}

void DataExportWindow::onUnselectAllClicked() {
    for(auto const& [id, chk] : m_checkBoxes) chk->setChecked(false);
}

void DataExportWindow::onLogMessage(const QString& msg) {
    m_logView->append(msg);
}

void DataExportWindow::onExtractionFinished() {
    m_startButton->setEnabled(true);
}

} // namespace lotro
