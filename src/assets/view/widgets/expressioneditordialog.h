/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QDialog>
#include <memory>

class AssetParameterModel;
class ExpressionPlainTextEdit;
class JsSyntaxHighlighter;
class QLabel;
class QPlainTextEdit;
class QSpinBox;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;

/**
 * @class ExpressionEditorDialog
 * @brief Resizable dialog with a full-size expression code editor, preview panel,
 *        and context variable inspector.
 *
 * Features:
 * - Large ExpressionPlainTextEdit with autocomplete
 * - Line number gutter
 * - Syntax highlighting
 * - Validation status
 * - Frame spinner + sample table for testing expression output
 * - Context Variables tree showing all available globals and their values
 * - Remembers window size via QSettings
 */
class ExpressionEditorDialog : public QDialog
{
    Q_OBJECT

public:
    /** @brief Create the expanded expression editor dialog.
     *  @param componentIndex -1 for scalar params; 0-4 for AnimatedRect components */
    explicit ExpressionEditorDialog(const QString &expression, std::shared_ptr<AssetParameterModel> model, const QString &paramName, QWidget *parent = nullptr,
                                    int componentIndex = -1);
    ~ExpressionEditorDialog() override;

    /** @brief Return the edited expression text */
    QString expression() const;

private:
    void validate();
    void evaluateAtFrame(int frame);
    void sampleRange();
    void updateContextVariables(int frame);

    /** @brief Evaluate expression at a given frame, return (value, error) */
    std::pair<double, QString> evalAt(const QString &expr, int frame) const;

    /** @brief Helper to add/update a row in the context tree */
    void setContextVar(QTreeWidgetItem *parent, const QString &name, const QString &value);

    std::shared_ptr<AssetParameterModel> m_model;
    QString m_paramName;
    ExpressionPlainTextEdit *m_editor;
    JsSyntaxHighlighter *m_highlighter;
    QLabel *m_statusLabel;

    // Preview panel
    QSpinBox *m_frameSpin;
    QLabel *m_frameResultLabel;
    QTableWidget *m_sampleTable;

    // Context variables
    QTreeWidget *m_contextTree;

    // Cached clip range
    int m_clipIn;
    int m_clipDuration;
    double m_fps;
    int m_componentIndex; ///< -1 for scalar, 0-4 for rect components
};
