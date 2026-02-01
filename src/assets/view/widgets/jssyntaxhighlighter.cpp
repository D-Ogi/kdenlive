/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "jssyntaxhighlighter.h"

JsSyntaxHighlighter::JsSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // Built-in expression functions — dark cyan
    QTextCharFormat functionFormat;
    functionFormat.setForeground(QColor(0, 139, 139)); // dark cyan
    functionFormat.setFontWeight(QFont::Bold);
    const QStringList builtinFunctions = {QStringLiteral("linear"),           QStringLiteral("ease"),          QStringLiteral("easeIn"),
                                          QStringLiteral("easeOut"),          QStringLiteral("clamp"),         QStringLiteral("wiggle"),
                                          QStringLiteral("random"),           QStringLiteral("gaussRandom"),   QStringLiteral("noise"),
                                          QStringLiteral("seedRandom"),       QStringLiteral("posterizeTime"), QStringLiteral("degreesToRadians"),
                                          QStringLiteral("radiansToDegrees"), QStringLiteral("smooth"),        QStringLiteral("audioLevel"),
                                          QStringLiteral("audioRms")};
    for (const QString &fn : builtinFunctions) {
        m_rules.append({QRegularExpression(QStringLiteral("\\b%1\\b").arg(fn)), functionFormat});
    }

    // Math.* methods — dark cyan (not bold)
    QTextCharFormat mathFormat;
    mathFormat.setForeground(QColor(0, 139, 139));
    m_rules.append({QRegularExpression(QStringLiteral("\\bMath\\.[a-zA-Z]+")), mathFormat});

    // Global variables — dark magenta
    QTextCharFormat varFormat;
    varFormat.setForeground(QColor(139, 0, 139)); // dark magenta
    varFormat.setFontWeight(QFont::Bold);
    const QStringList globalVars = {QStringLiteral("time"), QStringLiteral("frame"), QStringLiteral("duration"),
                                    QStringLiteral("fps"),  QStringLiteral("value"), QStringLiteral("index")};
    for (const QString &v : globalVars) {
        m_rules.append({QRegularExpression(QStringLiteral("\\b%1\\b").arg(v)), varFormat});
    }

    // Numbers — dark red
    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor(178, 34, 34)); // firebrick
    m_rules.append({QRegularExpression(QStringLiteral("\\b\\d+\\.?\\d*\\b")), numberFormat});

    // Strings (single and double quotes) — dark green
    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor(34, 139, 34)); // forest green
    m_rules.append({QRegularExpression(QStringLiteral("\"[^\"]*\"")), stringFormat});
    m_rules.append({QRegularExpression(QStringLiteral("'[^']*'")), stringFormat});

    // Comments (// to end of line) — gray
    QTextCharFormat commentFormat;
    commentFormat.setForeground(QColor(128, 128, 128));
    commentFormat.setFontItalic(true);
    m_rules.append({QRegularExpression(QStringLiteral("//[^\n]*")), commentFormat});

    // JavaScript keywords — dark blue
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor(0, 0, 139));
    keywordFormat.setFontWeight(QFont::Bold);
    const QStringList keywords = {QStringLiteral("var"),  QStringLiteral("let"),   QStringLiteral("const"), QStringLiteral("if"),
                                  QStringLiteral("else"), QStringLiteral("for"),   QStringLiteral("while"), QStringLiteral("return"),
                                  QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("null"),  QStringLiteral("undefined")};
    for (const QString &kw : keywords) {
        m_rules.append({QRegularExpression(QStringLiteral("\\b%1\\b").arg(kw)), keywordFormat});
    }
}

void JsSyntaxHighlighter::highlightBlock(const QString &text)
{
    for (const HighlightRule &rule : std::as_const(m_rules)) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}
