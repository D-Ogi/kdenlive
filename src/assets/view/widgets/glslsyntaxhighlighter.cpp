/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "glslsyntaxhighlighter.h"

GlslSyntaxHighlighter::GlslSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // GLSL types — blue
    QTextCharFormat typeFormat;
    typeFormat.setForeground(QColor(0, 0, 200));
    typeFormat.setFontWeight(QFont::Bold);
    const QStringList types = {
        QStringLiteral("void"),   QStringLiteral("bool"),      QStringLiteral("int"),       QStringLiteral("uint"),  QStringLiteral("float"),
        QStringLiteral("double"), QStringLiteral("vec2"),      QStringLiteral("vec3"),      QStringLiteral("vec4"),  QStringLiteral("ivec2"),
        QStringLiteral("ivec3"),  QStringLiteral("ivec4"),     QStringLiteral("uvec2"),     QStringLiteral("uvec3"), QStringLiteral("uvec4"),
        QStringLiteral("bvec2"),  QStringLiteral("bvec3"),     QStringLiteral("bvec4"),     QStringLiteral("mat2"),  QStringLiteral("mat3"),
        QStringLiteral("mat4"),   QStringLiteral("sampler2D"), QStringLiteral("sampler3D"),
    };
    for (const QString &t : types) {
        m_rules.append({QRegularExpression(QStringLiteral("\\b%1\\b").arg(t)), typeFormat});
    }

    // GLSL keywords — purple
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor(128, 0, 128));
    keywordFormat.setFontWeight(QFont::Bold);
    const QStringList keywords = {
        QStringLiteral("if"),      QStringLiteral("else"),   QStringLiteral("for"),       QStringLiteral("while"),   QStringLiteral("do"),
        QStringLiteral("return"),  QStringLiteral("break"),  QStringLiteral("continue"),  QStringLiteral("discard"), QStringLiteral("struct"),
        QStringLiteral("const"),   QStringLiteral("in"),     QStringLiteral("out"),       QStringLiteral("inout"),   QStringLiteral("uniform"),
        QStringLiteral("varying"), QStringLiteral("layout"), QStringLiteral("precision"), QStringLiteral("highp"),   QStringLiteral("mediump"),
        QStringLiteral("lowp"),    QStringLiteral("define"),
    };
    for (const QString &kw : keywords) {
        m_rules.append({QRegularExpression(QStringLiteral("\\b%1\\b").arg(kw)), keywordFormat});
    }

    // GLSL built-in functions — dark cyan
    QTextCharFormat builtinFormat;
    builtinFormat.setForeground(QColor(0, 139, 139));
    const QStringList builtins = {
        QStringLiteral("mix"),     QStringLiteral("clamp"),      QStringLiteral("smoothstep"), QStringLiteral("step"),    QStringLiteral("pow"),
        QStringLiteral("sqrt"),    QStringLiteral("abs"),        QStringLiteral("sign"),       QStringLiteral("floor"),   QStringLiteral("ceil"),
        QStringLiteral("fract"),   QStringLiteral("mod"),        QStringLiteral("min"),        QStringLiteral("max"),     QStringLiteral("dot"),
        QStringLiteral("cross"),   QStringLiteral("length"),     QStringLiteral("normalize"),  QStringLiteral("reflect"), QStringLiteral("refract"),
        QStringLiteral("texture"), QStringLiteral("textureLod"), QStringLiteral("exp"),        QStringLiteral("exp2"),    QStringLiteral("log"),
        QStringLiteral("log2"),    QStringLiteral("sin"),        QStringLiteral("cos"),        QStringLiteral("tan"),     QStringLiteral("asin"),
        QStringLiteral("acos"),    QStringLiteral("atan"),       QStringLiteral("radians"),    QStringLiteral("degrees"), QStringLiteral("distance"),
    };
    for (const QString &fn : builtins) {
        m_rules.append({QRegularExpression(QStringLiteral("\\b%1\\b").arg(fn)), builtinFormat});
    }

    // mpv/libplacebo macros — orange
    QTextCharFormat macroFormat;
    macroFormat.setForeground(QColor(204, 120, 0));
    macroFormat.setFontWeight(QFont::Bold);
    m_rules.append({QRegularExpression(QStringLiteral("\\bHOOKED_tex(?:Off)?\\b")), macroFormat});
    m_rules.append({QRegularExpression(QStringLiteral("\\bHOOKED_(?:pos|size|pt|map|rot)\\b")), macroFormat});
    m_rules.append({QRegularExpression(QStringLiteral("\\b[A-Z]+_tex(?:Off)?\\b")), macroFormat});
    m_rules.append({QRegularExpression(QStringLiteral("\\b[A-Z]+_(?:pos|size|pt|map|rot)\\b")), macroFormat});
    m_rules.append({QRegularExpression(QStringLiteral("\\bhook\\b")), macroFormat});

    // Numbers — dark red
    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor(178, 34, 34));
    m_rules.append({QRegularExpression(QStringLiteral("\\b\\d+\\.?\\d*([eE][+-]?\\d+)?\\b")), numberFormat});

    // Single-line comments — gray italic (exclude //! directives)
    QTextCharFormat commentFormat;
    commentFormat.setForeground(QColor(128, 128, 128));
    commentFormat.setFontItalic(true);
    m_rules.append({QRegularExpression(QStringLiteral("//(?!!)[^\n]*")), commentFormat});

    // libplacebo directives AFTER comments so they win over comment coloring
    QTextCharFormat directiveFormat;
    directiveFormat.setForeground(QColor(200, 0, 0));
    directiveFormat.setFontWeight(QFont::Bold);
    m_rules.append({QRegularExpression(QStringLiteral("^\\s*//![A-Z].*")), directiveFormat});

    // Multi-line comment support
    m_multiLineCommentFormat.setForeground(QColor(128, 128, 128));
    m_multiLineCommentFormat.setFontItalic(true);
    m_commentStartPattern = QRegularExpression(QStringLiteral("/\\*"));
    m_commentEndPattern = QRegularExpression(QStringLiteral("\\*/"));
}

void GlslSyntaxHighlighter::highlightBlock(const QString &text)
{
    // Apply single-line rules
    for (const HighlightRule &rule : std::as_const(m_rules)) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Handle multi-line /* ... */ comments
    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() != 1) {
        QRegularExpressionMatch startMatch = m_commentStartPattern.match(text);
        startIndex = startMatch.hasMatch() ? startMatch.capturedStart() : -1;
    }

    while (startIndex >= 0) {
        QRegularExpressionMatch endMatch = m_commentEndPattern.match(text, startIndex + 2);
        int endIndex = endMatch.hasMatch() ? endMatch.capturedStart() : -1;
        int commentLength;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + endMatch.capturedLength();
        }
        setFormat(startIndex, commentLength, m_multiLineCommentFormat);

        QRegularExpressionMatch nextStart = m_commentStartPattern.match(text, startIndex + commentLength);
        startIndex = nextStart.hasMatch() ? nextStart.capturedStart() : -1;
    }
}
