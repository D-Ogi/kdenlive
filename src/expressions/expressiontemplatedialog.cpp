/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "expressiontemplatedialog.h"
#include "assets/model/assetparametermodel.hpp"
#include "assets/view/widgets/jssyntaxhighlighter.h"
#include "expressions/expressioncache.h"
#include "expressiontemplaterepository.h"

#include <KLocalizedString>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

ExpressionTemplateDialog::ExpressionTemplateDialog(std::shared_ptr<AssetParameterModel> model, const QString &paramName, QWidget *parent)
    : QDialog(parent)
    , m_model(std::move(model))
    , m_paramName(paramName)
{
    setWindowTitle(i18n("Expression Template Manager"));
    resize(780, 500);

    auto *mainLayout = new QHBoxLayout(this);
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(splitter);

    // ── Left panel: tree + toolbar ──────────────────────
    auto *leftWidget = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    m_tree = new QTreeWidget(leftWidget);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setMinimumWidth(220);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, &ExpressionTemplateDialog::onSelectionChanged);
    leftLayout->addWidget(m_tree);

    auto *toolbar = new QHBoxLayout();
    m_addButton = new QPushButton(i18n("+"), leftWidget);
    m_addButton->setToolTip(i18n("New template (User)"));
    m_addButton->setMaximumWidth(32);
    connect(m_addButton, &QPushButton::clicked, this, &ExpressionTemplateDialog::onNewTemplate);
    toolbar->addWidget(m_addButton);

    m_deleteButton = new QPushButton(i18n("x"), leftWidget);
    m_deleteButton->setToolTip(i18n("Delete template"));
    m_deleteButton->setMaximumWidth(32);
    m_deleteButton->setEnabled(false);
    connect(m_deleteButton, &QPushButton::clicked, this, &ExpressionTemplateDialog::onDeleteTemplate);
    toolbar->addWidget(m_deleteButton);

    m_importButton = new QPushButton(i18n("Import"), leftWidget);
    m_importButton->setToolTip(i18n("Import template from JSON file"));
    connect(m_importButton, &QPushButton::clicked, this, &ExpressionTemplateDialog::onImportTemplate);
    toolbar->addWidget(m_importButton);

    m_exportButton = new QPushButton(i18n("Export"), leftWidget);
    m_exportButton->setToolTip(i18n("Export selected template to JSON file"));
    m_exportButton->setEnabled(false);
    connect(m_exportButton, &QPushButton::clicked, this, &ExpressionTemplateDialog::onExportTemplate);
    toolbar->addWidget(m_exportButton);

    toolbar->addStretch();
    leftLayout->addLayout(toolbar);
    splitter->addWidget(leftWidget);

    // ── Right panel: details ────────────────────────────
    auto *rightWidget = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightWidget);

    // Name
    rightLayout->addWidget(new QLabel(i18n("Name:"), rightWidget));
    m_nameEdit = new QLineEdit(rightWidget);
    rightLayout->addWidget(m_nameEdit);

    // Category
    rightLayout->addWidget(new QLabel(i18n("Category:"), rightWidget));
    m_categoryEdit = new QLineEdit(rightWidget);
    rightLayout->addWidget(m_categoryEdit);

    // Expression
    rightLayout->addWidget(new QLabel(i18n("Expression:"), rightWidget));
    m_exprEdit = new QPlainTextEdit(rightWidget);
    m_exprEdit->setMaximumHeight(120);
    QFont monoFont(QStringLiteral("Monospace"));
    monoFont.setStyleHint(QFont::TypeWriter);
    monoFont.setPointSize(9);
    m_exprEdit->setFont(monoFont);
    m_highlighter = new JsSyntaxHighlighter(m_exprEdit->document());
    rightLayout->addWidget(m_exprEdit);

    // Description
    rightLayout->addWidget(new QLabel(i18n("Description:"), rightWidget));
    m_descEdit = new QPlainTextEdit(rightWidget);
    m_descEdit->setMaximumHeight(60);
    rightLayout->addWidget(m_descEdit);

    // Preview + tier info
    m_previewLabel = new QLabel(rightWidget);
    m_previewLabel->setStyleSheet(QStringLiteral("color: palette(mid);"));
    rightLayout->addWidget(m_previewLabel);

    m_tierLabel = new QLabel(rightWidget);
    m_tierLabel->setStyleSheet(QStringLiteral("font-style: italic; color: palette(mid);"));
    rightLayout->addWidget(m_tierLabel);

    // Action buttons
    auto *actionRow = new QHBoxLayout();
    m_saveButton = new QPushButton(i18n("Save"), rightWidget);
    m_saveButton->setEnabled(false);
    connect(m_saveButton, &QPushButton::clicked, this, &ExpressionTemplateDialog::onSaveChanges);
    actionRow->addWidget(m_saveButton);

    m_applyLinkedButton = new QPushButton(i18n("Apply Linked"), rightWidget);
    m_applyLinkedButton->setToolTip(i18n("Apply as a linked Project Template (changes propagate to all linked clips)"));
    m_applyLinkedButton->setVisible(m_model != nullptr);
    connect(m_applyLinkedButton, &QPushButton::clicked, this, &ExpressionTemplateDialog::onApplyLinked);
    actionRow->addWidget(m_applyLinkedButton);

    m_applyDetachedButton = new QPushButton(i18n("Apply Detached"), rightWidget);
    m_applyDetachedButton->setToolTip(i18n("Copy expression text into editor (no link)"));
    m_applyDetachedButton->setVisible(m_model != nullptr);
    connect(m_applyDetachedButton, &QPushButton::clicked, this, &ExpressionTemplateDialog::onApplyDetached);
    actionRow->addWidget(m_applyDetachedButton);

    actionRow->addStretch();
    rightLayout->addLayout(actionRow);
    rightLayout->addStretch();

    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    // Populate
    populateTree();
    clearDetails();

    // React to repository changes
    auto &repo = ExpressionTemplateRepository::instance();
    connect(&repo, &ExpressionTemplateRepository::templateAdded, this, [this](const QString &) { populateTree(); });
    connect(&repo, &ExpressionTemplateRepository::templateRemoved, this, [this](const QString &) { populateTree(); });
}

