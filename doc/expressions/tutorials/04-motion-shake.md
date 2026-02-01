# Motion and Shake Effects

The expression engine provides several tools for adding organic, procedural motion to effect parameters. These range from smooth random drift to precise periodic oscillation, and they can be combined to create complex, natural-looking animation without placing a single keyframe.

## Wiggle -- Organic Random Motion

`wiggle()` is the workhorse function for organic motion. It produces smooth, random values using Perlin noise.

### Basic Usage

```javascript
wiggle(4, 0.1)
```

- First argument: **frequency** -- 4 oscillations per second.
- Second argument: **amplitude** -- the output fluctuates up to 0.1 (10% of the normalized range) above and below the parameter's base value.

The result is smooth and natural-looking. Perlin noise avoids the sharp jumps you would get from pure random numbers. The motion is also deterministic -- the same clip with the same expression always produces the same result, so your edit is reproducible.

### Octaves for Richer Detail

```javascript
wiggle(2, 0.15, 3, 0.5)
```

- Third argument: **octaves** (default 1) -- the number of noise layers.
- Fourth argument: **amplitude multiplier** (default 0.5) -- each successive layer has its amplitude multiplied by this factor.

With 3 octaves and a multiplier of 0.5, you get:
1. Base layer: frequency 2, amplitude 0.15
2. Second layer: frequency 4, amplitude 0.075
3. Third layer: frequency 8, amplitude 0.0375

The layers are summed. The result is a primary slow drift with faster, smaller variations on top -- much more organic than a single-layer wiggle.

### Practical Applications

| Parameter     | Expression               | Effect                                    |
|---------------|--------------------------|-------------------------------------------|
| Position X    | `wiggle(3, 0.05)`        | Gentle horizontal drift                   |
| Position Y    | `wiggle(3, 0.05)`        | Gentle vertical drift                     |
| Rotation      | `wiggle(2, 0.03)`        | Subtle rotational sway                    |
| Scale         | `value + wiggle(1, 0.05)`| Breathing scale pulse around base value   |
| Opacity       | `value + wiggle(2, 0.1)` | Flickering transparency around base value |

## Sine Wave -- Periodic, Predictable Motion

When you need motion that repeats at a precise rhythm, use `Math.sin()`.

### Basic Sine Oscillation

```javascript
value + 20 * Math.sin(time * Math.PI * 2)
```

- `time * Math.PI * 2` -- one full sine cycle per second (since `sin` completes a cycle over 2*PI).
- `20` -- amplitude. The value swings 20 units above and below `value`.

### Controlling Frequency

Multiply the `time` factor to change speed:

```javascript
value + 20 * Math.sin(time * Math.PI * 4)
```

`Math.PI * 4` gives 2 cycles per second. The general formula is `time * Math.PI * 2 * Hz` where Hz is cycles per second.

### Phase Offset

Add a constant inside the sine to shift the starting point:

```javascript
value + 20 * Math.sin(time * Math.PI * 2 + 1.5)
```

This shifts the wave by 1.5 radians. Useful when you have two parameters with sine expressions and you want them offset from each other (e.g., X and Y position for circular or elliptical motion).

## Breathing Rhythm

A very slow sine wave creates a breathing or pulsing effect:

```javascript
value + 10 * Math.sin(time * Math.PI * 0.8)
```

At `Math.PI * 0.8`, the frequency is approximately 0.4 Hz -- one full breath cycle every 2.5 seconds. The amplitude of 10 keeps the movement subtle.

This works well on:
- **Opacity** -- a gentle pulse between slightly transparent and fully opaque.
- **Scale** -- a subtle zoom-in/zoom-out that suggests breathing.
- **Brightness** -- a slow luminance throb.

## Pendulum (Damped Oscillation)

An oscillation that starts strong and fades to nothing over the clip duration:

```javascript
value + 30 * Math.sin(time * Math.PI * 4) * Math.max(0, 1 - time / duration)
```

- `Math.sin(time * Math.PI * 4)` -- oscillation at 2 Hz.
- `Math.max(0, 1 - time / duration)` -- a linear ramp from 1 down to 0 over the clip. This multiplies the sine amplitude, so the oscillation diminishes over time.
- `Math.max(0, ...)` prevents the multiplier from going negative if time somehow exceeds duration.

The result looks like a pendulum winding down or a vibration dampening. Good for rotation or position after an impact.

## Camera Shake

High-frequency wiggle on position parameters creates a handheld camera effect:

```javascript
wiggle(8, 0.1)
```

8 Hz with an amplitude of 0.1 is aggressive. For a subtle handheld look, try `wiggle(5, 0.05)`.

Apply this expression to both Position X and Position Y of a transform effect. Each parameter gets its own independent wiggle trajectory, creating 2D shake.

### Clamped Shake

To prevent the shake from pushing the value too far outside bounds:

```javascript
clamp(wiggle(8, 0.2), 0, 1)
```

`clamp()` restricts the output to the normalized range 0.0 to 1.0, no matter how extreme the wiggle tries to go. This is a safety net for high-amplitude shake.

### Impact Shake (Strong Start, Quick Settle)

```javascript
wiggle(10, 0.3) * Math.max(0, 1 - time * 4)
```

The `Math.max(0, 1 - time * 4)` term drops from 1 to 0 over 0.25 seconds, so the shake is intense at the start and gone after a quarter second. Place the expression on a clip that starts at the moment of impact.

## Noise Function

The `noise()` function provides raw Perlin noise values:

```javascript
value + noise(time * 4) * 0.15
```

- `noise()` returns values roughly in the range -1 to 1.
- Multiply by the desired normalized amplitude (0.15 in this example).
- The argument controls the "position" in the noise field. Multiplying `time` by a larger number makes the noise change faster.

The difference between `noise()` and `wiggle()`:
- `wiggle(freq, amp)` is a convenience wrapper that handles frequency-to-noise mapping and centers around the parameter's value.
- `noise()` is the raw building block. You control everything manually.

Use `noise()` when you need to share the same noise pattern across parameters or when you want to feed noise into other calculations.

## Combining Motion Types

Motion expressions can be layered by addition:

```javascript
wiggle(2, 0.1) + 0.05 * Math.sin(time * Math.PI * 0.6)
```

This gives a slow sine pulse as the primary motion with a random wiggle layered on top. The sine provides a predictable rhythm and the wiggle adds organic variation.

For position-based effects, apply different expressions to X and Y:
- Position X: `wiggle(3, 0.1)` -- random horizontal drift
- Position Y: `value + 0.1 * Math.sin(time * Math.PI * 1.5)` -- periodic vertical bob

The combination of random horizontal and periodic vertical creates complex, interesting motion from simple components.
