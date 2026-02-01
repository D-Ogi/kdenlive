/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "expressioneditordialog.h"
#include "assets/keyframes/model/keyframemodel.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "core.h"
#include "definitions.h"
#include "doc/kdenlivedoc.h"
#include "expressions/expressioncache.h"
#include "expressions/expressionengine.h"
#include "expressionwidget.h"
#include "jssyntaxhighlighter.h"
#include "profiles/profilemodel.hpp"
#include "project/projectmanager.h"
#include "timeline2/model/timelineitemmodel.hpp"

#include <KLocalizedString>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

// ── Line number gutter ──────────────────────────────────────────

class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(ExpressionPlainTextEdit *editor)
        : QWidget(editor)
        , m_editor(editor)
    {
    }

    QSize sizeHint() const override { return QSize(lineNumberAreaWidth(), 0); }

    int lineNumberAreaWidth() const
    {
        int digits = 1;
        int maxBlock = qMax(1, m_editor->blockCount());
        while (maxBlock >= 10) {
            maxBlock /= 10;
            ++digits;
        }
        int space = 8 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
        return space;
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
        painter.fillRect(event->rect(), palette().color(QPalette::AlternateBase));
        painter.setPen(palette().color(QPalette::PlaceholderText));

        QTextBlock block = m_editor->firstVisibleBlock();
        int blockNumber = block.blockNumber();
        int top = qRound(m_editor->blockBoundingGeometry(block).translated(m_editor->contentOffset()).top());
        int bottom = top + qRound(m_editor->blockBoundingRect(block).height());

        while (block.isValid() && top <= event->rect().bottom()) {
            if (block.isVisible() && bottom >= event->rect().top()) {
                QString number = QString::number(blockNumber + 1);
                painter.drawText(0, top, width() - 4, fontMetrics().height(), Qt::AlignRight, number);
            }
            block = block.next();
            top = bottom;
            bottom = top + qRound(m_editor->blockBoundingRect(block).height());
            ++blockNumber;
        }
    }

private:
    ExpressionPlainTextEdit *m_editor;
};

// ── ExpressionEditorDialog ──────────────────────────────────────

