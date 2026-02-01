/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "expressionfunctions.h"
#include "expressionengine.h"

extern "C" {
#include "quickjs.h"
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

// ── Forward declarations (used before definition) ─────────────────────
static std::vector<double> getArray(JSContext *ctx, JSValueConst val);
static JSValue makeJSArray(JSContext *ctx, const std::vector<double> &vec);

// ── Helpers ────────────────────────────────────────────────────────────

static double getDouble(JSContext *ctx, JSValueConst val)
{
    double d = 0.0;
    JS_ToFloat64(ctx, &d, val);
    return d;
}

static int32_t getInt(JSContext *ctx, JSValueConst val)
{
    int32_t i = 0;
    JS_ToInt32(ctx, &i, val);
    return i;
}

// Simple hash for deterministic seeding
static uint32_t hashSeed(uint32_t a, uint32_t b)
{
    a ^= b + 0x9e3779b9 + (a << 6) + (a >> 2);
    return a;
}

// Fast xorshift32 PRNG
static uint32_t xorshift32(uint32_t &state)
{
    if (state == 0) state = 1; // Avoid zero-state degeneracy (all-zero forever)
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

static double xorshiftDouble(uint32_t &state)
{
    return static_cast<double>(xorshift32(state)) / 4294967296.0;
}

// ── 1D Perlin noise ───────────────────────────────────────────────────

static double fade(double t)
{
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}
static double lerp(double a, double b, double t)
{
    return a + t * (b - a);
}

static double grad1d(int hash, double x)
{
    return (hash & 1) ? x : -x;
}

static const int perm[512] = {
    151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,   225, 140, 36,  103, 30,  69,  142, 8,   99,  37,  240, 21,  10,  23,  190, 6,
    148, 247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203, 117, 35,  11,  32,  57,  177, 33,  88,  237, 149, 56,  87,  174, 20,  125, 136, 171, 168,
    68,  175, 74,  165, 71,  134, 139, 48,  27,  166, 77,  146, 158, 231, 83,  111, 229, 122, 60,  211, 133, 230, 220, 105, 92,  41,  55,  46,  245, 40,  244,
    102, 143, 54,  65,  25,  63,  161, 1,   216, 80,  73,  209, 76,  132, 187, 208, 89,  18,  169, 200, 196, 135, 130, 116, 188, 159, 86,  164, 100, 109, 198,
    173, 186, 3,   64,  52,  217, 226, 250, 124, 123, 5,   202, 38,  147, 118, 126, 255, 82,  85,  212, 207, 206, 59,  227, 47,  16,  58,  17,  182, 189, 28,
    42,  223, 183, 170, 213, 119, 248, 152, 2,   44,  154, 163, 70,  221, 153, 101, 155, 167, 43,  172, 9,   129, 22,  39,  253, 19,  98,  108, 110, 79,  113,
    224, 232, 178, 185, 112, 104, 218, 246, 97,  228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241, 81,  51,  145, 235, 249, 14,  239, 107, 49,
    192, 214, 31,  181, 199, 106, 157, 184, 84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,  222, 114, 67,  29,  24,  72,  243, 141,
    128, 195, 78,  66,  215, 61,  156, 180, 151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,   225, 140, 36,  103, 30,  69,  142, 8,
    99,  37,  240, 21,  10,  23,  190, 6,   148, 247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203, 117, 35,  11,  32,  57,  177, 33,  88,  237, 149,
    56,  87,  174, 20,  125, 136, 171, 168, 68,  175, 74,  165, 71,  134, 139, 48,  27,  166, 77,  146, 158, 231, 83,  111, 229, 122, 60,  211, 133, 230, 220,
    105, 92,  41,  55,  46,  245, 40,  244, 102, 143, 54,  65,  25,  63,  161, 1,   216, 80,  73,  209, 76,  132, 187, 208, 89,  18,  169, 200, 196, 135, 130,
    116, 188, 159, 86,  164, 100, 109, 198, 173, 186, 3,   64,  52,  217, 226, 250, 124, 123, 5,   202, 38,  147, 118, 126, 255, 82,  85,  212, 207, 206, 59,
    227, 47,  16,  58,  17,  182, 189, 28,  42,  223, 183, 170, 213, 119, 248, 152, 2,   44,  154, 163, 70,  221, 153, 101, 155, 167, 43,  172, 9,   129, 22,
    39,  253, 19,  98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104, 218, 246, 97,  228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241, 81,
    51,  145, 235, 249, 14,  239, 107, 49,  192, 214, 31,  181, 199, 106, 157, 184, 84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,
    222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156, 180};

static double perlin1d(double x)
{
    int xi = static_cast<int>(std::floor(x)) & 255;
    double xf = x - std::floor(x);
    double u = fade(xf);
    int a = perm[xi];
    int b = perm[xi + 1];
    return lerp(grad1d(a, xf), grad1d(b, xf - 1.0), u);
}

// ── Cubic Hermite for ease functions ──────────────────────────────────

static double hermite(double t)
{
    // Smooth step: 3t^2 - 2t^3
    return t * t * (3.0 - 2.0 * t);
}

static double hermiteIn(double t)
{
    return t * t;
}

static double hermiteOut(double t)
{
    return 1.0 - (1.0 - t) * (1.0 - t);
}

// ── JS function implementations ───────────────────────────────────────

// linear(t, tMin, tMax, vMin, vMax) or linear(t, vMin, vMax)
static JSValue js_linear(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc == 3) {
        double t = getDouble(ctx, argv[0]);
        double vMin = getDouble(ctx, argv[1]);
        double vMax = getDouble(ctx, argv[2]);
        t = std::max(0.0, std::min(1.0, t));
        return JS_NewFloat64(ctx, vMin + (vMax - vMin) * t);
    }
    if (argc >= 5) {
        double t = getDouble(ctx, argv[0]);
        double tMin = getDouble(ctx, argv[1]);
        double tMax = getDouble(ctx, argv[2]);
        double vMin = getDouble(ctx, argv[3]);
        double vMax = getDouble(ctx, argv[4]);
        if (tMax == tMin) return JS_NewFloat64(ctx, vMin);
        double ratio = (t - tMin) / (tMax - tMin);
        ratio = std::max(0.0, std::min(1.0, ratio));
        return JS_NewFloat64(ctx, vMin + (vMax - vMin) * ratio);
    }
    return JS_ThrowTypeError(ctx, "linear() requires 3 or 5 arguments");
}

// ease(t, tMin, tMax, vMin, vMax) or ease(t, vMin, vMax)
static JSValue js_ease(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc == 3) {
        double t = getDouble(ctx, argv[0]);
        double vMin = getDouble(ctx, argv[1]);
        double vMax = getDouble(ctx, argv[2]);
        t = std::max(0.0, std::min(1.0, t));
        t = hermite(t);
        return JS_NewFloat64(ctx, vMin + (vMax - vMin) * t);
    }
    if (argc >= 5) {
        double t = getDouble(ctx, argv[0]);
        double tMin = getDouble(ctx, argv[1]);
        double tMax = getDouble(ctx, argv[2]);
        double vMin = getDouble(ctx, argv[3]);
        double vMax = getDouble(ctx, argv[4]);
        if (tMax == tMin) return JS_NewFloat64(ctx, vMin);
        double ratio = (t - tMin) / (tMax - tMin);
        ratio = std::max(0.0, std::min(1.0, ratio));
        ratio = hermite(ratio);
        return JS_NewFloat64(ctx, vMin + (vMax - vMin) * ratio);
    }
    return JS_ThrowTypeError(ctx, "ease() requires 3 or 5 arguments");
}

// easeIn(t, tMin, tMax, vMin, vMax) or easeIn(t, vMin, vMax)
static JSValue js_easeIn(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc == 3) {
        double t = getDouble(ctx, argv[0]);
        double vMin = getDouble(ctx, argv[1]);
        double vMax = getDouble(ctx, argv[2]);
        t = std::max(0.0, std::min(1.0, t));
        t = hermiteIn(t);
        return JS_NewFloat64(ctx, vMin + (vMax - vMin) * t);
    }
    if (argc >= 5) {
        double t = getDouble(ctx, argv[0]);
        double tMin = getDouble(ctx, argv[1]);
        double tMax = getDouble(ctx, argv[2]);
        double vMin = getDouble(ctx, argv[3]);
        double vMax = getDouble(ctx, argv[4]);
        if (tMax == tMin) return JS_NewFloat64(ctx, vMin);
        double ratio = (t - tMin) / (tMax - tMin);
        ratio = std::max(0.0, std::min(1.0, ratio));
        ratio = hermiteIn(ratio);
        return JS_NewFloat64(ctx, vMin + (vMax - vMin) * ratio);
    }
    return JS_ThrowTypeError(ctx, "easeIn() requires 3 or 5 arguments");
}

// easeOut(t, tMin, tMax, vMin, vMax) or easeOut(t, vMin, vMax)
static JSValue js_easeOut(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc == 3) {
        double t = getDouble(ctx, argv[0]);
        double vMin = getDouble(ctx, argv[1]);
        double vMax = getDouble(ctx, argv[2]);
        t = std::max(0.0, std::min(1.0, t));
        t = hermiteOut(t);
        return JS_NewFloat64(ctx, vMin + (vMax - vMin) * t);
    }
    if (argc >= 5) {
        double t = getDouble(ctx, argv[0]);
        double tMin = getDouble(ctx, argv[1]);
        double tMax = getDouble(ctx, argv[2]);
        double vMin = getDouble(ctx, argv[3]);
        double vMax = getDouble(ctx, argv[4]);
        if (tMax == tMin) return JS_NewFloat64(ctx, vMin);
        double ratio = (t - tMin) / (tMax - tMin);
        ratio = std::max(0.0, std::min(1.0, ratio));
        ratio = hermiteOut(ratio);
        return JS_NewFloat64(ctx, vMin + (vMax - vMin) * ratio);
    }
    return JS_ThrowTypeError(ctx, "easeOut() requires 3 or 5 arguments");
}

// clamp(value, limit1, limit2)
// AE-compatible: polymorphic — works per-component on arrays
static JSValue js_clamp(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 3) return JS_ThrowTypeError(ctx, "clamp() requires 3 arguments");

    std::vector<double> valArr = getArray(ctx, argv[0]);
    std::vector<double> minArr = getArray(ctx, argv[1]);
    std::vector<double> maxArr = getArray(ctx, argv[2]);

    // If any argument is an array, do per-component clamp
    if (!valArr.empty() || !minArr.empty() || !maxArr.empty()) {
        size_t len = std::max({valArr.size(), minArr.size(), maxArr.size()});
        std::vector<double> result(len);
        for (size_t i = 0; i < len; i++) {
            double v = (i < valArr.size()) ? valArr[i] : (valArr.empty() ? getDouble(ctx, argv[0]) : 0.0);
            double mn = (i < minArr.size()) ? minArr[i] : (minArr.empty() ? getDouble(ctx, argv[1]) : 0.0);
            double mx = (i < maxArr.size()) ? maxArr[i] : (maxArr.empty() ? getDouble(ctx, argv[2]) : 0.0);
            result[i] = std::max(mn, std::min(mx, v));
        }
        return makeJSArray(ctx, result);
    }

    // All scalars
    double val = getDouble(ctx, argv[0]);
    double mn = getDouble(ctx, argv[1]);
    double mx = getDouble(ctx, argv[2]);
    return JS_NewFloat64(ctx, std::max(mn, std::min(mx, val)));
}

// wiggle(freq, amp) or wiggle(freq, amp, octaves, ampMult, t)
// Uses Perlin noise seeded with clip context for determinism
static JSValue js_wiggle(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 2) return JS_ThrowTypeError(ctx, "wiggle() requires at least 2 arguments");

    double freq = getDouble(ctx, argv[0]);
    double amp = getDouble(ctx, argv[1]);
    int octaves = (argc > 2) ? getInt(ctx, argv[2]) : 1;
    double ampMult = (argc > 3) ? getDouble(ctx, argv[3]) : 0.5;

    // Read time and value from C++ cache (Issue #7: avoids JS global reads)
    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));
    double t = (argc > 4) ? getDouble(ctx, argv[4]) : engine->cachedTime();
    double baseVal = engine->cachedValue();

    double result = 0.0;
    double currentAmp = amp;
    double currentFreq = freq;
    for (int i = 0; i < octaves; i++) {
        result += perlin1d(t * currentFreq + i * 100.0) * currentAmp;
        currentAmp *= ampMult;
        currentFreq *= 2.0;
    }
    return JS_NewFloat64(ctx, baseVal + result);
}

