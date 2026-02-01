/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <functional>

#include <QPair>
#include <QString>
#include <QVector>
#include <array>

struct JSRuntime;
struct JSContext;

// Opaque handle for compiled JS bytecode.
// JSValue layout varies by QuickJS config (uint64_t, struct, or pointer).
// We store a heap copy internally; callers just pass the opaque handle.
struct ExprCompiledHandle
{
    void *opaque{nullptr};
};

/** @brief Path data returned by createPath() expressions (AE-compatible) */
struct ExprPathData
{
    struct Point
    {
        double x;
        double y;
    };
    QVector<Point> points;
    QVector<Point> inTangents;  ///< Handle offsets relative to point ([0,0] = linear)
    QVector<Point> outTangents; ///< Handle offsets relative to point ([0,0] = linear)
    bool isClosed{true};
};

/** @brief Marker data for expression engine injection (AE-compatible) */
struct ExprMarkerData
{
    double time;     ///< Position in seconds (clip-relative)
    QString comment; ///< Marker label/comment text
    double duration; ///< Duration in seconds (0 for point markers)
};

/**
 * @class ExpressionEngine
 * @brief QuickJS-based JavaScript expression evaluator for effect parameters.
 *
 * Evaluates JS expressions per-frame, compatible with After Effects Expressions.
 * Designed for the bake-to-dense-keyframes approach: expressions are evaluated for
 * every frame and the results stored as MLT animation strings.
 */
class ExpressionEngine
{
public:
    ExpressionEngine();
    ~ExpressionEngine();

    // Non-copyable
    ExpressionEngine(const ExpressionEngine &) = delete;
    ExpressionEngine &operator=(const ExpressionEngine &) = delete;

    /**
     * @brief Set the per-frame evaluation context (global JS variables).
     * @param time Current time in seconds from clip start
     * @param frame Current frame number (0-based, relative to clip)
     * @param duration Clip duration in seconds
     * @param fps Project FPS
     * @param value Base parameter value (from keyframes or constant)
     * @param index Clip index on the track (0-based)
     */
    void setContext(double time, int frame, double duration, double fps, double value, int index);

    /**
     * @brief Set clip metadata for thisClip/thisTrack JS objects.
     * @param clipPosition Clip position on timeline in frames
     * @param clipDurationFrames Clip duration in frames
     * @param clipName Source file name (or clip name)
     * @param trackIndex Track index (0-based)
     */
    void setClipContext(int clipPosition, int clipDurationFrames, const QString &clipName, int trackIndex);

    /** Callback type for resolving cross-clip parameter references.
     *  Arguments: clipName, effectId, paramName → parameter value */
    using ClipParamResolver = std::function<double(const QString &clipName, const QString &effectId, const QString &paramName)>;

    /** @brief Set a callback for resolving clip(name).effect(name).param(name) */
    void setClipResolver(ClipParamResolver resolver);

    /** @brief Clear the clip resolver */
    void clearClipResolver();

    /** @brief Resolve a cross-clip parameter reference (called from JS clip().effect().param()) */
    double resolveClipParam(const QString &clipName, const QString &effectId, const QString &paramName);

    /** Callback type for resolving thisEffect.param() references.
     *  Reads another parameter from the same effect. */
    using EffectParamResolver = std::function<double(const QString &paramName)>;

    /** @brief Set a callback for resolving thisEffect.param(name) */
    void setEffectResolver(EffectParamResolver resolver);

    /** @brief Clear the effect resolver */
    void clearEffectResolver();

    /** @brief Resolve a same-effect parameter reference (called from JS thisEffect.param()) */
    double resolveEffectParam(const QString &paramName);

    /** Callback type for sampling source image pixels.
     *  Arguments: frame (0-based relative to clip), x, y, radius
     *  Returns: [r, g, b, a] normalized 0.0-1.0 */
    using ImageSampler = std::function<std::array<double, 4>(int frame, double x, double y, double radius)>;

    /** @brief Set a callback for sampleImage() pixel sampling */
    void setImageSampler(ImageSampler sampler);

    /** @brief Clear the image sampler */
    void clearImageSampler();

    /** @brief Sample source image at given coordinates (called from JS sampleImage()) */
    std::array<double, 4> sampleImage(int frame, double x, double y, double radius);

