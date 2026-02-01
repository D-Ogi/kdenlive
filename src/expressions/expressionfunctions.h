/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

struct JSContext;

/**
 * @brief Register all built-in expression functions into a QuickJS context.
 *
 * Functions registered:
 * - Interpolation: linear, ease, easeIn, easeOut (each supports both 3-arg and 5-arg forms)
 * - Random/Noise: wiggle, temporalWiggle, random, gaussRandom, noise, seedRandom
 * - Utility: clamp, posterizeTime, degreesToRadians, radiansToDegrees, smooth (keyframe-aware temporal smoothing)
 * - Audio: audioLevel, audioRms (require audio cache set on ExpressionEngine)
 * - Looping: loopIn, loopOut, loopInDuration, loopOutDuration (modes: cycle, pingpong, offset, continue)
 * - Color: rgbToHsl, hslToRgb, hexToRgb (AE-compatible color space conversions)
 * - Markers: marker.numKeys, marker.key(i), marker.nearestKey(t) (require marker cache)
 * - Time conversion: framesToTime, timeToFrames, timeToTimecode, timeToCurrentFormat
 * - Vector math: add, sub, mul, div, length, normalize, dot, cross, lookAt (AE-compatible, polymorphic)
 * - Coordinate: toComp, fromComp (normalized ↔ pixel), sourceRectAtTime (clip bounding box)
 * - Path: createPath(points, inTangents, outTangents, isClosed) (AE-compatible mask/path generation)
 *
 * Additionally registered via registerClipReferenceFunctions():
 * - thisProperty: AE-compatible property reference with .value, .numKeys, .wiggle(), .smooth(),
 *   .valueAtTime(), .velocityAtTime(), .speedAtTime(), .loopIn(), .loopOut(), .key(), .nearestKey()
 */
void registerExpressionFunctions(JSContext *ctx);

/**
 * @brief Register clip reference functions and thisProperty object.
 *
 * Registers:
 * - clip(name).effect(name).param(name) — cross-clip parameter references
 * - clip(index).effect(name).param(name) — index-based clip references
 * - clip(clipRef, relIndex).effect(name).param(name) — relative clip references (AE layer(other, rel))
 * - thisEffect.param(name) — same-effect parameter references
 * - thisProperty — AE-compatible property object with methods
 * - marker.key()/marker.nearestKey() — timeline marker access
 *
 * These functions use JS_GetContextOpaque() to access the ExpressionEngine
 * instance and its ClipParamResolver callback.
 */
void registerClipReferenceFunctions(JSContext *ctx);

/**
 * @brief Register createPath() function for AE-compatible path/mask expressions.
 *
 * createPath(points, inTangents, outTangents, isClosed) returns a JS object
 * with _isPath sentinel that ExpressionEngine::evaluatePath() can extract.
 * Coordinates are normalized 0.0-1.0 (frame-relative).
 */
void registerPathFunctions(JSContext *ctx);

/**
 * @brief Register AE-compatible keyframe access functions.
 *
 * Functions registered:
 * - key(index) — 1-based keyframe lookup, returns {time, index, value}
 * - nearestKey(t) — nearest keyframe by time, returns {time, index, value}
 * - valueAtTime(t) — linear interpolation of keyframes at arbitrary time
 * - velocityAtTime(t) — central-difference velocity at time t
 * - speedAtTime(t) — absolute value of velocity at time t
 *
 * Also initializes the numKeys global property to 0.
 * Requires _keyframes array set by ExpressionEngine::setKeyframes().
 */
void registerKeyframeFunctions(JSContext *ctx);
