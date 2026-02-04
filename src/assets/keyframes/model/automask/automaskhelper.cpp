/*
SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "automaskhelper.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "jobs/masktask.h"
#include "kdenlivesettings.h"
#include "monitor/monitor.h"
#include "monitor/monitorproxy.h"
#include "pythoninterfaces/saminterface.h"
#include "utils/qstringutils.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <QDateTime>
#include <QProcess>
#include <QSize>
#include <QtConcurrent/QtConcurrentRun>

#include <utility>

AutomaskHelper::AutomaskHelper(QObject *parent)
    : QObject(parent)
{
    m_previewDebounce.setSingleShot(true);
    m_previewDebounce.setInterval(300);
    connect(&m_previewDebounce, &QTimer::timeout, this, &AutomaskHelper::sendPreviewCommand);
}

void AutomaskHelper::refreshQmlOverlay(const QSize &frameSize)
{
    QVariantList points;
    QVariantList pointsTypes;
    QList<QPoint> inclPoints = m_includePoints.value(m_lastPos);
    for (const QPoint &p : inclPoints) {
        points << QPointF(double(p.x()) / frameSize.width(), double(p.y()) / frameSize.height());
        pointsTypes << 1;
    }
    QList<QPoint> exclPoints = m_excludePoints.value(m_lastPos);
    for (const QPoint &p : exclPoints) {
        points << QPointF(double(p.x()) / frameSize.width(), double(p.y()) / frameSize.height());
        pointsTypes << 0;
    }
    QRect box;
    if (m_boxes.contains(m_lastPos)) {
        box = m_boxes.value(m_lastPos);
    }
    QVariantList keyframes;
    for (auto i = m_includePoints.cbegin(), end = m_includePoints.cend(); i != end; ++i) {
        keyframes << i.key();
    }
    for (auto i = m_excludePoints.cbegin(), end = m_excludePoints.cend(); i != end; ++i) {
        if (!m_includePoints.contains(i.key())) {
            keyframes << i.key();
        }
    }
    for (auto i = m_boxes.cbegin(), end = m_boxes.cend(); i != end; ++i) {
        if (!m_includePoints.contains(i.key()) && !m_excludePoints.contains(i.key())) {
            keyframes << i.key();
        }
    }
    pCore->getMonitor(Kdenlive::ClipMonitor)->setUpEffectGeometry(points, pointsTypes, keyframes, box);
}

void AutomaskHelper::addMonitorControlPoint(int position, const QSize frameSize, int xPos, int yPos, bool extend, bool exclude)
{
    const QPoint p(xPos, yPos);
    m_lastPos = position;

    // Record undo action
    MaskUndoAction undo;
    undo.type = MaskUndoAction::Type::AddPoint;
    undo.frame = m_lastPos;
    undo.point = p;
    undo.isExclude = exclude;
    if (!extend && (m_includePoints.contains(m_lastPos) || m_boxes.contains(m_lastPos))) {
        undo.hadPreviousData = true;
        undo.previousIncludePoints = m_includePoints.value(m_lastPos);
        undo.previousExcludePoints = m_excludePoints.value(m_lastPos);
        undo.previousBox = m_boxes.value(m_lastPos);
    }

    QList<QPoint> pointsList;
    if (exclude) {
        if (m_excludePoints.contains(m_lastPos)) {
            pointsList = m_excludePoints.value(m_lastPos);
            if (pointsList.contains(p)) {
                return;
            }
        }
        pointsList.append(p);
        m_excludePoints.insert(m_lastPos, pointsList);
    } else {
        if (!extend && (m_includePoints.contains(m_lastPos) || m_boxes.contains(m_lastPos))) {
            m_includePoints.remove(m_lastPos);
            m_excludePoints.remove(m_lastPos);
            m_boxes.remove(m_lastPos);
        } else {
            if (m_includePoints.contains(m_lastPos)) {
                pointsList = m_includePoints.value(m_lastPos);
                if (pointsList.contains(p)) {
                    return;
                }
            }
        }
        pointsList.append(p);
        m_includePoints.insert(m_lastPos, pointsList);
    }
    m_undoStack.push(undo);
    if (m_undoStack.size() > MAX_UNDO_STACK_SIZE) {
        m_undoStack.removeFirst();
    }
    m_redoStack.clear();
    refreshQmlOverlay(frameSize);
    generateImage();
}

void AutomaskHelper::moveMonitorControlPoint(int ix, int position, const QSize frameSize, int xPos, int yPos)
{
    const QPoint p(xPos, yPos);
    m_lastPos = position;
    QList<QPoint> pointsList;
    bool isExcludeMove = m_excludePoints.contains(position) && ix >= m_includePoints.value(position).size();
    int adjustedIx = isExcludeMove ? ix - m_includePoints.value(position).size() : ix;

    // Record undo for the move
    MaskUndoAction undo;
    undo.type = MaskUndoAction::Type::MovePoint;
    undo.frame = m_lastPos;
    undo.pointIndex = adjustedIx;
    undo.movedExclude = isExcludeMove;
    undo.point = p; // new position (for redo)
    if (isExcludeMove) {
        if (adjustedIx < 0 || adjustedIx >= m_excludePoints[position].size()) {
            return;
        }
        undo.oldPosition = m_excludePoints[position].value(adjustedIx);
        m_excludePoints[position][adjustedIx] = p;
    } else {
        if (adjustedIx < 0 || adjustedIx >= m_includePoints[position].size()) {
            return;
        }
        undo.oldPosition = m_includePoints[position].value(adjustedIx);
        m_includePoints[position][adjustedIx] = p;
    }
    m_undoStack.push(undo);
    if (m_undoStack.size() > MAX_UNDO_STACK_SIZE) {
        m_undoStack.removeFirst();
    }
    m_redoStack.clear();
    refreshQmlOverlay(frameSize);
    generateImage();
}

void AutomaskHelper::addMonitorControlStroke(int position, const QSize frameSize, const QList<QPoint> &strokePoints, bool isExclude)
{
    m_lastPos = position;
    // Record undo for the stroke
    MaskUndoAction undo;
    undo.type = MaskUndoAction::Type::AddStroke;
    undo.frame = m_lastPos;
    undo.isExclude = isExclude;
    // Track which points were actually added (dedup)
    QList<QPoint> addedPoints;
    QList<QPoint> &targetList = isExclude ? m_excludePoints[m_lastPos] : m_includePoints[m_lastPos];
    for (const QPoint &p : strokePoints) {
        if (!targetList.contains(p)) {
            targetList.append(p);
            addedPoints.append(p);
        }
    }
    undo.strokePoints = addedPoints;
    if (!addedPoints.isEmpty()) {
        m_undoStack.push(undo);
        if (m_undoStack.size() > MAX_UNDO_STACK_SIZE) {
            m_undoStack.removeFirst();
        }
        m_redoStack.clear();
    }
    refreshQmlOverlay(frameSize);
    generateImage();
}

void AutomaskHelper::addMonitorControlRect(int position, const QSize frameSize, const QRect rect, bool extend)
{
    m_lastPos = position;
    // Record undo
    MaskUndoAction undo;
    undo.type = MaskUndoAction::Type::AddBox;
    undo.frame = m_lastPos;
    undo.box = rect;
    if (!extend && (m_includePoints.contains(m_lastPos) || m_boxes.contains(m_lastPos))) {
        undo.hadPreviousData = true;
        undo.previousIncludePoints = m_includePoints.value(m_lastPos);
        undo.previousExcludePoints = m_excludePoints.value(m_lastPos);
        undo.previousBox = m_boxes.value(m_lastPos);
    }

    QList<QPoint> pointsList;
    if (!extend && (m_includePoints.contains(m_lastPos) || m_boxes.contains(m_lastPos))) {
        m_includePoints.remove(m_lastPos);
        m_excludePoints.remove(m_lastPos);
        m_boxes.remove(m_lastPos);
    }
    m_boxes.insert(m_lastPos, rect);
    m_undoStack.push(undo);
    if (m_undoStack.size() > MAX_UNDO_STACK_SIZE) {
        m_undoStack.removeFirst();
    }
    m_redoStack.clear();
    refreshQmlOverlay(frameSize);
    generateImage();
}

bool AutomaskHelper::pythonReady()
{
    SamInterface sam;
    const QString pythonExe = sam.venvPythonExecs().python;
    return !pythonExe.isEmpty();
}

void AutomaskHelper::launchSam(const QDir &previewFolder, int offset, const ObjectId &ownerForFilter, bool autoAdd, int previewPos)
{
    m_ownerForFilter = ownerForFilter;
    m_maskCreationMode = true;
    QStringList pointsList;
    QStringList labelsList;
    m_previewFolder = previewFolder;
    m_offset = offset;
    if (m_includePoints.contains(m_lastPos)) {
        const QList<QPoint> points = m_includePoints.value(m_lastPos);
        for (const auto &p : points) {
            pointsList << QString::number(p.x());
            pointsList << QString::number(p.y());
            labelsList << QStringLiteral("1");
        }
    }
    if (m_excludePoints.contains(m_lastPos)) {
        const QList<QPoint> points = m_excludePoints.value(m_lastPos);
        for (const auto &p : points) {
            pointsList << QString::number(p.x());
            pointsList << QString::number(p.y());
            labelsList << QStringLiteral("0");
        }
    }
    QRect box;
    if (m_boxes.contains(m_lastPos)) {
        box = m_boxes.value(m_lastPos);
    }
    bool ok;
    QDir maskSrcFolder = pCore->currentDoc()->getCacheDir(CacheMaskSource, &ok);
    if (!ok) {
        return;
    }
    SamInterface sam;
    std::pair<QString, QString> maskScript = {sam.venvPythonExecs().python, sam.getScript(QStringLiteral("automask/sam-objectmask.py"))};
    QStringList args = {maskScript.second,
                        QStringLiteral("-I"),
                        maskSrcFolder.absolutePath(),
                        QStringLiteral("-F"),
                        QString::number(m_lastPos),
                        QStringLiteral("-O"),
                        previewFolder.absolutePath(),
                        QStringLiteral("-M"),
                        KdenliveSettings::samModelFile(),
                        QStringLiteral("-C"),
                        SamInterface::configForModel()};
    if (!pointsList.isEmpty()) {
        args << QStringLiteral("-P") << QStringLiteral("%1=%2").arg(m_lastPos).arg(pointsList.join(QLatin1Char(','))) << QStringLiteral("-L")
             << QStringLiteral("%1=%2").arg(m_lastPos).arg(labelsList.join(QLatin1Char(',')));
    }
    if (!KdenliveSettings::samDevice().isEmpty()) {
        args << QStringLiteral("-D") << KdenliveSettings::samDevice();
    }
    if (KdenliveSettings::sam_offload_video()) {
        args << QStringLiteral("--offload");
    }
    if (!box.isNull()) {
        args << QStringLiteral("-B") << QStringLiteral("%1=%2,%3,%4,%5").arg(m_lastPos).arg(box.x()).arg(box.y()).arg(box.right()).arg(box.bottom());
    }
    if (KdenliveSettings::maskBorderWidth() > 0) {
        args << QStringLiteral("--border") << QString::number(KdenliveSettings::maskBorderWidth()) << QStringLiteral("--bordercolor")
             << QStringLiteral("%1,%2,%3,%4")
                    .arg(KdenliveSettings::maskBorderColor().red())
                    .arg(KdenliveSettings::maskBorderColor().green())
                    .arg(KdenliveSettings::maskBorderColor().blue())
                    .arg(KdenliveSettings::maskBorderColor().alpha());
    }
    args << QStringLiteral("--color")
         << QStringLiteral("%1,%2,%3,%4")
                .arg(KdenliveSettings::maskColor().red())
                .arg(KdenliveSettings::maskColor().green())
                .arg(KdenliveSettings::maskColor().blue())
                .arg(KdenliveSettings::maskColor().alpha());

    // Disconnect previous lambda connections to prevent accumulation on repeated calls
    disconnect(&m_samProcess, &QProcess::stateChanged, this, nullptr);
    disconnect(&m_samProcess, &QProcess::readyReadStandardOutput, this, nullptr);
    disconnect(&m_samProcess, &QProcess::readyReadStandardError, this, nullptr);

    connect(&m_samProcess, &QProcess::stateChanged, this, [this](QProcess::ProcessState state) {
        if (state == QProcess::NotRunning) {
            pCore->getMonitor(Kdenlive::ClipMonitor)->abortPreviewMask();
            m_jobStatus = QProcess::NotRunning;
            if (m_killedOnRequest) {
                Q_EMIT showMessage(QString(), KMessageWidget::Information);
            } else if (m_samProcess.exitStatus() == QProcess::CrashExit || m_samProcess.exitCode() != 0) {
                Q_EMIT processCrashed(m_errorLog);
            }
            m_maskCreationMode = false;
            Q_EMIT samJobFinished();
        }
        m_errorLog.clear();
        m_killedOnRequest = false;
    });
    connect(&m_samProcess, &QProcess::readyReadStandardOutput, this, [this, autoAdd]() {
        m_stdoutBuffer.append(m_samProcess.readAllStandardOutput());
        // Process only complete lines (terminated by '\n') to avoid partial-line parsing
        int newlineIdx;
        while ((newlineIdx = m_stdoutBuffer.indexOf('\n')) != -1) {
            const QByteArray lineData = m_stdoutBuffer.left(newlineIdx);
            m_stdoutBuffer.remove(0, newlineIdx + 1);
            const QString command = QString::fromUtf8(lineData).simplified();
            if (command.isEmpty()) {
                continue;
            }
            if (command.startsWith(QLatin1String("preview ok"))) {
                // Load preview image
                int frame = command.section(QLatin1Char(' '), -1).toInt();
                m_jobStatus = QProcess::NotRunning;
                Q_EMIT previewProcessing(false);
                QUrl url = QUrl::fromLocalFile(m_previewFolder.absoluteFilePath(QStringLiteral("preview-%1.png").arg(frame, 5, 10, QLatin1Char('0'))));
                url.setQuery(QStringLiteral("pos=%1&ctrl=%2").arg(m_lastPos).arg(QDateTime::currentSecsSinceEpoch()));
                pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->m_previewOverlay = url;
                Q_EMIT pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->previewOverlayChanged();
            } else if (command.startsWith(QLatin1String("frame_done"))) {
                // Streaming propagation progress: "frame_done N/total"
                const QString progress = command.section(QLatin1Char(' '), 1);
                int current = progress.section(QLatin1Char('/'), 0, 0).toInt();
                int total = progress.section(QLatin1Char('/'), 1, 1).toInt();
                if (total > 0) {
                    int percent = int(100.0 * current / total);
                    Q_EMIT updateProgress(percent);
                }
            } else if (command == QLatin1String("mask ok")) {
                m_jobStatus = QProcess::NotRunning;
                // Tell Python process to exit cleanly
                if (m_samProcess.state() == QProcess::Running) {
                    m_samProcess.write(QStringLiteral("finish\n").toUtf8());
                }
                auto binClip = pCore->projectItemModel()->getClipByBinID(m_binId);
                Q_ASSERT(binClip);
                MaskTask::start(ObjectId(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid()), m_ownerForFilter, m_maskParams, binClip.get(), autoAdd);
                // Ensure we hide the progress bar on completion
                Q_EMIT updateProgress(100);
            } else if (command.startsWith(QLatin1String("INFO:"))) {
                const QString msg = command.section(QLatin1Char(':'), 1);
                Q_EMIT showMessage(msg, KMessageWidget::Information);
            }
        }
    });
    connect(&m_samProcess, &QProcess::readyReadStandardError, this, [this]() {
        QString output = m_samProcess.readAllStandardError();
        if (output.contains(QLatin1String("%|"))) {
            output = output.section(QLatin1String("%|"), 0, 0).section(QLatin1Char(' '), -1);
            bool ok;
            int progress = output.toInt(&ok);
            if (ok) {
                Q_EMIT updateProgress(progress);
            }
        } else {
            m_errorLog.append(output);
        }
    });
    m_samProcess.setProcessChannelMode(QProcess::SeparateChannels);
    m_samProcess.setProgram(maskScript.first);
    QDir venvDir = QFileInfo(maskScript.first).dir();
    venvDir.cdUp();
    m_samProcess.setWorkingDirectory(venvDir.absolutePath());
    m_samProcess.setArguments(args);
    m_samProcess.start(QIODevice::ReadWrite | QIODevice::Text);
    m_samProcess.waitForStarted();
    if (previewPos > -1 && (!m_includePoints.isEmpty() || !m_boxes.isEmpty())) {
        m_lastPos = previewPos - m_offset;
        generateImage();
    }
}

void AutomaskHelper::generateImage()
{
    // Debounce rapid stroke/point additions — restart the timer on each call
    Q_EMIT previewProcessing(true);
    m_previewDebounce.start();
}

void AutomaskHelper::sendPreviewCommand()
{
    QStringList pointsList;
    QStringList labelsList;
    if (m_includePoints.contains(m_lastPos)) {
        const QList<QPoint> points = m_includePoints.value(m_lastPos);
        for (const auto &p : points) {
            pointsList << QString::number(p.x());
            pointsList << QString::number(p.y());
            labelsList << QStringLiteral("1");
        }
    }
    if (m_excludePoints.contains(m_lastPos)) {
        const QList<QPoint> points = m_excludePoints.value(m_lastPos);
        for (const auto &p : points) {
            pointsList << QString::number(p.x());
            pointsList << QString::number(p.y());
            labelsList << QStringLiteral("0");
        }
    }
    QRect box;
    if (m_boxes.contains(m_lastPos)) {
        box = m_boxes.value(m_lastPos);
    }
    bool ok;
    // Ensure the source cache dir exists
    pCore->currentDoc()->getCacheDir(CacheMaskSource, &ok);
    if (!ok) {
        Q_EMIT previewProcessing(false);
        return;
    }
    QStringList args = {QStringLiteral("-F"), QString::number(m_lastPos)};
    if (!pointsList.isEmpty()) {
        args << QStringLiteral("-P") << QStringLiteral("%1=%2").arg(m_lastPos).arg(pointsList.join(QLatin1Char(','))) << QStringLiteral("-L")
             << QStringLiteral("%1=%2").arg(m_lastPos).arg(labelsList.join(QLatin1Char(',')));
    }
    /*if (!KdenliveSettings::samDevice().isEmpty()) {
        args << QStringLiteral("-D") << KdenliveSettings::samDevice();
    }*/
    if (!box.isNull()) {
        args << QStringLiteral("-B") << QStringLiteral("%1=%2,%3,%4,%5").arg(m_lastPos).arg(box.x()).arg(box.y()).arg(box.right()).arg(box.bottom());
    }
    if (KdenliveSettings::maskBorderWidth() > 0) {
        args << QStringLiteral("--border") << QString::number(KdenliveSettings::maskBorderWidth()) << QStringLiteral("--bordercolor")
             << QStringLiteral("%1,%2,%3,%4")
                    .arg(KdenliveSettings::maskBorderColor().red())
                    .arg(KdenliveSettings::maskBorderColor().green())
                    .arg(KdenliveSettings::maskBorderColor().blue())
                    .arg(KdenliveSettings::maskBorderColor().alpha());
    } else {
        args << QStringLiteral("--border") << QString::number(0);
    }
    args << QStringLiteral("--color")
         << QStringLiteral("%1,%2,%3,%4")
                .arg(KdenliveSettings::maskColor().red())
                .arg(KdenliveSettings::maskColor().green())
                .arg(KdenliveSettings::maskColor().blue())
                .arg(KdenliveSettings::maskColor().alpha());
    // Pass overlay visualization mode: 0=color, 1=boundary, 2=alpha
    int overlayMode = pCore->getMonitor(Kdenlive::ClipMonitor)->getControllerProxy()->maskOverlayMode();
    args << QStringLiteral("--overlay-mode") << QString::number(overlayMode);
    const QString samCommand = QStringLiteral("preview=%1\n").arg(args.join(QLatin1Char(' ')));
    if (m_samProcess.state() == QProcess::Running) {
        m_jobStatus = QProcess::Running;
        m_samProcess.write(samCommand.toUtf8());
    } else {
        Q_EMIT previewProcessing(false);
    }
}

