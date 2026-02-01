/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "expressiontemplate.h"
#include <QDialog>
#include <memory>

class AssetParameterModel;
class JsSyntaxHighlighter;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

/**
 * @class ExpressionTemplateDialog
 * @brief Manager dialog for expression templates (Default / User / Project).
 *
 * Layout:
 * ┌──────────────┬─────────────────────────────────────┐
 * │ QTreeWidget  │ Name / Category / Expression / Desc │
 * │              │ Preview / Apply buttons              │
 * │ [+] [x] [v] [^]                                   │
 * └──────────────┴─────────────────────────────────────┘
 */
class ExpressionTemplateDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @param model If non-null, Apply Linked / Apply Detached buttons are shown.
     * @param paramName The parameter this dialog was opened for (used for Apply).
     */
    explicit ExpressionTemplateDialog(std::shared_ptr<AssetParameterModel> model = nullptr, const QString &paramName = QString(), QWidget *parent = nullptr);
    ~ExpressionTemplateDialog() override;

private Q_SLOTS:
    void onSelectionChanged();
    void onNewTemplate();
    void onDeleteTemplate();
    void onImportTemplate();
    void onExportTemplate();
    void onSaveChanges();
    void onApplyLinked();
    void onApplyDetached();

private:
    void buildTree();
    void populateTree();
    QTreeWidgetItem *findOrCreateCategoryItem(QTreeWidgetItem *tierItem, const QString &category);
    void showTemplateDetails(const ExpressionTemplate &tmpl);
    void clearDetails();
    void setDetailsEditable(bool editable);
    ExpressionTemplate currentTemplateFromUI() const;

    // Left panel
    QTreeWidget *m_tree;
    QPushButton *m_addButton;
    QPushButton *m_deleteButton;
    QPushButton *m_importButton;
    QPushButton *m_exportButton;

    // Right panel
    QLineEdit *m_nameEdit;
    QLineEdit *m_categoryEdit;
    QPlainTextEdit *m_exprEdit;
    JsSyntaxHighlighter *m_highlighter;
    QPlainTextEdit *m_descEdit;
    QLabel *m_previewLabel;
    QLabel *m_tierLabel;
    QPushButton *m_saveButton;
    QPushButton *m_applyLinkedButton;
    QPushButton *m_applyDetachedButton;

    // Context
    std::shared_ptr<AssetParameterModel> m_model;
    QString m_paramName;
    QString m_selectedTemplateId;

    // Tier root items
    QTreeWidgetItem *m_defaultRoot{nullptr};
    QTreeWidgetItem *m_userRoot{nullptr};
    QTreeWidgetItem *m_projectRoot{nullptr};
};
