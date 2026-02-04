/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "shadereditorwidget.h"
#include "assets/model/assetparametermodel.hpp"
#include "glslsyntaxhighlighter.h"

#include <QDebug>
#include <QFile>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextStream>
#include <QVBoxLayout>

ShaderEditorWidget::ShaderEditorWidget(std::shared_ptr<AssetParameterModel> model, const QModelIndex &index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    auto *mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(4, 0, 4, 0);
    mainLay->setSpacing(2);

    // Toggle button
    m_toggleButton = new QPushButton(QStringLiteral("\u25B6 Edit Shader Code"), this);
    m_toggleButton->setFlat(true);
    m_toggleButton->setCursor(Qt::PointingHandCursor);
    m_toggleButton->setStyleSheet(QStringLiteral("text-align: left; padding: 2px 4px;"));
    connect(m_toggleButton, &QPushButton::clicked, this, &ShaderEditorWidget::toggleExpanded);
    mainLay->addWidget(m_toggleButton);

    // Collapsible editor container
    m_editorContainer = new QWidget(this);
    m_editorContainer->setVisible(false);
    auto *editorLay = new QVBoxLayout(m_editorContainer);
    editorLay->setContentsMargins(0, 0, 0, 0);
    editorLay->setSpacing(2);

    // Code editor
    m_editor = new QPlainTextEdit(m_editorContainer);
    m_editor->setReadOnly(false);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont monoFont(QStringLiteral("Consolas"));
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(9);
    // StyleHint ensures fallback to system monospace if Consolas is unavailable
    m_editor->setFont(monoFont);
    m_editor->setTabStopDistance(QFontMetrics(monoFont).horizontalAdvance(QLatin1Char(' ')) * 4);
    m_editor->setMinimumHeight(300);
    m_editor->setMaximumHeight(600);
    editorLay->addWidget(m_editor);

    // Syntax highlighter
    m_highlighter = new GlslSyntaxHighlighter(m_editor->document());

    // Button row
    auto *buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->addStretch();

    m_applyButton = new QPushButton(QStringLiteral("Apply"), m_editorContainer);
    connect(m_applyButton, &QPushButton::clicked, this, &ShaderEditorWidget::applyChanges);
    buttonRow->addWidget(m_applyButton);

    m_revertButton = new QPushButton(QStringLiteral("Revert"), m_editorContainer);
    connect(m_revertButton, &QPushButton::clicked, this, &ShaderEditorWidget::revertChanges);
    buttonRow->addWidget(m_revertButton);

    editorLay->addLayout(buttonRow);
    mainLay->addWidget(m_editorContainer);

    // Load initial content
    loadFromFile();
}

void ShaderEditorWidget::toggleExpanded()
{
    m_expanded = !m_expanded;
    m_editorContainer->setVisible(m_expanded);
    m_toggleButton->setText(m_expanded ? QStringLiteral("\u25BC Edit Shader Code") : QStringLiteral("\u25B6 Edit Shader Code"));
    if (m_expanded) {
        loadFromFile();
    }
    Q_EMIT updateHeight();
}

QString ShaderEditorWidget::shaderPath() const
{
    // Look up the shader_path param from the model
    QModelIndex pathIndex = m_model->getParamIndexFromName(QStringLiteral("shader_path"));
    if (pathIndex.isValid()) {
        return m_model->data(pathIndex, AssetParameterModel::ValueRole).toString();
    }
    return {};
}

void ShaderEditorWidget::loadFromFile()
{
    // Primary source: shader_text from m_params (contains full text with //!PARAM blocks)
    QModelIndex textIndex = m_model->getParamIndexFromName(QStringLiteral("shader_text"));
    if (textIndex.isValid()) {
        const QString text = m_model->data(textIndex, AssetParameterModel::ValueRole).toString();
        if (!text.isEmpty()) {
            m_editor->setPlainText(text);
            m_applyButton->setEnabled(true);
            return;
        }
    }

    // Fallback: read from shader_path file (before import runs)
    const QString path = shaderPath();
    if (path.isEmpty()) {
        m_editor->setPlainText(QStringLiteral("// No shader file selected.\n// Select a .hook or .glsl file above."));
        m_applyButton->setEnabled(false);
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_editor->setPlainText(QStringLiteral("// Cannot open: %1").arg(path));
        m_applyButton->setEnabled(false);
        return;
    }

    QTextStream stream(&file);
    m_editor->setPlainText(stream.readAll());
    file.close();
    m_applyButton->setEnabled(true);
}

void ShaderEditorWidget::applyChanges()
{
    const QString content = m_editor->toPlainText();
    m_model->setParameter(QStringLiteral("shader_text"), content, true);
}

void ShaderEditorWidget::revertChanges()
{
    loadFromFile();
}

void ShaderEditorWidget::slotRefresh()
{
    // Reload when the model signals a change (e.g. shader_path changed externally)
    if (m_expanded) {
        loadFromFile();
    }
}
