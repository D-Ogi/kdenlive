/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "expressionengine.h"
#include "expressionfunctions.h"

extern "C" {
#include "quickjs.h"
}

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

ExpressionEngine::ExpressionEngine()
{
    m_runtime = JS_NewRuntime();
    // Limit memory to 8MB — expressions should be simple
    JS_SetMemoryLimit(m_runtime, 8 * 1024 * 1024);
    // Limit stack size to 256KB
    JS_SetMaxStackSize(m_runtime, 256 * 1024);

    m_ctx = JS_NewContext(m_runtime);
    registerBuiltins();
}

ExpressionEngine::~ExpressionEngine()
{
    if (m_ctx) {
        JS_FreeContext(m_ctx);
    }
    if (m_runtime) {
        JS_FreeRuntime(m_runtime);
    }
}

void ExpressionEngine::registerBuiltins()
{
    // Register all built-in expression functions
    registerExpressionFunctions(m_ctx);

    // Register path functions: createPath()
    registerPathFunctions(m_ctx);

    // Register keyframe access functions: key(), nearestKey(), valueAtTime(), etc.
    registerKeyframeFunctions(m_ctx);

    // Store this pointer for cross-clip functions
    JS_SetContextOpaque(m_ctx, this);

    // Register clip reference functions: clip(name).effect(name).param(name)
    registerClipReferenceFunctions(m_ctx);

    // Initialize global variables with defaults
    JSValue global = JS_GetGlobalObject(m_ctx);
    JS_SetPropertyStr(m_ctx, global, "time", JS_NewFloat64(m_ctx, 0.0));
    JS_SetPropertyStr(m_ctx, global, "frame", JS_NewInt32(m_ctx, 0));
    JS_SetPropertyStr(m_ctx, global, "duration", JS_NewFloat64(m_ctx, 1.0));
    JS_SetPropertyStr(m_ctx, global, "fps", JS_NewFloat64(m_ctx, 25.0));
    JS_SetPropertyStr(m_ctx, global, "value", JS_NewFloat64(m_ctx, 0.0));
    JS_SetPropertyStr(m_ctx, global, "index", JS_NewInt32(m_ctx, 0));

    // Initialize thisClip and thisTrack objects
    // Kdenlive-native properties:
    JSValue thisClip = JS_NewObject(m_ctx);
    JS_SetPropertyStr(m_ctx, thisClip, "position", JS_NewInt32(m_ctx, 0));
    JS_SetPropertyStr(m_ctx, thisClip, "duration", JS_NewInt32(m_ctx, 0));
    JS_SetPropertyStr(m_ctx, thisClip, "name", JS_NewString(m_ctx, ""));
    JS_SetPropertyStr(m_ctx, thisClip, "width", JS_NewInt32(m_ctx, 0));
    JS_SetPropertyStr(m_ctx, thisClip, "height", JS_NewInt32(m_ctx, 0));
    // AE Layer-compatible properties (thisLayer → thisClip mapping):
    JS_SetPropertyStr(m_ctx, thisClip, "index", JS_NewInt32(m_ctx, 0));
    JS_SetPropertyStr(m_ctx, thisClip, "inPoint", JS_NewFloat64(m_ctx, 0.0));
    JS_SetPropertyStr(m_ctx, thisClip, "outPoint", JS_NewFloat64(m_ctx, 0.0));
    JS_SetPropertyStr(m_ctx, thisClip, "startTime", JS_NewFloat64(m_ctx, 0.0));
    JS_SetPropertyStr(m_ctx, thisClip, "hasVideo", JS_TRUE);
    JS_SetPropertyStr(m_ctx, thisClip, "hasAudio", JS_FALSE);
    // source sub-object (AE Layer.source — footage item metadata)
    JSValue thisClipSource = JS_NewObject(m_ctx);
    JS_SetPropertyStr(m_ctx, thisClipSource, "name", JS_NewString(m_ctx, ""));
    JS_SetPropertyStr(m_ctx, thisClipSource, "width", JS_NewInt32(m_ctx, 0));
    JS_SetPropertyStr(m_ctx, thisClipSource, "height", JS_NewInt32(m_ctx, 0));
    JS_SetPropertyStr(m_ctx, thisClip, "source", thisClipSource);
    JS_SetPropertyStr(m_ctx, global, "thisClip", thisClip);
    // AE alias: thisLayer → thisClip (same object reference)
    JSValue thisClipRef = JS_GetPropertyStr(m_ctx, global, "thisClip");
    JS_SetPropertyStr(m_ctx, global, "thisLayer", thisClipRef);

    JSValue thisTrack = JS_NewObject(m_ctx);
    JS_SetPropertyStr(m_ctx, thisTrack, "index", JS_NewInt32(m_ctx, 0));
    JS_SetPropertyStr(m_ctx, global, "thisTrack", thisTrack);

    // thisEffect is initialized here as a placeholder; param() is registered
    // in registerClipReferenceFunctions via JS_NewCFunction
    // (no properties needed — only .param() method)

    // Initialize thisProject object (AE thisComp equivalent)
    JSValue thisProject = JS_NewObject(m_ctx);
    JS_SetPropertyStr(m_ctx, thisProject, "width", JS_NewInt32(m_ctx, 0));
    JS_SetPropertyStr(m_ctx, thisProject, "height", JS_NewInt32(m_ctx, 0));
    JS_SetPropertyStr(m_ctx, thisProject, "fps", JS_NewFloat64(m_ctx, 25.0));
    JS_SetPropertyStr(m_ctx, thisProject, "duration", JS_NewFloat64(m_ctx, 0.0));
    JS_SetPropertyStr(m_ctx, thisProject, "frameDuration", JS_NewFloat64(m_ctx, 1.0 / 25.0));
    JS_SetPropertyStr(m_ctx, thisProject, "pixelAspect", JS_NewFloat64(m_ctx, 1.0));
    JS_SetPropertyStr(m_ctx, thisProject, "name", JS_NewString(m_ctx, ""));
    JS_SetPropertyStr(m_ctx, thisProject, "fullPath", JS_NewString(m_ctx, ""));
    JS_SetPropertyStr(m_ctx, thisProject, "numTracks", JS_NewInt32(m_ctx, 0));
    JS_SetPropertyStr(m_ctx, thisProject, "displayStartTime", JS_NewFloat64(m_ctx, 0.0));
    // bgColor: [r, g, b, a] normalized 0-1, default black
    JSValue bgColor = JS_NewArray(m_ctx);
    JS_SetPropertyUint32(m_ctx, bgColor, 0, JS_NewFloat64(m_ctx, 0.0));
    JS_SetPropertyUint32(m_ctx, bgColor, 1, JS_NewFloat64(m_ctx, 0.0));
    JS_SetPropertyUint32(m_ctx, bgColor, 2, JS_NewFloat64(m_ctx, 0.0));
    JS_SetPropertyUint32(m_ctx, bgColor, 3, JS_NewFloat64(m_ctx, 1.0));
    JS_SetPropertyStr(m_ctx, thisProject, "bgColor", bgColor);
    JS_SetPropertyStr(m_ctx, global, "thisProject", thisProject);
    // AE alias: thisComp → thisProject (same object reference)
    JSValue thisProjectRef = JS_GetPropertyStr(m_ctx, global, "thisProject");
    JS_SetPropertyStr(m_ctx, global, "thisComp", thisProjectRef);

    JS_FreeValue(m_ctx, global);
}

