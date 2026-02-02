/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractparamwidget.hpp"

class QPlainTextEdit;
class QPushButton;
class GlslSyntaxHighlighter;

/**
 * @class ShaderEditorWidget
 * @brief Collapsible GLSL code editor for the placebo.shader effect.
 *
 * Reads the shader file from the shader_path property, displays it in a
 * monospace QPlainTextEdit with GLSL syntax highlighting. "Apply" saves
 * back to file (triggering MLT mtime-based reload), "Revert" reloads from disk.
 */
class ShaderEditorWidget : public AbstractParamWidget
{
    Q_OBJECT

public:
    ShaderEditorWidget(std::shared_ptr<AssetParameterModel> model, const QModelIndex &index, QWidget *parent);

    void slotRefresh() override;

private Q_SLOTS:
    void toggleExpanded();
    void applyChanges();
    void revertChanges();

private:
    /** @brief Resolve the current shader_path from the model */
    QString shaderPath() const;
    /** @brief Load shader text from disk into the editor */
    void loadFromFile();

    QPushButton *m_toggleButton;
    QWidget *m_editorContainer;
    QPlainTextEdit *m_editor;
    GlslSyntaxHighlighter *m_highlighter;
    QPushButton *m_applyButton;
    QPushButton *m_revertButton;
    bool m_expanded{false};
};
