# Keyframe Access Functions

These functions provide After Effects-compatible keyframe introspection for the Kdenlive Expression Engine. They enable dynamic expressions like overshoot, bounce, elastic, and inertia that respond to the parameter's own keyframe data.

## Prerequisites

Expressions using these functions require keyframes on the parameter. The engine automatically detects usage and injects the `_keyframes` array. If no keyframes exist, functions degrade gracefully (see Edge Cases).

## API Reference

### numKeys

- Type: `int` (global property, not a function)
- Returns the number of keyframes on the current parameter
- Updated automatically by the engine when keyframes are set
- Returns 0 when no keyframes exist

### key(index)

```
key(index: int) -> {time: double, index: int, value: double}
```

Returns a keyframe object by 1-based index.

- `index` -- 1-based keyframe index (1 = first keyframe, numKeys = last)
- Returns object with `.time` (seconds), `.index` (1-based), `.value`
- Throws RangeError if index < 1 or index > numKeys
- Throws RangeError if no keyframes exist

### nearestKey(t)

```
nearestKey(t: double) -> {time: double, index: int, value: double}
```

Returns the keyframe nearest to time `t` (by absolute time difference).

- `t` -- time in seconds
- Returns same object structure as key()
- Throws RangeError if no keyframes exist

### valueAtTime(t)

```
valueAtTime(t: double) -> double
```

Returns the linearly interpolated value of the parameter's keyframes at time `t`.

- Uses linear interpolation between adjacent keyframes
- Clamps to first/last keyframe value outside the keyframe range
- Falls back to current `value` if no keyframes exist

### velocityAtTime(t)

```
velocityAtTime(t: double) -> double
```

Returns the rate of change (first derivative) of the parameter at time `t`.

- Uses central difference method: `(v(t+dt) - v(t-dt)) / (2*dt)` where `dt = 0.5/fps`
- Returns 0.0 if fewer than 2 keyframes exist
- Sign indicates direction: positive = increasing, negative = decreasing

### speedAtTime(t)

```
speedAtTime(t: double) -> double
```

Returns the absolute rate of change at time `t`. Always non-negative.

- Equivalent to `Math.abs(velocityAtTime(t))`
- Returns 0.0 if fewer than 2 keyframes exist

## Edge Cases

| Scenario | Behavior |
|---|---|
| No keyframes -> key(1) | RangeError "no keyframes available" |
| No keyframes -> nearestKey(0) | RangeError "no keyframes available" |
| No keyframes -> valueAtTime(t) | Returns current `value` global |
| No keyframes -> velocityAtTime(t) | Returns 0.0 |
| Single keyframe -> velocityAtTime(t) | Returns 0.0 (constant, delta is 0) |
| key(0) or key(numKeys+1) | RangeError "index out of range" |

## Recipes

### Overshoot

Damped sine oscillation after each keyframe -- the classic springy settling effect.

```js
var freq = 4;    // oscillation frequency (Hz)
var decay = 6;   // decay rate (higher = faster settling)
var n = nearestKey(time);
if (time > n.time) {
    var dt = time - n.time;
    var amp = velocityAtTime(n.time) / (Math.PI * 2 * freq);
    value + amp * Math.sin(dt * Math.PI * 2 * freq) * Math.exp(-decay * dt);
} else {
    value;
}
```

### Bounce

Always-positive bounce -- like an object landing and bouncing.

```js
var freq = 5;
var decay = 5;
var n = nearestKey(time);
if (time > n.time) {
    var dt = time - n.time;
    var amp = velocityAtTime(n.time) / (Math.PI * 2 * freq);
    value + Math.abs(amp * Math.sin(dt * Math.PI * 2 * freq)) * Math.exp(-decay * dt);
} else {
    value;
}
```

### Elastic Snap

Amplitude scales with velocity -- fast keyframe transitions ring more.

```js
var freq = 3;
var decay = 4;
var n = nearestKey(time);
if (time > n.time) {
    var dt = time - n.time;
    var vel = velocityAtTime(n.time);
    var amp = vel * 0.03;
    value + amp * Math.sin(dt * Math.PI * 2 * freq) * Math.exp(-decay * dt);
} else {
    value;
}
```

### Inertia

Continue at the last keyframe's velocity -- smooth drift after animation ends.

```js
if (numKeys > 0 && time > key(numKeys).time) {
    var lastKey = key(numKeys);
    var vel = velocityAtTime(lastKey.time);
    var dt = time - lastKey.time;
    lastKey.value + vel * dt;
} else {
    value;
}
```

### Combining with loopOut

These functions compose with existing loop functions:

```js
// Overshoot on the last keyframe, loop on the rest
if (time > key(numKeys).time) {
    var n = key(numKeys);
    var dt = time - n.time;
    var amp = velocityAtTime(n.time) / (Math.PI * 2 * 4);
    n.value + amp * Math.sin(dt * Math.PI * 2 * 4) * Math.exp(-6 * dt);
} else {
    loopOut("cycle");
}
```

## Tuning Guide

| Parameter | Effect | Typical Range |
|---|---|---|
| `freq` | Oscillation speed (Hz) | 2-8 |
| `decay` | How fast oscillation dies out | 3-10 |
| `amp` scaling | Connect to `velocityAtTime()` for velocity-proportional amplitude | 0.01-0.05 multiplier |

Higher `freq` = tighter oscillations. Higher `decay` = faster settling. The velocity-based amplitude scaling makes the effect respond naturally to the animation speed.