void ExpressionEngine::setContext(double time, int frame, double duration, double fps, double value, int index)
{
    // Cache in C++ for hot-path functions (buildRandomSeed, audio, keyframes)
    m_cachedTime = time;
    m_cachedFrame = frame;
    m_cachedFps = fps;
    m_cachedValue = value;
    m_cachedIndex = index;

    JSValue global = JS_GetGlobalObject(m_ctx);
    JS_SetPropertyStr(m_ctx, global, "time", JS_NewFloat64(m_ctx, time));
    JS_SetPropertyStr(m_ctx, global, "frame", JS_NewInt32(m_ctx, frame));
    JS_SetPropertyStr(m_ctx, global, "duration", JS_NewFloat64(m_ctx, duration));
    JS_SetPropertyStr(m_ctx, global, "fps", JS_NewFloat64(m_ctx, fps));
    JS_SetPropertyStr(m_ctx, global, "value", JS_NewFloat64(m_ctx, value));
    JS_SetPropertyStr(m_ctx, global, "index", JS_NewInt32(m_ctx, index));

    // Keep thisProperty.value, velocity, speed in sync
    JSValue thisProp = JS_GetPropertyStr(m_ctx, global, "thisProperty");
    if (!JS_IsUndefined(thisProp)) {
        JS_SetPropertyStr(m_ctx, thisProp, "value", JS_NewFloat64(m_ctx, value));

        // Compute velocity/speed from C++ keyframe cache (O(log n) binary search, no JS overhead)
        double velocity = velocityCached(time);
        JS_SetPropertyStr(m_ctx, thisProp, "velocity", JS_NewFloat64(m_ctx, velocity));
        JS_SetPropertyStr(m_ctx, thisProp, "speed", JS_NewFloat64(m_ctx, std::fabs(velocity)));
    }
    JS_FreeValue(m_ctx, thisProp);

    JS_FreeValue(m_ctx, global);
}

void ExpressionEngine::setAudioCache(const QVector<float> &peakBoth, const QVector<float> &peakLeft, const QVector<float> &peakRight, int totalFrames,
                                     double fps)
{
    m_audioPeakBoth = peakBoth;
    m_audioPeakLeft = peakLeft;
    m_audioPeakRight = peakRight;
    m_audioTotalFrames = totalFrames;
    m_audioFps = fps;

    // Store as JS object on globalThis._audio
    JSValue global = JS_GetGlobalObject(m_ctx);
    JSValue audioObj = JS_NewObject(m_ctx);

    // Create JS arrays for each channel
    auto makeArray = [&](const QVector<float> &data) -> JSValue {
        JSValue arr = JS_NewArray(m_ctx);
        for (int i = 0; i < data.size(); i++) {
            JS_SetPropertyUint32(m_ctx, arr, static_cast<uint32_t>(i), JS_NewFloat64(m_ctx, static_cast<double>(data[i])));
        }
        return arr;
    };

    JS_SetPropertyStr(m_ctx, audioObj, "peakBoth", makeArray(peakBoth));
    JS_SetPropertyStr(m_ctx, audioObj, "peakLeft", makeArray(peakLeft));
    JS_SetPropertyStr(m_ctx, audioObj, "peakRight", makeArray(peakRight));
    JS_SetPropertyStr(m_ctx, audioObj, "totalFrames", JS_NewInt32(m_ctx, totalFrames));
    JS_SetPropertyStr(m_ctx, audioObj, "fps", JS_NewFloat64(m_ctx, fps));

    JS_SetPropertyStr(m_ctx, global, "_audio", audioObj);
    JS_FreeValue(m_ctx, global);
}

void ExpressionEngine::clearAudioCache()
{
    m_audioPeakBoth.clear();
    m_audioPeakLeft.clear();
    m_audioPeakRight.clear();
    m_audioTotalFrames = 0;
    m_audioFps = 25.0; // Issue #10: reset to default, not stale value

    JSValue global = JS_GetGlobalObject(m_ctx);
    JS_SetPropertyStr(m_ctx, global, "_audio", JS_UNDEFINED);
    JS_FreeValue(m_ctx, global);
}

float ExpressionEngine::audioPeak(const char *channel, int frame) const
{
    if (m_audioTotalFrames <= 0 || frame < 0) return 0.0f;
    int idx = std::min(frame, m_audioTotalFrames - 1);
    if (std::strcmp(channel, "Left") == 0) {
        return (idx < m_audioPeakLeft.size()) ? m_audioPeakLeft[idx] : 0.0f;
    } else if (std::strcmp(channel, "Right") == 0) {
        return (idx < m_audioPeakRight.size()) ? m_audioPeakRight[idx] : 0.0f;
    }
    return (idx < m_audioPeakBoth.size()) ? m_audioPeakBoth[idx] : 0.0f;
}

QPair<double, QString> ExpressionEngine::evaluate(const QString &expression)
{
    QByteArray utf8 = expression.toUtf8();
    JSValue result = JS_Eval(m_ctx, utf8.constData(), utf8.size(), "<expression>", JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(m_ctx);
        const char *str = JS_ToCString(m_ctx, exc);
        QString errMsg = str ? QString::fromUtf8(str) : QStringLiteral("Unknown error");
        JS_FreeCString(m_ctx, str);
        JS_FreeValue(m_ctx, exc);
        JS_FreeValue(m_ctx, result);
        return {0.0, errMsg};
    }

    double value = 0.0;
    if (JS_ToFloat64(m_ctx, &value, result) != 0) {
        JS_FreeValue(m_ctx, result);
        return {0.0, QStringLiteral("Expression did not return a number")};
    }

    JS_FreeValue(m_ctx, result);

    // Handle NaN/Inf
    if (std::isnan(value) || std::isinf(value)) {
        return {0.0, QStringLiteral("Expression returned NaN or Infinity")};
    }

    return {value, QString()};
}

// ── Compiled expression support (compile once, call per-frame) ────────

// Forward declaration — defined later in this file, used by evaluatePathCompiled
static QVector<ExprPathData::Point> readPointArray(JSContext *ctx, JSValueConst arr, int expectedLen);

// Helper: heap-allocate a JSValue so it can be stored in ExprCompiledHandle.opaque
static ExprCompiledHandle wrapJSValue(JSValue val)
{
    auto *heap = new JSValue(val);
    return ExprCompiledHandle{heap};
}