// Helper: build deterministic seed incorporating _userSeed and _timeless from seedRandom()
// Reads from C++ cache via JS_GetContextOpaque (Issue 3: avoids 4 JS global reads per call)
static uint32_t buildRandomSeed(JSContext *ctx)
{
    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));

    uint32_t frame = static_cast<uint32_t>(engine->cachedFrame());
    uint32_t idx = static_cast<uint32_t>(engine->cachedIndex());
    bool timeless = engine->timelessSeed();
    bool hasSeed = engine->hasUserSeed();
    uint32_t userSeed = engine->userSeed();

    uint32_t seed;
    if (timeless) {
        // Time-independent: seed from userSeed and index only
        seed = hashSeed(userSeed + 1, idx + 1);
    } else {
        seed = hashSeed(frame + 1, idx + 1);
        if (hasSeed) {
            // Mix in user seed — even seedRandom(0, false) differs from no seedRandom
            seed = hashSeed(seed, userSeed + 0x9e3779b9);
        }
    }
    return seed;
}

// random(), random(maxValOrArray), random(minValOrArray, maxValOrArray)
// AE-compatible: polymorphic — works on scalars and arrays
static JSValue js_random(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    uint32_t seed = buildRandomSeed(ctx);

    if (argc == 0) {
        return JS_NewFloat64(ctx, xorshiftDouble(seed));
    }

    if (argc == 1) {
        // random(maxValOrArray)
        std::vector<double> maxArr = getArray(ctx, argv[0]);
        if (!maxArr.empty()) {
            std::vector<double> result(maxArr.size());
            for (size_t i = 0; i < maxArr.size(); i++) {
                result[i] = xorshiftDouble(seed) * maxArr[i];
            }
            return makeJSArray(ctx, result);
        }
        double mx = getDouble(ctx, argv[0]);
        return JS_NewFloat64(ctx, xorshiftDouble(seed) * mx);
    }

    // argc >= 2: random(minValOrArray, maxValOrArray)
    std::vector<double> minArr = getArray(ctx, argv[0]);
    std::vector<double> maxArr = getArray(ctx, argv[1]);

    if (!minArr.empty() || !maxArr.empty()) {
        // Broadcast scalar to match array length (AE behavior)
        double minScalar = minArr.empty() ? getDouble(ctx, argv[0]) : 0.0;
        double maxScalar = maxArr.empty() ? getDouble(ctx, argv[1]) : 0.0;
        size_t len = std::max(minArr.size(), maxArr.size());
        std::vector<double> result(len);
        for (size_t i = 0; i < len; i++) {
            double mn = (i < minArr.size()) ? minArr[i] : minScalar;
            double mx = (i < maxArr.size()) ? maxArr[i] : maxScalar;
            result[i] = mn + (mx - mn) * xorshiftDouble(seed);
        }
        return makeJSArray(ctx, result);
    }

    double mn = getDouble(ctx, argv[0]);
    double mx = getDouble(ctx, argv[1]);
    return JS_NewFloat64(ctx, mn + (mx - mn) * xorshiftDouble(seed));
}

// gaussRandom(), gaussRandom(maxValOrArray), gaussRandom(minValOrArray, maxValOrArray)
// AE-compatible: Gaussian (bell-shaped) distribution, polymorphic on scalars and arrays.
// ~90% of results in specified range, ~10% outside.
static JSValue js_gaussRandom(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    uint32_t seed = buildRandomSeed(ctx);

    // Helper: generate one Gaussian sample via Box-Muller
    auto gaussSample = [&seed]() -> double {
        double u1 = xorshiftDouble(seed);
        double u2 = xorshiftDouble(seed);
        if (u1 < 1e-10) u1 = 1e-10;
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
    };

    if (argc == 0) {
        return JS_NewFloat64(ctx, gaussSample());
    }

    if (argc == 1) {
        // gaussRandom(maxValOrArray) — range [0, max], ~90% within
        std::vector<double> maxArr = getArray(ctx, argv[0]);
        if (!maxArr.empty()) {
            std::vector<double> result(maxArr.size());
            for (size_t i = 0; i < maxArr.size(); i++) {
                double mid = maxArr[i] / 2.0;
                double range = maxArr[i] / 3.29;
                result[i] = mid + gaussSample() * range;
            }
            return makeJSArray(ctx, result);
        }
        double mx = getDouble(ctx, argv[0]);
        double mid = mx / 2.0;
        double range = mx / 3.29;
        return JS_NewFloat64(ctx, mid + gaussSample() * range);
    }

    // argc >= 2: gaussRandom(minValOrArray, maxValOrArray)
    std::vector<double> minArr = getArray(ctx, argv[0]);
    std::vector<double> maxArr = getArray(ctx, argv[1]);

    if (!minArr.empty() || !maxArr.empty()) {
        // Broadcast scalar to match array length (AE behavior)
        double minScalar = minArr.empty() ? getDouble(ctx, argv[0]) : 0.0;
        double maxScalar = maxArr.empty() ? getDouble(ctx, argv[1]) : 0.0;
        size_t len = std::max(minArr.size(), maxArr.size());
        std::vector<double> result(len);
        for (size_t i = 0; i < len; i++) {
            double mn = (i < minArr.size()) ? minArr[i] : minScalar;
            double mx = (i < maxArr.size()) ? maxArr[i] : maxScalar;
            double mid = (mn + mx) / 2.0;
            double range = (mx - mn) / 3.29;
            result[i] = mid + gaussSample() * range;
        }
        return makeJSArray(ctx, result);
    }

    double mn = getDouble(ctx, argv[0]);
    double mx = getDouble(ctx, argv[1]);
    double mid = (mn + mx) / 2.0;
    double range = (mx - mn) / 3.29;
    return JS_NewFloat64(ctx, mid + gaussSample() * range);
}

// noise(v) — 1D Perlin noise, returns -1 to 1
static JSValue js_noise(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "noise() requires 1 argument");
    double v = getDouble(ctx, argv[0]);
    return JS_NewFloat64(ctx, perlin1d(v) * 2.0); // scale to roughly -1..1
}

// temporalWiggle(freq, amp, octaves=1, ampMult=0.5, t=time) — step-hold wiggle
// Unlike wiggle() which uses smooth Perlin noise, temporalWiggle holds each
// random value for 1/freq seconds, producing a stepped/quantized random motion.
// AE-compatible: deterministic, seeded by step index and clip context.
static JSValue js_temporalWiggle(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 2) return JS_ThrowTypeError(ctx, "temporalWiggle() requires at least 2 arguments (freq, amp)");

    double freq = getDouble(ctx, argv[0]);
    double amp = getDouble(ctx, argv[1]);
    int octaves = (argc > 2) ? getInt(ctx, argv[2]) : 1;
    double ampMult = (argc > 3) ? getDouble(ctx, argv[3]) : 0.5;

    if (freq <= 0.0) freq = 1.0;
    if (octaves < 1) octaves = 1;
    if (octaves > 10) octaves = 10;

    // Read time, value, index from C++ cache (Issue #7: avoids JS global reads)
    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));
    double t = (argc > 4) ? getDouble(ctx, argv[4]) : engine->cachedTime();
    double baseVal = engine->cachedValue();
    uint32_t idx = static_cast<uint32_t>(engine->cachedIndex());

    double result = 0.0;
    double currentAmp = amp;
    double currentFreq = freq;

    for (int oct = 0; oct < octaves; oct++) {
        // Compute which step we're in for this octave
        int32_t stepIndex = static_cast<int32_t>(std::floor(t * currentFreq));

        // Deterministic seed: combine step index, octave, and clip index.
        // Cast directly to uint32_t — two's complement wrapping is well-defined
        // for unsigned types and produces distinct hashes for positive vs negative.
        uint32_t stepHash = static_cast<uint32_t>(stepIndex);
        uint32_t seed = hashSeed(stepHash + static_cast<uint32_t>(oct) * 10000u, idx + 1);

        // Generate random value in [-1, 1]
        double r = xorshiftDouble(seed) * 2.0 - 1.0;
        result += r * currentAmp;

        currentAmp *= ampMult;
        currentFreq *= 2.0;
    }

    return JS_NewFloat64(ctx, baseVal + result);
}

// seedRandom(seed, timeless)
static JSValue js_seedRandom(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 2) return JS_ThrowTypeError(ctx, "seedRandom() requires 2 arguments");
    int32_t seed = getInt(ctx, argv[0]);
    bool timeless = JS_ToBool(ctx, argv[1]) > 0;

    // Store in C++ engine cache (Issue 3: buildRandomSeed reads from C++ directly)
    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));
    engine->setUserSeed(static_cast<uint32_t>(seed), timeless);

    // Also set JS globals for validate() context save/restore compatibility
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "_userSeed", JS_NewInt32(ctx, seed));
    JS_SetPropertyStr(ctx, global, "_timeless", JS_DupValue(ctx, argv[1]));
    JS_FreeValue(ctx, global);
    return JS_UNDEFINED;
}

// posterizeTime(fps) — sets a global that bakeToAnimString uses
static JSValue js_posterizeTime(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "posterizeTime() requires 1 argument");
    double targetFps = getDouble(ctx, argv[0]);
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "_posterizeFps", JS_NewFloat64(ctx, targetFps));
    JS_FreeValue(ctx, global);
    return JS_UNDEFINED;
}

// degreesToRadians(deg)
static JSValue js_degreesToRadians(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "degreesToRadians() requires 1 argument");
    double deg = getDouble(ctx, argv[0]);
    return JS_NewFloat64(ctx, deg * M_PI / 180.0);
}

// radiansToDegrees(rad)
static JSValue js_radiansToDegrees(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "radiansToDegrees() requires 1 argument");
    double rad = getDouble(ctx, argv[0]);
    return JS_NewFloat64(ctx, rad * 180.0 / M_PI);
}

// smooth(width, samples) — temporal smoothing via keyframe interpolation
// Averages interpCached() at evenly-spaced sample points within the window.
// Reads from C++ keyframe cache (Issue 1: no JS array traversal, O(log n) binary search).
static JSValue js_smooth(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    double width = (argc >= 1) ? getDouble(ctx, argv[0]) : 0.2;
    int32_t samples = (argc >= 2) ? getInt(ctx, argv[1]) : 5;

    if (samples < 1) samples = 1;
    if (samples > 100) samples = 100;

    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));
    double t = engine->cachedTime();
    double fallback = engine->cachedValue();

    if (width <= 0.0) return JS_NewFloat64(ctx, fallback);

    const auto &kfs = engine->keyframeCache();
    if (kfs.isEmpty()) {
        return JS_NewFloat64(ctx, fallback);
    }

    double halfW = width / 2.0;
    double sum = 0.0;
    for (int32_t i = 0; i < samples; i++) {
        double sampleT;
        if (samples == 1) {
            sampleT = t;
        } else {
            sampleT = (t - halfW) + (width * static_cast<double>(i) / static_cast<double>(samples - 1));
        }
        sum += engine->interpCached(sampleT);
    }
    return JS_NewFloat64(ctx, sum / static_cast<double>(samples));
}

// ── Audio functions ───────────────────────────────────────────────────

// Audio cache is stored as typed arrays on globalThis._audio
// Set by ExpressionEngine::setAudioCache()

// audioLevel/audioRms read from C++ cache via JS_GetContextOpaque (Issue 2)
// No JS global reads — direct QVector indexing.

