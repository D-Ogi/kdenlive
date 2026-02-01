/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QCompleter>
#include <QPlainTextEdit>
#include <QStringListModel>
#include <QWidget>
#include <memory>

class AssetParameterModel;
class JsSyntaxHighlighter;
class QLabel;
class QPushButton;
class QToolButton;
class QTimer;

/**
 * @class ExpressionPlainTextEdit
 * @brief QPlainTextEdit subclass with QCompleter autocomplete support.
 *
 * Provides inline autocomplete for expression functions, globals,
 * Math.* methods, and JS keywords triggered after typing >= 2 chars.
 */
class ExpressionPlainTextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit ExpressionPlainTextEdit(QWidget *parent = nullptr);

    // Expose protected QPlainTextEdit methods for line number gutter
    using QAbstractScrollArea::setViewportMargins;
    using QPlainTextEdit::blockBoundingGeometry;
    using QPlainTextEdit::blockBoundingRect;
    using QPlainTextEdit::contentOffset;
    using QPlainTextEdit::firstVisibleBlock;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

private Q_SLOTS:
    void insertCompletion(const QString &completion);

private:
    QString wordUnderCursor() const;
    static QStringList completionWordList();

    QCompleter *m_completer;
};

/**
 * @class ExpressionWidget
 * @brief Editor widget for JavaScript parameter expressions.
 *
 * Layout:
 * ┌─────────────────────────────────────┐
 * │ Linked to: "X"           [Detach]   │  (visible when linked)
 * ├─────────────────────────────────────┤
 * │ QPlainTextEdit (3-5 lines, mono)    │  JS expression with syntax highlighting
 * ├─────────────────────────────────────┤
 * │ i linear . ease . wiggle . ... [Templates...] │
 * ├─────────────────────────────────────┤
 * │ Result: 0.73    Valid  [⤢] [Clear] │
 * └─────────────────────────────────────┘
 */
class ExpressionWidget : public QWidget
{
    Q_OBJECT

public:
    /** @brief Create an expression widget.
     *  @param componentIndex -1 for scalar params; 0-4 for AnimatedRect components (X,Y,W,H,Opacity) */
    explicit ExpressionWidget(std::shared_ptr<AssetParameterModel> model, const QString &paramName, QWidget *parent = nullptr, int componentIndex = -1);
    ~ExpressionWidget() override;

    /** @brief Set the expression text (e.g., when loading from project) */
    void setExpression(const QString &expression);
    /** @brief Get the current expression text */
    QString expression() const;

public Q_SLOTS:
    /** @brief Update the live preview at the given frame position */
    void updatePreview(int frame);

Q_SIGNALS:
    /** @brief Emitted when the expression changes (after debounce) */
    void expressionChanged(const QString &expression);

private Q_SLOTS:
    void onTextChanged();
    void onDebounceTimeout();
    void onClearClicked();
    void onTemplatesClicked();
    void onDetachClicked();
    void onExpandClicked();

private:
    void insertFunctionTemplate(const QString &tmpl);
    void buildFunctionBar();
    void updateStatus(const QString &error);
    void updateLinkedState();

    std::shared_ptr<AssetParameterModel> m_model;
    QString m_paramName;
    int m_componentIndex; ///< -1 for scalar, 0-4 for rect components
    ExpressionPlainTextEdit *m_editor;
    JsSyntaxHighlighter *m_highlighter;
    QLabel *m_previewLabel;
    QLabel *m_statusLabel;
    QPushButton *m_clearButton;
    QPushButton *m_expandButton;
    QWidget *m_functionBar;
    QTimer *m_debounceTimer;

    // Linked mode
    QWidget *m_linkedBadge;
    QLabel *m_linkedLabel;
    QPushButton *m_detachButton;
};
