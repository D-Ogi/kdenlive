/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "expressionwidget.h"
#include "assets/keyframes/model/keyframemodel.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "core.h"
#include "definitions.h"
#include "expressioneditordialog.h"
#include "expressions/expressioncache.h"
#include "expressions/expressionengine.h"
#include "expressions/expressiontemplatedialog.h"
#include "expressions/expressiontemplaterepository.h"
#include "jssyntaxhighlighter.h"
#include "project/projectmanager.h"
#include "timeline2/model/timelineitemmodel.hpp"

#include <KLocalizedString>
#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

// ── ExpressionPlainTextEdit ─────────────────────────────────────

QStringList ExpressionPlainTextEdit::completionWordList()
{
    static const QStringList words = {
        // Functions (global)
        QStringLiteral("linear"),
        QStringLiteral("ease"),
        QStringLiteral("easeIn"),
        QStringLiteral("easeOut"),
        QStringLiteral("clamp"),
        QStringLiteral("wiggle"),
        QStringLiteral("random"),
        QStringLiteral("gaussRandom"),
        QStringLiteral("noise"),
        QStringLiteral("seedRandom"),
        QStringLiteral("posterizeTime"),
        QStringLiteral("degreesToRadians"),
        QStringLiteral("radiansToDegrees"),
        QStringLiteral("smooth"),
        QStringLiteral("audioLevel"),
        QStringLiteral("audioRms"),
        QStringLiteral("key"),
        QStringLiteral("nearestKey"),
        QStringLiteral("valueAtTime"),
        QStringLiteral("velocityAtTime"),
        QStringLiteral("speedAtTime"),
        QStringLiteral("createPath"),
        QStringLiteral("loopIn"),
        QStringLiteral("loopOut"),
        QStringLiteral("loopInDuration"),
        QStringLiteral("loopOutDuration"),
        QStringLiteral("temporalWiggle"),
        // Globals
        QStringLiteral("time"),
        QStringLiteral("frame"),
        QStringLiteral("duration"),
        QStringLiteral("fps"),
        QStringLiteral("value"),
        QStringLiteral("index"),
        QStringLiteral("numKeys"),
        QStringLiteral("Math"),
        QStringLiteral("marker"),
        QStringLiteral("thisProperty"),
        QStringLiteral("thisEffect"),
        QStringLiteral("thisClip"),
        QStringLiteral("thisTrack"),
        // Math methods
        QStringLiteral("Math.abs"),
        QStringLiteral("Math.floor"),
        QStringLiteral("Math.ceil"),
        QStringLiteral("Math.round"),
        QStringLiteral("Math.sin"),
        QStringLiteral("Math.cos"),
        QStringLiteral("Math.tan"),
        QStringLiteral("Math.sqrt"),
        QStringLiteral("Math.pow"),
        QStringLiteral("Math.exp"),
        QStringLiteral("Math.log"),
        QStringLiteral("Math.min"),
        QStringLiteral("Math.max"),
        QStringLiteral("Math.PI"),
        QStringLiteral("Math.random"),
        // JS keywords
        QStringLiteral("var"),
        QStringLiteral("let"),
        QStringLiteral("const"),
        QStringLiteral("if"),
        QStringLiteral("else"),
        QStringLiteral("for"),
        QStringLiteral("while"),
        QStringLiteral("return"),
        QStringLiteral("true"),
        QStringLiteral("false"),
        QStringLiteral("null"),
        QStringLiteral("undefined"),
    };
    return words;
}

ExpressionPlainTextEdit::ExpressionPlainTextEdit(QWidget *parent)
    : QPlainTextEdit(parent)
{
    m_completer = new QCompleter(this);
    m_completer->setModel(new QStringListModel(completionWordList(), m_completer));
    m_completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setWidget(this);
    connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated), this, &ExpressionPlainTextEdit::insertCompletion);
}

QString ExpressionPlainTextEdit::wordUnderCursor() const
{
    QTextCursor tc = textCursor();
    // Select the word fragment left of cursor (alphanumeric + dots for Math.xxx)
    int pos = tc.position();
    QString text = toPlainText();
    int start = pos;
    while (start > 0) {
        QChar ch = text.at(start - 1);
        if (ch.isLetterOrNumber() || ch == QLatin1Char('_') || ch == QLatin1Char('.')) {
            --start;
        } else {
            break;
        }
    }
    return text.mid(start, pos - start);
}