static JSValue js_audioLevel(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 2) return JS_ThrowTypeError(ctx, "audioLevel() requires 2 arguments (channel, time)");

    const char *channel = JS_ToCString(ctx, argv[0]);
    if (!channel) return JS_ThrowTypeError(ctx, "audioLevel() first argument must be a string");

    double t = getDouble(ctx, argv[1]);

    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));
    int totalFrames = engine->audioTotalFrames();
    if (totalFrames <= 0) {
        JS_FreeCString(ctx, channel);
        return JS_NewFloat64(ctx, 0.0);
    }

    double fps = engine->audioFps();
    if (fps <= 0.0) fps = 25.0;
    int frame = static_cast<int>(std::round(t * fps));
    double result = static_cast<double>(engine->audioPeak(channel, frame));

    JS_FreeCString(ctx, channel);
    return JS_NewFloat64(ctx, result);
}

// audioRms(channel, t, window) — RMS in a window around t
static JSValue js_audioRms(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 3) return JS_ThrowTypeError(ctx, "audioRms() requires 3 arguments (channel, time, window)");

    const char *channel = JS_ToCString(ctx, argv[0]);
    if (!channel) return JS_ThrowTypeError(ctx, "audioRms() first argument must be a string");

    double t = getDouble(ctx, argv[1]);
    double window = getDouble(ctx, argv[2]);

    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));
    int totalFrames = engine->audioTotalFrames();
    if (totalFrames <= 0) {
        JS_FreeCString(ctx, channel);
        return JS_NewFloat64(ctx, 0.0);
    }

    double fps = engine->audioFps();
    if (fps <= 0.0) fps = 25.0;

    int centerFrame = static_cast<int>(std::round(t * fps));
    int halfWindow = static_cast<int>(std::round(window * fps / 2.0));
    int startF = std::max(0, centerFrame - halfWindow);
    int endF = std::min(totalFrames - 1, centerFrame + halfWindow);

    double sumSq = 0.0;
    int count = 0;
    for (int f = startF; f <= endF; f++) {
        double v = static_cast<double>(engine->audioPeak(channel, f));
        sumSq += v * v;
        count++;
    }

    JS_FreeCString(ctx, channel);
    return JS_NewFloat64(ctx, (count > 0) ? std::sqrt(sumSq / count) : 0.0);
}

// ── Vector helpers ────────────────────────────────────────────────────

// Read a JSValue as a vector of doubles. Returns empty vector if val is not an array.
static std::vector<double> getArray(JSContext *ctx, JSValueConst val)
{
    std::vector<double> result;
    if (!JS_IsArray(ctx, val)) return result;

    JSValue lenVal = JS_GetPropertyStr(ctx, val, "length");
    int32_t len = 0;
    JS_ToInt32(ctx, &len, lenVal);
    JS_FreeValue(ctx, lenVal);

    result.reserve(len);
    for (int32_t i = 0; i < len; i++) {
        JSValue elem = JS_GetPropertyUint32(ctx, val, static_cast<uint32_t>(i));
        double d = 0.0;
        JS_ToFloat64(ctx, &d, elem);
        JS_FreeValue(ctx, elem);
        result.push_back(d);
    }
    return result;
}

// Create a JS array from a vector of doubles.
static JSValue makeJSArray(JSContext *ctx, const std::vector<double> &vec)
{
    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < vec.size(); i++) {
        JS_SetPropertyUint32(ctx, arr, static_cast<uint32_t>(i), JS_NewFloat64(ctx, vec[i]));
    }
    return arr;
}

// ── Vector math functions (AE-compatible) ─────────────────────────────

// add(a, b) — element-wise addition of two vectors, or scalar addition
static JSValue js_add(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 2) return JS_ThrowTypeError(ctx, "add() requires 2 arguments");

    std::vector<double> arrA = getArray(ctx, argv[0]);
    std::vector<double> arrB = getArray(ctx, argv[1]);

    // Both arrays
    if (!arrA.empty() && !arrB.empty()) {
        size_t len = std::max(arrA.size(), arrB.size());
        std::vector<double> result(len, 0.0);
        for (size_t i = 0; i < len; i++) {
            double a = (i < arrA.size()) ? arrA[i] : 0.0;
            double b = (i < arrB.size()) ? arrB[i] : 0.0;
            result[i] = a + b;
        }
        return makeJSArray(ctx, result);
    }

    // Array + scalar or scalar + array
    if (!arrA.empty()) {
        double s = getDouble(ctx, argv[1]);
        std::vector<double> result(arrA.size());
        for (size_t i = 0; i < arrA.size(); i++) {
            result[i] = arrA[i] + s;
        }
        return makeJSArray(ctx, result);
    }
    if (!arrB.empty()) {
        double s = getDouble(ctx, argv[0]);
        std::vector<double> result(arrB.size());
        for (size_t i = 0; i < arrB.size(); i++) {
            result[i] = s + arrB[i];
        }
        return makeJSArray(ctx, result);
    }

    // Both scalars
    return JS_NewFloat64(ctx, getDouble(ctx, argv[0]) + getDouble(ctx, argv[1]));
}

// sub(a, b) — element-wise subtraction of two vectors, or scalar subtraction
static JSValue js_sub(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 2) return JS_ThrowTypeError(ctx, "sub() requires 2 arguments");

    std::vector<double> arrA = getArray(ctx, argv[0]);
    std::vector<double> arrB = getArray(ctx, argv[1]);

    // Both arrays
    if (!arrA.empty() && !arrB.empty()) {
        size_t len = std::max(arrA.size(), arrB.size());
        std::vector<double> result(len, 0.0);
        for (size_t i = 0; i < len; i++) {
            double a = (i < arrA.size()) ? arrA[i] : 0.0;
            double b = (i < arrB.size()) ? arrB[i] : 0.0;
            result[i] = a - b;
        }
        return makeJSArray(ctx, result);
    }

    // Array - scalar
    if (!arrA.empty()) {
        double s = getDouble(ctx, argv[1]);
        std::vector<double> result(arrA.size());
        for (size_t i = 0; i < arrA.size(); i++) {
            result[i] = arrA[i] - s;
        }
        return makeJSArray(ctx, result);
    }
    // scalar - Array
    if (!arrB.empty()) {
        double s = getDouble(ctx, argv[0]);
        std::vector<double> result(arrB.size());
        for (size_t i = 0; i < arrB.size(); i++) {
            result[i] = s - arrB[i];
        }
        return makeJSArray(ctx, result);
    }

    // Both scalars
    return JS_NewFloat64(ctx, getDouble(ctx, argv[0]) - getDouble(ctx, argv[1]));
}

// mul(a, b) — element-wise multiplication, or scalar * vector broadcast
static JSValue js_mul(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 2) return JS_ThrowTypeError(ctx, "mul() requires 2 arguments");

    std::vector<double> arrA = getArray(ctx, argv[0]);
    std::vector<double> arrB = getArray(ctx, argv[1]);

    // Both arrays — element-wise (AE behavior)
    if (!arrA.empty() && !arrB.empty()) {
        size_t len = std::max(arrA.size(), arrB.size());
        std::vector<double> result(len, 0.0);
        for (size_t i = 0; i < len; i++) {
            double a = (i < arrA.size()) ? arrA[i] : 0.0;
            double b = (i < arrB.size()) ? arrB[i] : 0.0;
            result[i] = a * b;
        }
        return makeJSArray(ctx, result);
    }

    // Array * scalar (broadcast)
    if (!arrA.empty()) {
        double s = getDouble(ctx, argv[1]);
        std::vector<double> result(arrA.size());
        for (size_t i = 0; i < arrA.size(); i++) {
            result[i] = arrA[i] * s;
        }
        return makeJSArray(ctx, result);
    }
    if (!arrB.empty()) {
        double s = getDouble(ctx, argv[0]);
        std::vector<double> result(arrB.size());
        for (size_t i = 0; i < arrB.size(); i++) {
            result[i] = s * arrB[i];
        }
        return makeJSArray(ctx, result);
    }

    // Both scalars
    return JS_NewFloat64(ctx, getDouble(ctx, argv[0]) * getDouble(ctx, argv[1]));
}

// div(a, b) — element-wise division, or vector / scalar
static JSValue js_div(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 2) return JS_ThrowTypeError(ctx, "div() requires 2 arguments");

    std::vector<double> arrA = getArray(ctx, argv[0]);
    std::vector<double> arrB = getArray(ctx, argv[1]);

    // Both arrays — element-wise
    if (!arrA.empty() && !arrB.empty()) {
        size_t len = std::max(arrA.size(), arrB.size());
        std::vector<double> result(len, 0.0);
        for (size_t i = 0; i < len; i++) {
            double a = (i < arrA.size()) ? arrA[i] : 0.0;
            double b = (i < arrB.size()) ? arrB[i] : 0.0;
            result[i] = (b != 0.0) ? a / b : 0.0;
        }
        return makeJSArray(ctx, result);
    }

    // Array / scalar
    if (!arrA.empty()) {
        double s = getDouble(ctx, argv[1]);
        if (s == 0.0) return JS_ThrowRangeError(ctx, "div() division by zero");
        std::vector<double> result(arrA.size());
        for (size_t i = 0; i < arrA.size(); i++) {
            result[i] = arrA[i] / s;
        }
        return makeJSArray(ctx, result);
    }
    // scalar / Array
    if (!arrB.empty()) {
        double s = getDouble(ctx, argv[0]);
        std::vector<double> result(arrB.size());
        for (size_t i = 0; i < arrB.size(); i++) {
            result[i] = (arrB[i] != 0.0) ? s / arrB[i] : 0.0;
        }
        return makeJSArray(ctx, result);
    }

    // Both scalars
    double b = getDouble(ctx, argv[1]);
    if (b == 0.0) return JS_ThrowRangeError(ctx, "div() division by zero");
    return JS_NewFloat64(ctx, getDouble(ctx, argv[0]) / b);
}

// length(vec) or length(point1, point2)
// AE-compatible: 1-arg returns vector magnitude, 2-arg returns distance between points
static JSValue js_length(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "length() requires at least 1 argument");

    if (argc >= 2) {
        // length(point1, point2) — distance between two points
        std::vector<double> a = getArray(ctx, argv[0]);
        std::vector<double> b = getArray(ctx, argv[1]);

        if (!a.empty() || !b.empty()) {
            // Mixed scalar+array: treat scalar as [scalar] (AE zero-pads missing dims)
            double aScalar = a.empty() ? getDouble(ctx, argv[0]) : 0.0;
            double bScalar = b.empty() ? getDouble(ctx, argv[1]) : 0.0;
            if (a.empty()) a = {aScalar};
            if (b.empty()) b = {bScalar};
            size_t len = std::max(a.size(), b.size());
            double sumSq = 0.0;
            for (size_t i = 0; i < len; i++) {
                double ai = (i < a.size()) ? a[i] : 0.0;
                double bi = (i < b.size()) ? b[i] : 0.0;
                double d = ai - bi;
                sumSq += d * d;
            }
            return JS_NewFloat64(ctx, std::sqrt(sumSq));
        }

        // Both scalars: absolute difference
        return JS_NewFloat64(ctx, std::fabs(getDouble(ctx, argv[0]) - getDouble(ctx, argv[1])));
    }

    // 1-arg: vector magnitude or scalar absolute value
    std::vector<double> arr = getArray(ctx, argv[0]);
    if (!arr.empty()) {
        double sumSq = 0.0;
        for (double v : arr) {
            sumSq += v * v;
        }
        return JS_NewFloat64(ctx, std::sqrt(sumSq));
    }

    return JS_NewFloat64(ctx, std::fabs(getDouble(ctx, argv[0])));
}

// normalize(a) — unit vector, or sign of scalar (-1, 0, 1)
static JSValue js_normalize(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "normalize() requires 1 argument");

    std::vector<double> arr = getArray(ctx, argv[0]);
    if (!arr.empty()) {
        double sumSq = 0.0;
        for (double v : arr) {
            sumSq += v * v;
        }
        double len = std::sqrt(sumSq);
        if (len < 1e-15) {
            // Zero vector — return zero vector of same dimension
            return makeJSArray(ctx, std::vector<double>(arr.size(), 0.0));
        }
        std::vector<double> result(arr.size());
        for (size_t i = 0; i < arr.size(); i++) {
            result[i] = arr[i] / len;
        }
        return makeJSArray(ctx, result);
    }

    // Scalar — return sign
    double v = getDouble(ctx, argv[0]);
    if (v > 0.0) return JS_NewFloat64(ctx, 1.0);
    if (v < 0.0) return JS_NewFloat64(ctx, -1.0);
    return JS_NewFloat64(ctx, 0.0);
}

