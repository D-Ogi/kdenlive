# Polymorphic Functions (AE-Compatible)

This document covers expression functions that accept both scalar (single number) and array (vector) arguments. These functions follow Adobe After Effects semantics for polymorphism and provide seamless interoperability between 1D, 2D, 3D, and higher-dimensional values.

---

## random()

Generates deterministic pseudo-random values seeded by frame number and clip index. Supports three signatures with full scalar/array polymorphism.

### Signatures

```javascript
random() -> Number
random(maxValOrArray) -> Number | Array
random(minValOrArray, maxValOrArray) -> Number | Array
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `maxValOrArray` | `Number` or `Array` | Upper bound. If scalar, returns scalar in `[0, max]`. If array, returns array with each component in `[0, max[i]]`. |
| `minValOrArray` | `Number` or `Array` | Lower bound. Pairs with `maxValOrArray` for range `[min, max]`. |

### Returns

- **No arguments:** Number in `[0.0, 1.0]`
- **Scalar arguments:** Number in specified range
- **Array arguments:** Array with per-component random values in specified ranges

### Behavior

- **Deterministic:** Same frame always produces same output. Seeded with `hash(frame + 1, index + 1)`.
- **Array padding:** Mismatched array dimensions are padded with zeros. Example: `random([100, 200], [300])` treats second array as `[300, 0]`.
- **Per-component independence:** Each array component uses the same seed but advances the RNG state, so components are uncorrelated.

### Examples

```javascript
// Scalar: random value 0-1
random()                        // → 0.73 (example)

// Scalar: random value 0-100
random(100)                     // → 73.0

// Array: per-component [0, max]
random([100, 200])              // → [73.0, 146.0]

// Array: per-component [min, max]
random([0, 100], [50, 200])     // → [36.5, 173.0]

// Mismatched dimensions (pads with 0)
random([100, 200], [300, 400])  // → [236.0, 346.0]
random([100, 200], [300])       // → [236.0, 0.0] (second component: [200, 0])

// Randomize 2D position each frame
random([0, 0], [1920, 1080])    // → [x, y] in screen space

// Randomize RGB color components
random([0, 0, 0], [255, 255, 255])  // → [r, g, b]
```

### AE Compatibility

**Fully compatible.** Matches After Effects `random()` behavior including:
- Deterministic seeding per frame
- Scalar/array overloading
- Zero-padding for mismatched dimensions

---

## gaussRandom()

Generates Gaussian (normally distributed) random values using the Box-Muller transform. Values cluster around the center of the specified range with approximately 90% falling within `[min, max]` and 10% outside.

### Signatures

```javascript
gaussRandom() -> Number
gaussRandom(maxValOrArray) -> Number | Array
gaussRandom(minValOrArray, maxValOrArray) -> Number | Array
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `maxValOrArray` | `Number` or `Array` | Upper bound. Distribution centered at `max/2` with `σ = max/6`. |
| `minValOrArray` | `Number` or `Array` | Lower bound. Distribution centered at `(min+max)/2` with `σ = (max-min)/6`. |

### Returns

- **No arguments:** Standard normal distribution (mean 0, σ=1)
- **Scalar arguments:** Number with Gaussian distribution centered in range
- **Array arguments:** Array with per-component Gaussian distributions

### Behavior

- **Deterministic:** Same seeding as `random()`.
- **Array padding:** Same zero-padding rules as `random()`.
- **Distribution:** Uses `mean + σ * gaussSample()` where `gaussSample()` is standard normal (Box-Muller).
- **1-arg form:** `mean = max/2`, `σ = max/6` (90% in `[0, max]`)
- **2-arg form:** `mean = (min+max)/2`, `σ = (max-min)/6` (90% in `[min, max]`)

### Examples

```javascript
// Standard normal (mean 0, σ=1)
gaussRandom()                   // → -0.42 (example, ~68% in [-1,1])

// Scalar: centered at 50, ~90% in [0, 100]
gaussRandom(100)                // → 52.3

// Array: per-component Gaussian
gaussRandom([100, 200])         // → [48.7, 203.1]

// Custom range: ~90% in [0, 50] for X, [100, 200] for Y
gaussRandom([0, 100], [50, 200])  // → [23.4, 147.8]

// Natural-looking camera shake (values concentrated near base)
value + gaussRandom(-5, 5)      // Subtle jitter, rare large spikes
```

### AE Compatibility

**Fully compatible.** Matches After Effects `gaussRandom()` including:
- Box-Muller transform algorithm
- 90% containment range semantics
- Scalar/array overloading
- Zero-padding for mismatched dimensions

