/*
    SPDX-FileCopyrightText: 2011 Till Theato <root@ttill.de>
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractparamwidget.hpp"
#include "curves/keyframe/keyframecurveeditor.h"
#include "definitions.h"
#include <QMap>
#include <QPersistentModelIndex>
#include <QPointer>
#include <memory>
#include <unordered_map>

class AssetParameterModel;
class DoubleWidget;
class KeyframeView;
class KeyframeCurveEditor;
class KeyframeModelList;
class QVBoxLayout;
class QToolButton;
class QToolBar;
class QGroupBox;
class TimecodeDisplay;
class KSelectAction;
class ExpressionWidget;
class KeyframeMonitorHelper;
class RotatedRectHelper;
class KDualAction;
class GeometryWidget;
class QStackedWidget;
class QTabWidget;

class KeyframeContainer : public QObject
{
    Q_OBJECT

public:
    enum ViewMode { TimelineView = 0, CurveEditorView = 1, ExpressionView = 2 };
    explicit KeyframeContainer(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QSize frameSize, QWidget *parent, QFormLayout *layout);
    ~KeyframeContainer() override;

    /** @brief Add a new parameter to be managed using the same keyframe viewer. Also handles creation of KeyframeCurveEditor objects */
    void addParameter(const QPersistentModelIndex &index);
    /** @brief Check all animated params for existing expressions and enable expression mode if any found. Call after all addParameter() calls. */
    void checkInitialExpressionMode();
    int getPosition() const;
    /** @brief Returns the monitor scene required for this asset
     */
    MonitorSceneType requiredScene() const;
    /** @brief Show / hide keyframe related widgets
     */
    void showKeyframes(bool enable);
    /** @brief Returns true if keyframes options are visible
     */
    bool keyframesVisible() const;
    void resetKeyframes();
    ViewMode currentViewMode() const;
    int minimumHeight() const;
    /** @brief Initialize the needed scene and monitor helper for the effect/asset. Should be called before addParameter when setting up the widget. */
    void initNeededSceneAndHelper();

public Q_SLOTS:
    void slotRefresh();
    /** @brief initialize qml overlay
     */
    void slotInitMonitor(bool active, bool);
    /** @brief Activate a standard action passed from the mainwindow, like copy or paste */
    void sendStandardCommand(int command);
    void slotAddRemove(bool addOnly);
    void slotGoToNext();
    void slotGoToPrev();
    void slotSetPosition(int pos = -1, bool update = true);
    /** @brief remove the keyframe at given position
       If pos is negative, we remove keyframe at current position
     */
    void slotRemoveKeyframe(const QVector<int> &positions);
    /** @brief Add a keyframe with given parameter value at given pos.
       If pos is negative, then keyframe is added at current position
    */
    bool slotAddKeyframe(int pos = -1);
    void disconnectEffectStack();

private Q_SLOTS:
    /** @brief Update the value of the widgets to reflect keyframe change */
    void slotRefreshParams();
    void slotAtKeyframe(bool atKeyframe, bool singleKeyframe);
    void slotEditKeyframeType(QAction *action);
    void slotUpdateKeyframesFromMonitor(const QPersistentModelIndex &index, const QVariant &res);
    /** @brief Paste a keyframe from clipboard */
    void slotPasteKeyframeFromClipBoard();
    void slotCopySelectedKeyframes();
    void slotCopyKeyframes();
    void slotCopyValueAtCursorPos();
    void slotImportKeyframes();
    void slotRemoveNextKeyframes();
    /** @brief Seek to keyframe.
     *  @param ix the index of the keyframe we want to reach
     *  @param offset if different than 0, the parameter ix  is ignored and we seek to next or previous keyframe
     */
    void slotSeekToKeyframe(int ix, int offset);
    void slotSeekToPos(int pos);
    void monitorSeek(int pos);

private:
    std::shared_ptr<AssetParameterModel> m_model;
    QModelIndex m_index;
    QWidget *m_parent;
    QToolBar *m_toolbar;
    std::shared_ptr<KeyframeModelList> m_keyframes;
    KeyframeView *m_keyframeview;
    KeyframeMonitorHelper *m_monitorHelper;
    QTabWidget *m_curveeditorcontainer;
    int m_lastKeyframePos{-1};
    QVector<KeyframeCurveEditor *> m_curveeditorview;
    QStackedWidget *m_editorviewcontainer;
    KDualAction *m_addDeleteAction;
    QAction *m_centerAction;
    QAction *m_copyAction;
    QAction *m_pasteAction;
    QAction *m_previousKFAction;
    QAction *m_nextKFAction;
    QAction *m_applyAction;
    KSelectAction *m_selectType;
    TimecodeDisplay *m_time;
    MonitorSceneType m_neededScene;
    bool m_monitorActive{false};
    bool m_isRelative{false};
    QSize m_sourceFrameSize;
    void connectMonitor(bool active);
    void setDuration(int duration);
    void addCurveEditor(const QPersistentModelIndex &index, QString name = "", int rectindex = -1);
    std::unordered_map<QPersistentModelIndex, QWidget *> m_parameters;
    int m_baseHeight;
    int m_addedHeight;
    QFormLayout *m_layout;
    QMap<QString, QPointer<QGroupBox>> m_paramGroups; ///< Collapsible groups for shader params
    std::unique_ptr<GeometryWidget> m_geom;
    int m_curveContainerHeight{0};
    int m_fixedHeight{0};

    // View mode: Timeline / Curve Editor / Expression
    ViewMode m_viewMode{TimelineView};
    KSelectAction *m_viewSelector{nullptr};
    QAction *m_timelineViewAction{nullptr};
    QAction *m_curveViewAction{nullptr};
    QAction *m_expressionViewAction{nullptr};
    QToolButton *m_viewswitch{nullptr};
    void setViewMode(ViewMode mode);

    // Expression support (per-parameter tabs)
    QTabWidget *m_expressionContainer{nullptr};
    QVector<ExpressionWidget *> m_expressionWidgets;
    QVector<QPersistentModelIndex> m_animatedIndices;

Q_SIGNALS:
    void addIndex(QPersistentModelIndex ix);
    void setKeyframes(const QString &);
    void updateEffectKeyframe(bool atkeyframe, bool outside);
    void goToNext();
    void goToPrevious();
    void addRemove(bool addOnly = false);
    void onCurveEditorView();
    void onKeyframeView();
    void seekToPos(int);
    void activateEffect();
    void updateHeight();
};