void AutomaskHelper::updateMaskParams()
{
    QStringList args;
    if (KdenliveSettings::maskBorderWidth() > 0) {
        args << QStringLiteral("--border") << QString::number(KdenliveSettings::maskBorderWidth()) << QStringLiteral("--bordercolor")
             << QStringLiteral("%1,%2,%3,%4")
                    .arg(KdenliveSettings::maskBorderColor().red())
                    .arg(KdenliveSettings::maskBorderColor().green())
                    .arg(KdenliveSettings::maskBorderColor().blue())
                    .arg(KdenliveSettings::maskBorderColor().alpha());
    } else {
        args << QStringLiteral("--border") << QString::number(0);
    }
    args << QStringLiteral("--color")
         << QStringLiteral("%1,%2,%3,%4")
                .arg(KdenliveSettings::maskColor().red())
                .arg(KdenliveSettings::maskColor().green())
                .arg(KdenliveSettings::maskColor().blue())
                .arg(KdenliveSettings::maskColor().alpha());
    const QString samCommand = QStringLiteral("edit=%1\n").arg(args.join(QLatin1Char(' ')));
    if (m_samProcess.state() == QProcess::Running) {
        m_jobStatus = QProcess::Running;
        m_samProcess.write(samCommand.toUtf8());
        // Re-render preview with updated visual params
        generateImage();
    }
}

