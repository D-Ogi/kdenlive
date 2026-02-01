# Getting Started with Expressions

## What You'll Learn

- How to open the expression editor
- How to write your first expression
- Understanding global variables
- Using the function reference bar
- Using ready-made templates

## Prerequisites

- Kdenlive with Expression Engine support enabled
- A project with at least one video clip on the timeline

## Step 1: Add an Effect

1. Select a clip on the timeline.
2. Go to the Effects panel.
3. Add an effect. For this tutorial, use **Brightness** (or any effect with a numeric parameter).

## Step 2: Open the Expression Editor

1. In the effect parameters panel, find the parameter you want to animate (e.g., "Level" or "Brightness").
2. Click the **[fx]** button next to the parameter slider (or next to the keyframe controls).
3. The expression editor will appear with the following components:
   - A code editor (monospace text area) where you write expressions.
   - A function reference bar with clickable function names.
   - A **Templates** button for ready-to-use presets.
   - A preview line showing the computed result at the current playhead position.
   - A status line showing the validation result.

## Step 3: Write Your First Expression

Type the following into the editor:

```javascript
time
```

Wait 300ms for the debounce to complete. You should see:

- **Status:** "Expression valid" (green)
- **Preview:** "Result: X.XXX at frame N" (where X is the current time in seconds)

This expression simply outputs the current time in seconds. At the start of the clip the value is 0, at 2 seconds in it reads 2.0, and so on.

## Step 4: Make It Useful -- Linear Ramp

Replace the expression with:

```javascript
linear(time, 0, duration, 0, value)
```

This maps time from `[0, clip duration]` to the normalized value range `[0, value]`, where `value` is the parameter's current slider setting:

- At the start of the clip: parameter = 0
- At the end of the clip: parameter = value (current slider setting)
- The change is linear (constant speed)

Move the playhead through the clip to see the preview value change smoothly from 0 to the parameter's base value.

## Step 5: Understanding Global Variables

Every expression has access to the following variables. They update automatically for each frame:

| Variable   | Meaning                                   | Example value |
|------------|-------------------------------------------|---------------|
| `time`     | Seconds since clip start                  | 1.5           |
| `frame`    | Frame number from clip start              | 37            |
| `duration` | Total clip length in seconds              | 4.0           |
| `fps`      | Project frame rate                        | 25.0          |
| `value`    | The parameter's base value (snapshotted from slider when expression is first applied) | 50.0 |
| `index`    | Clip position on track (0 = first)        | 0             |

Try these one at a time in the editor to see what they return:

- `frame` -- shows the frame counter.
- `duration` -- shows clip length (constant for the entire clip).
- `value` -- shows whatever the slider is currently set to.

## Step 6: Using the Function Bar

Below the editor, you will see clickable function names: `linear`, `ease`, `wiggle`, `audioLevel`, and others.

Click on **ease**. It inserts a template into the editor:

```javascript
ease(time, 0, duration, 0, value)
```

This behaves similarly to `linear` but applies smooth acceleration and deceleration (an S-curve) instead of a constant rate of change. The result feels more natural and polished. The parameter animates from 0 to `value` (the current slider setting) over the clip duration.

## Step 7: Try a Template

Click the **Templates...** button to open the Expression Template Manager. You will see three tiers of templates:

- **Default** -- 16 read-only presets in categories like Fade & Transition, Oscillation & Motion, Audio-Reactive, and Stepped & Quantized.
- **User** -- Your personal templates (initially empty).
- **Project** -- Templates stored in the current project file, supporting linked mode.

Select **"Breathing"** from the Default > Oscillation & Motion category, then click **Apply Detached**. It inserts:

```javascript
value + 10 * Math.sin(time * Math.PI * 0.8)
```

This creates a slow sinusoidal pulse around the base value, producing a rhythm similar to breathing. The parameter oscillates gently above and below whatever the slider is set to.

## Step 8: Clear the Expression

Click the **Clear** button in the bottom-right of the expression editor. This removes the expression entirely and restores the parameter to its normal slider/keyframe mode.

## What's Next?

- [Tutorial 2: Fade Effects](02-fade-effects.md) -- Linear and eased fades, fade in/out combinations
- [Tutorial 3: Audio-Reactive Effects](03-audio-reactive.md) -- Make parameters respond to music
- [Tutorial 4: Motion and Shake](04-motion-shake.md) -- Wiggle, sine waves, pendulum effects
- [Tutorial 5: Advanced Patterns](05-advanced-patterns.md) -- Combining functions, conditional logic