ExpressionEditorDialog::ExpressionEditorDialog(const QString &expression, std::shared_ptr<AssetParameterModel> model, const QString &paramName, QWidget *parent,
                                               int componentIndex)
    : QDialog(parent)
    , m_model(std::move(model))
    , m_paramName(paramName)
    , m_componentIndex(componentIndex)
{
    setWindowTitle(i18n("Expression Editor"));
    setMinimumSize(500, 400);

    // Cache clip range
    m_fps = pCore->getCurrentFps();
    m_clipIn = m_model->data(m_model->getParamIndexFromName(m_paramName), AssetParameterModel::InRole).toInt();
    m_clipDuration = m_model->data(m_model->getParamIndexFromName(m_paramName), AssetParameterModel::ParentDurationRole).toInt();

    // Restore saved size
    QSettings settings;
    settings.beginGroup(QStringLiteral("ExpressionEditorDialog"));
    resize(settings.value(QStringLiteral("size"), QSize(800, 600)).toSize());
    settings.endGroup();

    auto *layout = new QVBoxLayout(this);

    // ── Code editor with autocomplete ───────────────────
    m_editor = new ExpressionPlainTextEdit(this);
    QFont monoFont(QStringLiteral("Monospace"));
    monoFont.setStyleHint(QFont::TypeWriter);
    monoFont.setPointSize(10);
    m_editor->setFont(monoFont);
    m_editor->setTabStopDistance(28);
    m_editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_editor->setPlaceholderText(i18n("Enter JavaScript expression..."));
    m_editor->setPlainText(expression);

    // Syntax highlighter
    m_highlighter = new JsSyntaxHighlighter(m_editor->document());

    // Line number gutter
    auto *gutter = new LineNumberArea(m_editor);
    auto updateGutterWidth = [m_editor = m_editor, gutter]() { m_editor->setViewportMargins(gutter->lineNumberAreaWidth(), 0, 0, 0); };
    auto updateGutter = [gutter](const QRect &rect, int dy) {
        if (dy) {
            gutter->scroll(0, dy);
        } else {
            gutter->update(0, rect.y(), gutter->width(), rect.height());
        }
    };

    connect(m_editor, &QPlainTextEdit::blockCountChanged, this, updateGutterWidth);
    connect(m_editor, &QPlainTextEdit::updateRequest, this, updateGutter);
    updateGutterWidth();

    connect(m_editor, &QPlainTextEdit::updateRequest, this, [m_editor = m_editor, gutter]() {
        QRect cr = m_editor->contentsRect();
        gutter->setGeometry(cr.left(), cr.top(), gutter->lineNumberAreaWidth(), cr.height());
    });
    QRect cr = m_editor->contentsRect();
    gutter->setGeometry(cr.left(), cr.top(), gutter->lineNumberAreaWidth(), cr.height());

    layout->addWidget(m_editor, 1);

    // ── Validation status ───────────────────────────────
    m_statusLabel = new QLabel(this);
    layout->addWidget(m_statusLabel);

    // ── Bottom area: Preview + Context side by side ─────
    auto *bottomSplitter = new QSplitter(Qt::Horizontal, this);

    // ── Left: Preview panel ─────────────────────────────
    auto *previewGroup = new QGroupBox(i18n("Preview"), this);
    auto *previewLayout = new QVBoxLayout(previewGroup);

    // Single-frame test row
    auto *frameRow = new QHBoxLayout();
    frameRow->addWidget(new QLabel(i18n("Frame:"), this));
    m_frameSpin = new QSpinBox(this);
    m_frameSpin->setRange(m_clipIn, m_clipIn + m_clipDuration - 1);
    m_frameSpin->setValue(pCore->getMonitorPosition());
    frameRow->addWidget(m_frameSpin);

    auto *evalButton = new QPushButton(i18n("Evaluate"), this);
    connect(evalButton, &QPushButton::clicked, this, [this]() { evaluateAtFrame(m_frameSpin->value()); });
    frameRow->addWidget(evalButton);

    m_frameResultLabel = new QLabel(this);
    m_frameResultLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    frameRow->addWidget(m_frameResultLabel, 1);
    previewLayout->addLayout(frameRow);

    // Sample range button
    auto *sampleRow = new QHBoxLayout();
    auto *sampleButton = new QPushButton(i18n("Sample Range"), this);
    sampleButton->setToolTip(i18n("Evaluate expression at 10 evenly spaced frames across the clip"));
    connect(sampleButton, &QPushButton::clicked, this, &ExpressionEditorDialog::sampleRange);
    sampleRow->addWidget(sampleButton);
    sampleRow->addStretch();
    previewLayout->addLayout(sampleRow);

    m_sampleTable = new QTableWidget(0, 3, this);
    m_sampleTable->setHorizontalHeaderLabels({i18n("Frame"), i18n("Time (s)"), i18n("Value")});
    m_sampleTable->horizontalHeader()->setStretchLastSection(true);
    m_sampleTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sampleTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    previewLayout->addWidget(m_sampleTable, 1);

    bottomSplitter->addWidget(previewGroup);

    // ── Right: Context Variables ────────────────────────
    auto *contextGroup = new QGroupBox(i18n("Context Variables"), this);
    auto *contextLayout = new QVBoxLayout(contextGroup);

    m_contextTree = new QTreeWidget(this);
    m_contextTree->setHeaderLabels({i18n("Variable"), i18n("Value")});
    m_contextTree->setRootIsDecorated(true);
    m_contextTree->setAlternatingRowColors(true);
    m_contextTree->header()->setStretchLastSection(true);
    m_contextTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    contextLayout->addWidget(m_contextTree, 1);

    bottomSplitter->addWidget(contextGroup);
    bottomSplitter->setStretchFactor(0, 3);
    bottomSplitter->setStretchFactor(1, 2);

    layout->addWidget(bottomSplitter, 1);

    // ── Button box ──────────────────────────────────────
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    // Live validation on text change
    connect(m_editor, &QPlainTextEdit::textChanged, this, &ExpressionEditorDialog::validate);

    // Re-evaluate + update context when spinner changes
    connect(m_frameSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ExpressionEditorDialog::evaluateAtFrame);
    connect(m_frameSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ExpressionEditorDialog::updateContextVariables);

    validate();
    evaluateAtFrame(m_frameSpin->value());
    updateContextVariables(m_frameSpin->value());
}

