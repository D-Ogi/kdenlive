/*
SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"

#include <KMessageWidget>

#include <QDir>
#include <QObject>
#include <QPoint>
#include <QProcess>
#include <QStack>
#include <QTimer>
#include <QVariant>

#include <memory>

class Monitor;
class ProjectClip;
class SamInterface;

/** @brief Represents a single undoable mask editing action.
 *
 * Stores the action type, affected frame, and enough state to restore
 * the previous point/box/stroke data on undo. For non-extend actions
 * that clear existing frame data, the previous state is preserved in
 * previousIncludePoints/previousExcludePoints/previousBox.
 */
struct MaskUndoAction
{
    enum class Type { AddPoint, AddStroke, AddBox, MovePoint };
    Type type;
    int frame;
    /// For AddPoint: the single point that was added
    QPoint point;
    bool isExclude{false};
    /// For AddStroke: batch of points added as a brush stroke
    QList<QPoint> strokePoints;
    /// For AddBox: the bounding box added
    QRect box;
    /// For MovePoint: the index and old position to restore
    int pointIndex{-1};
    QPoint oldPosition;
    bool movedExclude{false};
    /// State before the action (for non-extend actions that cleared existing data)
    QList<QPoint> previousIncludePoints;
    QList<QPoint> previousExcludePoints;
    QRect previousBox;
    bool hadPreviousData{false};
};

class AutomaskHelper : public QObject
{
    Q_OBJECT

public:
    /** @brief Helper for interactive mask creation with SAM2 (point/stroke/box input, undo, preview) */
    explicit AutomaskHelper(QObject *parent = nullptr);
    void launchSam(const QDir &previewFolder, int offset, const ObjectId &ownerForFilter = ObjectId(), bool autoAdd = false, int previewPos = -1);
    bool jobRunning() const;
    void terminate();
    /** @brief Remove all masks tmp data */
    void cleanup();
    void loadData(const QString &points, const QString &labels, const QString &boxes, int in, const QDir &previewFolder);
    bool pythonReady();
    /** @brief Return false if the venv python exec is missing*/

public Q_SLOTS:
    bool generateMask(const QString &binId, const QString &maskName, const QString &maskFile, const QPoint &zone);
    void monitorSeek(int pos);
    void addMonitorControlPoint(int position, const QSize frameSize, int xPos, int yPos, bool extend, bool exclude);
    void moveMonitorControlPoint(int ix, int position, const QSize frameSize, int xPos, int yPos);
    void addMonitorControlRect(int position, const QSize frameSize, const QRect rect, bool extend);
    /** @brief Process a brush stroke as a batch of include/exclude points */
    void addMonitorControlStroke(int position, const QSize frameSize, const QList<QPoint> &points, bool isExclude);
    void abortJob();
    void updateMaskParams();
    /** @brief Undo the last mask editing action */
    void undoAction();
    /** @brief Redo the last undone action */
    void redoAction();

private:
    Monitor *m_monitor;
    std::shared_ptr<ProjectClip> m_clip;
    int m_lastPos{0};
    int m_offset{0};
    int m_seekPos{0};
    QMap<int, QList<QPoint>> m_includePoints;
    QMap<int, QList<QPoint>> m_excludePoints;
    QMap<int, QRect> m_boxes;
    QProcess m_samProcess;
    QDir m_previewFolder;
    QString m_errorLog;
    QProcess::ProcessState m_jobStatus{QProcess::NotRunning};
    QMap<int, QString> m_maskParams;
    QString m_binId;
    bool m_killedOnRequest{false};
    bool m_maskCreationMode{false};
    /** @brief Debounce timer for preview generation — batches rapid stroke additions */
    QTimer m_previewDebounce;
    /** @brief Buffer for partial stdout lines from the SAM process */
    QByteArray m_stdoutBuffer;
    /** @brief Undo/redo stacks for mask editing actions */
    QStack<MaskUndoAction> m_undoStack;
    QStack<MaskUndoAction> m_redoStack;
    static constexpr int MAX_UNDO_STACK_SIZE = 100;
    /** @brief Refresh the QML overlay with current point/box state */
    void refreshQmlOverlay(const QSize &frameSize);
    ObjectId m_ownerForFilter{KdenliveObjectType::NoItem, {}};

private Q_SLOTS:
    void generateImage();
    /** @brief Actually send the preview command to SAM (called by debounce timer) */
    void sendPreviewCommand();
    void sceneUpdated(MonitorSceneType sceneType);

Q_SIGNALS:
    void showMessage(const QString &message, KMessageWidget::MessageType type = KMessageWidget::Information);
    void updateProgress(int progress);
    /** @brief Emitted when a preview is being computed (true) or finished (false) */
    void previewProcessing(bool processing);
    void samJobFinished();
    void processCrashed(const QString &message);
};
