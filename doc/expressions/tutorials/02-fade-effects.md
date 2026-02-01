# Fade Effects

Fading is the most common animation pattern in video editing. The expression engine makes it trivial to create fades that automatically adapt to clip length, and gives you precise control over timing and easing curves.

## Simple Fade In

```javascript
linear(time, 0, duration, 0, value)
```

This maps the current time across the full clip duration from 0 to the parameter's keyframed value. At the start of the clip, the result is 0. At the end, it equals `value`. Everything in between is a straight line.

Use this on opacity to fade a clip from invisible to fully visible. Use it on volume to ramp audio from silence to full level.

## Simple Fade Out

```javascript
linear(time, 0, duration, value, 0)
```

The reverse: starts at `value` and drops to 0 by the end of the clip. The only difference from fade-in is that the last two arguments are swapped.

## Smooth Fade with Ease

```javascript
ease(time, 0, duration, 0, value)
```

`ease()` works identically to `linear()` in terms of arguments, but the interpolation follows a smooth S-curve instead of a straight line. The result accelerates gently at the start and decelerates gently at the end, producing a more natural-looking transition.

For most visual parameters (opacity, scale, position), `ease()` looks better than `linear()`. For audio volume, `linear()` is usually fine.

## easeIn vs easeOut

The `ease()` function applies both easing in and easing out (an S-curve). When you need one-sided easing, use `easeIn()` or `easeOut()`.

**easeIn** -- starts slow, builds momentum, arrives at full speed:

```javascript
easeIn(time, 0, duration, 0, value)
```

Use `easeIn` when something is departing or accelerating away. A fade-out that starts gently and then drops off quickly at the end feels like something being pulled away.

**easeOut** -- starts at full speed, then settles into the target:

```javascript
easeOut(time, 0, duration, 0, value)
```

Use `easeOut` when something is arriving or settling into place. A fade-in that snaps in quickly and then eases into its final value feels like something landing softly.

Rule of thumb:
- **Fade in** (element appearing): use `easeOut` -- it arrives fast and settles.
- **Fade out** (element disappearing): use `easeIn` -- it leaves slowly then accelerates away.

This feels counterintuitive at first. Think of it from the perspective of the motion, not the visibility.

## Fade In Then Out (Hold in Middle)

For a clip that fades in, holds at full value, then fades out, use a multi-line conditional expression with ternary operators:

```javascript
time < duration * 0.2
  ? ease(time, 0, duration * 0.2, 0, value)
  : time > duration * 0.8
    ? ease(time, duration * 0.8, duration, value, 0)
    : value
```

Breaking this down line by line:

1. `time < duration * 0.2` -- During the first 20% of the clip...
2. `? ease(time, 0, duration * 0.2, 0, value)` -- ...ease from 0 up to `value`.
3. `: time > duration * 0.8` -- Otherwise, if we are in the last 20%...
4. `? ease(time, duration * 0.8, duration, value, 0)` -- ...ease from `value` down to 0.
5. `: value` -- Otherwise (the middle 60%), hold at full `value`.

The result is a smooth fade-in over 20% of the clip, a solid hold for 60%, and a smooth fade-out over the final 20%. Adjust the 0.2 and 0.8 thresholds to change the balance between fade time and hold time.

## Timed Fade (Absolute Duration)

Sometimes you want a fade of a fixed length regardless of how long the clip is. For example, a 1-second fade-in:

```javascript
time < 1.0
  ? linear(time, 0, 1.0, 0, value)
  : value
```

During the first second (`time < 1.0`), linearly ramp from 0 to `value`. After that, hold at `value` for the rest of the clip.

For a 1-second fade-out at the end:

```javascript
time > duration - 1.0
  ? linear(time, duration - 1.0, duration, value, 0)
  : value
```

You can combine both for fixed-length fades on each end:

```javascript
time < 1.0
  ? linear(time, 0, 1.0, 0, value)
  : time > duration - 1.0
    ? linear(time, duration - 1.0, duration, value, 0)
    : value
```

This gives exactly 1 second of fade-in and 1 second of fade-out, with the parameter at full value in between, regardless of clip length.

## Combining with Offset

`linear()` and `ease()` are not limited to the range 0-`value`. You can specify any start and end values:

```javascript
linear(time, 0, duration, 20, 80)
```

This ramps from 20 to 80 over the clip duration. The parameter never hits 0 and never reaches 100. This is useful when you want a partial fade -- for example, opacity that goes from 20% (translucent) to 80% (mostly visible) without ever being fully transparent or fully opaque.

Another example -- brightness that starts dim and ends bright, but stays within a comfortable range:

```javascript
ease(time, 0, duration, 40, 90)
```

The start and end values are entirely independent. You can also fade "backwards" by making the start value higher than the end value:

```javascript
linear(time, 0, duration, 80, 20)
```

This goes from 80 down to 20 -- a partial fade-out that never reaches zero.