**Note:** After Effects documentation states "approximately 90% of values fall within the specified range" — this is σ ≈ 1.645 (z-score for 90% confidence interval), but actual implementation uses σ = range/6 ≈ 1.67σ, which yields ~90.4% containment. Kdenlive matches this implementation detail exactly.

---

## length()

Calculates Euclidean magnitude of a vector or distance between two points. Supports scalar arguments as a convenience (returns absolute value or absolute difference).

### Signatures

```javascript
length(vec) -> Number
length(point1, point2) -> Number
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `vec` | `Number` or `Array` | Vector to measure. If scalar, returns `abs(vec)`. |
| `point1` | `Number` or `Array` | First point. |
| `point2` | `Number` or `Array` | Second point. Distance is `length(sub(point1, point2))`. |

### Returns

Number — Euclidean magnitude (1-arg form) or distance (2-arg form).

### Behavior

- **1-arg, array:** `sqrt(x² + y² + z² + ...)` — standard Euclidean norm
- **1-arg, scalar:** `abs(x)` — absolute value
- **2-arg, arrays:** `sqrt((x1-x2)² + (y1-y2)² + ...)` — Euclidean distance
- **2-arg, scalars:** `abs(x1 - x2)` — absolute difference
- **Array padding:** Mismatched dimensions are zero-padded. `length([3, 4], [0])` treats second array as `[0, 0]`.

### Examples

```javascript
// Vector magnitude
length([3, 4])                  // → 5.0 (Pythagorean triple)
length([1, 2, 2])               // → 3.0 (√(1+4+4))

// Scalar absolute value
length(-42)                     // → 42.0

// Distance between two points
length([0, 0], [3, 4])          // → 5.0
length([100, 200], [400, 600])  // → 500.0 (√(300² + 400²))

// Distance in 3D
length([0, 0, 0], [1, 1, 1])    // → 1.732 (√3)

// Classic AE pattern: camera focus distance
var camPos = [0, 0, -1000];
var targetPos = position;       // Auto-focus on this layer
length(camPos, targetPos)       // Distance to use for camera depth-of-field

// 2D velocity magnitude from position keyframes
var vel = [velocityAtTime(time)[0], velocityAtTime(time)[1]];
length(vel)                     // Speed in pixels/second

// Zero-padding example
length([3, 4, 0], [0, 0])       // → 5.0 (second array treated as [0, 0, 0])
```

### AE Compatibility

**Fully compatible.** Matches After Effects `length()` behavior including:
- 1-arg form: vector magnitude or scalar absolute value
- 2-arg form: Euclidean distance between points
- Zero-padding for mismatched dimensions

**Classic use case:** In After Effects, `length(position, toComp(anchorPoint))` is a common pattern for measuring distance from layer position to a point in comp space. In Kdenlive (2D-only), use `length(thisClip.position, otherClip.position)` to measure distance between clips on timeline.

---

## clamp()

Constrains a value or array to a specified range. Works per-component on arrays, allowing independent clamping for each dimension.

### Signature

```javascript
clamp(value, limit1, limit2) -> Number | Array
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `value` | `Number` or `Array` | Value(s) to constrain. |
| `limit1` | `Number` or `Array` | Lower bound (or upper if swapped). |
| `limit2` | `Number` or `Array` | Upper bound (or lower if swapped). |

### Returns

- **All scalars:** Number clamped to `[min(limit1, limit2), max(limit1, limit2)]`
- **Any array:** Array with per-component clamping

### Behavior

- **Per-component:** Each array element is clamped independently. `clamp([150, 50], [0, 0], [100, 100])` → `[100, 50]`.
- **Array padding:** Mismatched dimensions are zero-padded. If `value` is array but limits are scalar, scalar is broadcast. If any argument is array, output is array.
- **Limit order:** `min` and `max` are computed from `limit1` and `limit2` using `std::min` and `std::max`, so order doesn't matter.

### Examples

