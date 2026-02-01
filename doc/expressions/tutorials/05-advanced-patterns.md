# Advanced Patterns

This tutorial covers techniques for building complex expressions by combining the core functions with JavaScript logic. These patterns give you conditional behavior, stepped animation, looping, physics-inspired motion, and clean multi-variable expressions.

## Conditional Expressions

JavaScript ternary operators let you switch between different values or behaviors based on conditions.

### Binary Switch

```javascript
time < duration / 2 ? 0 : value
```

The parameter is 0 during the first half of the clip and `value` during the second half. The switch is instantaneous -- no transition.

### Multi-Way Conditional

Ternary operators can be chained for multi-step logic:

```javascript
time < duration * 0.25
  ? 0
  : time < duration * 0.5
    ? value * 0.5
    : time < duration * 0.75
      ? value * 0.75
      : value
```

Four discrete levels: 0 for the first quarter, 50% of value for the second, 75% of value for the third, and full value for the last. Each step change is instantaneous.

### Conditional with Smooth Transition

Combine conditionals with interpolation functions to get smooth transitions between states:

```javascript
time < duration * 0.5
  ? ease(time, 0, duration * 0.5, 0, value)
  : ease(time, duration * 0.5, duration, value, value * 0.5)
```

Eases from 0 to `value` over the first half, then eases from `value` down to 50% of `value` over the second half.

## Combining Functions

Any expression that returns a number can be added to, multiplied by, or otherwise combined with any other expression.

### Audio-Reactive with Organic Jitter

```javascript
linear(audioLevel("Both", time), 0.05, 0.8, value * 0.3, value) + wiggle(2, 0.05)
```

The base value is driven by audio (30% to 100% of value depending on loudness). On top of that, `wiggle(2, 0.05)` adds a subtle +/-0.05 random drift. The audio provides the macro movement; the wiggle prevents the result from looking mechanically locked to the waveform.

### Fade In with Shake

```javascript
ease(time, 0, duration, 0, value) + wiggle(3, 0.08) * Math.min(1, time / 0.5)
```

The `ease()` fades the value in over the clip. The wiggle adds shake, but it is multiplied by `Math.min(1, time / 0.5)` which ramps from 0 to 1 over the first 0.5 seconds. This prevents the shake from being visible before the parameter has faded in.

## Stepped Animation with posterizeTime

`posterizeTime()` reduces the effective frame rate of the expression. The expression is evaluated at the reduced rate and held until the next evaluation.

### Basic Stepped Ramp

```javascript
posterizeTime(2);
linear(time, 0, duration, 0, value)
```

The linear ramp from 0 to `value` is sampled only twice per second. Instead of a smooth ramp, the value jumps in discrete steps every 0.5 seconds. The visual result looks like a retro, low-frame-rate animation.

### Random Per Second

```javascript
posterizeTime(1);
random(0, value)
```

`random(0, value)` normally produces a new random value every frame, which looks like noise. With `posterizeTime(1)`, a new random value is chosen once per second and held for the full second. The result is a series of random jumps at a controlled pace.

### Slow-Motion Steps

```javascript
posterizeTime(4);
wiggle(2, 0.3)
```

The wiggle still generates smooth noise, but it is only sampled 4 times per second. The output jumps between wiggle values in quarter-second intervals -- a stylized, stuttering version of organic motion.

## Step Grow (Discrete Steps)

```javascript
Math.floor(time / duration * 5) / 5 * value
```

This divides the clip into 5 equal time segments. Within each segment, the value is held constant:
- 0% to 20%: value * 0/5 = 0
- 20% to 40%: value * 1/5 = 20% of value
- 40% to 60%: value * 2/5 = 40% of value
- 60% to 80%: value * 3/5 = 60% of value
- 80% to 100%: value * 4/5 = 80% of value

Change the `5` to any integer to control the number of steps. Replace `value` with a specific number if needed.

## Looping (Repeating Pattern)

### Basic Loop

```javascript
var phase = (time % 2) / 2;
ease(phase, 0, 1, 0, value)
```

`time % 2` gives a sawtooth wave that resets to 0 every 2 seconds. Dividing by 2 normalizes it to a 0-1 range. Feeding this into `ease()` with input range 0-1 produces a smooth ramp from 0 to `value` that repeats every 2 seconds.

Change the `2` in both places to adjust the loop duration (in seconds).

### Looping Sine

```javascript
Math.sin(time % 3 / 3 * Math.PI * 2) * 0.5 + 0.5
```

A sine wave that completes one cycle every 3 seconds, normalized to the 0-1 range. Multiply by your desired amplitude and add your desired offset.

