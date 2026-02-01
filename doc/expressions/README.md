# Kdenlive Expression Engine

## What is the Expression Engine?

The Expression Engine allows you to animate effect parameters in Kdenlive using JavaScript expressions. Instead of manually placing keyframes, you write a short script that computes the parameter value for every frame of the clip. The engine evaluates your expression per-frame and bakes the results into dense keyframes that MLT (Kdenlive's rendering backend) can process natively.

The design is inspired by After Effects Expressions, adapted for Kdenlive's architecture. Under the hood, expressions are evaluated by [QuickJS](https://bellard.org/quickjs/), a lightweight embeddable JavaScript engine. This means you have access to full JavaScript syntax -- variables, conditionals, loops, `Math.*` functions -- plus a set of Kdenlive-specific built-in functions for interpolation, randomness, and audio reactivity.

Expressions are stored in the project XML alongside the baked keyframes. Older versions of Kdenlive that do not support expressions will simply use the baked keyframe data and ignore the expression source, preserving full backward compatibility.


## Quick Start

1. **Add an effect** to a clip on the timeline (e.g., Brightness/Contrast).
2. **Click the [fx] button** next to the parameter you want to animate. This opens the Expression Editor.
3. **Type an expression** in the editor. For example, to fade brightness from 0 to full over the clip duration:
   ```js
   linear(time, 0, duration, 0, value)
   ```
4. **See the live preview** update in the monitor. The expression is evaluated across all frames and the result is applied immediately.

That single line replaces what would otherwise require manually setting a keyframe at the start (value 0) and another at the end (full `value`). The `linear` function maps the current `time` from the input range `[0, duration]` to the output range `[0, value]`.

**Important:** Kdenlive effect parameters use normalized float values, typically ranging from 0.0 to 1.0, not percentage integers (0-100). Always use `value` (the parameter's current value) as the safe default target for expressions. For example, if a parameter's slider goes from 0 to 100, the engine normalizes this internally to 0.0-1.0. The `value` variable always represents the correct base value for your parameter.


## Global Variables

Every expression has access to six context variables that Kdenlive injects before evaluation:

| Variable   | Type     | Description                                                    |
|------------|----------|----------------------------------------------------------------|
| `time`     | `double` | Current time in seconds, measured from the start of the clip.  |
| `frame`    | `int`    | Current frame number, 0-based, relative to the clip start.     |
| `duration` | `double` | Total clip duration in seconds.                                |
| `fps`      | `double` | Project frame rate (e.g., 25.0, 29.97, 30.0).                 |
| `value`    | `double` | The base parameter value, snapshotted from the slider when the expression is first applied. This is the value the user set before the expression took effect. It remains constant (or keyframe-interpolated from the original keyframes) regardless of the expression output. Useful as a target or reference value in expressions. |
| `index`    | `int`    | The clip's index on its track, 0-based. Useful for staggered effects across multiple clips. |


## Built-in Functions Overview

The Expression Engine provides 16 built-in functions across four categories. Full signatures, parameter descriptions, return values, and examples are documented in [reference.md](reference.md).

### Interpolation

| Function   | Description                                                        |
|------------|--------------------------------------------------------------------|
| `linear`   | Linear interpolation between two value ranges.                     |
| `ease`     | Smooth interpolation with ease-in and ease-out (cubic bezier).     |
| `easeIn`   | Interpolation that starts slow and accelerates.                    |
| `easeOut`  | Interpolation that starts fast and decelerates.                    |

### Random and Noise

| Function       | Description                                                    |
|----------------|----------------------------------------------------------------|
| `wiggle`       | Smooth per-frame random oscillation at a given frequency and amplitude. |
| `random`       | Uniform random number in a range. Deterministic per frame.     |
| `gaussRandom`  | Gaussian (bell-curve) distributed random number.               |
| `noise`        | Perlin-style 1D noise function. Continuous and smooth.         |
| `seedRandom`   | Set the random seed for reproducible results.                  |

### Utility

| Function             | Description                                            |
|----------------------|--------------------------------------------------------|
| `clamp`              | Constrain a value between a minimum and maximum.       |
| `posterizeTime`      | Reduce the effective frame rate of an expression (step/hold effect). |
| `degreesToRadians`   | Convert degrees to radians.                            |
| `radiansToDegrees`   | Convert radians to degrees.                            |
| `smooth`             | Smooth a value over time using temporal averaging.     |

### Audio

| Function     | Description                                                      |
|--------------|------------------------------------------------------------------|
| `audioLevel` | Peak audio amplitude at the current frame. Useful for beat-sync effects. |
| `audioRms`   | RMS (root mean square) audio level. Smoother than peak, good for sustained reactions. |

### JavaScript Standard Library

Because the engine runs QuickJS, the full `Math` object is available natively:

- `Math.sin()`, `Math.cos()`, `Math.tan()`
- `Math.abs()`, `Math.floor()`, `Math.ceil()`, `Math.round()`
- `Math.min()`, `Math.max()`, `Math.pow()`, `Math.sqrt()`, `Math.log()`
- `Math.PI`, `Math.E`
- All other standard `Math` methods

You can also use standard JavaScript constructs: ternary operators, `if/else`, variable declarations, and arrow functions.


## Expression Templates

Kdenlive provides a full **Expression Template Manager**, accessible via the **Templates...** button in the Expression Editor toolbar or from the main menu (**Timeline > Expression Templates...**). It manages templates across three tiers:

- **Default** -- 16 read-only presets shipped with Kdenlive, organized into four categories: Fade & Transition, Oscillation & Motion, Audio-Reactive (including audioRms), and Stepped & Quantized.
- **User** -- Your personal template library, stored on disk. Create, edit, import/export templates freely. These persist across projects and Kdenlive sessions.
- **Project** -- Templates stored inside the `.kdenlive` project file. Project templates support **linked mode**: when applied as "linked", editing the template automatically propagates the updated expression to all clips using it.

### Applying Templates

The Template Manager offers two ways to apply a template to a parameter:

- **Apply Linked** -- The expression is applied and linked to a Project template. The Expression Editor becomes read-only, showing a "Linked to: ..." badge. Editing the template in the manager updates all linked instances. You can **Detach** at any time to make the expression independently editable.
- **Apply Detached** -- The expression text is copied into the editor with no link. You can freely modify it without affecting other clips.

Templates are a good starting point for learning how expressions work and for maintaining consistent animations across multiple clips in a project.


## How It Works (Architecture)

When you write an expression for a parameter, the following happens:

1. **Evaluation** -- Kdenlive iterates over every frame of the clip and evaluates the JavaScript expression in a QuickJS context. Before each evaluation, the global variables (`time`, `frame`, `duration`, `fps`, `value`, `index`) are updated to reflect the current frame.

2. **Baking** -- The computed value for each frame is stored as a dense MLT animation keyframe string (e.g., `0=0.00;1=0.04;2=0.08;...`). This keyframe data is what MLT actually uses during playback and rendering.

3. **Storage** -- Both the expression source and the baked keyframe data are saved in the project XML. The expression is stored in a custom attribute; the baked data goes into the standard parameter field.

4. **Re-bake** -- Editing the expression triggers a full re-evaluation and re-bake. Changing the clip's duration or the project frame rate also triggers a re-bake so the values stay correct.

5. **Backward Compatibility** -- If the project is opened in an older version of Kdenlive that does not support the Expression Engine, the baked keyframe data is still valid and will render correctly. The expression source attribute is simply ignored.

This bake-on-edit approach means expressions have zero runtime cost during playback and rendering -- the work is done once at edit time.


## Documentation Map

- **[Function Reference](reference.md)** -- Complete signatures, parameter descriptions, return values, and examples for all 16 built-in functions.
- **[Tutorial 1: Getting Started](tutorials/01-getting-started.md)** -- Your first expression, understanding global variables, basic parameter animation.
- **[Tutorial 2: Fade Effects](tutorials/02-fade-effects.md)** -- Building fade-in, fade-out, and crossfade expressions with `linear`, `ease`, and `clamp`.
- **[Tutorial 3: Audio-Reactive Effects](tutorials/03-audio-reactive.md)** -- Driving parameters from audio using `audioLevel` and `audioRms`.
- **[Tutorial 4: Motion and Shake](tutorials/04-motion-shake.md)** -- Camera shake, wiggle, and oscillation with `wiggle`, `noise`, and `Math.sin`.
- **[Tutorial 5: Advanced Patterns](tutorials/05-advanced-patterns.md)** -- Combining functions, conditional logic, staggered clip effects, and custom easing curves.
- **[Troubleshooting](troubleshooting.md)** -- Common errors, debugging tips, and performance considerations.