bool AutomaskHelper::generateMask(const QString &binId, const QString &maskName, const QString &maskFile, const QPoint &zone)
{
    m_previewDebounce.stop();
    // Generate params
    m_binId = binId;
    m_maskParams.insert(MaskTask::ZONEIN, QString::number(zone.x()));
    m_maskParams.insert(MaskTask::ZONEOUT, QString::number(zone.y()));
    bool ok;
    QDir maskSrcFolder = pCore->currentDoc()->getCacheDir(CacheMaskSource, &ok);
    if (!ok) {
        return false;
    }
    if (!m_includePoints.contains(0) && !m_boxes.contains(0)) {
        KMessageBox::information(pCore->getMonitor(Kdenlive::ClipMonitor), i18n("You must define include points in the first video frame of the sequence."));
        pCore->getMonitor(Kdenlive::ClipMonitor)->requestSeek(zone.x());
        return false;
    }
    m_maskParams.insert(MaskTask::INPUTFOLDER, maskSrcFolder.absolutePath());
    QString outFolder = QStringLiteral("output-frames");
    if (maskSrcFolder.exists(outFolder)) {
        QDir toRemove(maskSrcFolder.absoluteFilePath(outFolder));
        if (toRemove.dirName() == outFolder) {
            toRemove.removeRecursively();
        }
    }
    maskSrcFolder.mkpath(outFolder);
    if (!maskSrcFolder.cd(outFolder)) {
        return false;
    }
    // Launch the sam analysis process
    m_jobStatus = QProcess::Running;
    // Use rerender= for corrections (re-propagation with reset state),
    // render= for first generation
    if (!maskFile.isEmpty()) {
        m_samProcess.write(QStringLiteral("rerender=%1\n").arg(maskSrcFolder.absolutePath()).toUtf8());
    } else {
        m_samProcess.write(QStringLiteral("render=%1\n").arg(maskSrcFolder.absolutePath()).toUtf8());
    }
    m_maskParams.insert(MaskTask::OUTPUTFOLDER, maskSrcFolder.absolutePath());
    m_maskParams.insert(MaskTask::NAME, maskName);
    // Generate points strings
    QStringList fullIncludePoints;
    QStringList fullExcludePoints;
    QStringList fullBoxList;
    for (auto i = m_includePoints.cbegin(), end = m_includePoints.cend(); i != end; ++i) {
        auto pts = i.value();
        QStringList pointsData;
        for (const auto &p : pts) {
            pointsData << QString::number(p.x()) << QString::number(p.y());
        }
        fullIncludePoints << QStringLiteral("%1=%2").arg(i.key()).arg(pointsData.join(QLatin1Char(',')));
    }
    for (auto i = m_excludePoints.cbegin(), end = m_excludePoints.cend(); i != end; ++i) {
        auto pts = i.value();
        QStringList pointsData;
        for (const auto &p : pts) {
            pointsData << QString::number(p.x()) << QString::number(p.y());
        }
        fullExcludePoints << QStringLiteral("%1=%2").arg(i.key()).arg(pointsData.join(QLatin1Char(',')));
    }
    for (auto i = m_boxes.cbegin(), end = m_boxes.cend(); i != end; ++i) {
        auto pts = i.value();
        QStringList pointsData = {QString::number(pts.x()), QString::number(pts.y()), QString::number(pts.right()), QString::number(pts.bottom())};
        fullBoxList << QStringLiteral("%1=%2").arg(i.key()).arg(pointsData.join(QLatin1Char(',')));
    }

    if (!fullIncludePoints.isEmpty()) {
        m_maskParams.insert(MaskTask::INCLUDEPOINTS, fullIncludePoints.join(QLatin1Char(';')));
    }
    if (!fullExcludePoints.isEmpty()) {
        m_maskParams.insert(MaskTask::EXCLUDEPOINTS, fullExcludePoints.join(QLatin1Char(';')));
    }
    if (!fullBoxList.isEmpty()) {
        m_maskParams.insert(MaskTask::BOXES, fullBoxList.join(QLatin1Char(';')));
    }
    std::shared_ptr<ProjectClip> clip = pCore->projectItemModel()->getClipByBinID(binId);
    if (clip) {
        bool ok;
        QDir maskFolder = pCore->currentDoc()->getCacheDir(CacheMask, &ok);
        if (!ok) {
            return false;
        }
        int ix = 1;
        QString outputFile;
        if (!maskFile.isEmpty()) {
            outputFile = maskFile;
        } else {
            const QString baseName = QStringLiteral("%1-%2-%3").arg(QStringUtils::getCleanFileName(maskName)).arg(zone.x()).arg(zone.y());
            outputFile = maskFolder.absoluteFilePath(baseName + QStringLiteral(".mkv"));
            while (QFile::exists(outputFile)) {
                QString secondName = QStringLiteral("%1-%2.mkv").arg(baseName).arg(ix, 4, 10, QLatin1Char('0'));
                outputFile = maskFolder.absoluteFilePath(secondName);
                ix++;
            }
        }
        m_maskParams.insert(MaskTask::OUTPUTFILE, outputFile);
    }
    return true;
}