// dot(a, b) — dot product of two vectors, or scalar multiplication
static JSValue js_dot(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 2) return JS_ThrowTypeError(ctx, "dot() requires 2 arguments");

    std::vector<double> arrA = getArray(ctx, argv[0]);
    std::vector<double> arrB = getArray(ctx, argv[1]);

    if (!arrA.empty() && !arrB.empty()) {
        size_t len = std::min(arrA.size(), arrB.size());
        double sum = 0.0;
        for (size_t i = 0; i < len; i++) {
            sum += arrA[i] * arrB[i];
        }
        return JS_NewFloat64(ctx, sum);
    }

    // Scalar fallback
    return JS_NewFloat64(ctx, getDouble(ctx, argv[0]) * getDouble(ctx, argv[1]));
}

// cross(a, b) — cross product of two 3D vectors, returns 3D vector
static JSValue js_cross(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 2) return JS_ThrowTypeError(ctx, "cross() requires 2 arguments");

    std::vector<double> arrA = getArray(ctx, argv[0]);
    std::vector<double> arrB = getArray(ctx, argv[1]);

    if (arrA.size() < 3 || arrB.size() < 3) {
        return JS_ThrowTypeError(ctx, "cross() requires two 3D vectors (arrays of length >= 3)");
    }

    std::vector<double> result(3);
    result[0] = arrA[1] * arrB[2] - arrA[2] * arrB[1];
    result[1] = arrA[2] * arrB[0] - arrA[0] * arrB[2];
    result[2] = arrA[0] * arrB[1] - arrA[1] * arrB[0];
    return makeJSArray(ctx, result);
}

// lookAt(fromPoint, atPoint) — AE-compatible orientation calculation
// Returns [xRotation, yRotation, 0] in degrees so that z-axis points from fromPoint to atPoint
static JSValue js_lookAt(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 2) return JS_ThrowTypeError(ctx, "lookAt() requires 2 arguments (fromPoint, atPoint)");

    std::vector<double> from = getArray(ctx, argv[0]);
    std::vector<double> at = getArray(ctx, argv[1]);

    if (from.size() < 3 || at.size() < 3) {
        return JS_ThrowTypeError(ctx, "lookAt() requires two 3D arrays [x,y,z]");
    }

    double dx = at[0] - from[0];
    double dy = at[1] - from[1];
    double dz = at[2] - from[2];

    double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (dist < 1e-15) {
        return makeJSArray(ctx, {0.0, 0.0, 0.0});
    }

    // X rotation: pitch (around X axis) — negate because AE Y is down
    double xRot = -std::atan2(dy, std::sqrt(dx * dx + dz * dz)) * 180.0 / M_PI;
    // Y rotation: yaw (around Y axis)
    double yRot = std::atan2(dx, dz) * 180.0 / M_PI;

    return makeJSArray(ctx, {xRot, yRot, 0.0});
}

// ── Keyframe loop helpers ─────────────────────────────────────────────
// All loop/keyframe functions read from C++ cache via JS_GetContextOpaque
// (Issue 1+7: no JS array traversal, O(log n) binary search via interpCached)

// Loop type enum for loopIn/loopOut (AE-compatible: cycle, pingpong, offset, continue)
enum class LoopType { Cycle, PingPong, Offset, Continue };

static LoopType parseLoopType(JSContext *ctx, int argc, JSValueConst *argv)
{
    if (argc < 1 || JS_IsUndefined(argv[0])) return LoopType::Cycle;
    const char *s = JS_ToCString(ctx, argv[0]);
    if (!s) return LoopType::Cycle;
    LoopType type = LoopType::Cycle;
    if (std::strcmp(s, "pingpong") == 0)
        type = LoopType::PingPong;
    else if (std::strcmp(s, "offset") == 0)
        type = LoopType::Offset;
    else if (std::strcmp(s, "continue") == 0)
        type = LoopType::Continue;
    JS_FreeCString(ctx, s);
    return type;
}

// loopOut(type="cycle", nKeys=0)
// AE modes: "cycle" (default), "pingpong", "offset", "continue"
static JSValue js_loopOut(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    LoopType type = parseLoopType(ctx, argc, argv);
    int32_t nKeys = (argc >= 2) ? getInt(ctx, argv[1]) : 0;

    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));
    double t = engine->cachedTime();
    double fallback = engine->cachedValue();
    double fps = engine->cachedFps();
    if (fps <= 0.0) fps = 25.0;

    const auto &kfs = engine->keyframeCache();
    int32_t len = kfs.size();
    if (len < 2) return JS_NewFloat64(ctx, fallback);

    // Determine segment: last nKeys keyframes (or all if nKeys==0 or nKeys>=len)
    int32_t segFirst = 0;
    int32_t segLast = len - 1;
    if (nKeys > 0 && nKeys < len) {
        segFirst = len - nKeys;
    }

    double segStart = kfs[segFirst].t;
    double segEnd = kfs[segLast].t;
    double loopDur = segEnd - segStart;

    // Within keyframe range: just interpolate normally
    if (t <= segEnd) {
        return JS_NewFloat64(ctx, engine->interpCached(t));
    }

    if (loopDur < 1e-12) return JS_NewFloat64(ctx, kfs[segLast].v);

    double elapsed = t - segEnd;

    switch (type) {
    case LoopType::Continue: {
        double vel = engine->velocityCached(segEnd);
        return JS_NewFloat64(ctx, kfs[segLast].v + vel * elapsed);
    }
    case LoopType::PingPong: {
        double cyclePos = std::fmod(elapsed, loopDur * 2.0);
        double remapped;
        if (cyclePos <= loopDur) {
            remapped = segEnd - cyclePos;
        } else {
            remapped = segStart + (cyclePos - loopDur);
        }
        return JS_NewFloat64(ctx, engine->interpCached(remapped));
    }
    case LoopType::Offset: {
        double valDelta = kfs[segLast].v - kfs[segFirst].v;
        double numCycles = std::floor(elapsed / loopDur);
        double remapped = segStart + std::fmod(elapsed, loopDur);
        double baseVal = engine->interpCached(remapped);
        return JS_NewFloat64(ctx, baseVal + valDelta * (numCycles + 1.0));
    }
    case LoopType::Cycle:
    default: {
        double remapped = segStart + std::fmod(elapsed, loopDur);
        return JS_NewFloat64(ctx, engine->interpCached(remapped));
    }
    }
}

// loopIn(type="cycle", nKeys=0)
// AE modes: "cycle" (default), "pingpong", "offset", "continue"
static JSValue js_loopIn(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    LoopType type = parseLoopType(ctx, argc, argv);
    int32_t nKeys = (argc >= 2) ? getInt(ctx, argv[1]) : 0;

    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));
    double t = engine->cachedTime();
    double fallback = engine->cachedValue();

    const auto &kfs = engine->keyframeCache();
    int32_t len = kfs.size();
    if (len < 2) return JS_NewFloat64(ctx, fallback);

    int32_t segFirst = 0;
    int32_t segLast = len - 1;
    if (nKeys > 0 && nKeys < len) {
        segLast = nKeys - 1;
    }

    double segStart = kfs[segFirst].t;
    double segEnd = kfs[segLast].t;
    double loopDur = segEnd - segStart;

    if (t >= segStart) return JS_NewFloat64(ctx, engine->interpCached(t));
    if (loopDur < 1e-12) return JS_NewFloat64(ctx, kfs[segFirst].v);

    double deficit = segStart - t;

    switch (type) {
    case LoopType::Continue: {
        double vel = engine->velocityCached(segStart);
        return JS_NewFloat64(ctx, kfs[segFirst].v - vel * deficit);
    }
    case LoopType::PingPong: {
        double cyclePos = std::fmod(deficit, loopDur * 2.0);
        double remapped = (cyclePos <= loopDur) ? segStart + cyclePos : segEnd - (cyclePos - loopDur);
        return JS_NewFloat64(ctx, engine->interpCached(remapped));
    }
    case LoopType::Offset: {
        double valDelta = kfs[segLast].v - kfs[segFirst].v;
        double numCycles = std::floor(deficit / loopDur);
        double remapped = segEnd - std::fmod(deficit, loopDur);
        return JS_NewFloat64(ctx, engine->interpCached(remapped) - valDelta * (numCycles + 1.0));
    }
    case LoopType::Cycle:
    default: {
        double remapped = segEnd - std::fmod(deficit, loopDur);
        return JS_NewFloat64(ctx, engine->interpCached(remapped));
    }
    }
}

// loopOutDuration(type="cycle", duration=0)
static JSValue js_loopOutDuration(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    LoopType type = parseLoopType(ctx, argc, argv);
    double durSec = (argc >= 2) ? getDouble(ctx, argv[1]) : 0.0;

    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));
    double t = engine->cachedTime();
    double fallback = engine->cachedValue();

    const auto &kfs = engine->keyframeCache();
    int32_t len = kfs.size();
    if (len < 2) return JS_NewFloat64(ctx, fallback);

    int32_t segLast = len - 1;
    int32_t segFirst = 0;
    if (durSec > 0.0) {
        double cutoff = kfs[segLast].t - durSec;
        for (int32_t i = segLast; i >= 0; i--) {
            if (kfs[i].t <= cutoff) {
                segFirst = i;
                break;
            }
        }
    }

    double segStart = kfs[segFirst].t;
    double segEnd = kfs[segLast].t;
    double loopDur = segEnd - segStart;

    if (t <= segEnd) return JS_NewFloat64(ctx, engine->interpCached(t));
    if (loopDur < 1e-12) return JS_NewFloat64(ctx, kfs[segLast].v);

    double elapsed = t - segEnd;

    switch (type) {
    case LoopType::Continue: {
        return JS_NewFloat64(ctx, kfs[segLast].v + engine->velocityCached(segEnd) * elapsed);
    }
    case LoopType::PingPong: {
        double cyclePos = std::fmod(elapsed, loopDur * 2.0);
        double remapped = (cyclePos <= loopDur) ? segEnd - cyclePos : segStart + (cyclePos - loopDur);
        return JS_NewFloat64(ctx, engine->interpCached(remapped));
    }
    case LoopType::Offset: {
        double valDelta = kfs[segLast].v - kfs[segFirst].v;
        double numCycles = std::floor(elapsed / loopDur);
        double remapped = segStart + std::fmod(elapsed, loopDur);
        return JS_NewFloat64(ctx, engine->interpCached(remapped) + valDelta * (numCycles + 1.0));
    }
    case LoopType::Cycle:
    default: {
        double remapped = segStart + std::fmod(elapsed, loopDur);
        return JS_NewFloat64(ctx, engine->interpCached(remapped));
    }
    }
}

// loopInDuration(type="cycle", duration=0)
static JSValue js_loopInDuration(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    LoopType type = parseLoopType(ctx, argc, argv);
    double durSec = (argc >= 2) ? getDouble(ctx, argv[1]) : 0.0;

    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));
    double t = engine->cachedTime();
    double fallback = engine->cachedValue();

    const auto &kfs = engine->keyframeCache();
    int32_t len = kfs.size();
    if (len < 2) return JS_NewFloat64(ctx, fallback);

    int32_t segFirst = 0;
    int32_t segLast = len - 1;
    if (durSec > 0.0) {
        double cutoff = kfs[segFirst].t + durSec;
        for (int32_t i = 0; i < len; i++) {
            if (kfs[i].t >= cutoff) {
                segLast = i;
                break;
            }
        }
    }

    double segStart = kfs[segFirst].t;
    double segEnd = kfs[segLast].t;
    double loopDur = segEnd - segStart;

    if (t >= segStart) return JS_NewFloat64(ctx, engine->interpCached(t));
    if (loopDur < 1e-12) return JS_NewFloat64(ctx, kfs[segFirst].v);

    double deficit = segStart - t;

    switch (type) {
    case LoopType::Continue: {
        return JS_NewFloat64(ctx, kfs[segFirst].v - engine->velocityCached(segStart) * deficit);
    }
    case LoopType::PingPong: {
        double cyclePos = std::fmod(deficit, loopDur * 2.0);
        double remapped = (cyclePos <= loopDur) ? segStart + cyclePos : segEnd - (cyclePos - loopDur);
        return JS_NewFloat64(ctx, engine->interpCached(remapped));
    }
    case LoopType::Offset: {
        double valDelta = kfs[segLast].v - kfs[segFirst].v;
        double numCycles = std::floor(deficit / loopDur);
        double remapped = segEnd - std::fmod(deficit, loopDur);
        return JS_NewFloat64(ctx, engine->interpCached(remapped) - valDelta * (numCycles + 1.0));
    }
    case LoopType::Cycle:
    default: {
        double remapped = segEnd - std::fmod(deficit, loopDur);
        return JS_NewFloat64(ctx, engine->interpCached(remapped));
    }
    }
}