void ExpressionPlainTextEdit::insertCompletion(const QString &completion)
{
    QTextCursor tc = textCursor();
    QString prefix = wordUnderCursor();
    // Remove the prefix, insert the full completion
    for (int i = 0; i < prefix.length(); ++i) {
        tc.deletePreviousChar();
    }
    tc.insertText(completion);
    setTextCursor(tc);
}

void ExpressionPlainTextEdit::keyPressEvent(QKeyEvent *event)
{
    // If completer popup is visible, let it handle navigation keys
    if (m_completer->popup()->isVisible()) {
        switch (event->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            event->ignore();
            return;
        default:
            break;
        }
    }

    QPlainTextEdit::keyPressEvent(event);

    // Don't show completer for modifier-only keys or special keys
    const bool ctrlOrShift = event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
    if (ctrlOrShift && event->text().isEmpty()) {
        return;
    }

    QString prefix = wordUnderCursor();
    if (prefix.length() < 2) {
        m_completer->popup()->hide();
        return;
    }

    if (prefix != m_completer->completionPrefix()) {
        m_completer->setCompletionPrefix(prefix);
        m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));
    }

    if (m_completer->completionCount() == 0) {
        m_completer->popup()->hide();
        return;
    }

    // Don't show popup if the only match is exactly what's typed
    if (m_completer->completionCount() == 1 && m_completer->currentCompletion() == prefix) {
        m_completer->popup()->hide();
        return;
    }

    QRect cr = cursorRect();
    cr.setWidth(m_completer->popup()->sizeHintForColumn(0) + m_completer->popup()->verticalScrollBar()->sizeHint().width());
    m_completer->complete(cr);
}

void ExpressionPlainTextEdit::focusInEvent(QFocusEvent *event)
{
    m_completer->setWidget(this);
    QPlainTextEdit::focusInEvent(event);
}

// ── ExpressionWidget ────────────────────────────────────────────

