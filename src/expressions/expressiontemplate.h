/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QUuid>
#include <QVector>

/** @brief Tier determines where a template is stored and its editability. */
enum class TemplateTier {
    Default, ///< Shipped with Kdenlive, read-only
    User,    ///< Personal library on disk
    Project  ///< Stored in .kdenlive project file
};

/** @brief Identifies a parameter instance linked to a project template. */
struct ExpressionTemplateLink
{
    QString effectId;  ///< MLT effect/asset ID
    QString paramName; ///< Parameter name within the effect

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj[QStringLiteral("effectId")] = effectId;
        obj[QStringLiteral("paramName")] = paramName;
        return obj;
    }

    static ExpressionTemplateLink fromJson(const QJsonObject &obj)
    {
        return {obj[QStringLiteral("effectId")].toString(), obj[QStringLiteral("paramName")].toString()};
    }
};

/** @brief A reusable expression preset with optional linking to timeline clips. */
struct ExpressionTemplate
{
    QString id;          ///< UUID string
    QString name;        ///< Display name
    QString category;    ///< Folder/group name
    QString expression;  ///< JavaScript expression text
    QString description; ///< Human-readable description
    TemplateTier tier;
    QVector<ExpressionTemplateLink> linkedInstances; ///< Project tier only

    bool isReadOnly() const { return tier == TemplateTier::Default; }

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj[QStringLiteral("id")] = id;
        obj[QStringLiteral("name")] = name;
        obj[QStringLiteral("category")] = category;
        obj[QStringLiteral("expression")] = expression;
        obj[QStringLiteral("description")] = description;
        if (!linkedInstances.isEmpty()) {
            QJsonArray links;
            for (const auto &link : linkedInstances) {
                links.append(link.toJson());
            }
            obj[QStringLiteral("linkedInstances")] = links;
        }
        return obj;
    }

    static ExpressionTemplate fromJson(const QJsonObject &obj, TemplateTier t)
    {
        ExpressionTemplate tmpl;
        tmpl.id = obj[QStringLiteral("id")].toString();
        if (tmpl.id.isEmpty()) {
            return {}; // Invalid: missing id
        }
        tmpl.name = obj[QStringLiteral("name")].toString();
        tmpl.category = obj[QStringLiteral("category")].toString();
        tmpl.expression = obj[QStringLiteral("expression")].toString();
        tmpl.description = obj[QStringLiteral("description")].toString();
        tmpl.tier = t;
        const QJsonArray links = obj[QStringLiteral("linkedInstances")].toArray();
        for (const auto &val : links) {
            tmpl.linkedInstances.append(ExpressionTemplateLink::fromJson(val.toObject()));
        }
        return tmpl;
    }

    /** @brief Generate a stable UUID v5 for default templates. */
    static QString stableId(const QString &seedName)
    {
        // Use a fixed namespace UUID for Kdenlive expression templates
        static const QUuid ns = QUuid(QStringLiteral("6ba7b810-9dad-11d1-80b4-00c04fd430c8")); // DNS namespace
        return QUuid::createUuidV5(ns, QStringLiteral("kdenlive-expr-") + seedName).toString(QUuid::WithoutBraces);
    }
};