// ── Keyframe access functions (AE-compatible) ────────────────────────
// key(i), nearestKey(t), valueAtTime(t), velocityAtTime(t), speedAtTime(t)
// numKeys is a global property set by setKeyframes()/clearKeyframes().

// Helper: create a {time, index, value} JS object (AE key object)
static JSValue makeKeyObj(JSContext *ctx, double t, double v, int32_t aeIndex)
{
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "time", JS_NewFloat64(ctx, t));
    JS_SetPropertyStr(ctx, obj, "index", JS_NewInt32(ctx, aeIndex));
    JS_SetPropertyStr(ctx, obj, "value", JS_NewFloat64(ctx, v));
    return obj;
}

// key(index) — 1-based keyframe lookup (C++ cache, no JS array traversal)
static JSValue js_key(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "key() requires 1 argument (1-based index)");

    int32_t aeIndex = getInt(ctx, argv[0]);
    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));
    const auto &kfs = engine->keyframeCache();

    if (kfs.isEmpty()) {
        return JS_ThrowRangeError(ctx, "key(): no keyframes available");
    }
    if (aeIndex < 1 || aeIndex > kfs.size()) {
        return JS_ThrowRangeError(ctx, "key(): index out of range");
    }

    const auto &kf = kfs[aeIndex - 1];
    return makeKeyObj(ctx, kf.t, kf.v, aeIndex);
}

// nearestKey(t) — nearest keyframe by |kf.t - t|
static JSValue js_nearestKey(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "nearestKey() requires 1 argument (time in seconds)");

    double t = getDouble(ctx, argv[0]);
    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));
    const auto &kfs = engine->keyframeCache();

    if (kfs.isEmpty()) {
        return JS_ThrowRangeError(ctx, "nearestKey(): no keyframes available");
    }

    int bestIdx = 0;
    double bestDist = std::fabs(kfs[0].t - t);
    for (int i = 1; i < kfs.size(); i++) {
        double dist = std::fabs(kfs[i].t - t);
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }

    return makeKeyObj(ctx, kfs[bestIdx].t, kfs[bestIdx].v, bestIdx + 1);
}

// valueAtTime(t) — linear interpolation via C++ interpCached(); falls back to value
static JSValue js_valueAtTime(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "valueAtTime() requires 1 argument (time in seconds)");

    double t = getDouble(ctx, argv[0]);
    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));

    if (engine->keyframeCache().isEmpty()) {
        return JS_NewFloat64(ctx, engine->cachedValue());
    }

    return JS_NewFloat64(ctx, engine->interpCached(t));
}

// velocityAtTime(t) — central difference via C++ velocityCached()
static JSValue js_velocityAtTime(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "velocityAtTime() requires 1 argument (time in seconds)");

    double t = getDouble(ctx, argv[0]);
    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));

    if (engine->keyframeCache().size() < 2) {
        return JS_NewFloat64(ctx, 0.0);
    }

    return JS_NewFloat64(ctx, engine->velocityCached(t));
}

// speedAtTime(t) — absolute value of velocity
static JSValue js_speedAtTime(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "speedAtTime() requires 1 argument (time in seconds)");

    double t = getDouble(ctx, argv[0]);
    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));

    if (engine->keyframeCache().size() < 2) {
        return JS_NewFloat64(ctx, 0.0);
    }

    return JS_NewFloat64(ctx, std::fabs(engine->velocityCached(t)));
}

void registerKeyframeFunctions(JSContext *ctx)
{
    JSValue global = JS_GetGlobalObject(ctx);

    // Register functions
    JS_SetPropertyStr(ctx, global, "key", JS_NewCFunction(ctx, js_key, "key", 1));
    JS_SetPropertyStr(ctx, global, "nearestKey", JS_NewCFunction(ctx, js_nearestKey, "nearestKey", 1));
    JS_SetPropertyStr(ctx, global, "valueAtTime", JS_NewCFunction(ctx, js_valueAtTime, "valueAtTime", 1));
    JS_SetPropertyStr(ctx, global, "velocityAtTime", JS_NewCFunction(ctx, js_velocityAtTime, "velocityAtTime", 1));
    JS_SetPropertyStr(ctx, global, "speedAtTime", JS_NewCFunction(ctx, js_speedAtTime, "speedAtTime", 1));

    // Initialize numKeys property to 0
    JS_SetPropertyStr(ctx, global, "numKeys", JS_NewInt32(ctx, 0));

    JS_FreeValue(ctx, global);
}

// ── Color conversion functions (AE-compatible) ───────────────────────
// All values normalized 0-1. AE convention: H in [0,1] (not 0-360).

// rgbToHsl([r, g, b]) or rgbToHsl([r, g, b, a]) → [h, s, l] or [h, s, l, a]
static JSValue js_rgbToHsl(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "rgbToHsl() requires 1 argument (array)");

    std::vector<double> rgb = getArray(ctx, argv[0]);
    if (rgb.size() < 3) {
        return JS_ThrowTypeError(ctx, "rgbToHsl() requires an array of at least 3 elements [r,g,b]");
    }

    double r = rgb[0], g = rgb[1], b = rgb[2];
    double a = (rgb.size() >= 4) ? rgb[3] : 1.0;

    double cMax = std::max({r, g, b});
    double cMin = std::min({r, g, b});
    double delta = cMax - cMin;

    double l = (cMax + cMin) / 2.0;

    double h = 0.0;
    double s = 0.0;

    if (delta > 1e-12) {
        s = (l > 0.5) ? delta / (2.0 - cMax - cMin) : delta / (cMax + cMin);

        if (cMax == r) {
            h = std::fmod((g - b) / delta, 6.0);
            if (h < 0.0) h += 6.0;
        } else if (cMax == g) {
            h = (b - r) / delta + 2.0;
        } else {
            h = (r - g) / delta + 4.0;
        }
        h /= 6.0; // Normalize to 0-1
    }

    std::vector<double> result = {h, s, l};
    if (rgb.size() >= 4) result.push_back(a);
    return makeJSArray(ctx, result);
}

// hslToRgb([h, s, l]) or hslToRgb([h, s, l, a]) → [r, g, b] or [r, g, b, a]
static JSValue js_hslToRgb(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "hslToRgb() requires 1 argument (array)");

    std::vector<double> hsl = getArray(ctx, argv[0]);
    if (hsl.size() < 3) {
        return JS_ThrowTypeError(ctx, "hslToRgb() requires an array of at least 3 elements [h,s,l]");
    }

    double h = hsl[0], s = hsl[1], l = hsl[2];
    double a = (hsl.size() >= 4) ? hsl[3] : 1.0;

    double r, g, b;

    if (s < 1e-12) {
        // Achromatic
        r = g = b = l;
    } else {
        // Hue to RGB helper
        auto hue2rgb = [](double p, double q, double t) -> double {
            if (t < 0.0) t += 1.0;
            if (t > 1.0) t -= 1.0;
            if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
            if (t < 1.0 / 2.0) return q;
            if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
            return p;
        };

        double q = (l < 0.5) ? l * (1.0 + s) : l + s - l * s;
        double p = 2.0 * l - q;

        r = hue2rgb(p, q, h + 1.0 / 3.0);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1.0 / 3.0);
    }

    std::vector<double> result = {r, g, b};
    if (hsl.size() >= 4) result.push_back(a);
    return makeJSArray(ctx, result);
}

// hexToRgb("#rrggbb") or hexToRgb("#rgb") → [r, g, b] (normalized 0-1)
static JSValue js_hexToRgb(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "hexToRgb() requires 1 argument (string)");

    const char *hex = JS_ToCString(ctx, argv[0]);
    if (!hex) return JS_ThrowTypeError(ctx, "hexToRgb() argument must be a string");

    const char *p = hex;
    if (*p == '#') p++;

    size_t len = std::strlen(p);

    double r = 0.0, g = 0.0, b = 0.0;

    auto hexDigit = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };

    if (len == 6 || len == 8) {
        // #rrggbb or #rrggbbaa
        r = static_cast<double>(hexDigit(p[0]) * 16 + hexDigit(p[1])) / 255.0;
        g = static_cast<double>(hexDigit(p[2]) * 16 + hexDigit(p[3])) / 255.0;
        b = static_cast<double>(hexDigit(p[4]) * 16 + hexDigit(p[5])) / 255.0;
        if (len == 8) {
            double a = static_cast<double>(hexDigit(p[6]) * 16 + hexDigit(p[7])) / 255.0;
            JS_FreeCString(ctx, hex);
            return makeJSArray(ctx, {r, g, b, a});
        }
    } else if (len == 3 || len == 4) {
        // #rgb or #rgba (each digit doubled: #f00 = #ff0000)
        r = static_cast<double>(hexDigit(p[0]) * 16 + hexDigit(p[0])) / 255.0;
        g = static_cast<double>(hexDigit(p[1]) * 16 + hexDigit(p[1])) / 255.0;
        b = static_cast<double>(hexDigit(p[2]) * 16 + hexDigit(p[2])) / 255.0;
        if (len == 4) {
            double a = static_cast<double>(hexDigit(p[3]) * 16 + hexDigit(p[3])) / 255.0;
            JS_FreeCString(ctx, hex);
            return makeJSArray(ctx, {r, g, b, a});
        }
    } else {
        JS_FreeCString(ctx, hex);
        return JS_ThrowTypeError(ctx, "hexToRgb() invalid hex format (use #rgb, #rrggbb, #rgba, or #rrggbbaa)");
    }

    JS_FreeCString(ctx, hex);
    return makeJSArray(ctx, {r, g, b});
}

// ── Time conversion functions (AE-compatible) ────────────────────────

// framesToTime(frames) → seconds
static JSValue js_framesToTime(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "framesToTime() requires 1 argument");
    double frames = getDouble(ctx, argv[0]);
    // Read fps from C++ cache (Issue #9: avoids JS global read)
    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));
    double fps = engine->cachedFps();
    if (fps <= 0.0) fps = 25.0;
    return JS_NewFloat64(ctx, frames / fps);
}

// timeToFrames(seconds) → frames
static JSValue js_timeToFrames(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "timeToFrames() requires 1 argument");
    double t = getDouble(ctx, argv[0]);
    // Read fps from C++ cache (Issue #9: avoids JS global read)
    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));
    double fps = engine->cachedFps();
    if (fps <= 0.0) fps = 25.0;
    return JS_NewFloat64(ctx, std::round(t * fps));
}

// timeToTimecode(t, timecodeBase, isDuration) → "HH:MM:SS:FF"
// AE-compatible: timecodeBase defaults to 30, isDuration defaults to false
// Absolute times are floored; durations are rounded away from zero.
static JSValue js_timeToTimecode(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    // Get time — default to current composition time
    double t;
    if (argc >= 1) {
        t = getDouble(ctx, argv[0]);
    } else {
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue gTime = JS_GetPropertyStr(ctx, global, "time");
        t = getDouble(ctx, gTime);
        JS_FreeValue(ctx, gTime);
        JS_FreeValue(ctx, global);
    }

    int32_t base = (argc >= 2) ? getInt(ctx, argv[1]) : 30;
    bool isDuration = false;
    if (argc >= 3) {
        isDuration = JS_ToBool(ctx, argv[2]) != 0;
    }
    if (base <= 0) base = 30;

    bool negative = (t < 0.0);
    double absT = std::fabs(t);

    int totalFrames;
    if (isDuration) {
        // Durations round away from zero
        totalFrames = static_cast<int>(std::ceil(absT * base));
    } else {
        // Absolute times floor toward negative infinity
        totalFrames = static_cast<int>(std::floor(absT * base));
    }

    int ff = totalFrames % base;
    int totalSec = totalFrames / base;
    int ss = totalSec % 60;
    int totalMin = totalSec / 60;
    int mm = totalMin % 60;
    int hh = totalMin / 60;

    char buf[32];
    if (negative) {
        std::snprintf(buf, sizeof(buf), "-%02d:%02d:%02d:%02d", hh, mm, ss, ff);
    } else {
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d:%02d", hh, mm, ss, ff);
    }
    return JS_NewString(ctx, buf);
}