static JSValue unwrapJSValue(ExprCompiledHandle handle)
{
    if (!handle.opaque) return JS_UNDEFINED;
    return *static_cast<JSValue *>(handle.opaque);
}

ExprCompiledHandle ExpressionEngine::compile(const QString &expression, QString &errorMsg)
{
    errorMsg.clear();

    // Strategy: try single-expression arrow form first (fastest).
    // If it fails (multi-statement with semicolons/var/let/const),
    // fall back to function body form with explicit return on last line.
    QString wrapped = QStringLiteral("() => (\n%1\n)").arg(expression);
    QByteArray utf8 = wrapped.toUtf8();

    JSValue fn = JS_Eval(m_ctx, utf8.constData(), utf8.size(), "<expression>", JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(fn)) {
        // Clear the exception from the arrow form attempt
        JSValue exc = JS_GetException(m_ctx);
        JS_FreeValue(m_ctx, exc);
        JS_FreeValue(m_ctx, fn);

        // Fallback: wrap in function body.
        QString body = expression.trimmed();
        // Remove trailing semicolons for cleaner splitting
        while (body.endsWith(QLatin1Char(';')))
            body.chop(1);

        if (body.endsWith(QLatin1Char('}'))) {
            // Block-structured expression (if/else, for, etc.) — the old
            // "insert return before last line" heuristic breaks implicit
            // returns inside branches. Use eval() to get correct JS
            // completion-value semantics (eval returns the last expression
            // value, even from inside if/else branches).
            QString escaped = expression;
            escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
            escaped.replace(QLatin1Char('"'), QLatin1String("\\\""));
            escaped.replace(QLatin1Char('\n'), QLatin1String("\\n"));
            escaped.replace(QLatin1Char('\r'), QLatin1String("\\r"));
            escaped.replace(QLatin1Char('\t'), QLatin1String("\\t"));
            wrapped = QStringLiteral("(function(){ return eval(\"%1\"); })").arg(escaped);
        } else {
            // Simple multi-line (var declarations + final expression).
            // Insert `return` before the last expression-statement.
            int lastSep = std::max(body.lastIndexOf(QLatin1Char(';')), body.lastIndexOf(QLatin1Char('\n')));
            if (lastSep >= 0) {
                body.insert(lastSep + 1, QStringLiteral(" return "));
            } else {
                body.prepend(QStringLiteral("return "));
            }
            wrapped = QStringLiteral("(function(){\n%1\n})").arg(body);
        }
        utf8 = wrapped.toUtf8();

        fn = JS_Eval(m_ctx, utf8.constData(), utf8.size(), "<expression>", JS_EVAL_TYPE_GLOBAL);

        if (JS_IsException(fn)) {
            exc = JS_GetException(m_ctx);
            const char *str = JS_ToCString(m_ctx, exc);
            errorMsg = str ? QString::fromUtf8(str) : QStringLiteral("Compilation error");
            JS_FreeCString(m_ctx, str);
            JS_FreeValue(m_ctx, exc);
            JS_FreeValue(m_ctx, fn);
            return ExprCompiledHandle{};
        }
    }

    if (!JS_IsFunction(m_ctx, fn)) {
        JS_FreeValue(m_ctx, fn);
        errorMsg = QStringLiteral("Expression did not compile to a function");
        return ExprCompiledHandle{};
    }

    return wrapJSValue(fn);
}

QPair<double, QString> ExpressionEngine::evaluateCompiled(ExprCompiledHandle compiled)
{
    JSValue fn = unwrapJSValue(compiled);
    JSValue global = JS_GetGlobalObject(m_ctx);
    JSValue result = JS_Call(m_ctx, fn, global, 0, nullptr);
    JS_FreeValue(m_ctx, global);

    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(m_ctx);
        const char *str = JS_ToCString(m_ctx, exc);
        QString errMsg = str ? QString::fromUtf8(str) : QStringLiteral("Unknown error");
        JS_FreeCString(m_ctx, str);
        JS_FreeValue(m_ctx, exc);
        JS_FreeValue(m_ctx, result);
        return {0.0, errMsg};
    }

    double value = 0.0;
    if (JS_ToFloat64(m_ctx, &value, result) != 0) {
        JS_FreeValue(m_ctx, result);
        return {0.0, QStringLiteral("Expression did not return a number")};
    }

    JS_FreeValue(m_ctx, result);

    if (std::isnan(value) || std::isinf(value)) {
        return {0.0, QStringLiteral("Expression returned NaN or Infinity")};
    }

    return {value, QString()};
}

QPair<ExprPathData, QString> ExpressionEngine::evaluatePathCompiled(ExprCompiledHandle compiled)
{
    JSValue fn = unwrapJSValue(compiled);
    JSValue global = JS_GetGlobalObject(m_ctx);
    JSValue result = JS_Call(m_ctx, fn, global, 0, nullptr);
    JS_FreeValue(m_ctx, global);

    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(m_ctx);
        const char *str = JS_ToCString(m_ctx, exc);
        QString errMsg = str ? QString::fromUtf8(str) : QStringLiteral("Unknown error");
        JS_FreeCString(m_ctx, str);
        JS_FreeValue(m_ctx, exc);
        JS_FreeValue(m_ctx, result);
        return {ExprPathData(), errMsg};
    }

    // Check for _isPath sentinel
    JSValue isPath = JS_GetPropertyStr(m_ctx, result, "_isPath");
    if (JS_IsUndefined(isPath) || !JS_ToBool(m_ctx, isPath)) {
        JS_FreeValue(m_ctx, isPath);
        JS_FreeValue(m_ctx, result);
        return {ExprPathData(), QStringLiteral("Expression did not return a createPath() object")};
    }
    JS_FreeValue(m_ctx, isPath);

    // Extract points (reuse readPointArray helper)
    JSValue pointsArr = JS_GetPropertyStr(m_ctx, result, "points");
    JSValue inTanArr = JS_GetPropertyStr(m_ctx, result, "inTangents");
    JSValue outTanArr = JS_GetPropertyStr(m_ctx, result, "outTangents");
    JSValue isClosedVal = JS_GetPropertyStr(m_ctx, result, "isClosed");

    ExprPathData pathData;
    JSValue lenVal = JS_GetPropertyStr(m_ctx, pointsArr, "length");
    int32_t numPoints = 0;
    JS_ToInt32(m_ctx, &numPoints, lenVal);
    JS_FreeValue(m_ctx, lenVal);

    pathData.points = readPointArray(m_ctx, pointsArr, numPoints);
    pathData.inTangents = readPointArray(m_ctx, inTanArr, numPoints);
    pathData.outTangents = readPointArray(m_ctx, outTanArr, numPoints);
    pathData.isClosed = JS_ToBool(m_ctx, isClosedVal);

    while (pathData.inTangents.size() < pathData.points.size())
        pathData.inTangents.append({0.0, 0.0});
    while (pathData.outTangents.size() < pathData.points.size())
        pathData.outTangents.append({0.0, 0.0});

    JS_FreeValue(m_ctx, isClosedVal);
    JS_FreeValue(m_ctx, outTanArr);
    JS_FreeValue(m_ctx, inTanArr);
    JS_FreeValue(m_ctx, pointsArr);
    JS_FreeValue(m_ctx, result);

    if (pathData.points.isEmpty()) {
        return {ExprPathData(), QStringLiteral("createPath() returned empty points array")};
    }

    return {pathData, QString()};
}

