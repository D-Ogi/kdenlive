/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "expressiontemplate.h"
#include <QMap>
#include <QObject>

class KdenliveDoc;

/**
 * @class ExpressionTemplateRepository
 * @brief Singleton managing expression templates across three tiers:
 *        Default (read-only), User (on disk), Project (in .kdenlive).
 */
class ExpressionTemplateRepository : public QObject
{
    Q_OBJECT

public:
    static ExpressionTemplateRepository &instance();

    // ── Query ─────────────────────────────────────────────
    QVector<ExpressionTemplate> allTemplates(TemplateTier tier) const;
    QVector<ExpressionTemplate> allTemplates() const;
    ExpressionTemplate getTemplate(const QString &id) const;
    bool hasTemplate(const QString &id) const;
    QStringList categories(TemplateTier tier) const;

    // ── CRUD (User + Project) ─────────────────────────────
    /** @brief Add a template, returns its UUID. */
    QString addTemplate(ExpressionTemplate tmpl);
    void updateTemplate(const ExpressionTemplate &tmpl);
    void removeTemplate(const QString &id);

    // ── Linking (Project tier) ────────────────────────────
    void linkInstance(const QString &templateId, const QString &effectId, const QString &paramName);
    void unlinkInstance(const QString &templateId, const QString &effectId, const QString &paramName);
    /** @brief After editing a project template, push the new expression to all linked instances. */
    void propagateTemplateChange(const QString &templateId);

    // ── Import / Export (User tier) ───────────────────────
    bool importFromFile(const QString &filePath);
    bool exportToFile(const QString &templateId, const QString &filePath);

    // ── Project lifecycle ─────────────────────────────────
    void loadProjectTemplates(KdenliveDoc *doc);
    void saveProjectTemplates(KdenliveDoc *doc);
    void clearProjectTemplates();

    /** @brief Promote a Default/User template to Project tier (for linking). Returns new ID. */
    QString promoteToProject(const QString &sourceId);

Q_SIGNALS:
    void templateAdded(const QString &id);
    void templateUpdated(const QString &id);
    void templateRemoved(const QString &id);
    /** @brief Emitted per linked instance when a project template's expression changes. */
    void linkedExpressionChanged(const QString &effectId, const QString &paramName, const QString &expression);

private:
    ExpressionTemplateRepository();
    ~ExpressionTemplateRepository() override = default;

    void loadDefaultTemplates();
    void loadUserTemplates();
    void saveUserTemplate(const ExpressionTemplate &tmpl);
    void deleteUserTemplateFile(const QString &id);
    QString userTemplatesDir() const;

    QMap<QString, ExpressionTemplate> m_defaults;
    QMap<QString, ExpressionTemplate> m_userTemplates;
    QMap<QString, ExpressionTemplate> m_projectTemplates;
};