void AutomaskHelper::abortJob()
{
    m_previewDebounce.stop();
    if (m_samProcess.state() == QProcess::Running) {
        m_killedOnRequest = true;
        m_samProcess.kill();
    } else {
        Q_EMIT samJobFinished();
    }
}

bool AutomaskHelper::jobRunning() const
{
    return m_maskCreationMode || m_jobStatus == QProcess::Running;
}

void AutomaskHelper::sceneUpdated(MonitorSceneType sceneType)
{
    if (sceneType == MonitorSceneAutoMask) {
        monitorSeek(m_seekPos);
    }
    disconnect(pCore->getMonitor(Kdenlive::ClipMonitor), &Monitor::sceneChanged, this, &AutomaskHelper::sceneUpdated);
}

void AutomaskHelper::monitorSeek(int pos)
{
    Monitor *mon = pCore->getMonitor(Kdenlive::ClipMonitor);
    if (!mon->effectSceneDisplayed(MonitorSceneAutoMask)) {
        // Monitor mask scene is not yet loaded
        m_seekPos = pos;
        connect(mon, &Monitor::sceneChanged, this, &AutomaskHelper::sceneUpdated, static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::UniqueConnection));
        return;
    }
    pos -= m_offset;
    QUrl url = QUrl::fromLocalFile(m_previewFolder.absoluteFilePath(QStringLiteral("preview-%1.png").arg(pos, 5, 10, QLatin1Char('0'))));
    mon->getControllerProxy()->m_previewOverlay = url;
    Q_EMIT mon->getControllerProxy()->previewOverlayChanged();
    // Update keyframe points
    m_lastPos = pos;
    QSize frameSize = pCore->getCurrentFrameDisplaySize();
    refreshQmlOverlay(frameSize);
}