    /** @brief Set source image dimensions as thisClip.width/height */
    void setImageContext(int width, int height);

    /** @brief Clear source image dimensions (set to 0) */
    void clearImageContext();

    /**
     * @brief Check if an expression references sampleImage.
     * @param expression JavaScript expression string
     * @return true if the expression contains sampleImage calls
     */
    static bool usesImage(const QString &expression);

    // ── Project context (AE thisComp equivalent) ─────────────────────

    /**
     * @brief Set project-level context for thisProject JS object.
     *
     * Populates the AE-compatible thisProject object with project/timeline
     * properties. Call before baking or evaluation.
     *
     * @param width Project resolution width in pixels
     * @param height Project resolution height in pixels
     * @param fps Project frame rate
     * @param duration Total timeline duration in seconds
     * @param pixelAspect Pixel aspect ratio (1.0 for square pixels)
     * @param name Project file name (without path)
     * @param fullPath Absolute path to project file
     * @param numTracks Total number of timeline tracks (video + audio)
     * @param displayStartTime Timeline start time offset in seconds
     */
    void setProjectContext(int width, int height, double fps, double duration, double pixelAspect, const QString &name, const QString &fullPath, int numTracks,
                           double displayStartTime);

    /** @brief Reset thisProject to defaults (all zeros/empty) */
    void clearProjectContext();

    /**
     * @brief Check if an expression references thisProject properties.
     * @param expression JavaScript expression string
     * @return true if the expression contains thisProject references
     */
    static bool usesProject(const QString &expression);

    // ── Clip-by-index resolution ─────────────────────────────────────

    /** Callback type for resolving clip references by track position index.
     *  Arguments: clipIndex (0-based on same track), effectId, paramName → value */
    using ClipByIndexResolver = std::function<double(int clipIndex, const QString &effectId, const QString &paramName)>;

    /** @brief Set a callback for resolving clip(index).effect(name).param(name) */
    void setClipByIndexResolver(ClipByIndexResolver resolver);

    /** @brief Clear the clip-by-index resolver */
    void clearClipByIndexResolver();

    /** @brief Resolve a clip parameter by track position index */
    double resolveClipByIndex(int clipIndex, const QString &effectId, const QString &paramName);

    // ── Clip metadata resolution (for clip ref properties) ───────────

    /** @brief Metadata about a clip on the timeline (AE Layer attributes) */
    struct ClipMetadata
    {
        QString name;        ///< Source clip name
        int index{0};        ///< 0-based position on track
        double inPoint{0};   ///< Clip in-point in seconds (AE: layer.inPoint)
        double outPoint{0};  ///< Clip out-point in seconds (AE: layer.outPoint)
        double startTime{0}; ///< Timeline start position in seconds (AE: layer.startTime)
        double duration{0};  ///< Clip duration in seconds
        int width{0};        ///< Source media width in pixels
        int height{0};       ///< Source media height in pixels
        bool hasVideo{true};
        bool hasAudio{false};
        bool valid{false}; ///< Set to true when metadata was successfully resolved
    };

    /** Callback type for resolving clip metadata by track position index.
     *  Returns ClipMetadata with valid=true on success. */
    using ClipMetadataResolver = std::function<ClipMetadata(int clipIndex)>;

    /** @brief Set a callback for resolving clip metadata (name, position, etc.) */
    void setClipMetadataResolver(ClipMetadataResolver resolver);

    /** @brief Clear the clip metadata resolver */
    void clearClipMetadataResolver();

    /** @brief Resolve clip metadata by track position index */
    ClipMetadata resolveClipMetadata(int clipIndex);

    /**
     * @brief Set the audio level cache for audioLevel()/audioRms() functions.
     * @param peakBoth Normalized peak levels (0.0-1.0), one per frame, both channels
     * @param peakLeft Left channel peaks
     * @param peakRight Right channel peaks
     * @param totalFrames Total number of frames in cache
     * @param fps Project FPS (for time-to-frame conversion)
     */
    void setAudioCache(const QVector<float> &peakBoth, const QVector<float> &peakLeft, const QVector<float> &peakRight, int totalFrames, double fps);

    /** @brief Clear the audio cache */
    void clearAudioCache();

