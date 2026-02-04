/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "shaderparamparser.h"

#include <QDebug>
#include <QFile>
#include <QTextStream>

QVector<ShaderParamInfo> ShaderParamParser::parse(const QString &shaderText)
{
    QVector<ShaderParamInfo> results;
    const QStringList lines = shaderText.split(QLatin1Char('\n'));

    int i = 0;
    while (i < lines.size()) {
        const QString trimmed = lines[i].trimmed();

        // Look for //!PARAM <name>
        if (!trimmed.startsWith(QLatin1String("//!PARAM "))) {
            ++i;
            continue;
        }

        ShaderParamInfo info;
        info.name = trimmed.mid(9).trimmed(); // len("//!PARAM ") == 9
        if (info.name.isEmpty()) {
            ++i;
            continue;
        }

        // Defaults
        info.glslType = QStringLiteral("float");
        info.isDynamic = false;
        info.minimum = 0.0;
        info.maximum = 1.0;
        info.defaultValue = 0.0;

        ++i;

        // Collect directives until first non-directive, non-empty line
        bool foundDefault = false;
        while (i < lines.size()) {
            const QString line = lines[i].trimmed();

            if (line.isEmpty()) {
                ++i;
                continue;
            }

            if (line.startsWith(QLatin1String("//!DESC "))) {
                info.description = line.mid(8).trimmed();
                ++i;
            } else if (line.startsWith(QLatin1String("//!TYPE "))) {
                // Format: //!TYPE [DYNAMIC] <type>
                QString typeStr = line.mid(8).trimmed();
                if (typeStr.startsWith(QLatin1String("DYNAMIC"), Qt::CaseInsensitive)) {
                    info.isDynamic = true;
                    typeStr = typeStr.mid(7).trimmed();
                }
                if (!typeStr.isEmpty()) {
                    info.glslType = typeStr.toLower();
                }
                ++i;
            } else if (line.startsWith(QLatin1String("//!MINIMUM "))) {
                bool ok;
                double val = line.mid(11).trimmed().toDouble(&ok);
                if (ok) info.minimum = val;
                ++i;
            } else if (line.startsWith(QLatin1String("//!MAXIMUM "))) {
                bool ok;
                double val = line.mid(11).trimmed().toDouble(&ok);
                if (ok) info.maximum = val;
                ++i;
            } else if (line.startsWith(QLatin1String("//!"))) {
                // Unknown directive, skip
                ++i;
            } else {
                // First non-directive line = default value
                bool ok;
                double val = line.toDouble(&ok);
                if (ok) {
                    info.defaultValue = val;
                }
                foundDefault = true;
                ++i;
                break;
            }
        }

        // Clamp default to [min, max]
        if (info.defaultValue < info.minimum) info.defaultValue = info.minimum;
        if (info.defaultValue > info.maximum) info.defaultValue = info.maximum;

        // Extract group from name prefix: "bloom_intensity" → group="Bloom"
        int underscorePos = info.name.indexOf(QLatin1Char('_'));
        if (underscorePos > 0 && underscorePos < info.name.length() - 1) {
            info.group = info.name.left(underscorePos);
            // Capitalize first letter
            info.group[0] = info.group[0].toUpper();
        }

        results.append(info);

        if (!foundDefault) {
            qDebug() << "ShaderParamParser: //!PARAM" << info.name << "has no default value line";
        }
    }

    return results;
}

QVector<ShaderParamInfo> ShaderParamParser::parseFile(const QString &filePath)
{
    if (filePath.isEmpty()) {
        return {};
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "ShaderParamParser: cannot open" << filePath;
        return {};
    }

    QTextStream stream(&file);
    const QString content = stream.readAll();
    file.close();

    return parse(content);
}