void AutomaskHelper::terminate()
{
    m_previewDebounce.stop();
    m_stdoutBuffer.clear();
    if (m_samProcess.state() == QProcess::Running) {
        m_killedOnRequest = true;
        m_samProcess.blockSignals(true);
        m_samProcess.kill();
        // m_samProcess.write("q\n");
        // m_samProcess.waitForFinished(1000);
    }
}

void AutomaskHelper::undoAction()
{
    if (m_undoStack.isEmpty()) {
        return;
    }
    MaskUndoAction action = m_undoStack.pop();
    m_redoStack.push(action);

    switch (action.type) {
    case MaskUndoAction::Type::AddPoint: {
        QList<QPoint> &list = action.isExclude ? m_excludePoints[action.frame] : m_includePoints[action.frame];
        list.removeOne(action.point);
        if (list.isEmpty()) {
            (action.isExclude ? m_excludePoints : m_includePoints).remove(action.frame);
        }
        // Restore previous data if the action had cleared it
        if (action.hadPreviousData) {
            m_includePoints[action.frame] = action.previousIncludePoints;
            m_excludePoints[action.frame] = action.previousExcludePoints;
            if (!action.previousBox.isNull()) {
                m_boxes[action.frame] = action.previousBox;
            }
        }
        break;
    }
    case MaskUndoAction::Type::AddStroke: {
        QList<QPoint> &list = action.isExclude ? m_excludePoints[action.frame] : m_includePoints[action.frame];
        for (const QPoint &p : action.strokePoints) {
            list.removeOne(p);
        }
        if (list.isEmpty()) {
            (action.isExclude ? m_excludePoints : m_includePoints).remove(action.frame);
        }
        break;
    }
    case MaskUndoAction::Type::AddBox: {
        m_boxes.remove(action.frame);
        if (action.hadPreviousData) {
            m_includePoints[action.frame] = action.previousIncludePoints;
            m_excludePoints[action.frame] = action.previousExcludePoints;
            if (!action.previousBox.isNull()) {
                m_boxes[action.frame] = action.previousBox;
            }
        }
        break;
    }
    case MaskUndoAction::Type::MovePoint: {
        QList<QPoint> &list = action.movedExclude ? m_excludePoints[action.frame] : m_includePoints[action.frame];
        if (action.pointIndex >= 0 && action.pointIndex < list.size()) {
            list[action.pointIndex] = action.oldPosition;
        }
        break;
    }
    }

    QSize frameSize = pCore->getCurrentFrameDisplaySize();
    refreshQmlOverlay(frameSize);
    generateImage();
}