void ExpressionEngine::freeCompiled(ExprCompiledHandle compiled)
{
    if (compiled.opaque) {
        JSValue fn = *static_cast<JSValue *>(compiled.opaque);
        if (!JS_IsUndefined(fn)) {
            JS_FreeValue(m_ctx, fn);
        }
        delete static_cast<JSValue *>(compiled.opaque);
    }
}

QString ExpressionEngine::bakeToAnimString(const QString &expression, int startFrame, int endFrame, double fps, double clipDuration, double baseValue,
                                           int clipIndex)
{
    if (expression.isEmpty() || startFrame > endFrame || fps <= 0) {
        return {};
    }

    // Reset seedRandom state to avoid leaks between independent bake calls
    m_hasUserSeed = false;
    m_timelessSeed = false;
    m_userSeed = 0;

    // Compile expression once — avoid re-parsing on every frame (Issue 6: 10-50x speedup)
    QString compileError;
    ExprCompiledHandle compiled = compile(expression, compileError);
    if (!compileError.isEmpty()) {
        qWarning() << "Expression compile error:" << compileError;
        return {};
    }

    const int numFrames = endFrame - startFrame + 1;

    // Pre-allocate output buffer (Issue 5): "frame|=value;" ≈ 16 chars/frame
    QByteArray output;
    output.reserve(numFrames * 18);
    char buf[64];

    // posterizeTime support (Issue 4): detect on first frame, then skip
    // re-evaluation when quantized time hasn't changed
    bool usesPosterize = expression.contains(QLatin1String("posterizeTime"));
    double posterizeFps = -1.0; // -1 = not yet detected
    double lastPosterizedValue = baseValue;
    double lastQuantizedTime = -1e30;

    for (int f = startFrame; f <= endFrame; f++) {
        double time = static_cast<double>(f - startFrame) / fps;
        setContext(time, f - startFrame, clipDuration, fps, baseValue, clipIndex);

        double value;

        if (usesPosterize && posterizeFps > 0.0) {
            // posterizeFps already known — skip evaluation if same quantized step
            double quantizedTime = std::floor(time * posterizeFps) / posterizeFps;
            if (std::fabs(quantizedTime - lastQuantizedTime) < 1e-12) {
                // Same posterized step — reuse last value (no JS_Call needed)
                value = lastPosterizedValue;
            } else {
                // New step — evaluate at quantized time and quantized frame
                int quantizedFrame = static_cast<int>(std::floor(quantizedTime * fps));
                setContext(quantizedTime, quantizedFrame, clipDuration, fps, baseValue, clipIndex);
                auto [pValue, pError] = evaluateCompiled(compiled);
                if (!pError.isEmpty()) {
                    qWarning() << "Expression bake error at frame" << f << ":" << pError;
                    freeCompiled(compiled);
                    return {};
                }
                lastPosterizedValue = pValue;
                lastQuantizedTime = quantizedTime;
                value = pValue;
            }
        } else {
            // Normal evaluation (compiled — no re-parse per frame)
            auto [evalValue, error] = evaluateCompiled(compiled);
            if (!error.isEmpty()) {
                qWarning() << "Expression bake error at frame" << f << ":" << error;
                freeCompiled(compiled);
                return {};
            }
            value = evalValue;

            // On first frame with posterizeTime: detect _posterizeFps side-effect
            if (usesPosterize && posterizeFps < 0.0) {
                JSValue global = JS_GetGlobalObject(m_ctx);
                JSValue pfVal = JS_GetPropertyStr(m_ctx, global, "_posterizeFps");
                if (!JS_IsUndefined(pfVal)) {
                    JS_ToFloat64(m_ctx, &posterizeFps, pfVal);
                    if (posterizeFps > 0.0) {
                        lastPosterizedValue = value;
                        lastQuantizedTime = std::floor(time * posterizeFps) / posterizeFps;
                    }
                }
                JS_FreeValue(m_ctx, pfVal);
                JS_FreeValue(m_ctx, global);
            }
        }

        // Direct formatting into pre-allocated buffer (Issue 5: no per-frame QString)
        // Use QByteArray::number() for the value to guarantee '.' decimal separator
        // regardless of C locale (snprintf %.6f is locale-dependent)
        if (f > startFrame) output.append(';');
        int n = std::snprintf(buf, sizeof(buf), "%d|=", f);
        output.append(buf, n);
        output.append(QByteArray::number(value, 'f', 6));
    }

    freeCompiled(compiled);
    return QString::fromLatin1(output);
}

