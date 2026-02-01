/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "expressioncache.h"
#include "expressionengine.h"

#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "definitions.h"
#include "project/projectmanager.h"
#include "timeline2/model/timelineitemmodel.hpp"

#include <QDebug>
#include <QMutexLocker>

#include <algorithm>
#include <cmath>

ExpressionCache &ExpressionCache::instance()
{
    static ExpressionCache s_instance;
    return s_instance;
}

ExpressionCache::ExpressionCache()
    : m_engine(new ExpressionEngine())
{
}

ExpressionCache::~ExpressionCache()
{
    delete m_engine;
}

QString ExpressionCache::bake(const QString &expression, int startFrame, int endFrame, double fps, double clipDuration, double baseValue, int clipIndex,
                              const QVector<float> &audioPeakBoth, const QVector<float> &audioPeakLeft, const QVector<float> &audioPeakRight, int clipPosition,
                              int clipDurationFrames, const QString &clipName, int trackIndex, const QVector<QPair<int, double>> &keyframes,
                              const QVector<ExprMarkerData> &markers)
{
    QMutexLocker locker(&m_mutex);

    // Set audio cache if the expression uses audio functions
    if (ExpressionEngine::usesAudio(expression) && !audioPeakBoth.isEmpty()) {
        m_engine->setAudioCache(audioPeakBoth, audioPeakLeft, audioPeakRight, audioPeakBoth.size(), fps);
    } else {
        m_engine->clearAudioCache();
    }

    // Set keyframe cache if the expression uses loopIn/loopOut
    if (ExpressionEngine::usesKeyframes(expression) && !keyframes.isEmpty()) {
        m_engine->setKeyframes(keyframes, fps);
    } else {
        m_engine->clearKeyframes();
    }

    // Set marker cache if the expression uses marker references
    if (ExpressionEngine::usesMarkers(expression) && !markers.isEmpty()) {
        m_engine->setMarkers(markers);
    } else {
        m_engine->clearMarkers();
    }

    // Set clip metadata for thisClip/thisTrack references
    m_engine->setClipContext(clipPosition, clipDurationFrames, clipName, trackIndex);

    QString result = m_engine->bakeToAnimString(expression, startFrame, endFrame, fps, clipDuration, baseValue, clipIndex);
    if (result.isEmpty()) {
        qWarning() << "Expression bake failed for:" << expression;
    }
    return result;
}

QString ExpressionCache::bakePath(const QString &expression, int startFrame, int endFrame, double fps, double clipDuration, int clipPosition,
                                  int clipDurationFrames, const QString &clipName, int trackIndex, const QVector<float> &audioPeakBoth,
                                  const QVector<float> &audioPeakLeft, const QVector<float> &audioPeakRight, const QVector<ExprMarkerData> &markers)
{
    QMutexLocker locker(&m_mutex);

    // Set audio cache if the expression uses audio functions
    if (ExpressionEngine::usesAudio(expression) && !audioPeakBoth.isEmpty()) {
        m_engine->setAudioCache(audioPeakBoth, audioPeakLeft, audioPeakRight, audioPeakBoth.size(), fps);
    } else {
        m_engine->clearAudioCache();
    }

    // Set marker cache if the expression uses marker references
    if (ExpressionEngine::usesMarkers(expression) && !markers.isEmpty()) {
        m_engine->setMarkers(markers);
    } else {
        m_engine->clearMarkers();
    }

    // Set clip metadata for thisClip/thisTrack references
    m_engine->setClipContext(clipPosition, clipDurationFrames, clipName, trackIndex);

    QString result = m_engine->bakeToPathJson(expression, startFrame, endFrame, fps, clipDuration, 0);
    if (result.isEmpty()) {
        qWarning() << "Path expression bake failed for:" << expression;
    }
    return result;
}

QString ExpressionCache::validate(const QString &expression)
{
    QMutexLocker locker(&m_mutex);
    return m_engine->validate(expression);
}

