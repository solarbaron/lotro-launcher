/**
 * LOTRO Launcher - Deed Browser Widget
 * 
 * Browse and search LOTRO deeds.
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QWidget>
#include "companion/GameDatabase.hpp"

class QLineEdit;
class QComboBox;
class QTableWidget;
class QLabel;
class QTextEdit;

namespace lotro {

/**
 * Widget for browsing deeds
 */
class DeedBrowserWidget : public QWidget {
    Q_OBJECT

public:
    explicit DeedBrowserWidget(QWidget* parent = nullptr);
    ~DeedBrowserWidget() override;
    
    /**
     * Refresh the deed list
     */
    void refresh();

private slots:
    void onSearchChanged(const QString& text);
    void onCategoryChanged(int index);
    void onDeedSelected();

private:
    void setupUi();
    void populateDeeds(const std::vector<Deed>& deeds);
    void showDeedDetails(const Deed& deed);
    
    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_categoryCombo = nullptr;
    QTableWidget* m_deedTable = nullptr;
    QTextEdit* m_detailsView = nullptr;
    QLabel* m_countLabel = nullptr;
    
    std::vector<Deed> m_currentDeeds;
};

} // namespace lotro