// timeToCurrentFormat(t, fps, isDuration) → timecode string in project format
// AE-compatible: defaults to project fps (not timecodeBase 30 like timeToTimecode).
// Uses the actual project frame rate for frame counting.
static JSValue js_timeToCurrentFormat(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    // Get project fps from thisProject.fps
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue proj = JS_GetPropertyStr(ctx, global, "thisProject");
    JSValue projFps = JS_GetPropertyStr(ctx, proj, "fps");
    JSValue projDst = JS_GetPropertyStr(ctx, proj, "displayStartTime");
    double defaultFps = getDouble(ctx, projFps);
    double displayStartTime = getDouble(ctx, projDst);
    JS_FreeValue(ctx, projDst);
    JS_FreeValue(ctx, projFps);
    JS_FreeValue(ctx, proj);

    // Get time — default to time + displayStartTime (AE behavior)
    double t;
    if (argc >= 1) {
        t = getDouble(ctx, argv[0]);
    } else {
        JSValue gTime = JS_GetPropertyStr(ctx, global, "time");
        t = getDouble(ctx, gTime) + displayStartTime;
        JS_FreeValue(ctx, gTime);
    }
    JS_FreeValue(ctx, global);

    double fps = (argc >= 2) ? getDouble(ctx, argv[1]) : defaultFps;
    bool isDuration = false;
    if (argc >= 3) {
        isDuration = JS_ToBool(ctx, argv[2]) != 0;
    }
    if (fps <= 0.0) fps = 25.0;

    bool negative = (t < 0.0);
    double absT = std::fabs(t);

    int totalFrames;
    if (isDuration) {
        totalFrames = static_cast<int>(std::ceil(absT * fps));
    } else {
        totalFrames = static_cast<int>(std::floor(absT * fps));
    }

    int fpsInt = static_cast<int>(std::round(fps));
    if (fpsInt <= 0) fpsInt = 25;
    int ff = totalFrames % fpsInt;
    int totalSec = totalFrames / fpsInt;
    int ss = totalSec % 60;
    int totalMin = totalSec / 60;
    int mm = totalMin % 60;
    int hh = totalMin / 60;

    char buf[32];
    if (negative) {
        std::snprintf(buf, sizeof(buf), "-%02d:%02d:%02d:%02d", hh, mm, ss, ff);
    } else {
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d:%02d", hh, mm, ss, ff);
    }
    return JS_NewString(ctx, buf);
}

// ── Marker functions (AE-compatible) ─────────────────────────────────
// Data stored as globalThis._markers = [{t, comment, duration}, ...]
// marker object has: .numKeys, .key(i), .nearestKey(t)
// Each returned key object has: .time, .index, .comment, .duration

// Helper: create a JS marker key object from _markers[arrayIdx] (0-based internal)
// AE uses 1-based index for marker keys
static JSValue makeMarkerKeyObj(JSContext *ctx, JSValueConst markersArr, int32_t arrayIdx, int32_t aeIndex)
{
    JSValue elem = JS_GetPropertyUint32(ctx, markersArr, static_cast<uint32_t>(arrayIdx));
    if (JS_IsUndefined(elem)) return JS_UNDEFINED;

    JSValue tVal = JS_GetPropertyStr(ctx, elem, "t");
    JSValue commentVal = JS_GetPropertyStr(ctx, elem, "comment");
    JSValue durVal = JS_GetPropertyStr(ctx, elem, "duration");

    JSValue keyObj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, keyObj, "time", JS_DupValue(ctx, tVal));
    JS_SetPropertyStr(ctx, keyObj, "index", JS_NewInt32(ctx, aeIndex));
    JS_SetPropertyStr(ctx, keyObj, "comment", JS_DupValue(ctx, commentVal));
    JS_SetPropertyStr(ctx, keyObj, "duration", JS_DupValue(ctx, durVal));
    // AE compatibility: protectedRegion is the same as duration
    JS_SetPropertyStr(ctx, keyObj, "protectedRegion", JS_DupValue(ctx, durVal));

    JS_FreeValue(ctx, durVal);
    JS_FreeValue(ctx, commentVal);
    JS_FreeValue(ctx, tVal);
    JS_FreeValue(ctx, elem);
    return keyObj;
}

// marker.key(i) — returns marker by 1-based index
static JSValue js_marker_key(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "marker.key() requires 1 argument (1-based index)");

    int32_t aeIndex = getInt(ctx, argv[0]); // 1-based

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue markersArr = JS_GetPropertyStr(ctx, global, "_markers");

    if (JS_IsUndefined(markersArr) || JS_IsNull(markersArr)) {
        JS_FreeValue(ctx, markersArr);
        JS_FreeValue(ctx, global);
        return JS_ThrowRangeError(ctx, "marker.key(): no markers available");
    }

    JSValue lenVal = JS_GetPropertyStr(ctx, markersArr, "length");
    int32_t len = getInt(ctx, lenVal);
    JS_FreeValue(ctx, lenVal);

    if (aeIndex < 1 || aeIndex > len) {
        JS_FreeValue(ctx, markersArr);
        JS_FreeValue(ctx, global);
        return JS_ThrowRangeError(ctx, "marker.key(): index out of range");
    }

    JSValue result = makeMarkerKeyObj(ctx, markersArr, aeIndex - 1, aeIndex);
    JS_FreeValue(ctx, markersArr);
    JS_FreeValue(ctx, global);
    return result;
}

// marker.nearestKey(t) — returns marker nearest to time t (seconds)
static JSValue js_marker_nearestKey(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "marker.nearestKey() requires 1 argument (time in seconds)");

    double t = getDouble(ctx, argv[0]);

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue markersArr = JS_GetPropertyStr(ctx, global, "_markers");

    if (JS_IsUndefined(markersArr) || JS_IsNull(markersArr)) {
        JS_FreeValue(ctx, markersArr);
        JS_FreeValue(ctx, global);
        return JS_ThrowRangeError(ctx, "marker.nearestKey(): no markers available");
    }

    JSValue lenVal = JS_GetPropertyStr(ctx, markersArr, "length");
    int32_t len = getInt(ctx, lenVal);
    JS_FreeValue(ctx, lenVal);

    if (len == 0) {
        JS_FreeValue(ctx, markersArr);
        JS_FreeValue(ctx, global);
        return JS_ThrowRangeError(ctx, "marker.nearestKey(): no markers available");
    }

    // Find nearest marker by time
    int32_t bestIdx = 0;
    double bestDist = 1e30;
    for (int32_t i = 0; i < len; i++) {
        JSValue elem = JS_GetPropertyUint32(ctx, markersArr, static_cast<uint32_t>(i));
        JSValue tVal = JS_GetPropertyStr(ctx, elem, "t");
        double markerTime = getDouble(ctx, tVal);
        JS_FreeValue(ctx, tVal);
        JS_FreeValue(ctx, elem);

        double dist = std::fabs(markerTime - t);
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }

    JSValue result = makeMarkerKeyObj(ctx, markersArr, bestIdx, bestIdx + 1); // 1-based index
    JS_FreeValue(ctx, markersArr);
    JS_FreeValue(ctx, global);
    return result;
}

// ── Image sampling ───────────────────────────────────────────────────
// sampleImage(x, y, radius) — sample source clip pixel color
// Returns [r, g, b, a] normalized 0.0-1.0

static JSValue js_sampleImage(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 2) return JS_ThrowTypeError(ctx, "sampleImage() requires at least 2 arguments (x, y)");

    double x = getDouble(ctx, argv[0]);
    double y = getDouble(ctx, argv[1]);
    double radius = (argc >= 3) ? getDouble(ctx, argv[2]) : 0.0;

    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));
    if (!engine) {
        // Return [0, 0, 0, 0]
        JSValue arr = JS_NewArray(ctx);
        for (int i = 0; i < 4; i++) {
            JS_SetPropertyUint32(ctx, arr, static_cast<uint32_t>(i), JS_NewFloat64(ctx, 0.0));
        }
        return arr;
    }

    // Read frame from C++ cache (Issue #8: avoids JS global read)
    int frame = engine->cachedFrame();

    auto rgba = engine->sampleImage(frame, x, y, radius);

    // Return [r, g, b, a] as JS array
    JSValue arr = JS_NewArray(ctx);
    for (int i = 0; i < 4; i++) {
        JS_SetPropertyUint32(ctx, arr, static_cast<uint32_t>(i), JS_NewFloat64(ctx, rgba[static_cast<size_t>(i)]));
    }
    return arr;
}

// ── Coordinate conversion (AE-compatible) ────────────────────────────
// In AE, toComp/fromComp convert between layer and composition space.
// In Kdenlive, we provide normalized [0-1] ↔ pixel coordinate conversion
// using thisProject.width/height.

// Helper: read project dimensions from thisProject
static void getProjectDimensions(JSContext *ctx, double &w, double &h)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue proj = JS_GetPropertyStr(ctx, global, "thisProject");
    JSValue wVal = JS_GetPropertyStr(ctx, proj, "width");
    JSValue hVal = JS_GetPropertyStr(ctx, proj, "height");
    w = getDouble(ctx, wVal);
    h = getDouble(ctx, hVal);
    JS_FreeValue(ctx, hVal);
    JS_FreeValue(ctx, wVal);
    JS_FreeValue(ctx, proj);
    JS_FreeValue(ctx, global);
    // Fallback to avoid division by zero
    if (w <= 0.0) w = 1920.0;
    if (h <= 0.0) h = 1080.0;
}

// toComp(point) — convert normalized [0-1] coordinates to pixel coordinates
// Polymorphic: [x,y] → [x*w, y*h], or scalar → scalar*w
static JSValue js_toComp(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "toComp() requires 1 argument");

    double w, h;
    getProjectDimensions(ctx, w, h);

    std::vector<double> arr = getArray(ctx, argv[0]);
    if (!arr.empty()) {
        std::vector<double> result;
        result.push_back(arr[0] * w);
        if (arr.size() >= 2) result.push_back(arr[1] * h);
        for (size_t i = 2; i < arr.size(); i++) {
            result.push_back(arr[i]); // pass through additional components
        }
        return makeJSArray(ctx, result);
    }

    // Scalar: treat as x-coordinate
    return JS_NewFloat64(ctx, getDouble(ctx, argv[0]) * w);
}

// fromComp(point) — convert pixel coordinates to normalized [0-1]
// Polymorphic: [x,y] → [x/w, y/h], or scalar → scalar/w
static JSValue js_fromComp(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "fromComp() requires 1 argument");

    double w, h;
    getProjectDimensions(ctx, w, h);

    std::vector<double> arr = getArray(ctx, argv[0]);
    if (!arr.empty()) {
        std::vector<double> result;
        result.push_back(arr[0] / w);
        if (arr.size() >= 2) result.push_back(arr[1] / h);
        for (size_t i = 2; i < arr.size(); i++) {
            result.push_back(arr[i]); // pass through additional components
        }
        return makeJSArray(ctx, result);
    }

    // Scalar: treat as x-coordinate
    return JS_NewFloat64(ctx, getDouble(ctx, argv[0]) / w);
}