QPair<double, QString> ExpressionCache::evaluateAtFrame(const QString &expression, double time, int frame, double duration, double fps, double baseValue,
                                                        int clipIndex, const QVector<float> &audioPeakBoth, const QVector<float> &audioPeakLeft,
                                                        const QVector<float> &audioPeakRight, int clipPosition, int clipDurationFrames, const QString &clipName,
                                                        int trackIndex, const QVector<QPair<int, double>> &keyframes, const QVector<ExprMarkerData> &markers)
{
    QMutexLocker locker(&m_mutex);

    // Set audio cache if available
    if (ExpressionEngine::usesAudio(expression) && !audioPeakBoth.isEmpty()) {
        m_engine->setAudioCache(audioPeakBoth, audioPeakLeft, audioPeakRight, audioPeakBoth.size(), fps);
    } else {
        m_engine->clearAudioCache();
    }

    // Set keyframe cache if the expression uses loopIn/loopOut
    if (ExpressionEngine::usesKeyframes(expression) && !keyframes.isEmpty()) {
        m_engine->setKeyframes(keyframes, fps);
    } else {
        m_engine->clearKeyframes();
    }

    // Set marker cache if the expression uses marker references
    if (ExpressionEngine::usesMarkers(expression) && !markers.isEmpty()) {
        m_engine->setMarkers(markers);
    } else {
        m_engine->clearMarkers();
    }

    // Set clip metadata for thisClip/thisTrack references
    m_engine->setClipContext(clipPosition, clipDurationFrames, clipName, trackIndex);

    m_engine->setContext(time, frame, duration, fps, baseValue, clipIndex);
    return m_engine->evaluate(expression);
}

void ExpressionCache::setClipResolver(ExpressionEngine::ClipParamResolver resolver)
{
    QMutexLocker locker(&m_mutex);
    m_engine->setClipResolver(std::move(resolver));
}

void ExpressionCache::clearClipResolver()
{
    QMutexLocker locker(&m_mutex);
    m_engine->clearClipResolver();
}

void ExpressionCache::setEffectResolver(ExpressionEngine::EffectParamResolver resolver)
{
    QMutexLocker locker(&m_mutex);
    m_engine->setEffectResolver(std::move(resolver));
}

void ExpressionCache::clearEffectResolver()
{
    QMutexLocker locker(&m_mutex);
    m_engine->clearEffectResolver();
}

void ExpressionCache::setImageSampler(ExpressionEngine::ImageSampler sampler)
{
    QMutexLocker locker(&m_mutex);
    m_engine->setImageSampler(std::move(sampler));
}

void ExpressionCache::clearImageSampler()
{
    QMutexLocker locker(&m_mutex);
    m_engine->clearImageSampler();
}

void ExpressionCache::setImageContext(int width, int height)
{
    QMutexLocker locker(&m_mutex);
    m_engine->setImageContext(width, height);
}

void ExpressionCache::clearImageContext()
{
    QMutexLocker locker(&m_mutex);
    m_engine->clearImageContext();
}

void ExpressionCache::setProjectContext(int width, int height, double fps, double duration, double pixelAspect, const QString &name, const QString &fullPath,
                                        int numTracks, double displayStartTime)
{
    QMutexLocker locker(&m_mutex);
    m_engine->setProjectContext(width, height, fps, duration, pixelAspect, name, fullPath, numTracks, displayStartTime);
}

void ExpressionCache::clearProjectContext()
{
    QMutexLocker locker(&m_mutex);
    m_engine->clearProjectContext();
}

void ExpressionCache::setClipByIndexResolver(ExpressionEngine::ClipByIndexResolver resolver)
{
    QMutexLocker locker(&m_mutex);
    m_engine->setClipByIndexResolver(std::move(resolver));
}

void ExpressionCache::clearClipByIndexResolver()
{
    QMutexLocker locker(&m_mutex);
    m_engine->clearClipByIndexResolver();
}

void ExpressionCache::setClipMetadataResolver(ExpressionEngine::ClipMetadataResolver resolver)
{
    QMutexLocker locker(&m_mutex);
    m_engine->setClipMetadataResolver(std::move(resolver));
}

void ExpressionCache::clearClipMetadataResolver()
{
    QMutexLocker locker(&m_mutex);
    m_engine->clearClipMetadataResolver();
}

