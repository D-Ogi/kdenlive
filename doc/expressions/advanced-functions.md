# Advanced Expression Functions

This guide covers the advanced expression functions added to the Kdenlive Expression Engine: temporal smoothing, step-hold wiggle, coordinate conversion, and defensive expression patterns.

## temporalWiggle(freq, amp, octaves, ampMult, t)

Step-hold random wiggle. Unlike `wiggle()` which generates smooth Perlin noise, `temporalWiggle()` holds each random value for `1/freq` seconds, producing a **stepped/quantized** random motion.

### Signature

```
temporalWiggle(freq: double, amp: double) -> double
temporalWiggle(freq: double, amp: double, octaves: int, ampMult: double, t: double) -> double
```

### Parameters

| Parameter | Type | Default | Description |
|---|---|---|---|
| `freq` | `double` | required | How many times per second the value changes |
| `amp` | `double` | required | Maximum deviation from base `value` |
| `octaves` | `int` | 1 | Noise layers (max 10). Higher = more detail |
| `ampMult` | `double` | 0.5 | Amplitude multiplier per octave |
| `t` | `double` | `time` | Override time input |

### Examples

```js
// Value changes to a new random position 4 times per second
temporalWiggle(4, 0.1)

// Same but with 2 octaves of detail
temporalWiggle(4, 0.1, 2, 0.5)

// Film projector gate weave (stepped horizontal jitter)
temporalWiggle(24, 0.003)
```

### Comparison: wiggle vs temporalWiggle

| Aspect | `wiggle()` | `temporalWiggle()` |
|---|---|---|
| Interpolation | Smooth Perlin noise | Step-hold (discrete) |
| Motion character | Fluid, organic | Jumpy, mechanical |
| Use cases | Camera shake, breathing | Strobe, glitch, data display |
| Equivalent manual | — | `posterizeTime(freq); wiggle(freq, amp)` |

## smooth(width, samples)

Real temporal smoothing that averages the parameter's keyframe-interpolated value over a time window.

### Signature

```
smooth(width: double, samples: int) -> double
```

### Parameters

| Parameter | Type | Default | Description |
|---|---|---|---|
| `width` | `double` | 0.2 | Window width in seconds |
| `samples` | `int` | 5 | Number of sample points (1-100) |

### How It Works

1. Defines a window `[time - width/2, time + width/2]`
2. Samples `interpKeyframes()` at `samples` evenly-spaced points within the window
3. Returns the arithmetic mean of all samples

### Requirements

- **Keyframes must exist** on the parameter for smoothing to work
- Without keyframes, `smooth()` falls back to the current `value`
- The `smooth()` function triggers keyframe cache injection (same as `loopIn`/`loopOut`)

### Examples

```js
// Smooth over 200ms window with 5 samples (good default)
smooth(0.2, 5)

// Heavy smoothing: 500ms window, 10 samples
smooth(0.5, 10)

// Minimal smoothing: 100ms, 3 samples (low latency)
smooth(0.1, 3)
```

### Tuning Guide

| Width | Effect | Use Case |
|---|---|---|
| 0.05-0.1 | Subtle smoothing | Reduce jitter while preserving responsiveness |
| 0.1-0.3 | Moderate smoothing | Tame audio-reactive parameters |
| 0.3-1.0 | Heavy smoothing | Slow, gentle transitions |

More samples = smoother result but higher computation during baking. 5-10 samples is sufficient for most use cases.

## Coordinate Conversion

### toComp(point)

Converts normalized [0-1] coordinates to pixel coordinates.

```
toComp(point: array | double) -> array | double
```

Uses `thisProject.width` and `thisProject.height` for the conversion.

**Polymorphic behavior:**
- Array: `toComp([0.5, 0.25])` → `[960, 270]` (on 1920x1080 project)
- Scalar: `toComp(0.5)` → `960` (uses width)

### fromComp(point)

Converts pixel coordinates to normalized [0-1].

```
fromComp(point: array | double) -> array | double
```

**Polymorphic behavior:**
- Array: `fromComp([960, 540])` → `[0.5, 0.5]` (on 1920x1080 project)
- Scalar: `fromComp(960)` → `0.5` (uses width)

### Examples

```js
// Convert a pixel position to normalized for a position parameter
fromComp([100, 200])

// Convert normalized coordinates to pixels for calculations
var pixelPos = toComp([0.5, 0.5]);  // center of frame in pixels

// Use with sampleImage (which takes normalized coords)
var color = sampleImage(fromComp([400, 300])[0], fromComp([400, 300])[1]);
```

## sourceRectAtTime(t, includeExtents)

Returns the clip's source rectangle as a JS object with pixel dimensions.

```
sourceRectAtTime(t: double, includeExtents: bool) -> {top, left, width, height}
```

### Parameters

| Parameter | Type | Default | Description |
|---|---|---|---|
| `t` | `double` | — | Time in seconds (accepted for AE compat, currently ignored) |
| `includeExtents` | `bool` | false | AE compat parameter (currently ignored) |

### Returns

Object with properties:
- `.top` — always 0 (no per-layer offset in Kdenlive)
- `.left` — always 0
- `.width` — clip source width in pixels
- `.height` — clip source height in pixels

### Examples

```js
// Get clip dimensions
var rect = sourceRectAtTime(time);
rect.width   // e.g. 1920
rect.height  // e.g. 1080

// Calculate aspect ratio
var aspect = sourceRectAtTime(time).width / sourceRectAtTime(time).height;

// Center a value based on clip dimensions
var centerX = sourceRectAtTime(time).width / 2;
```

## Defensive Expressions with try/catch

QuickJS supports full JavaScript try/catch/finally. Use this for expressions that reference other clips or effects that might not exist.

### Pattern

```js
try {
    clip("Other Clip").effect("Blur").param("amount");
} catch(e) {
    value;  // fallback to base value if reference fails
}
```

### Use Cases

```js
// Safe cross-clip reference with fallback
var otherVal;
try {
    otherVal = clip("Title").effect("Transform").param("opacity");
} catch(e) {
    otherVal = 1.0;
}
linear(otherVal, 0, 1, 0, value)

// Conditional effect chain — apply if effect exists, skip if not
try {
    var blur = thisEffect.param("blur_amount");
    value * (1 - blur);
} catch(e) {
    value;
}
```

### Notes

- `try/catch` has negligible performance overhead in QuickJS
- Always provide a meaningful fallback value in the `catch` block
- This pattern works with `clip()`, `thisEffect.param()`, `key()`, `nearestKey()`, and any function that can throw