```javascript
// Scalar clamp
clamp(150, 0, 100)              // → 100
clamp(-10, 0, 100)              // → 0
clamp(50, 0, 100)               // → 50 (unchanged)

// Array clamp (per-component)
clamp([50, 200], [0, 0], [100, 100])        // → [50, 100]
clamp([150, -10, 50], [0, 0, 0], [100, 100, 100])  // → [100, 0, 50]

// Mismatched dimensions (zero-padding)
clamp([50, 200, 75], [0, 0], [100, 100])    // → [50, 100, 0]
// Explanation: value[2]=75, min[2]=0 (padded), max[2]=0 (padded) → clamp(75,0,0)=0

// Mixed scalar/array (scalar broadcasts)
clamp([50, 200], 0, 100)        // Treated as clamp([50,200], [0,0], [100,100])
                                 // → [50, 100]

// Keep wiggle within bounds
clamp(wiggle(4, 0.2), 0, 1)     // Wiggle around value, hard-clamp to [0,1]

// Clamp RGB color to valid range
var color = [r, g, b];
clamp(color, [0, 0, 0], [255, 255, 255])

// Prevent position from leaving screen bounds
clamp(position, [0, 0], [1920, 1080])
```

### AE Compatibility

**Fully compatible.** Matches After Effects `clamp()` including:
- Per-component array clamping
- Scalar broadcasting (if limits are scalar and value is array)
- Zero-padding for mismatched dimensions

**Note:** After Effects `clamp(value, minValOrArray, maxValOrArray)` treats scalar limits as broadcast to all components. Kdenlive matches this exactly.

---

## ease(), easeIn(), easeOut() — 3-Argument Forms

Smooth interpolation with automatic `[0, 1]` input clamping. These are shorthand forms of the standard 5-argument ease functions, assuming `t` is in the normalized `[0, 1]` range.

### Signatures

```javascript
ease(t, v1, v2) -> Number
easeIn(t, v1, v2) -> Number
easeOut(t, v1, v2) -> Number
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `t` | `Number` | Normalized input time in `[0, 1]`. Values outside this range are clamped. |
| `v1` | `Number` | Output value when `t = 0`. |
| `v2` | `Number` | Output value when `t = 1`. |

### Returns

Number — smoothly interpolated value between `v1` and `v2`.

### Behavior

- **Input clamping:** `t` is clamped to `[0, 1]` before interpolation.
- **ease(t, v1, v2):** Hermite interpolation `3t² - 2t³` — smooth start and end (S-curve).
- **easeIn(t, v1, v2):** Quadratic ease-in `t²` — slow start, accelerating end.
- **easeOut(t, v1, v2):** Quadratic ease-out `1 - (1-t)²` — fast start, decelerating end.
- **Equivalent to:** `ease(t, v1, v2)` = `ease(t, 0, 1, v1, v2)` where 5-arg form does the clamping internally.

### Examples

```javascript
// Smooth 0→1 ramp over clip duration (assumes time normalized to [0,1])
var t = time / duration;
ease(t, 0, 1)                   // → S-curve from 0 to 1

// Accelerating fade-in
var t = time / 2.0;             // 2-second duration
easeIn(t, 0, 1)                 // Slow start, fast end

// Decelerating slide-out
var t = (time - 3.0) / 1.5;     // Start at 3s, 1.5s duration
easeOut(t, 0, 800)              // Fast start (position jumps), settles at 800px

// Safe with out-of-range t (clamped to [0,1])
ease(5.0, 0, 100)               // → 100 (t clamped to 1.0)
ease(-2.0, 0, 100)              // → 0 (t clamped to 0.0)

// Smooth opacity fade using normalized time
var t = time / duration;
ease(t, 0.0, 1.0)               // Opacity 0→1 with smooth start/end

// Compare all three curves (same t, same range)
var t = 0.5;
ease(t, 0, 100)                 // → 50 (symmetrical)
easeIn(t, 0, 100)               // → 25 (below midpoint, still accelerating)
easeOut(t, 0, 100)              // → 75 (above midpoint, already slowing)
```

### AE Compatibility

**Fully compatible.** After Effects supports both 3-arg and 5-arg forms:
- `ease(t, value1, value2)` — 3-arg form (t in `[0,1]`)
- `ease(t, tMin, tMax, value1, value2)` — 5-arg form (t in `[tMin, tMax]`)

Kdenlive implements both forms identically. The 3-arg form is syntactic sugar for the 5-arg form with `tMin=0, tMax=1`.

**Note:** These functions only accept scalar `t`. For per-component easing of arrays, use explicit component access:
```javascript
var t = time / duration;
[ease(t, 0, 1920), ease(t, 0, 1080)]  // Smooth 2D position animation
```

---

## lookAt()

Calculates 3D orientation (rotation) so that the Z-axis of a layer points from `fromPoint` toward `atPoint`. Returns Euler angles in degrees suitable for 3D camera or light orientation.

### Signature

```javascript
lookAt(fromPoint, atPoint) -> [xRotation, yRotation, 0]
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `fromPoint` | `Array` | 3D position `[x, y, z]` of the object looking (camera or light). |
| `atPoint` | `Array` | 3D position `[x, y, z]` of the target point of interest. |