    /**
     * @brief Set the keyframe cache for loopIn()/loopOut() functions.
     * @param keyframes List of (frame, value) pairs, sorted by frame
     * @param fps Project FPS (for frame-to-seconds conversion)
     */
    void setKeyframes(const QVector<QPair<int, double>> &keyframes, double fps);

    /** @brief Clear the keyframe cache */
    void clearKeyframes();

    /**
     * @brief Check if an expression references loopIn/loopOut functions.
     * @param expression JavaScript expression string
     * @return true if the expression contains loopIn or loopOut calls
     */
    static bool usesKeyframes(const QString &expression);

    /**
     * @brief Set the marker cache for marker.key()/marker.nearestKey() functions.
     *
     * Stores markers as globalThis._markers JS array of {t, comment, duration}.
     * Also updates marker.numKeys on the existing marker object.
     *
     * @param markers List of marker data, sorted by time
     */
    void setMarkers(const QVector<ExprMarkerData> &markers);

    /** @brief Clear the marker cache */
    void clearMarkers();

    /**
     * @brief Check if an expression references marker functions.
     * @param expression JavaScript expression string
     * @return true if the expression contains marker. references
     */
    static bool usesMarkers(const QString &expression);

    /**
     * @brief Evaluate a JS expression and return the result.
     * @param expression JavaScript expression string
     * @return QPair of (numeric result, error string). Error is empty on success.
     */
    QPair<double, QString> evaluate(const QString &expression);

    /**
     * @brief Compile a JS expression into bytecode for repeated execution.
     *
     * Returns an opaque handle. The compiled function is freed by freeCompiled().
     * Use evaluateCompiled() for per-frame calls in bake loops to avoid
     * repeated parsing (10-50x faster than evaluate()).
     *
     * @param expression JavaScript expression string
     * @param[out] errorMsg Set to error message on failure (empty on success)
     * @return Compiled bytecode handle (JS_UNDEFINED on error)
     */
    ExprCompiledHandle compile(const QString &expression, QString &errorMsg);

    /**
     * @brief Execute a previously compiled expression.
     * @param compiled Handle from compile()
     * @return QPair of (numeric result, error string). Error is empty on success.
     */
    QPair<double, QString> evaluateCompiled(ExprCompiledHandle compiled);

    /**
     * @brief Execute a previously compiled path expression.
     * @param compiled Handle from compile()
     * @return QPair of (path data, error string). Error is empty on success.
     */
    QPair<ExprPathData, QString> evaluatePathCompiled(ExprCompiledHandle compiled);

    /**
     * @brief Free a compiled expression handle.
     * @param compiled Handle from compile()
     */
    void freeCompiled(ExprCompiledHandle compiled);

    /**
     * @brief Bake an expression into a dense MLT animation string.
     *
     * Iterates over every frame in [startFrame, endFrame], sets the context,
     * evaluates the expression, and builds a string like "0|=v0;1|=v1;2|=v2;..."
     * using discrete keyframes (|= prefix) so MLT doesn't interpolate.
     *
     * @param expression JavaScript expression string
     * @param startFrame First frame (inclusive)
     * @param endFrame Last frame (inclusive)
     * @param fps Project FPS
     * @param clipDuration Clip duration in seconds
     * @param baseValue Base parameter value
     * @param clipIndex Clip index on track
     * @return MLT animation string, or empty string on error
     */
    QString bakeToAnimString(const QString &expression, int startFrame, int endFrame, double fps, double clipDuration, double baseValue, int clipIndex);

    /**
     * @brief Evaluate a JS expression that returns a createPath() object.
     * @param expression JavaScript expression string
     * @return QPair of (path data, error string). Error is empty on success.
     */
    QPair<ExprPathData, QString> evaluatePath(const QString &expression);

    /**
     * @brief Bake a path expression into roto-spline JSON.
     *
     * Iterates over every frame, evaluates the path expression, and builds JSON
     * matching the rotoscoping filter's spline format:
     * {"000": [[h1x,h1y],[px,py],[h2x,h2y],...], "001": [...], ...}
     *
     * @param expression JavaScript expression string
     * @param startFrame First frame (inclusive)
     * @param endFrame Last frame (inclusive)
     * @param fps Project FPS
     * @param clipDuration Clip duration in seconds
     * @param clipIndex Clip index on track
     * @return JSON string for roto-spline, or empty string on error
     */
    QString bakeToPathJson(const QString &expression, int startFrame, int endFrame, double fps, double clipDuration, int clipIndex);