// sourceRectAtTime(t, includeExtents) → {top, left, width, height}
// Returns the clip's source rectangle in pixel coordinates.
// In Kdenlive: uses thisClip.width/height. Position is always (0,0)
// since we don't have per-layer transforms like AE.
// The t parameter is accepted for AE compatibility but currently ignored
// (clip dimensions don't change over time in Kdenlive).
static JSValue js_sourceRectAtTime(JSContext *ctx, JSValueConst /*this_val*/, int /*argc*/, JSValueConst * /*argv*/)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue thisClip = JS_GetPropertyStr(ctx, global, "thisClip");
    JSValue wVal = JS_GetPropertyStr(ctx, thisClip, "width");
    JSValue hVal = JS_GetPropertyStr(ctx, thisClip, "height");
    double w = getDouble(ctx, wVal);
    double h = getDouble(ctx, hVal);
    JS_FreeValue(ctx, hVal);
    JS_FreeValue(ctx, wVal);
    JS_FreeValue(ctx, thisClip);
    JS_FreeValue(ctx, global);

    JSValue rect = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, rect, "top", JS_NewFloat64(ctx, 0.0));
    JS_SetPropertyStr(ctx, rect, "left", JS_NewFloat64(ctx, 0.0));
    JS_SetPropertyStr(ctx, rect, "width", JS_NewFloat64(ctx, w));
    JS_SetPropertyStr(ctx, rect, "height", JS_NewFloat64(ctx, h));
    return rect;
}

// ── thisProperty object (AE-compatible) ──────────────────────────────
// thisProperty represents the property containing the expression.
// Methods delegate to existing global functions with proper context.
// AE reference: thisProperty.value, .velocity, .speed, .wiggle(), .smooth(),
//               .valueAtTime(), .velocityAtTime(), .speedAtTime(),
//               .numKeys, .key(), .nearestKey()

// thisProperty.wiggle(freq, amp, octaves, ampMult, t)
// Delegates to global wiggle() — identical behavior.
static JSValue js_thisProp_wiggle(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    // Forward directly to the global wiggle function
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue wiggleFn = JS_GetPropertyStr(ctx, global, "wiggle");
    JSValue result = JS_Call(ctx, wiggleFn, global, argc, argv);
    JS_FreeValue(ctx, wiggleFn);
    JS_FreeValue(ctx, global);
    return result;
}

// thisProperty.temporalWiggle(freq, amp, octaves, ampMult, t)
static JSValue js_thisProp_temporalWiggle(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, "temporalWiggle");
    JSValue result = JS_Call(ctx, fn, global, argc, argv);
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global);
    return result;
}

// thisProperty.smooth(width, samples)
static JSValue js_thisProp_smooth(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, "smooth");
    JSValue result = JS_Call(ctx, fn, global, argc, argv);
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global);
    return result;
}

// thisProperty.valueAtTime(t)
static JSValue js_thisProp_valueAtTime(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, "valueAtTime");
    JSValue result = JS_Call(ctx, fn, global, argc, argv);
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global);
    return result;
}

// thisProperty.velocityAtTime(t)
static JSValue js_thisProp_velocityAtTime(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, "velocityAtTime");
    JSValue result = JS_Call(ctx, fn, global, argc, argv);
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global);
    return result;
}

// thisProperty.speedAtTime(t)
static JSValue js_thisProp_speedAtTime(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, "speedAtTime");
    JSValue result = JS_Call(ctx, fn, global, argc, argv);
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global);
    return result;
}

// thisProperty.loopIn(type, nKeys)
static JSValue js_thisProp_loopIn(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, "loopIn");
    JSValue result = JS_Call(ctx, fn, global, argc, argv);
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global);
    return result;
}

// thisProperty.loopOut(type, nKeys)
static JSValue js_thisProp_loopOut(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, "loopOut");
    JSValue result = JS_Call(ctx, fn, global, argc, argv);
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global);
    return result;
}

// thisProperty.loopInDuration(type, dur)
static JSValue js_thisProp_loopInDuration(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, "loopInDuration");
    JSValue result = JS_Call(ctx, fn, global, argc, argv);
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global);
    return result;
}

// thisProperty.loopOutDuration(type, dur)
static JSValue js_thisProp_loopOutDuration(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, "loopOutDuration");
    JSValue result = JS_Call(ctx, fn, global, argc, argv);
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global);
    return result;
}

// thisProperty.key(index) — 1-based
static JSValue js_thisProp_key(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, "key");
    JSValue result = JS_Call(ctx, fn, global, argc, argv);
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global);
    return result;
}

// thisProperty.nearestKey(t)
static JSValue js_thisProp_nearestKey(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, "nearestKey");
    JSValue result = JS_Call(ctx, fn, global, argc, argv);
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global);
    return result;
}

// ── Cross-clip reference functions ───────────────────────────────────
// clip("name") → object with .effect("name") → object with .param("name") → double

// Forward declarations
static JSValue js_effectRef_param(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_clipRef_effect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

// effectRef.param("paramName") — resolves the value via C++ callback
// Supports both name-based and index-based clip resolution.
// _clipIndex >= 0 → index-based; _clipIndex < 0 or absent → name-based.
static JSValue js_effectRef_param(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "param() requires 1 argument");

    const char *paramName = JS_ToCString(ctx, argv[0]);
    if (!paramName) return JS_ThrowTypeError(ctx, "param() argument must be a string");

    JSValue jsClipIndex = JS_GetPropertyStr(ctx, this_val, "_clipIndex");
    JSValue jsClipName = JS_GetPropertyStr(ctx, this_val, "_clipName");
    JSValue jsEffectName = JS_GetPropertyStr(ctx, this_val, "_effectName");
    const char *effectName = JS_ToCString(ctx, jsEffectName);

    double result = 0.0;

    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));
    if (engine && effectName) {
        int32_t clipIndex = -1;
        if (!JS_IsUndefined(jsClipIndex)) {
            JS_ToInt32(ctx, &clipIndex, jsClipIndex);
        }

        if (clipIndex >= 0) {
            // Index-based: clip(N).effect(name).param(name)
            result = engine->resolveClipByIndex(clipIndex, QString::fromUtf8(effectName), QString::fromUtf8(paramName));
        } else {
            // Name-based: clip("name").effect(name).param(name)
            const char *clipName = JS_ToCString(ctx, jsClipName);
            if (clipName) {
                result = engine->resolveClipParam(QString::fromUtf8(clipName), QString::fromUtf8(effectName), QString::fromUtf8(paramName));
                JS_FreeCString(ctx, clipName);
            }
        }
    }

    JS_FreeCString(ctx, paramName);
    JS_FreeCString(ctx, effectName);
    JS_FreeValue(ctx, jsClipIndex);
    JS_FreeValue(ctx, jsClipName);
    JS_FreeValue(ctx, jsEffectName);

    return JS_NewFloat64(ctx, result);
}

// clipRef.effect("effectName") — returns an object with .param() method
// Propagates both _clipName and _clipIndex from the parent clip reference.
static JSValue js_clipRef_effect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "effect() requires 1 argument");

    const char *effectName = JS_ToCString(ctx, argv[0]);
    if (!effectName) return JS_ThrowTypeError(ctx, "effect() argument must be a string");

    // Propagate clip reference (both name and index) from parent
    JSValue jsClipName = JS_GetPropertyStr(ctx, this_val, "_clipName");
    JSValue jsClipIndex = JS_GetPropertyStr(ctx, this_val, "_clipIndex");

    JSValue effectRef = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, effectRef, "_clipName", JS_DupValue(ctx, jsClipName));
    JS_SetPropertyStr(ctx, effectRef, "_clipIndex", JS_DupValue(ctx, jsClipIndex));
    JS_SetPropertyStr(ctx, effectRef, "_effectName", JS_NewString(ctx, effectName));
    JS_SetPropertyStr(ctx, effectRef, "param", JS_NewCFunction(ctx, js_effectRef_param, "param", 1));

    JS_FreeValue(ctx, jsClipIndex);
    JS_FreeValue(ctx, jsClipName);
    JS_FreeCString(ctx, effectName);
    return effectRef;
}

// Helper: populate a clip reference object with metadata properties (AE Layer attributes).
// Tries to resolve metadata via the engine's ClipMetadataResolver.
// Sets: .name, .index, .inPoint, .outPoint, .startTime, .duration, .width, .height,
//       .hasVideo, .hasAudio, .source (sub-object with .name, .width, .height)
static void populateClipRefMetadata(JSContext *ctx, JSValueConst clipRef, int32_t clipIndex)
{
    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));
    if (!engine || clipIndex < 0) return;

    auto meta = engine->resolveClipMetadata(clipIndex);
    if (!meta.valid) return;

    QByteArray nameUtf8 = meta.name.toUtf8();
    JS_SetPropertyStr(ctx, clipRef, "name", JS_NewString(ctx, nameUtf8.constData()));
    JS_SetPropertyStr(ctx, clipRef, "index", JS_NewInt32(ctx, meta.index));
    JS_SetPropertyStr(ctx, clipRef, "inPoint", JS_NewFloat64(ctx, meta.inPoint));
    JS_SetPropertyStr(ctx, clipRef, "outPoint", JS_NewFloat64(ctx, meta.outPoint));
    JS_SetPropertyStr(ctx, clipRef, "startTime", JS_NewFloat64(ctx, meta.startTime));
    JS_SetPropertyStr(ctx, clipRef, "duration", JS_NewFloat64(ctx, meta.duration));
    JS_SetPropertyStr(ctx, clipRef, "width", JS_NewInt32(ctx, meta.width));
    JS_SetPropertyStr(ctx, clipRef, "height", JS_NewInt32(ctx, meta.height));
    JS_SetPropertyStr(ctx, clipRef, "hasVideo", JS_NewBool(ctx, meta.hasVideo));
    JS_SetPropertyStr(ctx, clipRef, "hasAudio", JS_NewBool(ctx, meta.hasAudio));

    // AE compatibility: source sub-object (layer.source.name, etc.)
    JSValue source = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, source, "name", JS_NewString(ctx, nameUtf8.constData()));
    JS_SetPropertyStr(ctx, source, "width", JS_NewInt32(ctx, meta.width));
    JS_SetPropertyStr(ctx, source, "height", JS_NewInt32(ctx, meta.height));
    JS_SetPropertyStr(ctx, clipRef, "source", source);
}

// clip("clipName") or clip(index) or clip(clipRef, relIndex) — returns clip reference
// String argument: lookup by clip name (cross-track search)
// Number argument: lookup by 0-based position index on the same track
// Two arguments (object, number): relative reference — AE layer(otherLayer, relIndex)
//   e.g. clip(thisClip, 1) = next clip, clip(thisClip, -1) = previous clip
static JSValue js_clip(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "clip() requires at least 1 argument");

    // Two-argument form: clip(referenceObj, relativeIndex)
    // AE: thisComp.layer(thisLayer, relIndex)
    // referenceObj can be thisClip (has .name property) or a clip ref (has _clipIndex)
    if (argc >= 2 && JS_IsObject(argv[0]) && JS_IsNumber(argv[1])) {
        int32_t relIndex = 0;
        JS_ToInt32(ctx, &relIndex, argv[1]);

        // Check if the reference object has _clipIndex (it's a clip ref from clip())
        JSValue refClipIndex = JS_GetPropertyStr(ctx, argv[0], "_clipIndex");
        if (!JS_IsUndefined(refClipIndex)) {
            int32_t baseIndex = 0;
            JS_ToInt32(ctx, &baseIndex, refClipIndex);
            JS_FreeValue(ctx, refClipIndex);

            int32_t targetIndex = baseIndex + relIndex;

            JSValue clipRef = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, clipRef, "_clipIndex", JS_NewInt32(ctx, targetIndex));
            JS_SetPropertyStr(ctx, clipRef, "_clipName", JS_NewString(ctx, ""));
            JS_SetPropertyStr(ctx, clipRef, "effect", JS_NewCFunction(ctx, js_clipRef_effect, "effect", 1));
            populateClipRefMetadata(ctx, clipRef, targetIndex);
            return clipRef;
        }
        JS_FreeValue(ctx, refClipIndex);

        // It's thisClip or similar object — use the global index as base
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue gIndex = JS_GetPropertyStr(ctx, global, "index");
        int32_t currentIndex = getInt(ctx, gIndex);
        JS_FreeValue(ctx, gIndex);
        JS_FreeValue(ctx, global);

        int32_t targetIndex = currentIndex + relIndex;

        JSValue clipRef = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, clipRef, "_clipIndex", JS_NewInt32(ctx, targetIndex));
        JS_SetPropertyStr(ctx, clipRef, "_clipName", JS_NewString(ctx, ""));
        JS_SetPropertyStr(ctx, clipRef, "effect", JS_NewCFunction(ctx, js_clipRef_effect, "effect", 1));
        populateClipRefMetadata(ctx, clipRef, targetIndex);
        return clipRef;
    }

    if (JS_IsNumber(argv[0])) {
        int32_t clipIndex = 0;
        JS_ToInt32(ctx, &clipIndex, argv[0]);

        JSValue clipRef = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, clipRef, "_clipIndex", JS_NewInt32(ctx, clipIndex));
        JS_SetPropertyStr(ctx, clipRef, "_clipName", JS_NewString(ctx, ""));
        JS_SetPropertyStr(ctx, clipRef, "effect", JS_NewCFunction(ctx, js_clipRef_effect, "effect", 1));
        populateClipRefMetadata(ctx, clipRef, clipIndex);
        return clipRef;
    }

    const char *clipName = JS_ToCString(ctx, argv[0]);
    if (!clipName) return JS_ThrowTypeError(ctx, "clip() argument must be a string, number, or (clipRef, relIndex)");

    JSValue clipRef = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, clipRef, "_clipIndex", JS_NewInt32(ctx, -1));
    JS_SetPropertyStr(ctx, clipRef, "_clipName", JS_NewString(ctx, clipName));
    JS_SetPropertyStr(ctx, clipRef, "effect", JS_NewCFunction(ctx, js_clipRef_effect, "effect", 1));

    JS_FreeCString(ctx, clipName);
    return clipRef;
}