### Returns

Array `[xRotation, yRotation, 0]` in degrees — Euler angles to orient Z-axis toward target.

### Behavior

- **Requires 3D arrays:** Both arguments must have at least 3 components `[x, y, z]`. Throws `TypeError` if not.
- **Z-axis pointing:** Returned rotations orient the Z-axis (depth) to point from `fromPoint` to `atPoint`.
- **X rotation (pitch):** Angle around X-axis (up/down tilt). Negative because After Effects Y-axis points down.
- **Y rotation (yaw):** Angle around Y-axis (left/right pan).
- **Z rotation:** Always `0.0` (roll is not constrained by look-at).
- **Zero distance:** If points are coincident, returns `[0, 0, 0]`.

### Examples

```javascript
// Camera auto-aiming at layer position
var camPos = [0, 0, -1000];
var target = thisLayer.position;  // 2D position, treated as [x, y, 0]
lookAt(camPos, target)             // → [xRot, yRot, 0] to aim camera at layer

// Light tracking a moving object
var lightPos = [500, -200, -500];
var objectPos = [x, y, z];  // Retrieved from another layer
lookAt(lightPos, objectPos)  // Orient light toward object

// Point layer toward center of comp
lookAt(position, [960, 540, 0])  // Aim at 1920x1080 center

// 3D orbit camera always facing origin
var angle = time * 30;  // 30 deg/sec
var radius = 1000;
var camPos = [
    radius * Math.sin(degreesToRadians(angle)),
    0,
    radius * Math.cos(degreesToRadians(angle))
];
lookAt(camPos, [0, 0, 0])  // Always point at origin while orbiting
```

### AE Compatibility

**Fully compatible.** Matches After Effects `lookAt(fromPoint, atPoint)` including:
- Returns `[xRotation, yRotation, zRotation]` where `zRotation = 0`
- X rotation is pitch (up/down), Y rotation is yaw (left/right)
- Coordinate system: Y-down (negated pitch angle)
- Used for auto-aiming cameras and lights in 3D space

**Classic AE use case:**
```javascript
// On a camera layer's Orientation property:
var camPos = transform.position;
var targetPos = thisComp.layer("Target Null").transform.position;
lookAt(camPos, targetPos)
```

**Kdenlive limitation:** Kdenlive does not currently support 3D cameras or lights, so this function is primarily future-proofing for 3D effect plugins or for calculation purposes (e.g., deriving angles for 2D rotation based on spatial logic).

---

## timeToTimecode()

Converts a time value (in seconds) to an SMPTE timecode string in `HH:MM:SS:FF` format.

### Signature

```javascript
timeToTimecode(t, timecodeBase = 30, isDuration = false) -> String
```

### Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `t` | `Number` | (required) | Time in seconds to convert. |
| `timecodeBase` | `Number` | `30` | Frame rate for timecode conversion (frames per second). |
| `isDuration` | `Boolean` | `false` | If `true`, treat as duration (round up frames). If `false`, treat as absolute time (round down). |

### Returns

String in format `"HH:MM:SS:FF"` where:
- `HH` = hours (zero-padded to 2 digits)
- `MM` = minutes (00-59, zero-padded)
- `SS` = seconds (00-59, zero-padded)
- `FF` = frames (00 to `timecodeBase-1`, zero-padded)

If `t` is negative, the string is prefixed with `-`.

### Behavior

- **Absolute time (isDuration=false):** Frames are floored. `t=1.99` at 30fps → `00:00:01:29` (1s + 29 frames).
- **Duration (isDuration=true):** Frames are ceiled. `t=1.01` at 30fps → `00:00:01:01` (rounds up to next frame).
- **Frame calculation:**
  - Absolute: `totalFrames = floor(abs(t) * timecodeBase)`
  - Duration: `totalFrames = ceil(abs(t) * timecodeBase)`
- **Breakdown:** `frames = totalFrames % base`, `seconds = (totalFrames / base) % 60`, `minutes = (totalFrames / (base*60)) % 60`, `hours = totalFrames / (base*3600)`.

### Examples