void AutomaskHelper::redoAction()
{
    if (m_redoStack.isEmpty()) {
        return;
    }
    MaskUndoAction action = m_redoStack.pop();
    m_undoStack.push(action);

    switch (action.type) {
    case MaskUndoAction::Type::AddPoint: {
        // If original action cleared existing data (non-extend), replicate that on redo
        if (action.hadPreviousData) {
            m_includePoints.remove(action.frame);
            m_excludePoints.remove(action.frame);
            m_boxes.remove(action.frame);
        }
        QList<QPoint> &list = action.isExclude ? m_excludePoints[action.frame] : m_includePoints[action.frame];
        if (!list.contains(action.point)) {
            list.append(action.point);
        }
        break;
    }
    case MaskUndoAction::Type::AddStroke: {
        QList<QPoint> &list = action.isExclude ? m_excludePoints[action.frame] : m_includePoints[action.frame];
        for (const QPoint &p : action.strokePoints) {
            if (!list.contains(p)) {
                list.append(p);
            }
        }
        break;
    }
    case MaskUndoAction::Type::AddBox: {
        // If original action cleared existing data (non-extend), replicate that on redo
        if (action.hadPreviousData) {
            m_includePoints.remove(action.frame);
            m_excludePoints.remove(action.frame);
            m_boxes.remove(action.frame);
        }
        m_boxes[action.frame] = action.box;
        break;
    }
    case MaskUndoAction::Type::MovePoint: {
        QList<QPoint> &list = action.movedExclude ? m_excludePoints[action.frame] : m_includePoints[action.frame];
        if (action.pointIndex >= 0 && action.pointIndex < list.size()) {
            list[action.pointIndex] = action.point; // point holds the new position
        }
        break;
    }
    }

    QSize frameSize = pCore->getCurrentFrameDisplaySize();
    refreshQmlOverlay(frameSize);
    generateImage();
}