ExpressionWidget::ExpressionWidget(std::shared_ptr<AssetParameterModel> model, const QString &paramName, QWidget *parent, int componentIndex)
    : QWidget(parent)
    , m_model(std::move(model))
    , m_paramName(paramName)
    , m_componentIndex(componentIndex)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(2);

    // ── Linked badge (hidden by default) ────────────────
    m_linkedBadge = new QWidget(this);
    auto *badgeLayout = new QHBoxLayout(m_linkedBadge);
    badgeLayout->setContentsMargins(4, 2, 4, 2);
    badgeLayout->setSpacing(4);
    m_linkedLabel = new QLabel(m_linkedBadge);
    m_linkedLabel->setStyleSheet(QStringLiteral("color: palette(link); font-weight: bold; font-size: 9pt;"));
    badgeLayout->addWidget(m_linkedLabel);
    badgeLayout->addStretch();
    m_detachButton = new QPushButton(i18n("Detach"), m_linkedBadge);
    m_detachButton->setFlat(true);
    m_detachButton->setMaximumWidth(60);
    m_detachButton->setToolTip(i18n("Detach from template — expression becomes independently editable"));
    connect(m_detachButton, &QPushButton::clicked, this, &ExpressionWidget::onDetachClicked);
    badgeLayout->addWidget(m_detachButton);
    m_linkedBadge->setVisible(false);
    layout->addWidget(m_linkedBadge);

    // ── Code editor ─────────────────────────────────────
    m_editor = new ExpressionPlainTextEdit(this);
    m_editor->setMaximumHeight(80);
    m_editor->setMinimumHeight(50);
    QFont monoFont(QStringLiteral("Monospace"));
    monoFont.setStyleHint(QFont::TypeWriter);
    monoFont.setPointSize(9);
    m_editor->setFont(monoFont);
    m_editor->setTabStopDistance(20);
    m_editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_editor->setPlaceholderText(i18n("Enter JavaScript expression..."));
    layout->addWidget(m_editor);

    // Syntax highlighter
    m_highlighter = new JsSyntaxHighlighter(m_editor->document());

    // Function reference bar + Templates button
    buildFunctionBar();
    layout->addWidget(m_functionBar);

    // Preview + status row
    auto *statusRow = new QHBoxLayout();
    statusRow->setContentsMargins(0, 0, 0, 0);
    m_previewLabel = new QLabel(this);
    m_previewLabel->setStyleSheet(QStringLiteral("color: palette(mid);"));
    statusRow->addWidget(m_previewLabel);
    statusRow->addStretch();

    m_statusLabel = new QLabel(this);
    statusRow->addWidget(m_statusLabel);

    m_expandButton = new QPushButton(QStringLiteral("\u2922"), this); // ⤢
    m_expandButton->setFlat(true);
    m_expandButton->setMaximumWidth(30);
    m_expandButton->setToolTip(i18n("Open expanded expression editor"));
    connect(m_expandButton, &QPushButton::clicked, this, &ExpressionWidget::onExpandClicked);
    statusRow->addWidget(m_expandButton);

    m_clearButton = new QPushButton(i18n("Clear"), this);
    m_clearButton->setFlat(true);
    m_clearButton->setMaximumWidth(50);
    connect(m_clearButton, &QPushButton::clicked, this, &ExpressionWidget::onClearClicked);
    statusRow->addWidget(m_clearButton);

    layout->addLayout(statusRow);

    // Debounce timer (300ms after last keystroke)
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(300);
    connect(m_debounceTimer, &QTimer::timeout, this, &ExpressionWidget::onDebounceTimeout);
    connect(m_editor, &QPlainTextEdit::textChanged, this, &ExpressionWidget::onTextChanged);

    // Load existing expression if any
    QString existingExpr;
    if (m_componentIndex >= 0) {
        existingExpr = m_model->getRectComponentExpression(m_paramName, m_componentIndex);
    } else {
        existingExpr = m_model->getExpression(m_paramName);
    }
    if (!existingExpr.isEmpty()) {
        m_editor->setPlainText(existingExpr);
    }
    updateStatus(QString());

    // Check initial linked state
    updateLinkedState();

    // React to linked expression changes from repository
    connect(&ExpressionTemplateRepository::instance(), &ExpressionTemplateRepository::linkedExpressionChanged, this,
            [this](const QString &effectId, const QString &paramName, const QString &expression) {
                if (effectId == m_model->getAssetId() && paramName == m_paramName) {
                    if (!expression.isEmpty()) {
                        m_editor->setPlainText(expression);
                        m_model->setExpression(m_paramName, expression);
                    }
                }
            });
}

ExpressionWidget::~ExpressionWidget() = default;

void ExpressionWidget::setExpression(const QString &expression)
{
    m_editor->setPlainText(expression);
}

QString ExpressionWidget::expression() const
{
    return m_editor->toPlainText().trimmed();
}