QString ExpressionEngine::validate(const QString &expression)
{
    if (expression.trimmed().isEmpty()) {
        return QStringLiteral("Expression is empty");
    }

    // Save current context globals before overwriting with dummy values
    JSValue global = JS_GetGlobalObject(m_ctx);
    JSValue savedTime = JS_GetPropertyStr(m_ctx, global, "time");
    JSValue savedFrame = JS_GetPropertyStr(m_ctx, global, "frame");
    JSValue savedDuration = JS_GetPropertyStr(m_ctx, global, "duration");
    JSValue savedFps = JS_GetPropertyStr(m_ctx, global, "fps");
    JSValue savedValue = JS_GetPropertyStr(m_ctx, global, "value");
    JSValue savedIndex = JS_GetPropertyStr(m_ctx, global, "index");
    // Save side-effect globals that posterizeTime()/seedRandom() may set
    JSValue savedPosterizeFps = JS_GetPropertyStr(m_ctx, global, "_posterizeFps");
    JSValue savedUserSeed = JS_GetPropertyStr(m_ctx, global, "_userSeed");
    JSValue savedTimeless = JS_GetPropertyStr(m_ctx, global, "_timeless");
    // Save thisProperty.velocity and thisProperty.speed (set by setContext via velocityCached)
    JSValue thisProp = JS_GetPropertyStr(m_ctx, global, "thisProperty");
    JSValue savedVelocity = JS_UNDEFINED;
    JSValue savedSpeed = JS_UNDEFINED;
    if (!JS_IsUndefined(thisProp)) {
        savedVelocity = JS_GetPropertyStr(m_ctx, thisProp, "velocity");
        savedSpeed = JS_GetPropertyStr(m_ctx, thisProp, "speed");
    }
    JS_FreeValue(m_ctx, thisProp);
    JS_FreeValue(m_ctx, global);

    // Save C++ mirrored seed state (seedRandom() writes both JS and C++)
    uint32_t savedCppUserSeed = m_userSeed;
    bool savedCppHasUserSeed = m_hasUserSeed;
    bool savedCppTimelessSeed = m_timelessSeed;
    // Save C++ cached context
    int savedCppFrame = m_cachedFrame;
    int savedCppIndex = m_cachedIndex;
    double savedCppTime = m_cachedTime;
    double savedCppFps = m_cachedFps;
    double savedCppValue = m_cachedValue;

    // Evaluate with dummy context to check syntax
    setContext(0.0, 0, 1.0, 25.0, 0.0, 0);
    auto [value, error] = evaluate(expression);
    Q_UNUSED(value)

    // Path expressions (createPath) return an object, not a number.
    // If evaluate() failed for that reason, try evaluatePath() instead.
    if (!error.isEmpty() && usesPath(expression)) {
        auto [pathData, pathError] = evaluatePath(expression);
        error = pathError;
    }

    // Restore previous context
    global = JS_GetGlobalObject(m_ctx);
    JS_SetPropertyStr(m_ctx, global, "time", savedTime);
    JS_SetPropertyStr(m_ctx, global, "frame", savedFrame);
    JS_SetPropertyStr(m_ctx, global, "duration", savedDuration);
    JS_SetPropertyStr(m_ctx, global, "fps", savedFps);
    JS_SetPropertyStr(m_ctx, global, "value", savedValue);
    JS_SetPropertyStr(m_ctx, global, "index", savedIndex);
    // Restore side-effect globals
    JS_SetPropertyStr(m_ctx, global, "_posterizeFps", savedPosterizeFps);
    JS_SetPropertyStr(m_ctx, global, "_userSeed", savedUserSeed);
    JS_SetPropertyStr(m_ctx, global, "_timeless", savedTimeless);
    // Restore thisProperty.velocity and thisProperty.speed
    thisProp = JS_GetPropertyStr(m_ctx, global, "thisProperty");
    if (!JS_IsUndefined(thisProp)) {
        JS_SetPropertyStr(m_ctx, thisProp, "velocity", savedVelocity);
        JS_SetPropertyStr(m_ctx, thisProp, "speed", savedSpeed);
    } else {
        JS_FreeValue(m_ctx, savedVelocity);
        JS_FreeValue(m_ctx, savedSpeed);
    }
    JS_FreeValue(m_ctx, thisProp);
    JS_FreeValue(m_ctx, global);

    // Restore C++ mirrored seed state
    m_userSeed = savedCppUserSeed;
    m_hasUserSeed = savedCppHasUserSeed;
    m_timelessSeed = savedCppTimelessSeed;
    // Restore C++ cached context
    m_cachedFrame = savedCppFrame;
    m_cachedIndex = savedCppIndex;
    m_cachedTime = savedCppTime;
    m_cachedFps = savedCppFps;
    m_cachedValue = savedCppValue;

    return error;
}

bool ExpressionEngine::usesAudio(const QString &expression)
{
    return expression.contains(QLatin1String("audioLevel")) || expression.contains(QLatin1String("audioRms"));
}

void ExpressionEngine::setKeyframes(const QVector<QPair<int, double>> &keyframes, double fps)
{
    if (fps <= 0.0) fps = 25.0; // Guard against division by zero
    JSValue global = JS_GetGlobalObject(m_ctx);
    JSValue arr = JS_NewArray(m_ctx);
    for (int i = 0; i < keyframes.size(); i++) {
        JSValue obj = JS_NewObject(m_ctx);
        double timeSec = static_cast<double>(keyframes[i].first) / fps;
        JS_SetPropertyStr(m_ctx, obj, "t", JS_NewFloat64(m_ctx, timeSec));
        JS_SetPropertyStr(m_ctx, obj, "v", JS_NewFloat64(m_ctx, keyframes[i].second));
        JS_SetPropertyUint32(m_ctx, arr, static_cast<uint32_t>(i), obj);
    }
    JS_SetPropertyStr(m_ctx, global, "_keyframes", arr);
    int nKeys = keyframes.size();
    JS_SetPropertyStr(m_ctx, global, "numKeys", JS_NewInt32(m_ctx, nKeys));
    // Keep thisProperty.numKeys in sync
    JSValue thisProp = JS_GetPropertyStr(m_ctx, global, "thisProperty");
    if (!JS_IsUndefined(thisProp)) {
        JS_SetPropertyStr(m_ctx, thisProp, "numKeys", JS_NewInt32(m_ctx, nKeys));
    }
    JS_FreeValue(m_ctx, thisProp);
    JS_FreeValue(m_ctx, global);

    // Populate C++ keyframe cache (mirror of JS _keyframes, avoids JS overhead in setContext)
    m_keyframeCache.resize(keyframes.size());
    m_keyframeFps = fps;
    for (int i = 0; i < keyframes.size(); i++) {
        double timeSec = static_cast<double>(keyframes[i].first) / fps;
        m_keyframeCache[i] = {timeSec, keyframes[i].second};
    }
}

void ExpressionEngine::clearKeyframes()
{
    JSValue global = JS_GetGlobalObject(m_ctx);
    JS_SetPropertyStr(m_ctx, global, "_keyframes", JS_UNDEFINED);
    JS_SetPropertyStr(m_ctx, global, "numKeys", JS_NewInt32(m_ctx, 0));
    // Keep thisProperty in sync: numKeys, velocity, speed (Issue #6)
    JSValue thisProp = JS_GetPropertyStr(m_ctx, global, "thisProperty");
    if (!JS_IsUndefined(thisProp)) {
        JS_SetPropertyStr(m_ctx, thisProp, "numKeys", JS_NewInt32(m_ctx, 0));
        JS_SetPropertyStr(m_ctx, thisProp, "velocity", JS_NewFloat64(m_ctx, 0.0));
        JS_SetPropertyStr(m_ctx, thisProp, "speed", JS_NewFloat64(m_ctx, 0.0));
    }
    JS_FreeValue(m_ctx, thisProp);
    JS_FreeValue(m_ctx, global);

    m_keyframeCache.clear();
}

double ExpressionEngine::interpCached(double t) const
{
    if (m_keyframeCache.isEmpty()) return 0.0;
    if (m_keyframeCache.size() == 1) return m_keyframeCache[0].v;

    if (t <= m_keyframeCache.first().t) return m_keyframeCache.first().v;
    if (t >= m_keyframeCache.last().t) return m_keyframeCache.last().v;

    // Binary search for surrounding keyframes
    int lo = 0, hi = m_keyframeCache.size() - 1;
    while (lo < hi - 1) {
        int mid = (lo + hi) / 2;
        if (m_keyframeCache[mid].t <= t) {
            lo = mid;
        } else {
            hi = mid;
        }
    }

    double ta = m_keyframeCache[lo].t;
    double tb = m_keyframeCache[hi].t;
    double va = m_keyframeCache[lo].v;
    double vb = m_keyframeCache[hi].v;
    double ratio = (tb > ta) ? (t - ta) / (tb - ta) : 0.0;
    return va + (vb - va) * ratio;
}