ExpressionEditorDialog::~ExpressionEditorDialog()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("ExpressionEditorDialog"));
    settings.setValue(QStringLiteral("size"), size());
    settings.endGroup();
}

QString ExpressionEditorDialog::expression() const
{
    return m_editor->toPlainText().trimmed();
}

std::pair<double, QString> ExpressionEditorDialog::evalAt(const QString &expr, int frame) const
{
    double clipDuration = static_cast<double>(m_clipDuration) / m_fps;
    double time = static_cast<double>(frame - m_clipIn) / m_fps;
    double baseValue =
        (m_componentIndex >= 0) ? m_model->getRectComponentBaseValue(m_paramName, m_componentIndex) : m_model->getExpressionBaseValue(m_paramName);

    QVector<float> audioPeakBoth, audioPeakLeft, audioPeakRight;
    if (ExpressionEngine::usesAudio(expr)) {
        ExpressionCache::loadTimelineAudioForRange(m_clipIn, m_clipIn + m_clipDuration - 1, m_fps, audioPeakBoth, audioPeakLeft, audioPeakRight);
    }

    int clipPosition = 0;
    int clipDurationFrames = m_clipDuration;
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
                    exprKeyframes.append({kfIt->first.frames(m_fps), kfIt->second.second.toDouble()});
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
                    int markerFrame = m.time().frames(m_fps);
                    double markerTime = static_cast<double>(markerFrame - m_clipIn) / m_fps;
                    double markerDuration = m.duration().seconds();
                    exprMarkers.append({markerTime, m.comment(), markerDuration});
                }
            }
        }
    }

    return ExpressionCache::instance().evaluateAtFrame(expr, time, frame - m_clipIn, clipDuration, m_fps, baseValue, 0, audioPeakBoth, audioPeakLeft,
                                                       audioPeakRight, clipPosition, clipDurationFrames, clipName, trackIndex, exprKeyframes, exprMarkers);
}

