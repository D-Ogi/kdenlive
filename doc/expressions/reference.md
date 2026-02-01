# Kdenlive Expression Engine -- Function Reference

This document is the complete reference for all global variables, functions, and objects available in the Kdenlive Expression Engine. Expressions are evaluated per-frame using QuickJS and can be attached to any numeric effect parameter.

## Important Note on Parameter Ranges

Kdenlive effect parameters use **normalized floating-point values**, typically in the range `[0.0, 1.0]`, not percentage integers (0-`value`). The `value` variable in all examples below refers to the parameter's current normalized value. When writing expressions:
- Opacity, brightness, and other normalized parameters: use `0.0` to `1.0`
- Position, offset, or scale parameters: check the effect documentation for the expected range
- Always scale amplitude values to match the parameter range (e.g., `wiggle(4, 0.05)` for a parameter that spans 0-1)

---

## Global Variables

The following variables are injected into every expression context automatically. They do not need to be declared or imported.

| Variable   | Type     | Description                                                                 |
|------------|----------|-----------------------------------------------------------------------------|
| `time`     | `double` | Current time in seconds, measured from the start of the clip.               |
| `frame`    | `int`    | Current frame number (0-based), relative to the clip's in-point.            |
| `duration` | `double` | Total duration of the clip in seconds.                                      |
| `fps`      | `double` | Project frame rate (frames per second).                                     |
| `value`    | `double` | The base parameter value, snapshotted from the slider (or interpolated from keyframes) when the expression is first applied. This value is preserved independently of the expression output, so expressions can safely reference `value` without circular dependency. |
| `index`    | `int`    | The 0-based index of the clip on its track, counted from the left.          |

---

## Functions

**Note:** Many functions support polymorphic (scalar and array) arguments. For detailed documentation of polymorphic behavior, array padding rules, and AE compatibility notes, see [polymorphic-functions.md](./polymorphic-functions.md).

### Interpolation

#### linear

```
linear(t: double, tMin: double, tMax: double, vMin: double, vMax: double) -> double
linear(t: double, vMin: double, vMax: double) -> double
```

Performs linear interpolation with clamping.

**5-argument form:** Maps the input value `t` from the source range `[tMin, tMax]` to the destination range `[vMin, vMax]`. If `t` is outside `[tMin, tMax]`, the result is clamped to `[vMin, vMax]`.

**3-argument form:** Assumes `t` is in the `[0, 1]` range and maps it to `[vMin, vMax]`. Clamped at both ends.

**Parameters:**

| Parameter | Type     | Description                              |
|-----------|----------|------------------------------------------|
| `t`       | `double` | Input value to remap.                    |
| `tMin`    | `double` | Lower bound of the input range.          |
| `tMax`    | `double` | Upper bound of the input range.          |
| `vMin`    | `double` | Output value when `t` equals `tMin`.     |
| `vMax`    | `double` | Output value when `t` equals `tMax`.     |

**Returns:** `double` -- The interpolated and clamped output value.

**Examples:**

```js
// Ramp from 0 to 1 over the entire clip duration (normalized parameter)
linear(time, 0, duration, 0, value)

// Map audio level (0-1) to the current effect parameter range
linear(audioLevel("Both", time), 0, 1)
```

**Notes:**
- The clamping behavior means the output will never exceed `vMin` or `vMax`, even if `t` overshoots the input range. This makes `linear` safe to use with unpredictable inputs like audio levels.

---

#### ease

```
ease(t: double, tMin: double, tMax: double, vMin: double, vMax: double) -> double
ease(t: double, vMin: double, vMax: double) -> double
```

Performs smooth Hermite interpolation (also known as smoothstep) between two values. The easing curve follows the polynomial `3t^2 - 2t^3`, which produces zero velocity at both endpoints. This creates a gentle acceleration at the start and a gentle deceleration at the end.

**5-argument form:** Maps `t` from range `[tMin, tMax]` to `[vMin, vMax]` with easing.

**3-argument form:** Assumes `t` is in `[0, 1]` and maps to `[vMin, vMax]` (shorthand). `t` is clamped before interpolation.

**Parameters:**

| Parameter | Type     | Description                              |
|-----------|----------|------------------------------------------|
| `t`       | `double` | Input value (current time or position).  |
| `tMin`    | `double` | Start of the input range.                |
| `tMax`    | `double` | End of the input range.                  |
| `vMin`    | `double` | Output value at `tMin`.                  |
| `vMax`    | `double` | Output value at `tMax`.                  |

**Returns:** `double` -- The smoothly interpolated value, clamped to `[vMin, vMax]`.

**Examples:**

```js
// Smooth ramp from 0 to 1 over the clip -- slow start and slow end (normalized)
ease(time, 0, duration, 0, value)

// Smooth opacity fade-in during the first 2 seconds
ease(time, 0, 2, 0, 1)
```