```javascript
// Current time as timecode (default 30fps)
timeToTimecode(time)            // → "00:01:23:15" (1m 23s 15fr)

// Custom frame rate (25fps, PAL)
timeToTimecode(time, 25)        // → "00:01:23:12"

// Duration (rounds up)
timeToTimecode(1.01, 30, true)  // → "00:00:01:01" (2 frames, rounded up)
timeToTimecode(1.01, 30, false) // → "00:00:01:00" (1 frame, rounded down)

// Negative time
timeToTimecode(-5.5, 30)        // → "-00:00:05:15"

// Display timecode in a text layer expression
var tc = timeToTimecode(time, fps, false);
"Current Time: " + tc;          // → "Current Time: 00:01:23:15"

// Calculate duration between two markers
var inTime = marker.key(1).time;
var outTime = marker.key(2).time;
var dur = outTime - inTime;
timeToTimecode(dur, fps, true); // Duration as timecode (rounded up)

// 24fps film timecode
timeToTimecode(time, 24)        // → "00:01:23:12" (24fps)
```

### AE Compatibility

**Fully compatible.** Matches After Effects `timeToTimecode(t, timecodeBase = 30, isDuration = false)` including:
- SMPTE timecode format `HH:MM:SS:FF`
- Frame rounding behavior (floor for absolute, ceil for duration)
- Negative time handling (prefix with `-`)

**Note:** After Effects defaults to `timecodeBase=30` for NTSC compatibility. Kdenlive matches this default but you should use `fps` (project frame rate) for accuracy:
```javascript
timeToTimecode(time, fps)  // Use project frame rate
```

**Drop-frame timecode:** After Effects supports drop-frame NTSC timecode (29.97fps with frame drops). Kdenlive `timeToTimecode()` does NOT implement drop-frame — it is non-drop-frame only. For 29.97fps projects, use `timecodeBase=29.97` but output will be non-drop (no semicolon separator, no frame skipping).

---

## Summary Table

| Function | Signatures | Polymorphism | AE Compatible |
|----------|-----------|--------------|---------------|
| `random()` | `()` `(max)` `(min, max)` | Scalar or array arguments, returns matching type | Yes |
| `gaussRandom()` | `()` `(max)` `(min, max)` | Scalar or array arguments, returns matching type | Yes |
| `length()` | `(vec)` `(point1, point2)` | Scalar or array arguments, always returns scalar | Yes |
| `clamp()` | `(val, min, max)` | Per-component on arrays | Yes |
| `ease()` | `(t, v1, v2)` | Scalar only (3-arg form) | Yes |
| `easeIn()` | `(t, v1, v2)` | Scalar only (3-arg form) | Yes |
| `easeOut()` | `(t, v1, v2)` | Scalar only (3-arg form) | Yes |
| `lookAt()` | `(fromPoint, atPoint)` | Requires 3D arrays, returns array | Yes |
| `timeToTimecode()` | `(t, base?, isDur?)` | Scalar only, returns string | Yes (non-drop-frame) |

---

## Implementation Notes

### Zero-Padding Rule

When array dimensions mismatch, the shorter array is zero-padded:
```javascript
random([100, 200], [300])       // Treated as random([100,200], [300,0])
clamp([50, 200, 75], [0], [100]) // Treated as clamp([50,200,75], [0,0,0], [100,0,0])
```

This matches After Effects behavior and allows flexible dimension mixing.

### Deterministic Seeding

`random()` and `gaussRandom()` use deterministic seeding:
```cpp
uint32_t seed = hashSeed(frame + 1, index + 1);
```

- `frame` — current frame number (0-based)
- `index` — clip index on track (0-based)

This ensures:
- Same frame always produces same random value (reproducible renders)
- Different clips on same track have different random sequences
- Same clip at different positions has different random sequences

To override, use `seedRandom(seed, timeless)` before calling `random()`.

### Per-Component RNG State

For array arguments, `random()` and `gaussRandom()` advance the RNG state for each component, ensuring uncorrelated values:
```javascript
random([100, 200])  // Component 0 uses xorshift(seed), component 1 uses xorshift(xorshift(seed))
```

Components are **not** correlated. If you need correlated random values, use:
```javascript
var r = random();       // Single random value
[r * 100, r * 200]      // Correlated components
```

---

## Related Functions

These functions complement the polymorphic set:

- **Vector math:** `add(a, b)`, `sub(a, b)`, `mul(a, s)`, `div(a, s)`, `normalize(vec)`, `dot(a, b)`, `cross(a, b)` — all polymorphic (see main reference)
- **Keyframe introspection:** `valueAtTime(t)`, `velocityAtTime(t)`, `speedAtTime(t)` — scalar only (see Keyframe Access section)
- **5-arg ease forms:** `ease(t, tMin, tMax, v1, v2)` — scalar only (see Interpolation section)

For full function reference, see `reference.md`.