double ExpressionEngine::velocityCached(double t) const
{
    if (m_keyframeCache.size() < 2 || m_keyframeFps <= 0.0) return 0.0;
    double dt = 0.5 / m_keyframeFps;
    return (interpCached(t + dt) - interpCached(t - dt)) / (2.0 * dt);
}

bool ExpressionEngine::usesKeyframes(const QString &expression)
{
    // Note: substring matching can produce false positives (e.g. variable named "numKeys")
    // and false negatives (e.g. "key (1)" with space). We match common patterns including
    // space-before-paren variants to minimize false negatives in real-world expressions.
    return expression.contains(QLatin1String("loopIn")) || expression.contains(QLatin1String("loopOut")) || expression.contains(QLatin1String("numKeys")) ||
           expression.contains(QLatin1String("key(")) || expression.contains(QLatin1String("key (")) || expression.contains(QLatin1String(".key")) ||
           expression.contains(QLatin1String("nearestKey")) || expression.contains(QLatin1String("valueAtTime")) ||
           expression.contains(QLatin1String("velocityAtTime")) || expression.contains(QLatin1String("speedAtTime")) ||
           expression.contains(QLatin1String("smooth(")) || expression.contains(QLatin1String("smooth (")) ||
           expression.contains(QLatin1String("thisProperty"));
}

void ExpressionEngine::setMarkers(const QVector<ExprMarkerData> &markers)
{
    JSValue global = JS_GetGlobalObject(m_ctx);

    // Store raw marker data as _markers array
    JSValue arr = JS_NewArray(m_ctx);
    for (int i = 0; i < markers.size(); i++) {
        JSValue obj = JS_NewObject(m_ctx);
        JS_SetPropertyStr(m_ctx, obj, "t", JS_NewFloat64(m_ctx, markers[i].time));
        QByteArray commentUtf8 = markers[i].comment.toUtf8();
        JS_SetPropertyStr(m_ctx, obj, "comment", JS_NewString(m_ctx, commentUtf8.constData()));
        JS_SetPropertyStr(m_ctx, obj, "duration", JS_NewFloat64(m_ctx, markers[i].duration));
        JS_SetPropertyUint32(m_ctx, arr, static_cast<uint32_t>(i), obj);
    }
    JS_SetPropertyStr(m_ctx, global, "_markers", arr);

    // Update marker.numKeys on the existing marker object
    JSValue markerObj = JS_GetPropertyStr(m_ctx, global, "marker");
    if (!JS_IsUndefined(markerObj) && !JS_IsNull(markerObj)) {
        JS_SetPropertyStr(m_ctx, markerObj, "numKeys", JS_NewInt32(m_ctx, markers.size()));
    }
    JS_FreeValue(m_ctx, markerObj);

    JS_FreeValue(m_ctx, global);
}

void ExpressionEngine::clearMarkers()
{
    JSValue global = JS_GetGlobalObject(m_ctx);
    JS_SetPropertyStr(m_ctx, global, "_markers", JS_UNDEFINED);

    // Reset marker.numKeys to 0
    JSValue markerObj = JS_GetPropertyStr(m_ctx, global, "marker");
    if (!JS_IsUndefined(markerObj) && !JS_IsNull(markerObj)) {
        JS_SetPropertyStr(m_ctx, markerObj, "numKeys", JS_NewInt32(m_ctx, 0));
    }
    JS_FreeValue(m_ctx, markerObj);

    JS_FreeValue(m_ctx, global);
}

bool ExpressionEngine::usesMarkers(const QString &expression)
{
    return expression.contains(QLatin1String("marker."));
}

void ExpressionEngine::setClipContext(int clipPosition, int clipDurationFrames, const QString &clipName, int trackIndex)
{
    JSValue global = JS_GetGlobalObject(m_ctx);

    // Update thisClip in-place (preserves width/height set by setImageContext)
    JSValue thisClip = JS_GetPropertyStr(m_ctx, global, "thisClip");
    // Kdenlive-native properties
    JS_SetPropertyStr(m_ctx, thisClip, "position", JS_NewInt32(m_ctx, clipPosition));
    JS_SetPropertyStr(m_ctx, thisClip, "duration", JS_NewInt32(m_ctx, clipDurationFrames));
    QByteArray nameUtf8 = clipName.toUtf8();
    JS_SetPropertyStr(m_ctx, thisClip, "name", JS_NewString(m_ctx, nameUtf8.constData()));

    // AE Layer-compatible properties
    // Use C++ cached values instead of reading JS globals (avoids ordering dependency
    // on setContext() — m_cachedFps defaults to 25.0 and m_cachedIndex to 0 if
    // setContext() hasn't been called yet, which matches the JS global defaults)
    double currentFps = m_cachedFps;
    if (currentFps <= 0.0) currentFps = 25.0;

    JS_SetPropertyStr(m_ctx, thisClip, "index", JS_NewInt32(m_ctx, m_cachedIndex));

    JS_SetPropertyStr(m_ctx, thisClip, "inPoint", JS_NewFloat64(m_ctx, 0.0));
    double outPointSec = static_cast<double>(clipDurationFrames) / currentFps;
    JS_SetPropertyStr(m_ctx, thisClip, "outPoint", JS_NewFloat64(m_ctx, outPointSec));
    double startTimeSec = static_cast<double>(clipPosition) / currentFps;
    JS_SetPropertyStr(m_ctx, thisClip, "startTime", JS_NewFloat64(m_ctx, startTimeSec));

    // Update source sub-object
    JSValue source = JS_GetPropertyStr(m_ctx, thisClip, "source");
    if (!JS_IsUndefined(source)) {
        JS_SetPropertyStr(m_ctx, source, "name", JS_NewString(m_ctx, nameUtf8.constData()));
        JSValue w = JS_GetPropertyStr(m_ctx, thisClip, "width");
        JSValue h = JS_GetPropertyStr(m_ctx, thisClip, "height");
        JS_SetPropertyStr(m_ctx, source, "width", JS_DupValue(m_ctx, w));
        JS_SetPropertyStr(m_ctx, source, "height", JS_DupValue(m_ctx, h));
        JS_FreeValue(m_ctx, w);
        JS_FreeValue(m_ctx, h);
    }
    JS_FreeValue(m_ctx, source);
    JS_FreeValue(m_ctx, thisClip);

    // Update thisTrack in-place
    JSValue thisTrack = JS_GetPropertyStr(m_ctx, global, "thisTrack");
    JS_SetPropertyStr(m_ctx, thisTrack, "index", JS_NewInt32(m_ctx, trackIndex));
    JS_FreeValue(m_ctx, thisTrack);

    JS_FreeValue(m_ctx, global);
}