void ExpressionWidget::updatePreview(int frame)
{
    QString expr = expression();
    if (expr.isEmpty()) {
        m_previewLabel->clear();
        return;
    }

    double fps = pCore->getCurrentFps();
    int in = m_model->data(m_model->getParamIndexFromName(m_paramName), AssetParameterModel::InRole).toInt();
    int duration = m_model->data(m_model->getParamIndexFromName(m_paramName), AssetParameterModel::ParentDurationRole).toInt();
    double clipDuration = static_cast<double>(duration) / fps;
    double time = static_cast<double>(frame - in) / fps;
    double baseValue =
        (m_componentIndex >= 0) ? m_model->getRectComponentBaseValue(m_paramName, m_componentIndex) : m_model->getExpressionBaseValue(m_paramName);

    // Load audio if the expression references audioLevel/audioRms
    QVector<float> audioPeakBoth, audioPeakLeft, audioPeakRight;
    if (ExpressionEngine::usesAudio(expr)) {
        int clipIn = in;
        int clipOut = in + duration - 1;
        ExpressionCache::loadTimelineAudioForRange(clipIn, clipOut, fps, audioPeakBoth, audioPeakLeft, audioPeakRight);
    }

    // Gather clip metadata for thisClip/thisTrack
    int clipPosition = 0;
    int clipDurationFrames = duration;
    QString clipName;
    int trackIndex = 0;
    ObjectId ownerId = m_model->getOwnerId();
    if (ownerId.type == KdenliveObjectType::TimelineClip) {
        clipPosition = pCore->getItemPosition(ownerId);
        int trackId = pCore->getItemTrack(ownerId);
        auto timeline = pCore->projectManager()->getTimeline();
        if (timeline) {
            clipName = timeline->getClipName(ownerId.itemId);
            trackIndex = timeline->getTrackPosition(trackId);
        }
    }

    // Load keyframes if the expression uses loopIn/loopOut/key/valueAtTime
    QVector<QPair<int, double>> exprKeyframes;
    if (ExpressionEngine::usesKeyframes(expr)) {
        auto kfModel = m_model->getKeyframeModel();
        if (kfModel) {
            QModelIndex paramIdx = m_model->getParamIndexFromName(m_paramName);
            auto *km = kfModel->getKeyModel(QPersistentModelIndex(paramIdx));
            if (km) {
                for (auto kfIt = km->begin(); kfIt != km->end(); ++kfIt) {
                    exprKeyframes.append({kfIt->first.frames(fps), kfIt->second.second.toDouble()});
                }
            }
        }
    }

    // Load timeline markers/guides if expression uses marker references
    QVector<ExprMarkerData> exprMarkers;
    if (ExpressionEngine::usesMarkers(expr)) {
        auto timeline = pCore->projectManager()->getTimeline();
        if (timeline) {
            auto guideModel = timeline->getGuideModel();
            if (guideModel) {
                QList<CommentedTime> allMarkers = guideModel->getAllMarkers();
                for (const auto &m : allMarkers) {
                    int markerFrame = m.time().frames(fps);
                    double markerTime = static_cast<double>(markerFrame - in) / fps;
                    double markerDuration = m.duration().seconds();
                    exprMarkers.append({markerTime, m.comment(), markerDuration});
                }
            }
        }
    }

    auto [value, error] =
        ExpressionCache::instance().evaluateAtFrame(expr, time, frame - in, clipDuration, fps, baseValue, 0, audioPeakBoth, audioPeakLeft, audioPeakRight,
                                                    clipPosition, clipDurationFrames, clipName, trackIndex, exprKeyframes, exprMarkers);

    if (error.isEmpty()) {
        m_previewLabel->setText(i18n("Result: %1 at frame %2").arg(value, 0, 'f', 3).arg(frame));
    }
}

void ExpressionWidget::onTextChanged()
{
    m_debounceTimer->start();
}

void ExpressionWidget::onDebounceTimeout()
{
    QString expr = expression();
    if (expr.isEmpty()) {
        updateStatus(QString());
        return;
    }

    // Validate
    QString error = ExpressionCache::instance().validate(expr);
    updateStatus(error);

    if (error.isEmpty()) {
        // Apply the expression
        if (m_componentIndex >= 0) {
            m_model->setRectComponentExpression(m_paramName, m_componentIndex, expr);
        } else {
            m_model->setExpression(m_paramName, expr);
        }
        Q_EMIT expressionChanged(expr);
    }
}

void ExpressionWidget::onClearClicked()
{
    if (m_componentIndex >= 0) {
        m_editor->clear();
        m_model->clearRectComponentExpression(m_paramName, m_componentIndex);
        updateStatus(QString());
        Q_EMIT expressionChanged(QString());
    } else {
        // Also clear any template link
        m_model->setExpressionTemplateLink(m_paramName, QString());
        m_editor->clear();
        m_model->clearExpression(m_paramName);
        updateStatus(QString());
        updateLinkedState();
        Q_EMIT expressionChanged(QString());
    }
}

void ExpressionWidget::onTemplatesClicked()
{
    ExpressionTemplateDialog dlg(m_model, m_paramName, this);
    if (dlg.exec() == QDialog::Accepted) {
        // Dialog may have applied an expression; refresh
        QString expr = m_model->getExpression(m_paramName);
        if (!expr.isEmpty() && expr != expression()) {
            m_editor->setPlainText(expr);
        }
        updateLinkedState();
    }
}