ExpressionTemplateDialog::~ExpressionTemplateDialog() = default;

// ── Tree ──────────────────────────────────────────────────────────────

void ExpressionTemplateDialog::populateTree()
{
    m_tree->clear();
    auto &repo = ExpressionTemplateRepository::instance();

    auto makeTierRoot = [this](const QString &label) -> QTreeWidgetItem * {
        auto *item = new QTreeWidgetItem(m_tree, {label});
        QFont f = item->font(0);
        f.setBold(true);
        item->setFont(0, f);
        item->setFlags(Qt::ItemIsEnabled);
        return item;
    };

    m_defaultRoot = makeTierRoot(i18n("Default"));
    m_userRoot = makeTierRoot(i18n("User"));
    m_projectRoot = makeTierRoot(i18n("Project"));

    auto addTemplates = [this](QTreeWidgetItem *root, const QVector<ExpressionTemplate> &templates) {
        for (const auto &tmpl : templates) {
            QTreeWidgetItem *parent = tmpl.category.isEmpty() ? root : findOrCreateCategoryItem(root, tmpl.category);
            auto *item = new QTreeWidgetItem(parent, {tmpl.name});
            item->setData(0, Qt::UserRole, tmpl.id);
            item->setToolTip(0, tmpl.description);
        }
    };

    addTemplates(m_defaultRoot, repo.allTemplates(TemplateTier::Default));
    addTemplates(m_userRoot, repo.allTemplates(TemplateTier::User));
    addTemplates(m_projectRoot, repo.allTemplates(TemplateTier::Project));

    m_defaultRoot->setExpanded(true);
    m_userRoot->setExpanded(true);
    m_projectRoot->setExpanded(true);
}

QTreeWidgetItem *ExpressionTemplateDialog::findOrCreateCategoryItem(QTreeWidgetItem *tierItem, const QString &category)
{
    for (int i = 0; i < tierItem->childCount(); ++i) {
        auto *child = tierItem->child(i);
        if (child->data(0, Qt::UserRole).toString().isEmpty() && child->text(0) == category) {
            return child;
        }
    }
    auto *catItem = new QTreeWidgetItem(tierItem, {category});
    catItem->setFlags(Qt::ItemIsEnabled);
    QFont f = catItem->font(0);
    f.setItalic(true);
    catItem->setFont(0, f);
    return catItem;
}