// ── thisEffect.param("name") — same-effect parameter reference ───────

static JSValue js_thisEffect_param(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "thisEffect.param() requires 1 argument");

    const char *paramName = JS_ToCString(ctx, argv[0]);
    if (!paramName) return JS_ThrowTypeError(ctx, "thisEffect.param() argument must be a string");

    double result = 0.0;
    auto *engine = static_cast<ExpressionEngine *>(JS_GetContextOpaque(ctx));
    if (engine) {
        result = engine->resolveEffectParam(QString::fromUtf8(paramName));
    }

    JS_FreeCString(ctx, paramName);
    return JS_NewFloat64(ctx, result);
}

// ── Registration ──────────────────────────────────────────────────────

static const JSCFunctionListEntry js_expression_funcs[] = {
    JS_CFUNC_DEF("linear", 5, js_linear),
    JS_CFUNC_DEF("ease", 5, js_ease),
    JS_CFUNC_DEF("easeIn", 5, js_easeIn),
    JS_CFUNC_DEF("easeOut", 5, js_easeOut),
    JS_CFUNC_DEF("clamp", 3, js_clamp),
    JS_CFUNC_DEF("wiggle", 2, js_wiggle),
    JS_CFUNC_DEF("temporalWiggle", 2, js_temporalWiggle),
    JS_CFUNC_DEF("random", 0, js_random),
    JS_CFUNC_DEF("gaussRandom", 0, js_gaussRandom),
    JS_CFUNC_DEF("noise", 1, js_noise),
    JS_CFUNC_DEF("seedRandom", 2, js_seedRandom),
    JS_CFUNC_DEF("posterizeTime", 1, js_posterizeTime),
    JS_CFUNC_DEF("degreesToRadians", 1, js_degreesToRadians),
    JS_CFUNC_DEF("radiansToDegrees", 1, js_radiansToDegrees),
    JS_CFUNC_DEF("smooth", 2, js_smooth),
    JS_CFUNC_DEF("audioLevel", 2, js_audioLevel),
    JS_CFUNC_DEF("audioRms", 3, js_audioRms),
    // Looping (AE-compatible: cycle, pingpong, offset, continue)
    JS_CFUNC_DEF("loopIn", 2, js_loopIn),
    JS_CFUNC_DEF("loopOut", 2, js_loopOut),
    JS_CFUNC_DEF("loopInDuration", 2, js_loopInDuration),
    JS_CFUNC_DEF("loopOutDuration", 2, js_loopOutDuration),
    // Color conversion (AE-compatible)
    JS_CFUNC_DEF("rgbToHsl", 1, js_rgbToHsl),
    JS_CFUNC_DEF("hslToRgb", 1, js_hslToRgb),
    JS_CFUNC_DEF("hexToRgb", 1, js_hexToRgb),
    // Time conversion (AE-compatible)
    JS_CFUNC_DEF("framesToTime", 1, js_framesToTime),
    JS_CFUNC_DEF("timeToFrames", 1, js_timeToFrames),
    JS_CFUNC_DEF("timeToTimecode", 0, js_timeToTimecode),
    JS_CFUNC_DEF("timeToCurrentFormat", 0, js_timeToCurrentFormat),
    // Vector math (AE-compatible)
    JS_CFUNC_DEF("add", 2, js_add),
    JS_CFUNC_DEF("sub", 2, js_sub),
    JS_CFUNC_DEF("mul", 2, js_mul),
    JS_CFUNC_DEF("div", 2, js_div),
    JS_CFUNC_DEF("length", 1, js_length),
    JS_CFUNC_DEF("normalize", 1, js_normalize),
    JS_CFUNC_DEF("dot", 2, js_dot),
    JS_CFUNC_DEF("cross", 2, js_cross),
    JS_CFUNC_DEF("lookAt", 2, js_lookAt),
    // Coordinate conversion (AE-compatible)
    JS_CFUNC_DEF("toComp", 1, js_toComp),
    JS_CFUNC_DEF("fromComp", 1, js_fromComp),
    JS_CFUNC_DEF("sourceRectAtTime", 0, js_sourceRectAtTime),
    // Image sampling (Phase 3)
    JS_CFUNC_DEF("sampleImage", 3, js_sampleImage),
};

void registerExpressionFunctions(JSContext *ctx)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyFunctionList(ctx, global, js_expression_funcs, sizeof(js_expression_funcs) / sizeof(js_expression_funcs[0]));
    JS_FreeValue(ctx, global);
}

void registerClipReferenceFunctions(JSContext *ctx)
{
    JSValue global = JS_GetGlobalObject(ctx);

    // clip("name").effect("name").param("name") — cross-clip references
    JS_SetPropertyStr(ctx, global, "clip", JS_NewCFunction(ctx, js_clip, "clip", 1));
    // AE alias: layer() → clip() (same function)
    JS_SetPropertyStr(ctx, global, "layer", JS_NewCFunction(ctx, js_clip, "layer", 1));

    // thisEffect.param("name") — same-effect parameter reference
    JSValue thisEffect = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, thisEffect, "param", JS_NewCFunction(ctx, js_thisEffect_param, "param", 1));
    JS_SetPropertyStr(ctx, global, "thisEffect", thisEffect);

    // marker object — AE-compatible marker/guide access
    // marker.numKeys is updated by ExpressionEngine::setMarkers()
    JSValue markerObj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, markerObj, "numKeys", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, markerObj, "key", JS_NewCFunction(ctx, js_marker_key, "key", 1));
    JS_SetPropertyStr(ctx, markerObj, "nearestKey", JS_NewCFunction(ctx, js_marker_nearestKey, "nearestKey", 1));
    JS_SetPropertyStr(ctx, global, "marker", markerObj);

    // thisProperty — AE-compatible property reference object
    // Represents the property containing the expression. Methods delegate to
    // global functions. Properties (value, numKeys, velocity, speed) are kept
    // in sync via updateThisProperty() called from setContext().
    JSValue thisProp = JS_NewObject(ctx);
    // Dynamic properties — will be updated each frame by setContext()
    JS_SetPropertyStr(ctx, thisProp, "value", JS_NewFloat64(ctx, 0.0));
    JS_SetPropertyStr(ctx, thisProp, "numKeys", JS_NewInt32(ctx, 0));
    // Methods — delegate to existing global functions
    JS_SetPropertyStr(ctx, thisProp, "wiggle", JS_NewCFunction(ctx, js_thisProp_wiggle, "wiggle", 2));
    JS_SetPropertyStr(ctx, thisProp, "temporalWiggle", JS_NewCFunction(ctx, js_thisProp_temporalWiggle, "temporalWiggle", 2));
    JS_SetPropertyStr(ctx, thisProp, "smooth", JS_NewCFunction(ctx, js_thisProp_smooth, "smooth", 2));
    JS_SetPropertyStr(ctx, thisProp, "valueAtTime", JS_NewCFunction(ctx, js_thisProp_valueAtTime, "valueAtTime", 1));
    JS_SetPropertyStr(ctx, thisProp, "velocityAtTime", JS_NewCFunction(ctx, js_thisProp_velocityAtTime, "velocityAtTime", 1));
    JS_SetPropertyStr(ctx, thisProp, "speedAtTime", JS_NewCFunction(ctx, js_thisProp_speedAtTime, "speedAtTime", 1));
    JS_SetPropertyStr(ctx, thisProp, "loopIn", JS_NewCFunction(ctx, js_thisProp_loopIn, "loopIn", 2));
    JS_SetPropertyStr(ctx, thisProp, "loopOut", JS_NewCFunction(ctx, js_thisProp_loopOut, "loopOut", 2));
    JS_SetPropertyStr(ctx, thisProp, "loopInDuration", JS_NewCFunction(ctx, js_thisProp_loopInDuration, "loopInDuration", 2));
    JS_SetPropertyStr(ctx, thisProp, "loopOutDuration", JS_NewCFunction(ctx, js_thisProp_loopOutDuration, "loopOutDuration", 2));
    JS_SetPropertyStr(ctx, thisProp, "key", JS_NewCFunction(ctx, js_thisProp_key, "key", 1));
    JS_SetPropertyStr(ctx, thisProp, "nearestKey", JS_NewCFunction(ctx, js_thisProp_nearestKey, "nearestKey", 1));
    JS_SetPropertyStr(ctx, global, "thisProperty", thisProp);

    JS_FreeValue(ctx, global);
}

// ── Path functions (AE-compatible createPath) ─────────────────────────

// createPath(points, inTangents, outTangents, isClosed)
// Returns a JS object with _isPath sentinel for ExpressionEngine::evaluatePath()
// points: [[x,y], ...], tangents: [[dx,dy], ...] (offsets from point), empty = all linear
static JSValue js_createPath(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_ThrowTypeError(ctx, "createPath() requires at least 1 argument (points)");

    // Validate points is an array
    if (!JS_IsArray(ctx, argv[0])) {
        return JS_ThrowTypeError(ctx, "createPath() first argument must be an array of [x,y] points");
    }

    JSValue result = JS_NewObject(ctx);

    // Sentinel for evaluatePath() detection
    JS_SetPropertyStr(ctx, result, "_isPath", JS_TRUE);

    // Store points array directly
    JS_SetPropertyStr(ctx, result, "points", JS_DupValue(ctx, argv[0]));

    // Store inTangents (or empty array)
    if (argc >= 2 && JS_IsArray(ctx, argv[1])) {
        JS_SetPropertyStr(ctx, result, "inTangents", JS_DupValue(ctx, argv[1]));
    } else {
        JS_SetPropertyStr(ctx, result, "inTangents", JS_NewArray(ctx));
    }

    // Store outTangents (or empty array)
    if (argc >= 3 && JS_IsArray(ctx, argv[2])) {
        JS_SetPropertyStr(ctx, result, "outTangents", JS_DupValue(ctx, argv[2]));
    } else {
        JS_SetPropertyStr(ctx, result, "outTangents", JS_NewArray(ctx));
    }

    // Store isClosed (default true)
    if (argc >= 4) {
        JS_SetPropertyStr(ctx, result, "isClosed", JS_DupValue(ctx, argv[3]));
    } else {
        JS_SetPropertyStr(ctx, result, "isClosed", JS_TRUE);
    }

    return result;
}

void registerPathFunctions(JSContext *ctx)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "createPath", JS_NewCFunction(ctx, js_createPath, "createPath", 4));
    JS_FreeValue(ctx, global);
}