void ExpressionEngine::setClipResolver(ClipParamResolver resolver)
{
    m_clipResolver = std::move(resolver);
}

void ExpressionEngine::clearClipResolver()
{
    m_clipResolver = nullptr;
}

double ExpressionEngine::resolveClipParam(const QString &clipName, const QString &effectId, const QString &paramName)
{
    if (m_clipResolver) {
        return m_clipResolver(clipName, effectId, paramName);
    }
    return 0.0;
}

void ExpressionEngine::setEffectResolver(EffectParamResolver resolver)
{
    m_effectResolver = std::move(resolver);
}

void ExpressionEngine::clearEffectResolver()
{
    m_effectResolver = nullptr;
}

double ExpressionEngine::resolveEffectParam(const QString &paramName)
{
    if (m_effectResolver) {
        return m_effectResolver(paramName);
    }
    return 0.0;
}

void ExpressionEngine::setImageSampler(ImageSampler sampler)
{
    m_imageSampler = std::move(sampler);
}

void ExpressionEngine::clearImageSampler()
{
    m_imageSampler = nullptr;
}

std::array<double, 4> ExpressionEngine::sampleImage(int frame, double x, double y, double radius)
{
    if (m_imageSampler) {
        return m_imageSampler(frame, x, y, radius);
    }
    return {0.0, 0.0, 0.0, 0.0};
}

bool ExpressionEngine::usesImage(const QString &expression)
{
    return expression.contains(QLatin1String("sampleImage"));
}

void ExpressionEngine::setImageContext(int width, int height)
{
    JSValue global = JS_GetGlobalObject(m_ctx);
    JSValue thisClip = JS_GetPropertyStr(m_ctx, global, "thisClip");
    JS_SetPropertyStr(m_ctx, thisClip, "width", JS_NewInt32(m_ctx, width));
    JS_SetPropertyStr(m_ctx, thisClip, "height", JS_NewInt32(m_ctx, height));
    // Keep source sub-object in sync (may be set before or after setClipContext)
    JSValue source = JS_GetPropertyStr(m_ctx, thisClip, "source");
    if (!JS_IsUndefined(source)) {
        JS_SetPropertyStr(m_ctx, source, "width", JS_NewInt32(m_ctx, width));
        JS_SetPropertyStr(m_ctx, source, "height", JS_NewInt32(m_ctx, height));
    }
    JS_FreeValue(m_ctx, source);
    JS_FreeValue(m_ctx, thisClip);
    JS_FreeValue(m_ctx, global);
}

void ExpressionEngine::clearImageContext()
{
    setImageContext(0, 0);
}

// ── Project context (AE thisComp equivalent) ─────────────────────────

void ExpressionEngine::setProjectContext(int width, int height, double fps, double duration, double pixelAspect, const QString &name, const QString &fullPath,
                                         int numTracks, double displayStartTime)
{
    JSValue global = JS_GetGlobalObject(m_ctx);
    JSValue thisProject = JS_GetPropertyStr(m_ctx, global, "thisProject");

    JS_SetPropertyStr(m_ctx, thisProject, "width", JS_NewInt32(m_ctx, width));
    JS_SetPropertyStr(m_ctx, thisProject, "height", JS_NewInt32(m_ctx, height));
    JS_SetPropertyStr(m_ctx, thisProject, "fps", JS_NewFloat64(m_ctx, fps));
    JS_SetPropertyStr(m_ctx, thisProject, "duration", JS_NewFloat64(m_ctx, duration));
    JS_SetPropertyStr(m_ctx, thisProject, "frameDuration", JS_NewFloat64(m_ctx, (fps > 0.0) ? 1.0 / fps : 0.0));
    JS_SetPropertyStr(m_ctx, thisProject, "pixelAspect", JS_NewFloat64(m_ctx, pixelAspect));

    QByteArray nameUtf8 = name.toUtf8();
    JS_SetPropertyStr(m_ctx, thisProject, "name", JS_NewString(m_ctx, nameUtf8.constData()));
    QByteArray pathUtf8 = fullPath.toUtf8();
    JS_SetPropertyStr(m_ctx, thisProject, "fullPath", JS_NewString(m_ctx, pathUtf8.constData()));

    JS_SetPropertyStr(m_ctx, thisProject, "numTracks", JS_NewInt32(m_ctx, numTracks));
    JS_SetPropertyStr(m_ctx, thisProject, "displayStartTime", JS_NewFloat64(m_ctx, displayStartTime));

    JS_FreeValue(m_ctx, thisProject);
    JS_FreeValue(m_ctx, global);
}

void ExpressionEngine::clearProjectContext()
{
    setProjectContext(0, 0, 25.0, 0.0, 1.0, {}, {}, 0, 0.0);
}

bool ExpressionEngine::usesProject(const QString &expression)
{
    return expression.contains(QLatin1String("thisProject")) || expression.contains(QLatin1String("thisComp"));
}

// ── Clip-by-index resolution ─────────────────────────────────────────

void ExpressionEngine::setClipByIndexResolver(ClipByIndexResolver resolver)
{
    m_clipByIndexResolver = std::move(resolver);
}

void ExpressionEngine::clearClipByIndexResolver()
{
    m_clipByIndexResolver = nullptr;
}

double ExpressionEngine::resolveClipByIndex(int clipIndex, const QString &effectId, const QString &paramName)
{
    if (m_clipByIndexResolver) {
        return m_clipByIndexResolver(clipIndex, effectId, paramName);
    }
    return 0.0;
}

// ── Clip metadata resolution ─────────────────────────────────────────

void ExpressionEngine::setClipMetadataResolver(ClipMetadataResolver resolver)
{
    m_clipMetadataResolver = std::move(resolver);
}

void ExpressionEngine::clearClipMetadataResolver()
{
    m_clipMetadataResolver = nullptr;
}

ExpressionEngine::ClipMetadata ExpressionEngine::resolveClipMetadata(int clipIndex)
{
    if (m_clipMetadataResolver) {
        return m_clipMetadataResolver(clipIndex);
    }
    return {};
}

// ── Path expression support ──────────────────────────────────────────

// Helper: read a [x,y] pair from a JS array element
static bool readPoint(JSContext *ctx, JSValueConst arr, int index, ExprPathData::Point &out)
{
    JSValue elem = JS_GetPropertyUint32(ctx, arr, static_cast<uint32_t>(index));
    if (!JS_IsArray(ctx, elem)) {
        JS_FreeValue(ctx, elem);
        return false;
    }
    JSValue xVal = JS_GetPropertyUint32(ctx, elem, 0);
    JSValue yVal = JS_GetPropertyUint32(ctx, elem, 1);
    JS_ToFloat64(ctx, &out.x, xVal);
    JS_ToFloat64(ctx, &out.y, yVal);
    JS_FreeValue(ctx, xVal);
    JS_FreeValue(ctx, yVal);
    JS_FreeValue(ctx, elem);
    return true;
}