// ── Selection ─────────────────────────────────────────────────────────

void ExpressionTemplateDialog::onSelectionChanged()
{
    auto *item = m_tree->currentItem();
    if (!item) {
        clearDetails();
        return;
    }

    QString id = item->data(0, Qt::UserRole).toString();
    if (id.isEmpty()) {
        clearDetails();
        return;
    }

    auto &repo = ExpressionTemplateRepository::instance();
    ExpressionTemplate tmpl = repo.getTemplate(id);
    if (tmpl.id.isEmpty()) {
        clearDetails();
        return;
    }

    m_selectedTemplateId = id;
    showTemplateDetails(tmpl);

    bool editable = !tmpl.isReadOnly();
    setDetailsEditable(editable);
    m_deleteButton->setEnabled(editable);
    m_exportButton->setEnabled(true);
    m_saveButton->setEnabled(editable);
    m_applyLinkedButton->setEnabled(m_model != nullptr);
    m_applyDetachedButton->setEnabled(m_model != nullptr);
}

void ExpressionTemplateDialog::showTemplateDetails(const ExpressionTemplate &tmpl)
{
    m_nameEdit->setText(tmpl.name);
    m_categoryEdit->setText(tmpl.category);
    m_exprEdit->setPlainText(tmpl.expression);
    m_descEdit->setPlainText(tmpl.description);

    // Preview
    QString error = ExpressionCache::instance().validate(tmpl.expression);
    if (error.isEmpty()) {
        auto [val, evalErr] = ExpressionCache::instance().evaluateAtFrame(tmpl.expression, 1.0, 25, 5.0, 25.0, 1.0, 0);
        if (evalErr.isEmpty()) {
            m_previewLabel->setText(i18n("Preview: %1 at frame 25").arg(val, 0, 'f', 3));
        } else {
            m_previewLabel->setText(i18n("Preview error: %1").arg(evalErr));
        }
    } else {
        m_previewLabel->setText(i18n("Invalid: %1").arg(error));
    }

    const char *tierName = (tmpl.tier == TemplateTier::Default) ? "Default (read-only)" : (tmpl.tier == TemplateTier::User) ? "User" : "Project";
    m_tierLabel->setText(i18n("Tier: %1").arg(QString::fromLatin1(tierName)));
}

void ExpressionTemplateDialog::clearDetails()
{
    m_selectedTemplateId.clear();
    m_nameEdit->clear();
    m_categoryEdit->clear();
    m_exprEdit->clear();
    m_descEdit->clear();
    m_previewLabel->clear();
    m_tierLabel->clear();
    setDetailsEditable(false);
    m_deleteButton->setEnabled(false);
    m_exportButton->setEnabled(false);
    m_saveButton->setEnabled(false);
    m_applyLinkedButton->setEnabled(false);
    m_applyDetachedButton->setEnabled(false);
}

void ExpressionTemplateDialog::setDetailsEditable(bool editable)
{
    m_nameEdit->setReadOnly(!editable);
    m_categoryEdit->setReadOnly(!editable);
    m_exprEdit->setReadOnly(!editable);
    m_descEdit->setReadOnly(!editable);
}

ExpressionTemplate ExpressionTemplateDialog::currentTemplateFromUI() const
{
    auto &repo = ExpressionTemplateRepository::instance();
    ExpressionTemplate tmpl = repo.getTemplate(m_selectedTemplateId);
    tmpl.name = m_nameEdit->text().trimmed();
    tmpl.category = m_categoryEdit->text().trimmed();
    tmpl.expression = m_exprEdit->toPlainText().trimmed();
    tmpl.description = m_descEdit->toPlainText().trimmed();
    return tmpl;
}

// ── Actions ───────────────────────────────────────────────────────────