## Bounce (Ping-Pong)

```javascript
var cycle = time % 2;
var phase = cycle < 1 ? cycle : 2 - cycle;
ease(phase, 0, 1, 0, value)
```

- `time % 2` creates a 2-second repeating cycle (0 to 2, then reset).
- The ternary converts this into a triangle wave: 0 to 1 over the first second, then 1 back to 0 over the second.
- `ease()` smooths the triangle into a soft bounce.

The result is a value that eases up to `value` and eases back down, continuously. This is a ping-pong or bounce loop.

## Exponential Decay

```javascript
value * Math.exp(-3 * time / duration)
```

`Math.exp(-x)` produces a curve that starts at 1 and drops toward 0. The rate constant (`-3`) controls how fast the decay happens:
- `-1`: very gradual, still at ~37% at the end.
- `-3`: moderate, reaches ~5% at the end.
- `-5`: aggressive, effectively zero by 60% through the clip.

This is useful for parameters that should drop off quickly after a trigger -- intensity after a flash, amplitude after an impact, or scale after a pop-in.

## Spring Effect

```javascript
value * (1 - Math.exp(-5 * time) * Math.cos(time * Math.PI * 8))
```

This simulates a damped spring oscillation:
- `Math.cos(time * Math.PI * 8)` -- oscillation at 4 Hz.
- `Math.exp(-5 * time)` -- exponential decay envelope.
- The product creates an oscillation that diminishes over time.
- `1 - (...)` inverts it so the value starts at 0, overshoots `value`, bounces back, and settles at `value`.

The result is a springy pop-in: the parameter flies past its target, rebounds, oscillates a few times, and comes to rest. Adjust the `8` (oscillation speed) and `-5` (damping rate) to control the spring feel.

- Higher oscillation (e.g., 12): tighter, faster bounces.
- Higher damping (e.g., -8): settles faster.
- Lower damping (e.g., -2): bounces persist longer.

## Using Math Functions Creatively

### Absolute Sine (Always Positive)

```javascript
Math.abs(Math.sin(time * Math.PI * 2)) * value
```

Normal sine goes from -1 to 1. Wrapping in `Math.abs()` folds the negative half upward, producing a value that bounces between 0 and 1 at double the apparent frequency. Useful for pulsing effects where you never want the value to go negative.

### Quadratic Ramp (Starts Slow, Accelerates)

```javascript
Math.pow(time / duration, 2) * value
```

`time / duration` is a linear 0-to-1 ramp. Squaring it makes the ramp start very slowly and then accelerate toward the end. At the halfway point, the value is only at 25% -- most of the change happens in the last quarter of the clip.

### Square Root Ramp (Starts Fast, Decelerates)

```javascript
Math.sqrt(time / duration) * value
```

The opposite of quadratic: the value jumps quickly at the start and then gradually levels off. At the halfway point, it is already at ~71% of the target.

### Power Curves as Easing Alternatives

```javascript
Math.pow(time / duration, 3) * value
```

Higher powers produce more extreme slow-start acceleration. `pow(..., 0.5)` is the same as `sqrt()`. `pow(..., 1)` is linear. `pow(..., 2)` is quadratic. `pow(..., 4)` is very aggressive late acceleration. These give you fine-grained control over easing behavior without relying on the built-in `ease()` curve.

## Multi-Variable Expressions

For complex logic, use `var` declarations to break the expression into named steps:

```javascript
var level = audioLevel("Both", time);
var mapped = linear(level, 0.05, 0.7, 0, 1);
var shaky = wiggle(3, mapped * 0.1);
clamp(shaky, 0, 1)
```

Line by line:

1. `var level = audioLevel("Both", time)` -- read the raw audio level.
2. `var mapped = linear(level, 0.05, 0.7, 0, 1)` -- remap audio to a clean 0-1 range.
3. `var shaky = wiggle(3, mapped * 0.1)` -- generate a wiggle whose amplitude scales with the audio level (0 to 0.1 normalized).
4. `clamp(shaky, 0, 1)` -- ensure the final value stays within bounds.

This is functionally identical to writing it as a single nested expression, but far easier to read and debug. Use this pattern whenever your expression involves more than two function calls.

### Variables for Reuse

Variables also prevent redundant computation when a value is used more than once:

```javascript
var t = time / duration;
var ramp = ease(t, 0, 1, 0, 1);
var pulse = Math.sin(t * Math.PI * 10) * (1 - ramp) * 20;
value * ramp + pulse
```

This creates a value that eases toward `value` while a sine pulse fades out. The pulse is strongest at the start and gone by the end. `t` and `ramp` are computed once and used multiple times.
