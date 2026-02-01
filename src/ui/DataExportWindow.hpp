#pragma once

#include <QDialog>
#include <map>
#include "companion/export/DataExporter.hpp"

class QCheckBox;
class QTextEdit;
class QPushButton;

namespace lotro {
    class DataExporter;
}

namespace lotro {

class DataExportWindow : public QDialog {
    Q_OBJECT
public:
    explicit DataExportWindow(DataExporter* exporter, QWidget* parent = nullptr);
    
private slots:
    void onStartClicked();
    void onSelectAllClicked();
    void onUnselectAllClicked();
    void onLogMessage(const QString& msg);
    void onExtractionFinished();

private:
    void setupUi();

    DataExporter* m_exporter;
    std::map<ExtractableElement, QCheckBox*> m_checkBoxes;
    QTextEdit* m_logView;
    QPushButton* m_startButton;
};

} // namespace lotro
