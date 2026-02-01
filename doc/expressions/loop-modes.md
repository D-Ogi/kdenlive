# Loop Modes Reference

Comprehensive guide to `loopIn`, `loopOut`, `loopInDuration`, `loopOutDuration` — the AE-compatible keyframe looping system in the Kdenlive Expression Engine.

## Overview

Loop functions repeat a keyframe animation segment before its start (`loopIn`) or after its end (`loopOut`). They require at least 2 keyframes on the parameter.

## Functions

### loopOut(type, nKeys)

Loops the animation **after** the last keyframe.

- `type` — `"cycle"` (default), `"pingpong"`, `"offset"`, or `"continue"`
- `nKeys` — number of keyframes at the end to use as the loop segment (0 = all keyframes)

### loopIn(type, nKeys)

Loops the animation **before** the first keyframe. Mirror of loopOut.

### loopOutDuration(type, duration)

Like `loopOut` but specifies the loop segment by time duration (seconds) measured back from the last keyframe.

- `duration` — seconds; 0 = all keyframes

### loopInDuration(type, duration)

Like `loopIn` but specifies the loop segment by time duration (seconds) measured forward from the first keyframe.

## Loop Modes

### cycle (default)

Repeats the keyframe segment identically. When the loop segment plays 0→50→100, after the last keyframe it plays 0→50→100→0→50→100→...

```js
loopOut("cycle")        // cycle all keyframes
loopOut("cycle", 3)     // cycle last 3 keyframes only
loopOutDuration("cycle", 2.0)  // cycle last 2 seconds of keyframes
```

**Best for:** Repeating patterns, oscillations, continuous rotation.

### pingpong

Alternates forward and backward. The segment plays forward, then backward, then forward again.

```js
loopOut("pingpong")
```

**Best for:** Back-and-forth motion, breathing, pendulum effects.

### offset

Like `cycle`, but each repetition adds the cumulative value change of one cycle. If keyframes go from 0 to 100:
- Cycle 1 (original): 0 → 100
- Cycle 2: 100 → 200
- Cycle 3: 200 → 300

```js
loopOut("offset")
```

**Best for:** Continuous forward motion (rotation that keeps spinning, position that keeps moving), accumulating values.

### continue

Linear extrapolation at the velocity of the boundary keyframe. The animation continues in a straight line at whatever speed it was moving when it hit the last (or first) keyframe.

```js
loopOut("continue")
```

**Best for:** Smooth exit from an animation, inertia-like drift, objects that should keep moving after their last keyframe.

## Edge Cases

| Scenario | Behavior |
|---|---|
| < 2 keyframes | Returns current `value` (no looping possible) |
| `nKeys` >= total keyframes | Uses all keyframes (same as nKeys=0) |
| `duration` = 0 | Uses all keyframes |
| `duration` > total span | Uses all keyframes |
| `loopDur` ≈ 0 (identical keyframe times) | Returns boundary value (no loop) |

## Recipes

### Continuous Rotation (360°/cycle)

```js
// Keyframes: frame 0 = 0°, frame 25 = 360°
// offset mode makes it continue: 360→720→1080...
loopOut("offset")
```

### Smooth Breathing with Pingpong

```js
// Keyframes: 0s = 0.95, 1s = 1.05 (scale)
// Pingpong creates: 0.95→1.05→0.95→1.05...
loopOut("pingpong")
```

### Pre-roll with loopIn

```js
// Animation starts at frame 25, but clip starts at frame 0
// loopIn fills the gap before the first keyframe
loopIn("cycle")
```

### Duration-based Loop Segment

```js
// Only loop the last 0.5 seconds of keyframe animation
loopOutDuration("cycle", 0.5)
```

### Continue + Overshoot Combo

```js
// Use continue for before the animation, overshoot after
if (time > key(numKeys).time) {
    // Overshoot expression after last keyframe
    var n = key(numKeys);
    var dt = time - n.time;
    var amp = velocityAtTime(n.time) / (Math.PI * 2 * 4);
    n.value + amp * Math.sin(dt * Math.PI * 2 * 4) * Math.exp(-6 * dt);
} else if (time < key(1).time) {
    loopIn("continue");
} else {
    value;
}
```

## Comparison: nKeys vs Duration

| Aspect | `loopOut(type, nKeys)` | `loopOutDuration(type, duration)` |
|---|---|---|
| Unit | Keyframe count | Seconds |
| Precision | Exact keyframe boundaries | Time-based (finds nearest) |
| Use case | "Loop last N keyframes" | "Loop last N seconds" |
| AE equivalent | `loopOut(type, numKeyframes)` | `loopOutDuration(type, duration)` |