void AutomaskHelper::cleanup()
{
    // We switched to another clip, delete all data if any
    m_previewDebounce.stop();
    m_stdoutBuffer.clear();
    m_includePoints.clear();
    m_excludePoints.clear();
    m_boxes.clear();
    m_undoStack.clear();
    m_redoStack.clear();
    // Remove source and preview data
    if (m_previewFolder.dirName() == QLatin1String("source-frames") && m_previewFolder.exists()) {
        m_previewFolder.removeRecursively();
    }
}

void AutomaskHelper::loadData(const QString &ipoints, const QString &epoints, const QString &boxes, int in, const QDir &previewFolder)
{
    m_includePoints.clear();
    m_excludePoints.clear();
    m_boxes.clear();
    m_offset = in;
    m_previewFolder = previewFolder;
    QSet<int> keyframes;
    QStringList pts = ipoints.split(QLatin1Char(';'));
    for (const auto &p : pts) {
        int frame = p.section(QLatin1Char('='), 0, 0).toInt();
        keyframes << frame;
        const QStringList pointsData = p.section(QLatin1Char('='), 1).split(QLatin1Char(','));
        if (pointsData.size() % 2 != 0) {
            // Invalid data
            continue;
        }
        QList<QPoint> pointsList;
        for (int ix = 0; ix < pointsData.size(); ix += 2) {
            pointsList << QPoint(pointsData.at(ix).toInt(), pointsData.at(ix + 1).toInt());
        }
        m_includePoints.insert(frame, pointsList);
    }
    pts = epoints.split(QLatin1Char(';'));
    for (const auto &p : pts) {
        int frame = p.section(QLatin1Char('='), 0, 0).toInt();
        keyframes << frame;
        const QStringList pointsData = p.section(QLatin1Char('='), 1).split(QLatin1Char(','));
        if (pointsData.size() % 2 != 0) {
            // Invalid data
            continue;
        }
        QList<QPoint> pointsList;
        for (int ix = 0; ix < pointsData.size(); ix += 2) {
            pointsList << QPoint(pointsData.at(ix).toInt(), pointsData.at(ix + 1).toInt());
        }
        m_excludePoints.insert(frame, pointsList);
    }
    pts = boxes.split(QLatin1Char(';'));
    for (const auto &p : pts) {
        int frame = p.section(QLatin1Char('='), 0, 0).toInt();
        keyframes << frame;
        QStringList pointsData = p.section(QLatin1Char('='), 1).split(QLatin1Char(','));
        if (pointsData.size() != 4) {
            // Invalid data
            continue;
        }
        QRect r(QPoint(pointsData.at(0).toInt(), pointsData.at(1).toInt()), QPoint(pointsData.at(2).toInt(), pointsData.at(3).toInt()));
        m_boxes.insert(frame, r);
    }
}