**Notes:**
- Prefer `ease` over `linear` when you want motion that feels natural and avoids abrupt starts or stops.
- For detailed polymorphic documentation including the 3-arg form, see [polymorphic-functions.md](./polymorphic-functions.md#ease-easein-easeout--3-argument-forms).

---

#### easeIn

```
easeIn(t: double, tMin: double, tMax: double, vMin: double, vMax: double) -> double
easeIn(t: double, vMin: double, vMax: double) -> double
```

Quadratic ease-in interpolation. The curve follows `t^2`, which starts slowly and accelerates toward the end.

**5-argument form:** Maps `t` from range `[tMin, tMax]` to `[vMin, vMax]` with ease-in.

**3-argument form:** Assumes `t` is in `[0, 1]` and maps to `[vMin, vMax]` (shorthand). `t` is clamped before interpolation.

**Parameters:**

| Parameter | Type     | Description                              |
|-----------|----------|------------------------------------------|
| `t`       | `double` | Input value.                             |
| `tMin`    | `double` | Start of the input range.                |
| `tMax`    | `double` | End of the input range.                  |
| `vMin`    | `double` | Output value at `tMin`.                  |
| `vMax`    | `double` | Output value at `tMax`.                  |

**Returns:** `double` -- The interpolated value with slow-start easing.

**Examples:**

```js
// Slow start, then accelerate to max value over the clip
easeIn(time, 0, duration, 0, value)

// Position animation: starts slow, accelerates to final position
easeIn(time, 0, 1.5, 0, 1)
```

---

#### easeOut

```
easeOut(t: double, tMin: double, tMax: double, vMin: double, vMax: double) -> double
easeOut(t: double, vMin: double, vMax: double) -> double
```

Quadratic ease-out interpolation. The curve follows `1 - (1-t)^2`, which starts fast and decelerates toward the end.

**5-argument form:** Maps `t` from range `[tMin, tMax]` to `[vMin, vMax]` with ease-out.

**3-argument form:** Assumes `t` is in `[0, 1]` and maps to `[vMin, vMax]` (shorthand). `t` is clamped before interpolation.

**Parameters:**

| Parameter | Type     | Description                              |
|-----------|----------|------------------------------------------|
| `t`       | `double` | Input value.                             |
| `tMin`    | `double` | Start of the input range.                |
| `tMax`    | `double` | End of the input range.                  |
| `vMin`    | `double` | Output value at `tMin`.                  |
| `vMax`    | `double` | Output value at `tMax`.                  |

**Returns:** `double` -- The interpolated value with slow-end easing.

**Examples:**

```js
// Fast start, then decelerate to max value
easeOut(time, 0, duration, 0, value)

// Slide-in effect: element arrives quickly and settles into position
easeOut(time, 0, 0.8, 0, 1)
```

---

### Random and Noise

#### wiggle

```
wiggle(freq: double, amp: double) -> double
wiggle(freq: double, amp: double, octaves: int, ampMult: double, t: double) -> double
```

Generates a smooth, Perlin noise-based oscillation around the current base `value`. This is the primary tool for organic, randomized animation -- camera shake, jitter, breathing effects, and similar motion.

**Parameters:**

| Parameter  | Type     | Default        | Description                                                        |
|------------|----------|----------------|--------------------------------------------------------------------|
| `freq`     | `double` | (required)     | Oscillation frequency in Hz (cycles per second).                   |
| `amp`      | `double` | (required)     | Maximum deviation from the base `value`.                           |
| `octaves`  | `int`    | `1`            | Number of noise layers. Higher values add finer detail.            |
| `ampMult`  | `double` | `0.5`          | Amplitude multiplier applied to each successive octave.            |
| `t`        | `double` | global `time`  | Override the time input instead of using the global `time` variable.|

**Returns:** `double` -- The base `value` plus a noise-derived offset.

**Examples:**

```js
// Simple 4 Hz shake with normalized amplitude of 0.05
wiggle(4, 0.05)

// Rich, organic 3-octave shake for a handheld camera feel
wiggle(2, 0.1, 3, 0.5)
```

**Notes:**
- `wiggle` is deterministic. The same expression on the same frame always produces the same result. This is critical for reproducible renders.
- The output is centered on `value`, so if your slider is set to 50 and amplitude is 10, results will range from approximately 40 to 60.
- Increasing `octaves` adds progressively smaller, higher-frequency detail. An `ampMult` of 0.5 means each octave has half the amplitude of the previous one.

---

#### temporalWiggle

```
temporalWiggle(freq: double, amp: double) -> double
temporalWiggle(freq: double, amp: double, octaves: int, ampMult: double, t: double) -> double
```

Step-hold wiggle. Unlike `wiggle()` which produces smooth Perlin noise, `temporalWiggle` holds each random value for `1/freq` seconds, producing discrete stepped random motion. The value changes abruptly at each step boundary rather than interpolating smoothly between values.

**Parameters:**

| Parameter  | Type     | Default        | Description                                                        |
|------------|----------|----------------|--------------------------------------------------------------------|
| `freq`     | `double` | (required)     | Step frequency in Hz. A new random value is chosen every `1/freq` seconds. |
| `amp`      | `double` | (required)     | Maximum deviation from the base `value`.                           |
| `octaves`  | `int`    | `1`            | Number of noise layers. Higher values add finer detail.            |
| `ampMult`  | `double` | `0.5`          | Amplitude multiplier applied to each successive octave.            |
| `t`        | `double` | global `time`  | Override the time input instead of using the global `time` variable.|

**Returns:** `double` -- The base `value` plus a stepped random offset.

**Examples:**

```js
// Flicker effect: new random value 8 times per second
temporalWiggle(8, 0.1)

// Stepped jitter with multi-octave detail
temporalWiggle(4, 0.2, 3, 0.5)
```

**Notes:**
- Deterministic: seeded by the step index and clip context. The same frame always produces the same result.
- Useful for strobe, glitch, or old-film-flicker effects where smooth noise would look too organic.
- Compare with `posterizeTime` + `wiggle` -- `temporalWiggle` is more convenient and produces a true sample-and-hold pattern.

---

#### random

```
random() -> Number
random(maxValOrArray) -> Number | Array
random(minValOrArray, maxValOrArray) -> Number | Array
```

Returns a deterministic pseudo-random value for the current frame. The random number generator is seeded by the frame number and clip index, so the same frame always produces the same output across renders.

**Supports polymorphism:** Accepts scalar or array arguments, returns matching type.

**Parameters:**

| Parameter | Type     | Description                                |
|-----------|----------|--------------------------------------------|
| `maxValOrArray` | `Number` or `Array` | Upper bound. Scalar returns scalar in `[0, max]`. Array returns per-component random. |
| `minValOrArray` | `Number` or `Array` | Lower bound. Pairs with max for `[min, max]` range. |

**Returns:**
- No arguments: `Number` in `[0.0, 1.0]`.
- Scalar arguments: `Number` in specified range.
- Array arguments: `Array` with per-component random values.

**Examples:**

```js
// Random value within normalized parameter range, different each frame
random(0, value)

// Random opacity flicker between 0.7 and 1.0
0.7 + random() * 0.3

// Array: per-component random
random([100, 200])              // → [73.0, 146.0]
random([0, 100], [50, 200])     // → [36.5, 173.0]
```

**Notes:**
- Uses xorshift32 internally. The seed combines `frame` and `index`, ensuring that two clips on the same track at different positions produce different sequences.
- For non-deterministic randomness, use `Math.random()` -- but be aware that non-deterministic values may differ between preview and render.
- **Polymorphic behavior:** See [polymorphic-functions.md](./polymorphic-functions.md#random) for detailed array handling, zero-padding rules, and AE compatibility.

---

#### gaussRandom

```
gaussRandom() -> Number
gaussRandom(maxValOrArray) -> Number | Array
gaussRandom(minValOrArray, maxValOrArray) -> Number | Array
```

Returns a Gaussian (normally distributed) random value using the Box-Muller transform. Values cluster around the center of the range with approximately 90% falling within `[min, max]`.

**Supports polymorphism:** Accepts scalar or array arguments, returns matching type.

**Parameters:**

| Parameter | Type     | Description                                |
|-----------|----------|--------------------------------------------|
| `maxValOrArray` | `Number` or `Array` | Upper bound. Distribution centered at `max/2` with σ = `max/6`. |
| `minValOrArray` | `Number` or `Array` | Lower bound. Distribution centered at `(min+max)/2` with σ = `(max-min)/6`. |

**Returns:**
- No arguments: Standard normal distribution (mean 0, σ=1).
- Scalar arguments: `Number` with Gaussian distribution in range.
- Array arguments: `Array` with per-component Gaussian distributions.

**Examples:**

```js
// Values clustered around 0.5, ~90% within 0.4-0.6
gaussRandom(0.4, 0.6)

// Subtle parameter jitter concentrated near the center
value + gaussRandom(-0.05, 0.05)

// Array: per-component Gaussian
gaussRandom([100, 200])         // → [48.7, 203.1] (example)
```

**Notes:**
- Unlike `random()`, which produces a uniform distribution, `gaussRandom()` produces values that cluster around the center. This is useful for natural-looking variation where extreme values are rare.
- **Polymorphic behavior:** See [polymorphic-functions.md](./polymorphic-functions.md#gaussrandom) for detailed array handling, distribution math, and AE compatibility.

---

#### noise

```
noise(v: double) -> double
```

Evaluates 1D Perlin noise at the given input value. The output is a smooth, continuous signal that varies roughly between -1 and 1.

**Parameters:**

| Parameter | Type     | Description                     |
|-----------|----------|---------------------------------|
| `v`       | `double` | The noise input coordinate.     |

**Returns:** `double` -- A smooth noise value, approximately in the range `[-1, 1]`.

**Examples:**

```js
// Smooth noise cycling at 4x the speed of time
noise(time * 4)

// Add smooth randomness of +/- 0.2 around the base normalized value
value + noise(time * 2) * 0.2
```

**Notes:**
- Perlin noise is continuous: small changes in the input produce small changes in the output. This makes it ideal for smooth, organic animation.
- To increase the frequency of variation, multiply the input by a larger constant. To increase the amplitude, multiply the output.
- The output range is approximately `[-1, 1]` but is not strictly bounded. For strict bounds, wrap the result with `clamp()`.

---

#### seedRandom

```
seedRandom(seed: int, timeless: bool) -> void
```

Overrides the random seed used by subsequent calls to `random()` and `gaussRandom()` within the same expression evaluation.

**Parameters:**

| Parameter  | Type   | Description                                                             |
|------------|--------|-------------------------------------------------------------------------|
| `seed`     | `int`  | Custom seed value.                                                      |
| `timeless` | `bool` | If `true`, the random generator ignores the frame number entirely, returning the same value on every frame. If `false`, the seed is combined with the frame number as usual. |

**Returns:** Nothing. Modifies the internal random state as a side effect.

**Examples:**

```js
// Use a fixed seed but still vary per frame
seedRandom(42, false);
random(0, value)

// Same random value on every frame (timeless)
seedRandom(99, true);
random(0, value)

// Different clips with different seeds
seedRandom(index, true);
random(0, value)

// Seed 0 and seed 1 produce different sequences
seedRandom(0, false);
random()  // → one sequence
// vs
seedRandom(1, false);
random()  // → different sequence
```

**Notes:**
- Call `seedRandom` before any `random()` or `gaussRandom()` calls in the expression. It only affects calls that come after it in the same expression string.
- The `timeless` flag is useful when you want a per-clip constant random value (e.g., randomize a property once per clip, not per frame).
- **Seed values matter:** `seed=0` produces different random values from `seed=1`. This is distinct from the default behavior (when `seedRandom` is not called), where the random generator is seeded from the frame number and clip index automatically.

---

### Utility

#### clamp

```
clamp(value, limit1, limit2) -> Number | Array
```

Constrains a value or array to a specified range. Works per-component on arrays.

**Supports polymorphism:** Accepts scalar or array arguments, returns matching type.

**Parameters:**

| Parameter | Type     | Description                |
|-----------|----------|----------------------------|
| `value`   | `Number` or `Array` | The value(s) to constrain. |
| `limit1`  | `Number` or `Array` | Lower bound (or upper if swapped). |
| `limit2`  | `Number` or `Array` | Upper bound (or lower if swapped). |

**Returns:**
- **All scalars:** `Number` clamped to `[min, max]`.
- **Any array:** `Array` with per-component clamping.

**Examples:**

```js
// Scalar clamp
clamp(150, 0, 100)              // → 100
clamp(wiggle(4, 0.1), 0, 1)     // Wiggle within normalized bounds

// Array clamp (per-component)
clamp([50, 200], [0, 0], [100, 100])        // → [50, 100]
clamp(position, [0, 0], [1920, 1080])       // Prevent position leaving screen
```

**Notes:**
- Essential when combining `wiggle`, `noise`, or `random` with parameters that have hard limits (opacity 0-1, color channels 0-255, etc.).
- **Polymorphic behavior:** See [polymorphic-functions.md](./polymorphic-functions.md#clamp) for array padding rules and scalar broadcasting.

---

#### posterizeTime

```
posterizeTime(fps: double) -> void
```

Reduces the effective frame rate of the expression. After this call, all subsequent time-dependent functions within the same expression evaluate as if the project were running at the specified lower frame rate. Time is quantized into steps of `1/fps` seconds.

This creates a "stepped" or "stuttery" animation effect where the expression value is held constant for multiple frames before jumping to the next value.

**Parameters:**

| Parameter | Type     | Description                                      |
|-----------|----------|--------------------------------------------------|
| `fps`     | `double` | The target frame rate for expression evaluation.  |

**Returns:** Nothing. Modifies the time context as a side effect.

**Examples:**

```js
// Step the ramp every 0.5 seconds (2 fps)
posterizeTime(2); linear(time, 0, duration, 0, value)

// Generate a new random value once per second
posterizeTime(1); random(0, value)

// Stepped wiggle: 6 times per second
posterizeTime(6); wiggle(4, 0.1)
```

**Notes:**
- `posterizeTime` must be called before the expression code that should be affected. Place it at the beginning of the expression, separated by a semicolon.
- This is useful for creating intentionally choppy, stroboscopic, or stepped animation effects.
- A `posterizeTime(fps)` call where `fps` matches the project frame rate has no visible effect.
- **Implementation:** The engine's bake loop detects the `_posterizeFps` internal state and only re-evaluates the expression at quantized time boundaries. Between those boundaries, the previous frame's value is held constant.

---

#### validate

```
validate(expression: string) -> bool
```

Validates an expression string without evaluating it or modifying the current parameter. Returns `true` if the expression compiles successfully and contains no syntax errors, `false` otherwise.

This function is primarily used by the Kdenlive UI to check expression validity before applying an expression to a parameter. It can also be used within expressions for conditional fallback logic.

**Parameters:**

| Parameter    | Type     | Description                                      |
|--------------|----------|--------------------------------------------------|
| `expression` | `string` | The expression string to validate.               |

**Returns:** `bool` -- `true` if the expression is valid, `false` if it contains syntax errors or cannot be compiled.

**Examples:**

```js
// Check if a complex expression is valid before using it
var complexExpr = "wiggle(4, 0.1) + loopOut('cycle')";
validate(complexExpr) ? eval(complexExpr) : value

// Validate user-provided expression (hypothetical UI scenario)
validate("time * 2 + value")  // → true
validate("time * * value")    // → false (syntax error)
```

**Notes:**
- `validate()` preserves the engine's internal state. It saves and restores all context globals including `_posterizeFps`, `_userSeed`, `_timeless`, and other internal state variables. This means calling `validate()` does not interfere with the current expression's behavior.
- The function creates a temporary isolated execution context for validation, so side effects in the validated expression (like `seedRandom()` or `posterizeTime()`) do not affect the calling expression's state.
- **Engine state preservation:** The validation process takes a snapshot of the random number generator state, posterize settings, and all other context variables before evaluating the test expression, then fully restores them afterward.
- This is a meta-function rarely used in typical expressions. It exists primarily for tooling and UI validation.

---

#### degreesToRadians

```
degreesToRadians(deg: double) -> double
```

Converts an angle from degrees to radians.

**Parameters:**

| Parameter | Type     | Description              |
|-----------|----------|--------------------------|
| `deg`     | `double` | Angle in degrees.        |

**Returns:** `double` -- The equivalent angle in radians.

**Examples:**

```js
// Convert 180 degrees to radians
degreesToRadians(180)   // returns 3.14159...

// Use with Math.sin for rotation-based animation (normalized)
0.5 + Math.sin(degreesToRadians(frame * 10)) * 0.5
```

---

#### radiansToDegrees

```
radiansToDegrees(rad: double) -> double
```

Converts an angle from radians to degrees.

**Parameters:**

| Parameter | Type     | Description              |
|-----------|----------|--------------------------|
| `rad`     | `double` | Angle in radians.        |

**Returns:** `double` -- The equivalent angle in degrees.

**Examples:**

```js
// Convert PI radians to degrees
radiansToDegrees(Math.PI)   // returns 180

// Convert an expression result back to degrees for a rotation parameter
radiansToDegrees(Math.atan2(y, x))
```

---

#### smooth

```
smooth(width: double, samples: int) -> double
```

Applies temporal smoothing by sampling the parameter's keyframe-interpolated value at evenly-spaced points within the time window `[t - width/2, t + width/2]` and averaging the results. This produces a moving-average filter over the keyframed animation curve.

**Parameters:**

| Parameter | Type     | Description                                            |
|-----------|----------|--------------------------------------------------------|
| `width`   | `double` | Width of the smoothing window in seconds.              |
| `samples` | `int`    | Number of evenly-spaced sample points taken within the window. |

**Returns:** `double` -- The temporally smoothed value. Falls back to `value` if no keyframes exist on the parameter.

**Examples:**

```js
// Smooth the keyframed value over a 0.5-second window with 5 samples
smooth(0.5, 5)

// Smooth a noisy audio-reactive parameter (normalized)
linear(audioLevel("Both", time), 0, 1); smooth(0.2, 3)

// Heavy smoothing for slow, gentle transitions
smooth(1.0, 11)
```

**Notes:**
- Requires keyframes on the parameter for meaningful smoothing. Without keyframes, returns `value` unchanged.
- Internally samples `interpKeyframes()` (the linear interpolation of the parameter's keyframe curve) at each sample point, then averages.
- Smoothing is most useful for taming noisy inputs (audio levels, high-frequency wiggle) or softening sharp keyframe transitions.
- Higher `samples` values produce smoother results but increase computation.

---

### Vector Math

#### length

```
length(vec) -> Number
length(point1, point2) -> Number
```

Calculates Euclidean magnitude of a vector or distance between two points.

**Supports polymorphism:** Accepts scalar or array arguments, always returns scalar.

**Parameters:**

| Parameter | Type     | Description                |
|-----------|----------|----------------------------|
| `vec`     | `Number` or `Array` | Vector to measure. If scalar, returns `abs(vec)`. |
| `point1`  | `Number` or `Array` | First point. |
| `point2`  | `Number` or `Array` | Second point. Distance is `length(sub(point1, point2))`. |

**Returns:** `Number` — Euclidean magnitude (1-arg) or distance (2-arg).

**Examples:**

```js
// Vector magnitude
length([3, 4])                  // → 5.0 (Pythagorean triple)
length([1, 2, 2])               // → 3.0

// Distance between two points
length([0, 0], [3, 4])          // → 5.0
length(position, [960, 540])    // Distance from position to screen center

// Scalar absolute value/difference
length(-42)                     // → 42.0
length(5, 10)                   // → 5.0 (abs difference)
```

**Notes:**
- Classic AE pattern: `length(position, targetPos)` for distance-based effects.
- **Polymorphic behavior:** See [polymorphic-functions.md](./polymorphic-functions.md#length) for array padding and use cases.

---

#### lookAt

```
lookAt(fromPoint, atPoint) -> [xRotation, yRotation, 0]
```

Calculates 3D orientation (rotation) so that the Z-axis points from `fromPoint` toward `atPoint`. Returns Euler angles in degrees.

**Parameters:**

| Parameter | Type     | Description                |
|-----------|----------|----------------------------|
| `fromPoint` | `Array` | 3D position `[x, y, z]` of the object looking. |
| `atPoint`   | `Array` | 3D position `[x, y, z]` of the target. |

**Returns:** `Array` `[xRotation, yRotation, 0]` in degrees — Euler angles to orient Z-axis toward target.

**Examples:**

```js
// Camera auto-aiming at layer position
var camPos = [0, 0, -1000];
var target = [x, y, 0];
lookAt(camPos, target)          // → [xRot, yRot, 0]

// Light tracking a moving object
var lightPos = [500, -200, -500];
var objectPos = clip("target.mp4").transform.position;  // Hypothetical
lookAt(lightPos, objectPos)
```

**Notes:**
- Requires 3D arrays (at least 3 components). Throws `TypeError` if not.
- Z rotation is always `0` (roll not constrained by look-at).
- **Full documentation:** See [polymorphic-functions.md](./polymorphic-functions.md#lookat) for coordinate system details and AE compatibility.

---

### Time Conversion

#### timeToTimecode

```
timeToTimecode(t, timecodeBase = 30, isDuration = false) -> String
```

Converts a time value (in seconds) to SMPTE timecode string `"HH:MM:SS:FF"`.

**Parameters:**

| Parameter | Type     | Default | Description                |
|-----------|----------|---------|----------------------------|
| `t`       | `Number` | (required) | Time in seconds to convert. |
| `timecodeBase` | `Number` | `30` | Frame rate for timecode (fps). |
| `isDuration` | `Boolean` | `false` | If `true`, treat as duration (round up frames). If `false`, treat as absolute time (round down). |

**Returns:** `String` in format `"HH:MM:SS:FF"` where `FF` is frames (00 to `timecodeBase-1`). Negative times are prefixed with `-`.

**Examples:**

```js
// Current time as timecode (default 30fps)
timeToTimecode(time)            // → "00:01:23:15"

// Custom frame rate (25fps PAL)
timeToTimecode(time, 25)        // → "00:01:23:12"

// Duration (rounds up)
timeToTimecode(1.01, 30, true)  // → "00:00:01:01" (2 frames)
timeToTimecode(1.01, 30, false) // → "00:00:01:00" (1 frame)

// Display in text layer
"Current Time: " + timeToTimecode(time, fps)
```

**Notes:**
- **AE compatible** (non-drop-frame only). Does not support drop-frame NTSC timecode.
- Use `fps` (project frame rate) for accuracy: `timeToTimecode(time, fps)`.
- **Full documentation:** See [polymorphic-functions.md](./polymorphic-functions.md#timetotimecode) for rounding behavior and examples.

---

#### timeToCurrentFormat

```
timeToCurrentFormat(t?, fps?, isDuration?) -> String
```

Converts time to a timecode string using the **project's frame rate** as the default base, instead of the fixed 30fps default used by `timeToTimecode`. This is the recommended function for displaying timecodes that match the project's actual frame rate.

**Parameters:**

| Parameter    | Type      | Default                              | Description                |
|--------------|-----------|--------------------------------------|----------------------------|
| `t`          | `Number`  | `time + thisProject.displayStartTime` | Time in seconds to convert. Defaults to the current time adjusted by the project's display start offset. |
| `fps`        | `Number`  | `thisProject.fps`                    | Frame rate for timecode. Defaults to the project frame rate. |
| `isDuration` | `Boolean` | `false`                              | If `true`, rounds away from zero (for durations). If `false`, rounds toward zero (for absolute times). |

**Returns:** `String` in format `"HH:MM:SS:FF"` where `FF` is frames at the specified (or project default) frame rate.

**AE Equivalent:** `timeToCurrentFormat(t, fps, isDuration)` -- identical signature and behavior.

**Difference from `timeToTimecode`:** `timeToTimecode` defaults to base 30fps regardless of the project setting. `timeToCurrentFormat` uses `thisProject.fps` as the default, so the output always matches the project's actual frame rate without requiring you to pass `fps` explicitly.

**Examples:**

```js
// Current time in project format (most common usage)
timeToCurrentFormat()

// Specific time at project fps
timeToCurrentFormat(2.5)

// Duration formatting (rounds up)
timeToCurrentFormat(duration, thisProject.fps, true)

// Explicit fps override (still works like timeToTimecode)
timeToCurrentFormat(time, 24)
```

---

### Audio Functions

#### audioLevel

```
audioLevel(channel: string, t: double) -> double
```

Returns the peak audio level at the specified time. The value represents the instantaneous peak amplitude of the audio waveform, normalized to a 0-to-1 range.

**Parameters:**

| Parameter | Type     | Description                                                    |
|-----------|----------|----------------------------------------------------------------|
| `channel` | `string` | Audio channel to sample: `"Both"`, `"Left"`, or `"Right"`.    |
| `t`       | `double` | Time position in seconds (typically the global `time` variable).|

**Returns:** `double` -- Peak audio level from `0.0` (silence) to `1.0` (full scale).

**Examples:**

```js
// Get the current combined audio level
audioLevel("Both", time)

// Map audio to a normalized parameter range, ignoring noise floor below 0.05
// and treating 0.8 as maximum
linear(audioLevel("Both", time), 0.05, 0.8, 0, value)
```

**Notes:**
- Returns `0.0` if no audio cache is available (e.g., clip has no audio track, or audio cache has not been built yet).
- Peak levels can be spiky. For smoother audio-reactive animation, consider using `audioRms()` or wrapping the result with `smooth()`.
- The `"Both"` channel returns the maximum of the left and right channels.

---

#### audioRms

```
audioRms(channel: string, t: double, window: double) -> double
```

Returns the RMS (root mean square) audio level averaged over a time window centered on `t`. RMS is a better representation of perceived loudness than peak level and produces smoother, more stable values.

**Parameters:**

| Parameter | Type     | Description                                                    |
|-----------|----------|----------------------------------------------------------------|
| `channel` | `string` | Audio channel: `"Both"`, `"Left"`, or `"Right"`.              |
| `t`       | `double` | Center time of the analysis window in seconds.                 |
| `window`  | `double` | Total width of the analysis window in seconds.                 |

**Returns:** `double` -- RMS audio level, normalized to approximately `[0.0, 1.0]`.

**Examples:**

```js
// RMS level over a 200ms window -- smooth and responsive
audioRms("Both", time, 0.2)

// Drive a glow intensity from RMS audio (normalized)
linear(audioRms("Both", time, 0.3), 0.05, 0.6, 0, value)
```

**Notes:**
- Larger `window` values produce smoother results but introduce more temporal lag. A window of `0.1` to `0.3` seconds is a good starting range for most audio-reactive effects.
- Like `audioLevel`, returns `0.0` when no audio cache is available.

---

### Keyframe Access

These functions provide After Effects-compatible keyframe introspection. They enable expressions like overshoot, bounce, elastic, and inertia that respond to the parameter's own keyframe data. Requires keyframes on the parameter — the engine detects usage automatically.

#### numKeys

```
numKeys -> int
```

Global property (not a function) that returns the number of keyframes on the current parameter. Returns `0` when no keyframes exist.

**Examples:**

```js
// Check if parameter has keyframes
if (numKeys > 0) { key(1).time; } else { 0; }
```

---

#### key

```
key(index: int) -> {time: double, index: int, value: double}
```

Returns a keyframe object by 1-based index.

**Parameters:**

| Parameter | Type  | Description                                    |
|-----------|-------|------------------------------------------------|
| `index`   | `int` | 1-based keyframe index (1 to `numKeys`).       |

**Returns:** Object with `.time` (seconds), `.index` (1-based), `.value` (keyframe value).

**Throws:** `RangeError` if no keyframes exist or index is out of range.

**Examples:**

```js
// Get time of first and last keyframe
key(1).time
key(numKeys).time

// Get value difference between first two keyframes
key(2).value - key(1).value
```

---

#### nearestKey

```
nearestKey(t: double) -> {time: double, index: int, value: double}
```

Returns the keyframe nearest to time `t`, measured by absolute time difference.

**Parameters:**

| Parameter | Type     | Description                     |
|-----------|----------|---------------------------------|
| `t`       | `double` | Time in seconds.                |

**Returns:** Same object structure as `key()`.

**Throws:** `RangeError` if no keyframes exist.

**Examples:**

```js
// Get the nearest keyframe to the current time
var n = nearestKey(time);
n.time   // when it occurs
n.value  // its value
n.index  // its 1-based index
```

---

#### valueAtTime

```
valueAtTime(t: double) -> double
```

Returns the linearly interpolated parameter value at time `t`. Clamps to the first or last keyframe value outside the keyframe range. Falls back to `value` if no keyframes exist.

**Parameters:**

| Parameter | Type     | Description                     |
|-----------|----------|---------------------------------|
| `t`       | `double` | Time in seconds.                |

**Returns:** `double` — the interpolated value.

**Examples:**

```js
// Sample value 0.5 seconds in the future
valueAtTime(time + 0.5)

// Compare current value to value 1 second ago
valueAtTime(time) - valueAtTime(time - 1)
```

---

#### velocityAtTime

```
velocityAtTime(t: double) -> double
```

Returns the rate of change (first derivative) of the parameter at time `t` using central difference: `(v(t+dt) - v(t-dt)) / (2*dt)` where `dt = 0.5/fps`. Returns `0.0` if fewer than 2 keyframes exist.

**Parameters:**

| Parameter | Type     | Description                     |
|-----------|----------|---------------------------------|
| `t`       | `double` | Time in seconds.                |

**Returns:** `double` — velocity in parameter-units per second. Positive means increasing, negative means decreasing.

**Examples:**

```js
// Scale overshoot amplitude by velocity at nearest keyframe
var n = nearestKey(time);
var amp = velocityAtTime(n.time) / (Math.PI * 2 * 4);
```

---

#### speedAtTime

```
speedAtTime(t: double) -> double
```

Returns the absolute rate of change at time `t`. Always non-negative. Equivalent to `Math.abs(velocityAtTime(t))`.

**Parameters:**

| Parameter | Type     | Description                     |
|-----------|----------|---------------------------------|
| `t`       | `double` | Time in seconds.                |

**Returns:** `double` — speed (always >= 0).

**Examples:**

```js
// Only trigger effect when animation is moving fast
if (speedAtTime(time) > 50) { wiggle(4, 0.1); } else { value; }
```

---

### Looping

These functions repeat the keyframe animation cycle before the first keyframe (`loopIn`) or after the last keyframe (`loopOut`). They require at least 2 keyframes on the parameter.

#### loopOut

```
loopOut(type: string, numKeyframes: int) -> double
```

Repeats the keyframe animation cycle after the last keyframe. The loop segment is defined by the last `numKeyframes` keyframes. If `numKeyframes` is `0`, all keyframes are used.

**Parameters:**

| Parameter      | Type     | Default    | Description                                                      |
|----------------|----------|------------|------------------------------------------------------------------|
| `type`         | `string` | `"cycle"`  | Loop mode (see below).                                           |
| `numKeyframes` | `int`    | `0`        | Number of keyframes from the end to define the loop segment. `0` = all keyframes. |

**Loop modes:**

| Mode          | Description                                                                                         |
|---------------|-----------------------------------------------------------------------------------------------------|
| `"cycle"`     | Repeats the keyframe segment verbatim. Values jump back to the start of the segment at each cycle boundary. |
| `"pingpong"`  | Alternates forward and backward through the segment, creating a seamless back-and-forth oscillation. |
| `"offset"`    | Repeats the keyframe cycle with cumulative value shift. If keyframes go 0→100, the second cycle plays 100→200, the third 200→300, etc. Useful for continuously accumulating motion. |
| `"continue"`  | Linear extrapolation at the velocity of the boundary keyframe. After the last keyframe, the value continues at the same speed indefinitely. |

**Returns:** `double` -- The looped value.

**Examples:**

```js
// Cycle all keyframes after the last keyframe
loopOut("cycle", 0)

// Ping-pong the last 3 keyframes
loopOut("pingpong", 3)

// Continuously accumulating offset (e.g., endless scrolling)
loopOut("offset", 0)

// Continue at the same velocity past the last keyframe
loopOut("continue")
```

---

#### loopIn

```
loopIn(type: string, numKeyframes: int) -> double
```

Repeats the keyframe animation cycle before the first keyframe. The loop segment is defined by the first `numKeyframes` keyframes. If `numKeyframes` is `0`, all keyframes are used.

**Parameters:**

| Parameter      | Type     | Default    | Description                                                       |
|----------------|----------|------------|-------------------------------------------------------------------|
| `type`         | `string` | `"cycle"`  | Loop mode: `"cycle"`, `"pingpong"`, `"offset"`, or `"continue"`. |
| `numKeyframes` | `int`    | `0`        | Number of keyframes from the start to define the loop segment. `0` = all keyframes. |

**Returns:** `double` -- The looped value.

**Examples:**

```js
// Cycle all keyframes before the first keyframe
loopIn("cycle", 0)

// Linear extrapolation before the first keyframe
loopIn("continue")
```

**Notes:**
- `loopIn` mirrors `loopOut` but operates on the time region before the first keyframe.
- The `"offset"` mode shifts cumulatively in reverse: each preceding cycle subtracts the segment's value range.
- The `"continue"` mode extrapolates backward at the velocity of the first keyframe.

---

#### loopOutDuration

```
loopOutDuration(type: string, duration: double) -> double
```

Like `loopOut` but the loop segment is specified by time duration (in seconds) instead of keyframe count. The segment spans from `lastKeyTime - duration` to `lastKeyTime`.

**Parameters:**

| Parameter  | Type     | Default    | Description                                                       |
|------------|----------|------------|-------------------------------------------------------------------|
| `type`     | `string` | `"cycle"`  | Loop mode: `"cycle"`, `"pingpong"`, `"offset"`, or `"continue"`. |
| `duration` | `double` | `0`        | Duration of the loop segment in seconds. `0` means all keyframes (equivalent to `loopOut(type, 0)`). |

**Returns:** `double` -- The looped value.

**Examples:**

```js
// Loop the last 2 seconds of keyframed animation
loopOutDuration("cycle", 2.0)

// Ping-pong the last 1.5 seconds
loopOutDuration("pingpong", 1.5)
```

---

#### loopInDuration

```
loopInDuration(type: string, duration: double) -> double
```

Like `loopIn` but the loop segment is specified by time duration (in seconds) instead of keyframe count. The segment spans from `firstKeyTime` to `firstKeyTime + duration`.

**Parameters:**

| Parameter  | Type     | Default    | Description                                                       |
|------------|----------|------------|-------------------------------------------------------------------|
| `type`     | `string` | `"cycle"`  | Loop mode: `"cycle"`, `"pingpong"`, `"offset"`, or `"continue"`. |
| `duration` | `double` | `0`        | Duration of the loop segment in seconds. `0` means all keyframes (equivalent to `loopIn(type, 0)`). |

**Returns:** `double` -- The looped value.

**Examples:**

```js
// Loop the first 3 seconds of keyframed animation before it starts
loopInDuration("cycle", 3.0)
```

---

### Coordinate Conversion

#### toComp

```
toComp(point) -> array or double
```

Converts normalized `[0-1]` coordinates to pixel coordinates using `thisProject.width` and `thisProject.height`.

**Polymorphic:** Accepts an `[x, y]` array or a scalar.
- Array: `[x, y]` → `[x * thisProject.width, y * thisProject.height]`
- Scalar: `value` → `value * thisProject.width`

**Parameters:**

| Parameter | Type               | Description                              |
|-----------|--------------------|------------------------------------------|
| `point`   | `Array` or `Number` | Normalized coordinate(s) in `[0, 1]`.   |

**Returns:** `Array` or `Number` -- Pixel coordinate(s).

**Examples:**

```js
// Convert normalized center to pixel coordinates
toComp([0.5, 0.5])    // → [960, 540] for a 1920x1080 project

// Convert a scalar normalized X position
toComp(0.25)           // → 480 for a 1920-wide project
```

---

#### fromComp

```
fromComp(point) -> array or double
```

Converts pixel coordinates to normalized `[0-1]` coordinates using `thisProject.width` and `thisProject.height`.

**Polymorphic:** Accepts an `[x, y]` array or a scalar.
- Array: `[x, y]` → `[x / thisProject.width, y / thisProject.height]`
- Scalar: `value` → `value / thisProject.width`

**Parameters:**

| Parameter | Type               | Description                              |
|-----------|--------------------|------------------------------------------|
| `point`   | `Array` or `Number` | Pixel coordinate(s).                     |

**Returns:** `Array` or `Number` -- Normalized coordinate(s) in `[0, 1]`.

**Examples:**

```js
// Convert pixel position to normalized
fromComp([960, 540])   // → [0.5, 0.5] for a 1920x1080 project

// Convert a pixel X position to normalized
fromComp(480)          // → 0.25 for a 1920-wide project
```

---

#### sourceRectAtTime

```
sourceRectAtTime(t: double, includeExtents: bool) -> object
```

Returns the clip's source rectangle as `{top, left, width, height}` in pixel coordinates. Uses `thisClip.width` and `thisClip.height` for the dimensions.

**Parameters:**

| Parameter        | Type     | Description                                                                 |
|------------------|----------|-----------------------------------------------------------------------------|
| `t`              | `double` | Time in seconds. Accepted for AE compatibility but currently ignored -- clip dimensions are constant in Kdenlive. |
| `includeExtents` | `bool`   | Include extents (masks, effects). Accepted for AE compatibility but currently ignored. |

**Returns:** Object with properties `top`, `left`, `width`, `height` (all in pixels).

**Examples:**

```js
// Get source clip dimensions
var rect = sourceRectAtTime(time, false);
rect.width    // clip width in pixels
rect.height   // clip height in pixels

// Center a text overlay relative to clip dimensions
var rect = sourceRectAtTime(time, false);
rect.left + rect.width / 2
```

**Notes:**
- The `t` parameter exists for After Effects compatibility. In Kdenlive, clip dimensions do not change over time, so the same rectangle is returned regardless of the time value.

---

### Math Object

The full JavaScript `Math` object is available via the QuickJS runtime. All standard constants and methods can be used directly in expressions.

#### Constants

| Constant      | Value                    | Description                  |
|---------------|--------------------------|------------------------------|
| `Math.PI`     | `3.141592653589793`      | Ratio of circle circumference to diameter. |
| `Math.E`      | `2.718281828459045`      | Euler's number (base of natural logarithm). |

#### Trigonometric Functions

| Function          | Description                                      |
|-------------------|--------------------------------------------------|
| `Math.sin(x)`     | Sine of `x` (radians). Returns `[-1, 1]`.       |
| `Math.cos(x)`     | Cosine of `x` (radians). Returns `[-1, 1]`.     |
| `Math.tan(x)`     | Tangent of `x` (radians).                        |
| `Math.asin(x)`    | Arcsine. Returns radians.                        |
| `Math.acos(x)`    | Arccosine. Returns radians.                      |
| `Math.atan(x)`    | Arctangent. Returns radians.                     |
| `Math.atan2(y, x)`| Two-argument arctangent. Returns radians.        |

#### Rounding and Absolute Value

| Function          | Description                                      |
|-------------------|--------------------------------------------------|
| `Math.abs(x)`     | Absolute value of `x`.                           |
| `Math.floor(x)`   | Largest integer less than or equal to `x`.       |
| `Math.ceil(x)`    | Smallest integer greater than or equal to `x`.   |
| `Math.round(x)`   | Rounds `x` to the nearest integer.               |

#### Exponential and Logarithmic

| Function          | Description                                      |
|-------------------|--------------------------------------------------|
| `Math.sqrt(x)`    | Square root of `x`.                              |
| `Math.pow(x, y)`  | `x` raised to the power `y`.                     |
| `Math.log(x)`     | Natural logarithm (base e) of `x`.              |
| `Math.exp(x)`     | `e` raised to the power `x`.                     |

#### Min and Max

| Function            | Description                                    |
|---------------------|------------------------------------------------|
| `Math.min(a, b)`    | Returns the smaller of `a` and `b`.            |
| `Math.max(a, b)`    | Returns the larger of `a` and `b`.             |

#### Math.random()

`Math.random()` is available but **not recommended** for expressions. It is non-deterministic -- different calls on the same frame may return different values between preview and final render. Use the built-in `random()` function instead, which is seeded deterministically from the frame number and clip index.

**Examples using Math:**

```js
// Sinusoidal oscillation: 2 cycles over the clip duration (normalized)
0.5 + Math.sin(time / duration * Math.PI * 4) * 0.5

// Circular motion (use on separate X and Y parameters, normalized)
// X: 0.5 + Math.cos(time * 2) * 0.5
// Y: 0.5 + Math.sin(time * 2) * 0.5

// Exponential decay (flash that fades out, normalized)
Math.exp(-time * 3) * value

// Bounce approximation using absolute sine (normalized)
Math.abs(Math.sin(time * Math.PI * 3)) * value
```

---

## Project Properties

The `thisProject` object provides read-only access to project-level properties. It is the Kdenlive equivalent of After Effects' `thisComp` object. All properties are available in every expression context without imports.

### thisProject.width

```
thisProject.width -> int
```

Project resolution width in pixels.

**AE Equivalent:** `thisComp.width`

**Examples:**

```js
// Center an element horizontally (pixel-based position parameter)
thisProject.width / 2

// Scale factor relative to a 1920-wide reference design
thisProject.width / 1920
```

---

### thisProject.height

```
thisProject.height -> int
```

Project resolution height in pixels.

**AE Equivalent:** `thisComp.height`

**Examples:**

```js
// Place an element at the vertical center
thisProject.height / 2

// Responsive lower-third: 10% from the bottom edge
thisProject.height * 0.9
```

---

### thisProject.fps

```
thisProject.fps -> double
```

Project frame rate (frames per second). Equivalent to the global `fps` variable.

**AE Equivalent:** `1 / thisComp.frameDuration`

**Examples:**

```js
// Convert a duration in seconds to frames
var holdFrames = 2.5 * thisProject.fps;
```

---

### thisProject.duration

```
thisProject.duration -> double
```

Total timeline duration in seconds (from start to the end of the last clip).

**AE Equivalent:** `thisComp.duration`

**Examples:**

```js
// Ramp a value over the entire timeline, not just the current clip
linear(time, 0, thisProject.duration, 0, 1)

// Progress indicator: how far into the timeline are we (0 to 1)
time / thisProject.duration
```

---

### thisProject.frameDuration

```
thisProject.frameDuration -> double
```

Duration of one frame in seconds (`1 / fps`).

**AE Equivalent:** `thisComp.frameDuration`

**Examples:**

```js
// Sample the value one frame ahead
valueAtTime(time + thisProject.frameDuration)

// Time step for manual integration
var dt = thisProject.frameDuration;
```

---

### thisProject.pixelAspect

```
thisProject.pixelAspect -> double
```

Pixel aspect ratio. `1.0` means square pixels (the most common case). Values other than 1.0 indicate non-square pixels (e.g., anamorphic footage).

**AE Equivalent:** `thisComp.pixelAspect`

**Examples:**

```js
// Correct a circle radius for non-square pixels
var correctedRadius = radius * thisProject.pixelAspect;
```

---

### thisProject.name

```
thisProject.name -> string
```

The project file name (without directory path), e.g. `"my-project.kdenlive"`.

**AE Equivalent:** `thisComp.name`

**Examples:**

```js
// Log project name for debugging (value is still the parameter output)
thisProject.name; value
```

---

### thisProject.fullPath

```
thisProject.fullPath -> string
```

Absolute path to the project file, e.g. `"/home/user/projects/my-project.kdenlive"`.

**AE Equivalent:** `thisProject.fullPath`

---

### thisProject.numTracks

```
thisProject.numTracks -> int
```

Total number of tracks in the timeline (video + audio combined).

**AE Equivalent:** `thisComp.numLayers`

**Examples:**

```js
// Scale intensity by total track count (e.g., more tracks = less opacity per track)
1.0 / thisProject.numTracks
```

---

### thisProject.displayStartTime

```
thisProject.displayStartTime -> double
```

Timeline start time offset in seconds. Most projects start at `0`, but some workflows use a non-zero start offset.

**AE Equivalent:** `thisComp.displayStartTime`

**Examples:**

```js
// Absolute timeline time (accounting for display offset)
var absoluteTime = time + thisProject.displayStartTime;
```

---

### thisProject.bgColor

```
thisProject.bgColor -> [double, double, double, double]
```

Background color as a normalized RGBA array where each component is in the range `[0.0, 1.0]`. The default is `[0, 0, 0, 1]` (opaque black).

**AE Equivalent:** `thisComp.bgColor`

**Examples:**

```js
// Read the red channel of the background color
thisProject.bgColor[0]

// Check if background is dark (luminance below 0.5)
var bg = thisProject.bgColor;
var luma = bg[0] * 0.299 + bg[1] * 0.587 + bg[2] * 0.114;
(luma < 0.5) ? 1.0 : 0.0
```

---

## Cross-Clip References

### clip

```
clip(name: string) -> ClipRef
clip(index: int) -> ClipRef
clip(referenceObject: ClipRef, relativeIndex: int) -> ClipRef
```

Returns a reference to another clip on the timeline, allowing you to read its effect parameters.

**String form:** Looks up a clip by source file name across all tracks.

**Numeric form:** Looks up a clip by 0-based position on the **same track** as the expression's clip (counted left to right). This is useful for relative references such as "the previous clip" or "the next clip".

**Relative form (two-argument):** Looks up a clip by offset relative to a reference clip. The `referenceObject` can be `thisClip` or any `ClipRef` returned by `clip()`. The `relativeIndex` is an integer offset from the reference clip's position on its track (positive = right, negative = left).

**Parameters:**

| Parameter         | Type             | Description                                                              |
|-------------------|------------------|--------------------------------------------------------------------------|
| `name`            | `string`         | Source file name of the target clip (searched across all tracks).         |
| `index`           | `int`            | 0-based position on the same track as the current clip (left to right).  |
| `referenceObject` | `ClipRef`        | A clip reference (`thisClip` or result of `clip()`). Used as the base for relative lookup. |
| `relativeIndex`   | `int`            | Offset from the reference clip's position. `1` = next clip to the right, `-1` = previous clip to the left. |

**Returns:** A `ClipRef` object. Use `.effect(name).param(name)` to read a parameter value from the referenced clip.

**AE Equivalent:**
- `clip("name")` corresponds to `thisComp.layer("name")`
- `clip(index)` corresponds to `thisComp.layer(index)`
- `clip(thisClip, relIndex)` corresponds to `thisComp.layer(thisLayer, relIndex)` -- e.g., `thisComp.layer(thisLayer, 1)` gets the layer below

**Examples:**

```js
// Read brightness from a clip named "intro.mp4"
clip("intro.mp4").effect("brightness").param("level")

// Read the brightness of the previous clip on the same track
clip(index - 1).effect("brightness").param("level")

// Match the opacity of the next clip
clip(index + 1).effect("qtblend").param("opacity")

// Relative reference: previous clip (AE-style)
index > 0 ? clip(thisClip, -1).effect("brightness").param("brightness") : value

// Relative reference: next clip
clip(thisClip, 1).effect("qtblend").param("opacity")

// Interpolate between neighbors
var prev = clip(thisClip, -1).effect("brightness").param("brightness");
var next = clip(thisClip, 1).effect("brightness").param("brightness");
(prev + next) / 2

// Chain from a known clip: get the 3rd clip on the track
clip(clip(0), 2).effect("blur").param("radius")

// Gradual parameter progression across clips on a track:
// Each clip reads the previous clip's blur and adds 2
var prevBlur = (index > 0) ? clip(index - 1).effect("boxblur").param("radius") : 0;
prevBlur + 2
```

**Notes:**
- If the referenced clip does not exist (e.g., `index - 1` when the clip is first on the track, or `clip(thisClip, 1)` when the clip is last), the expression will throw a `RangeError`. Guard with an `index > 0` check when using relative references.
- The numeric form and the relative form only search the same track. Use the string form to reference clips on other tracks.
- Changes to the referenced clip's parameter are reflected in real time -- there is no caching across clips.
- The relative form operates on the same track as the reference clip, sorted by timeline position (left to right, 0-based). `clip(thisClip, 1)` is equivalent to `clip(index + 1)` but is more portable when used inside helper functions or when the reference clip is not `thisClip`.

---

## thisProperty Object

The `thisProperty` object represents the property (parameter) that contains the current expression. It provides object-oriented access to property methods that also exist as global functions. This enables direct compatibility with After Effects expressions that use `thisProperty.wiggle()`, `thisProperty.loopOut()`, and similar patterns.

All methods and properties on `thisProperty` delegate to the corresponding global function or variable. The object exists purely for AE compatibility -- there is no functional difference between `thisProperty.wiggle(3, 50)` and `wiggle(3, 50)`.

### Properties

| Property               | Type     | Description                                                        |
|------------------------|----------|--------------------------------------------------------------------|
| `thisProperty.value`   | `double` | Current value of the property. Same as the global `value` variable. Updated each frame. |
| `thisProperty.numKeys` | `int`    | Number of keyframes on this property. Same as the global `numKeys`. Updated when keyframes change. |
| `thisProperty.velocity`| `double` | Instantaneous rate of change (first derivative) at the current time. Calculated using central difference. Updated per-frame. Equivalent to `velocityAtTime(time)`. |
| `thisProperty.speed`   | `double` | Absolute value of velocity (always non-negative). Updated per-frame. Equivalent to `speedAtTime(time)`. |

### Methods

All methods have identical signatures and behavior to their global counterparts. See the corresponding global function documentation for full details.

| Method                                               | Delegates to          |
|------------------------------------------------------|-----------------------|
| `thisProperty.wiggle(freq, amp, octaves?, ampMult?, t?)` | `wiggle()`        |
| `thisProperty.temporalWiggle(freq, amp, octaves?, ampMult?, t?)` | `temporalWiggle()` |
| `thisProperty.smooth(width?, samples?)`              | `smooth()`            |
| `thisProperty.valueAtTime(t)`                        | `valueAtTime()`       |
| `thisProperty.velocityAtTime(t)`                     | `velocityAtTime()`    |
| `thisProperty.speedAtTime(t)`                        | `speedAtTime()`       |
| `thisProperty.loopIn(type?, nKeys?)`                 | `loopIn()`            |
| `thisProperty.loopOut(type?, nKeys?)`                | `loopOut()`           |
| `thisProperty.loopInDuration(type?, dur?)`           | `loopInDuration()`    |
| `thisProperty.loopOutDuration(type?, dur?)`          | `loopOutDuration()`   |
| `thisProperty.key(index)`                            | `key()`               |
| `thisProperty.nearestKey(t)`                         | `nearestKey()`        |

**AE Equivalent:** `thisProperty` in After Effects.

**Why it matters:** After Effects expressions frequently use `thisProperty.wiggle()` and `thisProperty.loopOut()` patterns. Without this object, users would need to rewrite every AE expression that uses these patterns to call the global functions instead. With `thisProperty`, most AE expressions work as-is.

### Examples

```js
// AE-style wiggle (identical to global wiggle(3, 50))
thisProperty.wiggle(3, 50)

// AE-style loop (identical to global loopOut("cycle"))
thisProperty.loopOut("cycle")

// Conditional based on keyframe count
thisProperty.numKeys > 0 ? thisProperty.loopOut("pingpong") : value

// Smooth the current property value
thisProperty.smooth(0.3, 5)

// Read value at a different time
thisProperty.valueAtTime(time - 0.5)

// Use velocity for motion blur intensity
Math.abs(thisProperty.velocity) * 0.1

// Conditional effect based on speed
thisProperty.speed > 50 ? wiggle(4, 0.1) : value

// Scale overshoot based on velocity
var n = nearestKey(time);
var overshoot = Math.sin((time - n.time) * Math.PI * 4) * thisProperty.velocity * 0.1;
thisProperty.value + overshoot

// Combine multiple property methods
thisProperty.numKeys > 0 ? thisProperty.loopOut("cycle") : thisProperty.wiggle(2, 0.1)
```

---

## Marker Properties

### protectedRegion (alias)

Marker key objects returned by `marker.key(index)` and `marker.nearestKey(t)` now include a `protectedRegion` property as an alias for `duration`. This provides compatibility with After Effects expressions that use `marker.key(n).protectedRegion`.

```js
// These are equivalent:
marker.key(1).duration
marker.key(1).protectedRegion

// AE-compatible pattern: check if marker has a protected region
var m = marker.key(1);
if (m.protectedRegion > 0) {
    // marker spans a time range
}
```

**AE Equivalent:** `marker.key(n).protectedRegion` in After Effects.

---

## Clip Properties

### thisClip

The `thisClip` object provides read-only access to the current clip's metadata. It is the Kdenlive equivalent of After Effects' `thisLayer` object. All properties are available in every expression context without imports.

| Property | Type | Description |
|----------|------|-------------|
| `thisClip.name` | `string` | Source clip file name (e.g. `"scene01.mp4"`). |
| `thisClip.duration` | `double` | Clip duration in seconds (same as global `duration`). |
| `thisClip.width` | `int` | Source media width in pixels. |
| `thisClip.height` | `int` | Source media height in pixels. |
| `thisClip.index` | `int` | 0-based position index of the clip on its track (same as global `index`). |
| `thisClip.inPoint` | `double` | Clip-relative in-point in seconds (always `0.0` -- Kdenlive trims are pre-applied). |
| `thisClip.outPoint` | `double` | Clip-relative out-point in seconds (= `clipDurationFrames / fps`). |
| `thisClip.startTime` | `double` | Timeline position of the clip's first frame in seconds (AE: `layer.startTime`). |
| `thisClip.hasVideo` | `bool` | Whether the source clip has a video stream. |
| `thisClip.hasAudio` | `bool` | Whether the source clip has an audio stream. |
| `thisClip.source` | `object` | Source footage metadata sub-object (see below). |
| `thisClip.source.name` | `string` | Source clip name (same as `thisClip.name`). |
| `thisClip.source.width` | `int` | Source media width in pixels (same as `thisClip.width`). |
| `thisClip.source.height` | `int` | Source media height in pixels (same as `thisClip.height`). |

**AE Equivalents:**

| Kdenlive | After Effects |
|----------|---------------|
| `thisClip.index` | `thisLayer.index` |
| `thisClip.inPoint` | `thisLayer.inPoint` |
| `thisClip.outPoint` | `thisLayer.outPoint` |
| `thisClip.startTime` | `thisLayer.startTime` |
| `thisClip.hasVideo` | `thisLayer.hasVideo` |
| `thisClip.hasAudio` | `thisLayer.hasAudio` |
| `thisClip.source` | `thisLayer.source` |

**Examples:**

```js
// Fade in based on clip timeline position
linear(time, thisClip.startTime, thisClip.startTime + 1, 0, 1)

// Scale relative to source resolution
thisClip.width / thisProject.width

// Check if clip has audio before using audio functions
thisClip.hasAudio ? audioLevel("Both", time) : 0.5
```

---

## AE Compatibility Layer

This section documents the After Effects compatibility aliases and properties that allow AE expressions to run in Kdenlive with minimal or no modifications. These aliases are global and available in every expression context.

### Global Aliases

The following global aliases map After Effects names to their Kdenlive equivalents. They reference the exact same objects -- no copies or wrappers are involved.

| AE Name | Kdenlive Equivalent | Description |
|---------|---------------------|-------------|
| `thisLayer` | `thisClip` | Current clip object. `thisLayer.startTime` is identical to `thisClip.startTime`. |
| `thisComp` | `thisProject` | Project-level properties. `thisComp.width` is identical to `thisProject.width`. |
| `layer(index)` | `clip(index)` | Look up a clip by 0-based track position. |
| `layer(name)` | `clip(name)` | Look up a clip by source file name. |

These aliases exist so that common AE expressions like `thisComp.width / 2` and `thisLayer.startTime` work without modification.

### clip(index) Metadata Properties

When using `clip(N)` to reference another clip by index, the returned `ClipRef` object exposes the following metadata properties in addition to the `.effect(name).param(name)` chain documented in the [Cross-Clip References](#cross-clip-references) section.

| Property | Type | Description |
|----------|------|-------------|
| `.name` | `string` | Source clip file name. |
| `.index` | `int` | 0-based position index on the clip's track. |
| `.inPoint` | `double` | Clip-relative in-point in seconds (always `0.0`). |
| `.outPoint` | `double` | Clip out-point in seconds (= duration). |
| `.startTime` | `double` | Timeline position of the clip's first frame in seconds. |
| `.duration` | `double` | Clip duration in seconds. |
| `.width` | `int` | Source media width in pixels. |
| `.height` | `int` | Source media height in pixels. |
| `.hasVideo` | `bool` | Whether the source has a video stream. |
| `.hasAudio` | `bool` | Whether the source has an audio stream. |
| `.source.name` | `string` | Source clip name (same as `.name`). |
| `.source.width` | `int` | Source media width (same as `.width`). |
| `.source.height` | `int` | Source media height (same as `.height`). |

**Examples:**

```js
// Access neighboring clip's duration (AE-style)
var prev = clip(index - 1);
prev.duration  // duration in seconds

// Stagger animation start based on previous clip's end time
var prev = clip(index - 1);
var offset = prev.startTime + prev.duration;
linear(time, offset, offset + 1, 0, value)

// Use thisLayer alias (AE compat)
thisLayer.startTime + thisLayer.outPoint

// Use thisComp alias (AE compat)
thisComp.width / 2

// AE-style layer reference using layer() alias
var bg = layer(0);
bg.width / bg.height  // aspect ratio of first clip on track

// Conditional based on neighbor clip properties
if (index > 0 && clip(index - 1).hasAudio) {
    audioLevel("Both", time);
} else {
    value;
}
```

**Notes:**
- The `inPoint` property always returns `0.0` because Kdenlive applies trim operations at the timeline level before expressions evaluate. This differs from After Effects where `inPoint` reflects the layer's trimmed start within the composition.
- The `source` sub-object provides AE compatibility for expressions that access `layer.source.name`, `layer.source.width`, and `layer.source.height`. In Kdenlive, these are identical to the top-level `.name`, `.width`, and `.height` properties.

---

## Expression Syntax Notes

- Expressions are standard JavaScript (ES2020) evaluated by QuickJS.
- Multiple statements can be separated with semicolons. The last evaluated value is used as the parameter output.
- The global variables (`time`, `frame`, `value`, etc.) and all built-in functions are available without any imports or prefixes.
- Expressions are evaluated independently for each frame. There is no persistent state between frames.
- Defensive expressions can use `try/catch` to handle missing references gracefully:
  ```js
  try { clip("Other Clip").effect("Blur").param("amount"); } catch(e) { value; }
  ```
  This is standard JavaScript — QuickJS supports full try/catch/finally.