void ExpressionEditorDialog::validate()
{
    QString expr = expression();
    if (expr.isEmpty()) {
        m_statusLabel->setText(i18n("No expression"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: palette(disabled-text);"));
        return;
    }

    QString error = ExpressionCache::instance().validate(expr);
    if (error.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("\u2713 ") + i18n("Expression valid"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: green;"));
    } else {
        m_statusLabel->setText(QStringLiteral("\u2717 ") + error);
        m_statusLabel->setStyleSheet(QStringLiteral("color: red;"));
    }

    evaluateAtFrame(m_frameSpin->value());
}

void ExpressionEditorDialog::evaluateAtFrame(int frame)
{
    QString expr = expression();
    if (expr.isEmpty()) {
        m_frameResultLabel->clear();
        return;
    }

    QString error = ExpressionCache::instance().validate(expr);
    if (!error.isEmpty()) {
        m_frameResultLabel->setText(QStringLiteral("\u2717 ") + error);
        m_frameResultLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: red;"));
        return;
    }

    auto [value, evalError] = evalAt(expr, frame);
    if (evalError.isEmpty()) {
        double time = static_cast<double>(frame - m_clipIn) / m_fps;
        m_frameResultLabel->setText(QStringLiteral("= %1  (t=%2s)").arg(value, 0, 'f', 4).arg(time, 0, 'f', 3));
        m_frameResultLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: palette(text);"));
    } else {
        m_frameResultLabel->setText(evalError);
        m_frameResultLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: red;"));
    }
}

void ExpressionEditorDialog::sampleRange()
{
    QString expr = expression();
    if (expr.isEmpty()) {
        return;
    }

    QString error = ExpressionCache::instance().validate(expr);
    if (!error.isEmpty()) {
        return;
    }

    const int numSamples = 10;
    m_sampleTable->setRowCount(numSamples);

    for (int i = 0; i < numSamples; ++i) {
        int frame = m_clipIn + (m_clipDuration - 1) * i / (numSamples - 1);
        double time = static_cast<double>(frame - m_clipIn) / m_fps;

        auto [value, evalError] = evalAt(expr, frame);

        auto *frameItem = new QTableWidgetItem(QString::number(frame));
        frameItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_sampleTable->setItem(i, 0, frameItem);

        auto *timeItem = new QTableWidgetItem(QString::number(time, 'f', 3));
        timeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_sampleTable->setItem(i, 1, timeItem);

        auto *valueItem = new QTableWidgetItem(evalError.isEmpty() ? QString::number(value, 'f', 4) : evalError);
        valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (!evalError.isEmpty()) {
            valueItem->setForeground(Qt::red);
        }
        m_sampleTable->setItem(i, 2, valueItem);
    }

    m_sampleTable->resizeColumnsToContents();
}

void ExpressionEditorDialog::setContextVar(QTreeWidgetItem *parent, const QString &name, const QString &value)
{
    // Look for existing child with this name
    for (int i = 0; i < parent->childCount(); ++i) {
        if (parent->child(i)->text(0) == name) {
            parent->child(i)->setText(1, value);
            return;
        }
    }
    auto *item = new QTreeWidgetItem(parent, {name, value});
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
}

void ExpressionEditorDialog::updateContextVariables(int frame)
{
    m_contextTree->clear();

    double clipDurationSec = static_cast<double>(m_clipDuration) / m_fps;
    double time = static_cast<double>(frame - m_clipIn) / m_fps;
    int relativeFrame = frame - m_clipIn;
    double baseValue =
        (m_componentIndex >= 0) ? m_model->getRectComponentBaseValue(m_paramName, m_componentIndex) : m_model->getExpressionBaseValue(m_paramName);

    // ── Per-frame globals ───────────────────────────────
    auto *globalsItem = new QTreeWidgetItem(m_contextTree, {i18n("Globals")});
    globalsItem->setFlags(globalsItem->flags() & ~Qt::ItemIsEditable);
    QFont boldFont = globalsItem->font(0);
    boldFont.setBold(true);
    globalsItem->setFont(0, boldFont);

    setContextVar(globalsItem, QStringLiteral("time"), QString::number(time, 'f', 4) + QStringLiteral(" s"));
    setContextVar(globalsItem, QStringLiteral("frame"), QString::number(relativeFrame));
    setContextVar(globalsItem, QStringLiteral("duration"), QString::number(clipDurationSec, 'f', 4) + QStringLiteral(" s"));
    setContextVar(globalsItem, QStringLiteral("fps"), QString::number(m_fps, 'f', 2));
    setContextVar(globalsItem, QStringLiteral("value"), QString::number(baseValue, 'f', 4));
    setContextVar(globalsItem, QStringLiteral("index"), QStringLiteral("0"));

    globalsItem->setExpanded(true);

    // ── thisClip ────────────────────────────────────────
    auto *clipItem = new QTreeWidgetItem(m_contextTree, {QStringLiteral("thisClip")});
    clipItem->setFlags(clipItem->flags() & ~Qt::ItemIsEditable);
    clipItem->setFont(0, boldFont);

    int clipPosition = 0;
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

    setContextVar(clipItem, QStringLiteral("position"), QString::number(clipPosition) + QStringLiteral(" frames"));
    setContextVar(clipItem, QStringLiteral("duration"), QString::number(m_clipDuration) + QStringLiteral(" frames"));
    setContextVar(clipItem, QStringLiteral("name"), clipName.isEmpty() ? QStringLiteral("\"\"") : QStringLiteral("\"%1\"").arg(clipName));
    setContextVar(clipItem, QStringLiteral("index"), QString::number(0));

    double outPointSec = clipDurationSec;
    double startTimeSec = static_cast<double>(clipPosition) / m_fps;
    setContextVar(clipItem, QStringLiteral("inPoint"), QStringLiteral("0.0 s"));
    setContextVar(clipItem, QStringLiteral("outPoint"), QString::number(outPointSec, 'f', 4) + QStringLiteral(" s"));
    setContextVar(clipItem, QStringLiteral("startTime"), QString::number(startTimeSec, 'f', 4) + QStringLiteral(" s"));

    clipItem->setExpanded(true);

    // ── thisTrack ───────────────────────────────────────
    auto *trackItem = new QTreeWidgetItem(m_contextTree, {QStringLiteral("thisTrack")});
    trackItem->setFlags(trackItem->flags() & ~Qt::ItemIsEditable);
    trackItem->setFont(0, boldFont);

    setContextVar(trackItem, QStringLiteral("index"), QString::number(trackIndex));

    trackItem->setExpanded(true);

    // ── thisProject ─────────────────────────────────────
    auto *projectItem = new QTreeWidgetItem(m_contextTree, {QStringLiteral("thisProject")});
    projectItem->setFlags(projectItem->flags() & ~Qt::ItemIsEditable);
    projectItem->setFont(0, boldFont);

    const auto &profile = pCore->getCurrentProfile();
    int projWidth = profile->width();
    int projHeight = profile->height();
    double pixelAspect = profile->sar();

    setContextVar(projectItem, QStringLiteral("width"), QString::number(projWidth) + QStringLiteral(" px"));
    setContextVar(projectItem, QStringLiteral("height"), QString::number(projHeight) + QStringLiteral(" px"));
    setContextVar(projectItem, QStringLiteral("fps"), QString::number(m_fps, 'f', 2));
    setContextVar(projectItem, QStringLiteral("pixelAspect"), QString::number(pixelAspect, 'f', 4));
    setContextVar(projectItem, QStringLiteral("frameDuration"), QString::number(1.0 / m_fps, 'f', 6) + QStringLiteral(" s"));

    auto timeline = pCore->projectManager()->getTimeline();
    if (timeline) {
        int numVideoTracks = timeline->getTracksIds(false).size();
        int numAudioTracks = timeline->getTracksIds(true).size();
        int numTracks = numVideoTracks + numAudioTracks;
        double timelineDuration = static_cast<double>(timeline->duration()) / m_fps;

        setContextVar(projectItem, QStringLiteral("duration"), QString::number(timelineDuration, 'f', 3) + QStringLiteral(" s"));
        setContextVar(projectItem, QStringLiteral("numTracks"), QString::number(numTracks));
    }

    QString projName = pCore->currentDoc()->url().fileName();
    if (!projName.isEmpty()) {
        setContextVar(projectItem, QStringLiteral("name"), QStringLiteral("\"%1\"").arg(projName));
    }

    projectItem->setExpanded(true);

    // ── Parameter info ──────────────────────────────────
    auto *paramItem = new QTreeWidgetItem(m_contextTree, {i18n("Parameter")});
    paramItem->setFlags(paramItem->flags() & ~Qt::ItemIsEditable);
    paramItem->setFont(0, boldFont);

    setContextVar(paramItem, QStringLiteral("name"), m_paramName);
    setContextVar(paramItem, QStringLiteral("effect"), m_model->getAssetId());
    setContextVar(paramItem, QStringLiteral("baseValue (value)"), QString::number(baseValue, 'f', 4));

    // Show min/max if available
    QModelIndex paramIndex = m_model->getParamIndexFromName(m_paramName);
    if (paramIndex.isValid()) {
        QVariant minVal = m_model->data(paramIndex, AssetParameterModel::MinRole);
        QVariant maxVal = m_model->data(paramIndex, AssetParameterModel::MaxRole);
        if (minVal.isValid()) {
            setContextVar(paramItem, QStringLiteral("min"), minVal.toString());
        }
        if (maxVal.isValid()) {
            setContextVar(paramItem, QStringLiteral("max"), maxVal.toString());
        }
    }

    paramItem->setExpanded(true);

    m_contextTree->resizeColumnToContents(0);
}
