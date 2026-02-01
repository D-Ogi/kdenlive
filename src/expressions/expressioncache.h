/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QHash>
#include <QMutex>
#include <QPair>
#include <QString>
#include <QVector>

#include "expressionengine.h"

class ExpressionEngine;

/**
 * @class ExpressionCache
 * @brief Singleton cache for baked expression results.
 *
 * Manages an ExpressionEngine instance and caches baked animation strings.
 * Thread-safe via mutex (baking can be triggered from UI or project load).
 */
class ExpressionCache
{
public:
    static ExpressionCache &instance();

    /**
     * @brief Bake an expression into an MLT animation string.
     *
     * @param expression JavaScript expression
     * @param startFrame First frame (timeline position)
     * @param endFrame Last frame (timeline position)
     * @param fps Project FPS
     * @param clipDuration Clip duration in seconds
     * @param baseValue Base parameter value
     * @param clipIndex Clip index on track
     * @param audioPeakBoth Audio peaks (both channels), empty if no audio
     * @param audioPeakLeft Audio peaks (left channel)
     * @param audioPeakRight Audio peaks (right channel)
     * @return MLT animation string with discrete keyframes, or empty on error
     */
    QString bake(const QString &expression, int startFrame, int endFrame, double fps, double clipDuration, double baseValue, int clipIndex,
                 const QVector<float> &audioPeakBoth = {}, const QVector<float> &audioPeakLeft = {}, const QVector<float> &audioPeakRight = {},
                 int clipPosition = 0, int clipDurationFrames = 0, const QString &clipName = {}, int trackIndex = 0,
                 const QVector<QPair<int, double>> &keyframes = {}, const QVector<ExprMarkerData> &markers = {});

    /**
     * @brief Bake a path expression into roto-spline JSON.
     *
     * @param expression JavaScript expression using createPath()
     * @param startFrame First frame (timeline position)
     * @param endFrame Last frame (timeline position)
     * @param fps Project FPS
     * @param clipDuration Clip duration in seconds
     * @param clipPosition Clip position on timeline in frames
     * @param clipDurationFrames Clip duration in frames
     * @param clipName Source clip name
     * @param trackIndex Track index (0-based)
     * @param audioPeakBoth Audio peaks (both channels), empty if no audio
     * @param audioPeakLeft Audio peaks (left channel)
     * @param audioPeakRight Audio peaks (right channel)
     * @param markers Timeline markers/guides
     * @return JSON string for roto-spline filter, or empty on error
     */
    QString bakePath(const QString &expression, int startFrame, int endFrame, double fps, double clipDuration, int clipPosition = 0, int clipDurationFrames = 0,
                     const QString &clipName = {}, int trackIndex = 0, const QVector<float> &audioPeakBoth = {}, const QVector<float> &audioPeakLeft = {},
                     const QVector<float> &audioPeakRight = {}, const QVector<ExprMarkerData> &markers = {});

    /**
     * @brief Validate an expression without baking.
     * @return Empty string if valid, error message otherwise
     */
    QString validate(const QString &expression);

    /**
     * @brief Evaluate an expression at a single frame (for live preview).
     * @return QPair of (result, error). Error is empty on success.
     */
    QPair<double, QString> evaluateAtFrame(const QString &expression, double time, int frame, double duration, double fps, double baseValue, int clipIndex,
                                           const QVector<float> &audioPeakBoth = {}, const QVector<float> &audioPeakLeft = {},
                                           const QVector<float> &audioPeakRight = {}, int clipPosition = 0, int clipDurationFrames = 0,
                                           const QString &clipName = {}, int trackIndex = 0, const QVector<QPair<int, double>> &keyframes = {},
                                           const QVector<ExprMarkerData> &markers = {});

    /**
     * @brief Load combined audio peaks from all audio clips on the timeline
     * that overlap the given frame range.
     *
     * Iterates audio tracks, finds overlapping clips, reads their bin clip
     * audio caches, and combines them (max) into per-frame normalized vectors.
     *
     * @param timelineStart First timeline frame (inclusive)
     * @param timelineEnd Last timeline frame (inclusive)
     * @param fps Project FPS
     * @param[out] peakBoth Combined peak levels (both channels), one per frame
     * @param[out] peakLeft Left channel peaks (or copy of peakBoth for mono)
     * @param[out] peakRight Right channel peaks (or copy of peakBoth for mono)
     */
    static void loadTimelineAudioForRange(int timelineStart, int timelineEnd, double fps, QVector<float> &peakBoth, QVector<float> &peakLeft,
                                          QVector<float> &peakRight);

    /** @brief Set the clip parameter resolver for cross-clip references */
    void setClipResolver(ExpressionEngine::ClipParamResolver resolver);

    /** @brief Clear the clip resolver */
    void clearClipResolver();

    /** @brief Set the effect parameter resolver for thisEffect.param() */
    void setEffectResolver(ExpressionEngine::EffectParamResolver resolver);

    /** @brief Clear the effect resolver */
    void clearEffectResolver();

    /** @brief Set the image sampler callback for sampleImage() */
    void setImageSampler(ExpressionEngine::ImageSampler sampler);

    /** @brief Clear the image sampler */
    void clearImageSampler();

    /** @brief Set source image dimensions (thisClip.width/height) */
    void setImageContext(int width, int height);

    /** @brief Clear source image dimensions */
    void clearImageContext();

    /** @brief Set project-level context for thisProject JS object (AE thisComp) */
    void setProjectContext(int width, int height, double fps, double duration, double pixelAspect, const QString &name, const QString &fullPath, int numTracks,
                           double displayStartTime);

    /** @brief Reset thisProject to defaults */
    void clearProjectContext();

    /** @brief Set the clip-by-index resolver for clip(N) references */
    void setClipByIndexResolver(ExpressionEngine::ClipByIndexResolver resolver);

    /** @brief Clear the clip-by-index resolver */
    void clearClipByIndexResolver();

    /** @brief Set the clip metadata resolver for clip ref properties (.name, .duration, etc.) */
    void setClipMetadataResolver(ExpressionEngine::ClipMetadataResolver resolver);

    /** @brief Clear the clip metadata resolver */
    void clearClipMetadataResolver();

private:
    ExpressionCache();
    ~ExpressionCache();
    ExpressionCache(const ExpressionCache &) = delete;
    ExpressionCache &operator=(const ExpressionCache &) = delete;

    ExpressionEngine *m_engine;
    QMutex m_mutex;
};