void ExpressionCache::loadTimelineAudioForRange(int timelineStart, int timelineEnd, double fps, QVector<float> &peakBoth, QVector<float> &peakLeft,
                                                QVector<float> &peakRight)
{
    const int numFrames = timelineEnd - timelineStart + 1;
    if (numFrames <= 0) return;

    peakBoth.fill(0.0f, numFrames);
    peakLeft.fill(0.0f, numFrames);
    peakRight.fill(0.0f, numFrames);

    // Access the current timeline model
    auto timeline = pCore->projectManager()->getTimeline();
    if (!timeline) return;

    // Get all audio track IDs
    QList<int> audioTrackIds = timeline->getTracksIds(true);
    if (audioTrackIds.isEmpty()) return;

    auto projectItemModel = pCore->projectItemModel();
    if (!projectItemModel) return;

    for (int trackId : audioTrackIds) {
        // Get all clips on this audio track that overlap our range
        std::unordered_set<int> clipIds = timeline->getItemsInRange(trackId, timelineStart, timelineEnd, false);

        for (int clipId : clipIds) {
            if (!timeline->isClip(clipId)) continue;

            int clipPosition = timeline->getClipPosition(clipId);
            int clipPlaytime = timeline->getClipPlaytime(clipId);
            int clipIn = timeline->getClipIn(clipId);
            QString binId = timeline->getClipBinId(clipId);

            int clipEnd = clipPosition + clipPlaytime - 1;

            // Verify overlap
            if (clipPosition > timelineEnd || clipEnd < timelineStart) continue;

            // Get the bin clip
            auto binClip = projectItemModel->getClipByBinID(binId);
            if (!binClip) continue;

            // Get raw audio cache (stream 0)
            QVector<int16_t> rawCache = binClip->audioFrameCache(0);
            if (rawCache.isEmpty()) continue;

            int16_t maxVal = binClip->getAudioMax(0);
            if (maxVal <= 0) maxVal = 1;
            float normMax = static_cast<float>(maxVal);

            // Determine channel count from the cache
            // Total cache size = numSourceFrames * AUDIOLEVELS_POINTS_PER_FRAME * channels
            // We need to figure out the source clip's frame count to deduce channels
            int ppf = AUDIOLEVELS_POINTS_PER_FRAME;
            // Guess channel count: try stereo first, then mono
            int channels = 2;
            int totalSourceSamples = rawCache.size();
            // If the cache doesn't divide evenly by 2*ppf but does by 1*ppf, it's mono
            if (totalSourceSamples % (2 * ppf) != 0 && totalSourceSamples % ppf == 0) {
                channels = 1;
            }

            // Iterate over each timeline frame in our range that this clip covers
            int overlapStart = std::max(timelineStart, clipPosition);
            int overlapEnd = std::min(timelineEnd, clipEnd);

            for (int tlFrame = overlapStart; tlFrame <= overlapEnd; tlFrame++) {
                int outIdx = tlFrame - timelineStart;
                int sourceFrame = tlFrame - clipPosition + clipIn;

                int cacheOffset = sourceFrame * ppf * channels;
                if (cacheOffset < 0 || cacheOffset + ppf * channels > rawCache.size()) continue;

                if (channels == 2) {
                    float maxL = 0.0f, maxR = 0.0f, maxBoth = 0.0f;
                    for (int s = 0; s < ppf; s++) {
                        float lVal = std::abs(static_cast<float>(rawCache[cacheOffset + s * 2])) / normMax;
                        float rVal = std::abs(static_cast<float>(rawCache[cacheOffset + s * 2 + 1])) / normMax;
                        maxL = std::max(maxL, lVal);
                        maxR = std::max(maxR, rVal);
                        maxBoth = std::max(maxBoth, std::max(lVal, rVal));
                    }
                    peakBoth[outIdx] = std::max(peakBoth[outIdx], std::min(maxBoth, 1.0f));
                    peakLeft[outIdx] = std::max(peakLeft[outIdx], std::min(maxL, 1.0f));
                    peakRight[outIdx] = std::max(peakRight[outIdx], std::min(maxR, 1.0f));
                } else {
                    // Mono
                    float maxMono = 0.0f;
                    for (int s = 0; s < ppf; s++) {
                        float val = std::abs(static_cast<float>(rawCache[cacheOffset + s])) / normMax;
                        maxMono = std::max(maxMono, val);
                    }
                    maxMono = std::min(maxMono, 1.0f);
                    peakBoth[outIdx] = std::max(peakBoth[outIdx], maxMono);
                    peakLeft[outIdx] = std::max(peakLeft[outIdx], maxMono);
                    peakRight[outIdx] = std::max(peakRight[outIdx], maxMono);
                }
            }
        }
    }
}