    /**
     * @brief Check if an expression references createPath.
     * @param expression JavaScript expression string
     * @return true if the expression contains createPath calls
     */
    static bool usesPath(const QString &expression);

    /**
     * @brief Validate an expression without side effects.
     * @param expression JavaScript expression string
     * @return Empty string if valid, error message otherwise
     */
    QString validate(const QString &expression);

    /**
     * @brief Check if an expression references audio functions.
     * @param expression JavaScript expression string
     * @return true if the expression contains audioLevel or audioRms calls
     */
    static bool usesAudio(const QString &expression);

    // ── Direct C++ cache access for static JS functions ─────────────────
    // Static JS-callable functions use JS_GetContextOpaque(ctx) to retrieve
    // the ExpressionEngine* and access these caches directly, avoiding
    // per-frame JS global reads.

    /** @brief Read audio peak for a given channel at frame index (clamped) */
    float audioPeak(const char *channel, int frame) const;

    /** @brief Audio cache total frames */
    int audioTotalFrames() const { return m_audioTotalFrames; }

    /** @brief Audio cache FPS */
    double audioFps() const { return m_audioFps; }

    /** @brief Cached keyframe entry (frame time in seconds + value) */
    struct CachedKF
    {
        double t;
        double v;
    };

    /** @brief Keyframe cache (read-only access for static functions) */
    const QVector<CachedKF> &keyframeCache() const { return m_keyframeCache; }

    /** @brief Keyframe cache FPS */
    double keyframeFps() const { return m_keyframeFps; }

    /** @brief Linearly interpolate cached keyframes at time t (O(log n) binary search) */
    double interpCached(double t) const;

    /** @brief Compute velocity at time t via central difference on cached keyframes */
    double velocityCached(double t) const;

    // ── Per-frame context cached in C++ (avoids JS global reads) ──────
    // Set by setContext(), read by buildRandomSeed() and other hot-path functions
    // via JS_GetContextOpaque().
    int cachedFrame() const { return m_cachedFrame; }
    int cachedIndex() const { return m_cachedIndex; }
    double cachedTime() const { return m_cachedTime; }
    double cachedFps() const { return m_cachedFps; }
    double cachedValue() const { return m_cachedValue; }

    // seedRandom() state (set by js_seedRandom, read by buildRandomSeed)
    uint32_t userSeed() const { return m_userSeed; }
    bool hasUserSeed() const { return m_hasUserSeed; }
    bool timelessSeed() const { return m_timelessSeed; }
    void setUserSeed(uint32_t seed, bool timeless)
    {
        m_userSeed = seed;
        m_hasUserSeed = true;
        m_timelessSeed = timeless;
    }

private:
    JSRuntime *m_runtime;
    JSContext *m_ctx;

    // Audio cache (owned by engine, set via setAudioCache)
    QVector<float> m_audioPeakBoth;
    QVector<float> m_audioPeakLeft;
    QVector<float> m_audioPeakRight;
    int m_audioTotalFrames{0};
    double m_audioFps{25.0};

    // Keyframe cache (C++ mirror of JS _keyframes array, set via setKeyframes)
    QVector<CachedKF> m_keyframeCache;
    double m_keyframeFps{25.0};

    // Per-frame context (C++ mirror of JS globals, set via setContext)
    int m_cachedFrame{0};
    int m_cachedIndex{0};
    double m_cachedTime{0.0};
    double m_cachedFps{25.0};
    double m_cachedValue{0.0};

    // seedRandom() state (C++ mirror, avoids JS _userSeed/_timeless reads)
    uint32_t m_userSeed{0};
    bool m_hasUserSeed{false};
    bool m_timelessSeed{false};

    /** @brief Register built-in functions and globals in the JS context */
    void registerBuiltins();

    ClipParamResolver m_clipResolver;
    ClipByIndexResolver m_clipByIndexResolver;
    ClipMetadataResolver m_clipMetadataResolver;
    EffectParamResolver m_effectResolver;
    ImageSampler m_imageSampler;
};
