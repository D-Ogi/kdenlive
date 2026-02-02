/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

/**
 * @class GlslSyntaxHighlighter
 * @brief Syntax highlighter for GLSL / mpv .hook shader code in the shader editor widget.
 *
 * Highlights GLSL types, keywords, built-in functions, mpv macros,
 * libplacebo directives (//!HOOK, //!PARAM, etc.), numbers, and comments.
 */
class GlslSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit GlslSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightRule> m_rules;

    // Multi-line block comment state
    QRegularExpression m_commentStartPattern;
    QRegularExpression m_commentEndPattern;
    QTextCharFormat m_multiLineCommentFormat;
};