void ExpressionWidget::onDetachClicked()
{
    QString templateId = m_model->getExpressionTemplateLink(m_paramName);
    if (!templateId.isEmpty()) {
        // Unlink from repository
        auto &repo = ExpressionTemplateRepository::instance();
        repo.unlinkInstance(templateId, m_model->getAssetId(), m_paramName);
    }
    m_model->setExpressionTemplateLink(m_paramName, QString());
    updateLinkedState();
}

void ExpressionWidget::onExpandClicked()
{
    ExpressionEditorDialog dlg(expression(), m_model, m_paramName, this, m_componentIndex);
    if (dlg.exec() == QDialog::Accepted) {
        m_editor->setPlainText(dlg.expression());
    }
}

void ExpressionWidget::insertFunctionTemplate(const QString &tmpl)
{
    QTextCursor cursor = m_editor->textCursor();
    cursor.insertText(tmpl);
    m_editor->setFocus();
}

void ExpressionWidget::buildFunctionBar()
{
    m_functionBar = new QWidget(this);
    auto *barLayout = new QHBoxLayout(m_functionBar);
    barLayout->setContentsMargins(0, 0, 0, 0);
    barLayout->setSpacing(2);

    struct FuncInfo
    {
        const char *name;
        const char *tmpl;
    };
    static const FuncInfo functions[] = {
        {"linear", "linear(time, 0, duration, 0, value)"},
        {"ease", "ease(time, 0, duration, 0, value)"},
        {"wiggle", "wiggle(4, 0.1)"},
        {"audioLevel", "audioLevel(\"Both\", time)"},
        {"clamp", "clamp(value, 0, 1)"},
        {"noise", "noise(time * 4)"},
        {"random", "random(0, value)"},
        {"smooth", "smooth(0.2, 5)"},
    };

    auto *infoLabel = new QLabel(QStringLiteral("\u24D8"), m_functionBar); // circled i
    infoLabel->setToolTip(i18n("Click a function name to insert a template"));
    barLayout->addWidget(infoLabel);

    for (const auto &fn : functions) {
        auto *btn = new QToolButton(m_functionBar);
        btn->setText(QString::fromLatin1(fn.name));
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        btn->setAutoRaise(true);
        btn->setStyleSheet(QStringLiteral("color: palette(link); font-size: 9pt;"));
        QString tmpl = QString::fromLatin1(fn.tmpl);
        connect(btn, &QToolButton::clicked, this, [this, tmpl]() { insertFunctionTemplate(tmpl); });
        barLayout->addWidget(btn);
    }

    barLayout->addStretch();

    // "Templates..." button (replaces old collapsible template panel)
    auto *templatesButton = new QPushButton(i18n("Templates..."), m_functionBar);
    templatesButton->setStyleSheet(QStringLiteral("font-size: 9pt; font-weight: bold;"));
    templatesButton->setToolTip(i18n("Open Expression Template Manager"));
    connect(templatesButton, &QPushButton::clicked, this, &ExpressionWidget::onTemplatesClicked);
    barLayout->addWidget(templatesButton);
}

void ExpressionWidget::updateLinkedState()
{
    // Component expressions don't support template linking
    if (m_componentIndex >= 0) {
        m_linkedBadge->setVisible(false);
        m_editor->setReadOnly(false);
        return;
    }
    bool linked = m_model->hasExpressionTemplateLink(m_paramName);
    m_linkedBadge->setVisible(linked);
    m_editor->setReadOnly(linked);

    if (linked) {
        QString templateId = m_model->getExpressionTemplateLink(m_paramName);
        auto &repo = ExpressionTemplateRepository::instance();
        ExpressionTemplate tmpl = repo.getTemplate(templateId);
        QString name = tmpl.name.isEmpty() ? templateId : tmpl.name;
        m_linkedLabel->setText(i18n("Linked to: \"%1\"").arg(name));
    }
}

void ExpressionWidget::updateStatus(const QString &error)
{
    if (expression().isEmpty()) {
        m_statusLabel->setText(i18n("No expression"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: palette(disabled-text);"));
        m_previewLabel->clear();
    } else if (error.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("\u2713 ") + i18n("Expression valid"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: green;"));
    } else {
        m_statusLabel->setText(QStringLiteral("\u2717 ") + error);
        m_statusLabel->setStyleSheet(QStringLiteral("color: red;"));
    }
}