QDomElement ShaderParamParser::toParameterElement(QDomDocument &doc, const ShaderParamInfo &param)
{
    QDomElement el = doc.createElement(QStringLiteral("parameter"));

    // Use "animated" type → enables keyframe + expression support in KeyframeContainer
    el.setAttribute(QStringLiteral("type"), QStringLiteral("animated"));
    el.setAttribute(QStringLiteral("name"), QStringLiteral("shader_param.%1").arg(param.name));
    el.setAttribute(QStringLiteral("default"), QString::number(param.defaultValue, 'g', 10));
    el.setAttribute(QStringLiteral("min"), QString::number(param.minimum, 'g', 10));
    el.setAttribute(QStringLiteral("max"), QString::number(param.maximum, 'g', 10));

    // Set decimals based on GLSL type
    if (param.glslType == QLatin1String("int") || param.glslType == QLatin1String("uint")) {
        el.setAttribute(QStringLiteral("decimals"), QStringLiteral("0"));
    } else {
        el.setAttribute(QStringLiteral("decimals"), QStringLiteral("3"));
    }

    // Group attribute for collapsible grouping in UI
    if (!param.group.isEmpty()) {
        el.setAttribute(QStringLiteral("group"), param.group);
    }

    // Display name: strip prefix if grouped (bloom_intensity → "Intensity")
    QString displayName = param.name;
    if (!param.group.isEmpty()) {
        int underscorePos = displayName.indexOf(QLatin1Char('_'));
        if (underscorePos >= 0 && underscorePos < displayName.length() - 1) {
            displayName = displayName.mid(underscorePos + 1);
        }
    }
    // Replace remaining underscores with spaces, trim, and capitalize first letter
    displayName.replace(QLatin1Char('_'), QLatin1Char(' '));
    displayName = displayName.trimmed();
    if (!displayName.isEmpty()) {
        displayName[0] = displayName[0].toUpper();
    }
    QDomElement nameEl = doc.createElement(QStringLiteral("name"));
    nameEl.appendChild(doc.createTextNode(displayName));
    el.appendChild(nameEl);

    // Comment / tooltip
    if (!param.description.isEmpty()) {
        QDomElement commentEl = doc.createElement(QStringLiteral("comment"));
        commentEl.appendChild(doc.createTextNode(param.description));
        el.appendChild(commentEl);
    }

    return el;
}

QString ShaderParamParser::strip(const QString &shaderText)
{
    const QStringList lines = shaderText.split(QLatin1Char('\n'));
    QStringList result;
    int i = 0;
    while (i < lines.size()) {
        const QString trimmed = lines[i].trimmed();
        if (!trimmed.startsWith(QLatin1String("//!PARAM "))) {
            result.append(lines[i]);
            ++i;
            continue;
        }
        // Skip the //!PARAM line
        ++i;
        // Skip associated directives, empty lines, and the default-value line
        while (i < lines.size()) {
            const QString line = lines[i].trimmed();
            if (line.isEmpty()) {
                ++i;
                continue;
            }
            if (line.startsWith(QLatin1String("//!DESC ")) ||
                line.startsWith(QLatin1String("//!TYPE ")) ||
                line.startsWith(QLatin1String("//!MINIMUM ")) ||
                line.startsWith(QLatin1String("//!MAXIMUM ")) ||
                (line.startsWith(QLatin1String("//!")) && !line.startsWith(QLatin1String("//!HOOK")) &&
                 !line.startsWith(QLatin1String("//!BIND")) && !line.startsWith(QLatin1String("//!SAVE")) &&
                 !line.startsWith(QLatin1String("//!WHEN")) && !line.startsWith(QLatin1String("//!COMPUTE")))) {
                ++i;
            } else {
                // First non-directive line = default value, skip it too
                bool ok;
                line.toDouble(&ok);
                if (ok) {
                    ++i;
                }
                break;
            }
        }
    }
    // Collapse runs of consecutive empty lines into a single empty line
    QStringList collapsed;
    bool lastWasEmpty = false;
    for (const QString &line : std::as_const(result)) {
        if (line.trimmed().isEmpty()) {
            if (!lastWasEmpty) {
                collapsed.append(line);
            }
            lastWasEmpty = true;
        } else {
            collapsed.append(line);
            lastWasEmpty = false;
        }
    }
    // Remove leading empty lines
    while (!collapsed.isEmpty() && collapsed.first().trimmed().isEmpty()) {
        collapsed.removeFirst();
    }
    return collapsed.join(QLatin1Char('\n'));
}