// Helper: read an array of [x,y] pairs from a JS array
static QVector<ExprPathData::Point> readPointArray(JSContext *ctx, JSValueConst arr, int expectedLen)
{
    QVector<ExprPathData::Point> result;
    if (JS_IsUndefined(arr) || JS_IsNull(arr) || !JS_IsArray(ctx, arr)) {
        return result;
    }
    JSValue lenVal = JS_GetPropertyStr(ctx, arr, "length");
    int32_t len = 0;
    JS_ToInt32(ctx, &len, lenVal);
    JS_FreeValue(ctx, lenVal);

    if (len == 0) return result;

    int count = (expectedLen > 0) ? std::min(static_cast<int>(len), expectedLen) : static_cast<int>(len);
    result.resize(count);
    for (int i = 0; i < count; i++) {
        if (!readPoint(ctx, arr, i, result[i])) {
            result[i] = {0.0, 0.0};
        }
    }
    return result;
}

QPair<ExprPathData, QString> ExpressionEngine::evaluatePath(const QString &expression)
{
    QByteArray utf8 = expression.toUtf8();
    JSValue result = JS_Eval(m_ctx, utf8.constData(), utf8.size(), "<expression>", JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(m_ctx);
        const char *str = JS_ToCString(m_ctx, exc);
        QString errMsg = str ? QString::fromUtf8(str) : QStringLiteral("Unknown error");
        JS_FreeCString(m_ctx, str);
        JS_FreeValue(m_ctx, exc);
        JS_FreeValue(m_ctx, result);
        return {ExprPathData(), errMsg};
    }

    // Check for _isPath sentinel
    JSValue isPath = JS_GetPropertyStr(m_ctx, result, "_isPath");
    if (JS_IsUndefined(isPath) || !JS_ToBool(m_ctx, isPath)) {
        JS_FreeValue(m_ctx, isPath);
        JS_FreeValue(m_ctx, result);
        return {ExprPathData(), QStringLiteral("Expression did not return a createPath() object")};
    }
    JS_FreeValue(m_ctx, isPath);

    // Extract points
    JSValue pointsArr = JS_GetPropertyStr(m_ctx, result, "points");
    JSValue inTanArr = JS_GetPropertyStr(m_ctx, result, "inTangents");
    JSValue outTanArr = JS_GetPropertyStr(m_ctx, result, "outTangents");
    JSValue isClosedVal = JS_GetPropertyStr(m_ctx, result, "isClosed");

    ExprPathData pathData;

    // Read points (required)
    JSValue lenVal = JS_GetPropertyStr(m_ctx, pointsArr, "length");
    int32_t numPoints = 0;
    JS_ToInt32(m_ctx, &numPoints, lenVal);
    JS_FreeValue(m_ctx, lenVal);

    pathData.points = readPointArray(m_ctx, pointsArr, numPoints);
    pathData.inTangents = readPointArray(m_ctx, inTanArr, numPoints);
    pathData.outTangents = readPointArray(m_ctx, outTanArr, numPoints);
    pathData.isClosed = JS_ToBool(m_ctx, isClosedVal);

    // Pad tangent arrays with [0,0] if shorter than points
    while (pathData.inTangents.size() < pathData.points.size()) {
        pathData.inTangents.append({0.0, 0.0});
    }
    while (pathData.outTangents.size() < pathData.points.size()) {
        pathData.outTangents.append({0.0, 0.0});
    }

    JS_FreeValue(m_ctx, isClosedVal);
    JS_FreeValue(m_ctx, outTanArr);
    JS_FreeValue(m_ctx, inTanArr);
    JS_FreeValue(m_ctx, pointsArr);
    JS_FreeValue(m_ctx, result);

    if (pathData.points.isEmpty()) {
        return {ExprPathData(), QStringLiteral("createPath() returned empty points array")};
    }

    return {pathData, QString()};
}

QString ExpressionEngine::bakeToPathJson(const QString &expression, int startFrame, int endFrame, double fps, double clipDuration, int clipIndex)
{
    if (expression.isEmpty() || startFrame > endFrame || fps <= 0) {
        return {};
    }

    // Reset seedRandom state to avoid leaks between independent bake calls
    m_hasUserSeed = false;
    m_timelessSeed = false;
    m_userSeed = 0;

    // Zero-padding width matches KeyframeModel::getRotoProperty():
    // int(log10(double(out))) + 1
    int padWidth = (endFrame > 0) ? static_cast<int>(std::log10(static_cast<double>(endFrame))) + 1 : 1;

    // Compile expression once (same as bakeToAnimString)
    QString compileError;
    ExprCompiledHandle compiled = compile(expression, compileError);
    if (!compileError.isEmpty()) {
        qWarning() << "Path expression compile error:" << compileError;
        return {};
    }

    QJsonObject root;

    for (int f = startFrame; f <= endFrame; f++) {
        double time = static_cast<double>(f - startFrame) / fps;
        setContext(time, f - startFrame, clipDuration, fps, 0.0, clipIndex);

        auto [pathData, error] = evaluatePathCompiled(compiled);
        if (!error.isEmpty()) {
            qWarning() << "Path expression bake error at frame" << f << ":" << error;
            freeCompiled(compiled);
            return {};
        }

        // Convert ExprPathData to BPoint array matching rotohelper.cpp format:
        // Each BPoint is [[h1x,h1y], [px,py], [h2x,h2y]] (nested, not flat)
        // h1 = point + inTangent, h2 = point + outTangent
        QJsonArray frameArray;
        for (int i = 0; i < pathData.points.size(); i++) {
            double px = pathData.points[i].x;
            double py = pathData.points[i].y;
            double inX = pathData.inTangents[i].x;
            double inY = pathData.inTangents[i].y;
            double outX = pathData.outTangents[i].x;
            double outY = pathData.outTangents[i].y;

            QJsonArray bpoint;

            // h1 (in-handle) = point + inTangent offset
            QJsonArray h1;
            h1.append(px + inX);
            h1.append(py + inY);
            bpoint.append(h1);

            // p (point position)
            QJsonArray p;
            p.append(px);
            p.append(py);
            bpoint.append(p);

            // h2 (out-handle) = point + outTangent offset
            QJsonArray h2;
            h2.append(px + outX);
            h2.append(py + outY);
            bpoint.append(h2);

            frameArray.append(bpoint);
        }

        // Key: frame number zero-padded to match getRotoProperty() convention
        QString key = QString::number(f).rightJustified(padWidth, QLatin1Char('0'));
        root.insert(key, frameArray);
    }

    freeCompiled(compiled);

    QJsonDocument doc(root);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

bool ExpressionEngine::usesPath(const QString &expression)
{
    return expression.contains(QLatin1String("createPath"));
}
