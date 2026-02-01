/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

/**
 * @class JsSyntaxHighlighter
 * @brief Syntax highlighter for JavaScript expression code in the expression editor.
 *
 * Colors built-in expression functions, global variables, numbers, strings,
 * comments, and Math.* methods.
 */
class JsSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit JsSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightRule> m_rules;
};