void ExpressionTemplateDialog::onNewTemplate()
{
    ExpressionTemplate tmpl;
    tmpl.name = i18n("New Template");
    tmpl.category = i18n("Custom");
    tmpl.expression = QStringLiteral("value");
    tmpl.description = i18n("My custom expression template");
    tmpl.tier = TemplateTier::User;

    auto &repo = ExpressionTemplateRepository::instance();
    QString id = repo.addTemplate(tmpl);
    // populateTree is called via signal; select the new item
    // Find and select it
    for (int i = 0; i < m_userRoot->childCount(); ++i) {
        auto *child = m_userRoot->child(i);
        // Check category items
        for (int j = 0; j < child->childCount(); ++j) {
            if (child->child(j)->data(0, Qt::UserRole).toString() == id) {
                m_tree->setCurrentItem(child->child(j));
                return;
            }
        }
        if (child->data(0, Qt::UserRole).toString() == id) {
            m_tree->setCurrentItem(child);
            return;
        }
    }
}

void ExpressionTemplateDialog::onDeleteTemplate()
{
    if (m_selectedTemplateId.isEmpty()) return;
    auto &repo = ExpressionTemplateRepository::instance();
    ExpressionTemplate tmpl = repo.getTemplate(m_selectedTemplateId);
    if (tmpl.isReadOnly()) return;

    if (QMessageBox::question(this, i18n("Delete Template"), i18n("Delete template \"%1\"?").arg(tmpl.name)) != QMessageBox::Yes) {
        return;
    }
    repo.removeTemplate(m_selectedTemplateId);
    clearDetails();
}

void ExpressionTemplateDialog::onImportTemplate()
{
    QString filePath = QFileDialog::getOpenFileName(this, i18n("Import Expression Template"), QString(), i18n("JSON files (*.json)"));
    if (filePath.isEmpty()) return;
    auto &repo = ExpressionTemplateRepository::instance();
    if (!repo.importFromFile(filePath)) {
        QMessageBox::warning(this, i18n("Import Failed"), i18n("Could not import template from file."));
    }
}

void ExpressionTemplateDialog::onExportTemplate()
{
    if (m_selectedTemplateId.isEmpty()) return;
    auto &repo = ExpressionTemplateRepository::instance();
    ExpressionTemplate tmpl = repo.getTemplate(m_selectedTemplateId);

    QString filePath = QFileDialog::getSaveFileName(this, i18n("Export Expression Template"), tmpl.name + QStringLiteral(".json"), i18n("JSON files (*.json)"));
    if (filePath.isEmpty()) return;
    if (!repo.exportToFile(m_selectedTemplateId, filePath)) {
        QMessageBox::warning(this, i18n("Export Failed"), i18n("Could not export template to file."));
    }
}

void ExpressionTemplateDialog::onSaveChanges()
{
    if (m_selectedTemplateId.isEmpty()) return;
    auto &repo = ExpressionTemplateRepository::instance();
    ExpressionTemplate tmpl = currentTemplateFromUI();
    if (tmpl.isReadOnly()) return;

    repo.updateTemplate(tmpl);

    // If it's a project template with linked instances, propagate
    if (tmpl.tier == TemplateTier::Project) {
        repo.propagateTemplateChange(tmpl.id);
    }

    // Refresh tree item text
    auto *item = m_tree->currentItem();
    if (item) {
        item->setText(0, tmpl.name);
    }
}

void ExpressionTemplateDialog::onApplyLinked()
{
    if (!m_model || m_selectedTemplateId.isEmpty()) return;
    auto &repo = ExpressionTemplateRepository::instance();
    ExpressionTemplate tmpl = repo.getTemplate(m_selectedTemplateId);

    QString projectId = m_selectedTemplateId;
    // If not already a Project template, promote it
    if (tmpl.tier != TemplateTier::Project) {
        projectId = repo.promoteToProject(m_selectedTemplateId);
        if (projectId.isEmpty()) return;
        tmpl = repo.getTemplate(projectId);
    }

    // Link this instance
    QString effectId = m_model->getAssetId();
    repo.linkInstance(projectId, effectId, m_paramName);

    // Apply expression and set template link
    m_model->setExpression(m_paramName, tmpl.expression);
    m_model->setExpressionTemplateLink(m_paramName, projectId);

    accept();
}

void ExpressionTemplateDialog::onApplyDetached()
{
    if (!m_model || m_selectedTemplateId.isEmpty()) return;
    auto &repo = ExpressionTemplateRepository::instance();
    ExpressionTemplate tmpl = repo.getTemplate(m_selectedTemplateId);

    // Apply expression without link
    m_model->setExpression(m_paramName, tmpl.expression);
    m_model->setExpressionTemplateLink(m_paramName, QString());

    accept();
}
