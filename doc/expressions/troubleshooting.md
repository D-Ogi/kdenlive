# Expression Engine Troubleshooting

This guide covers common errors, audio-related issues, behavioral quirks, and debugging strategies for the Kdenlive Expression Engine.

---

## Common Errors

### 1. "X is not defined"

A typo in a function or variable name. The expression engine runs in a sandboxed QuickJS environment with a limited set of built-in names. If you misspell one, you get a reference error.

**Available variables:** `time`, `frame`, `duration`, `fps`, `value`, `index`

**Available functions:** `linear()`, `ease()`, `easeIn()`, `easeOut()`, `wiggle()`, `random()`, `gaussRandom()`, `noise()`, `seedRandom()`, `clamp()`, `posterizeTime()`, `degreesToRadians()`, `radiansToDegrees()`, `smooth()`, `audioLevel()`, `audioRms()`, and standard `Math.*` methods.

Common typos:

| What you wrote     | What you meant   |
|--------------------|------------------|
| `audioLevl()`      | `audioLevel()`   |
| `Time`             | `time`           |
| `Frame`            | `frame`          |
| `Duration`         | `duration`       |
| `posterize_time()` | `posterizeTime()`|
| `audiolvl()`       | `audioLevel()`   |

All names are **case-sensitive**. `time` and `Time` are not the same thing.

---

### 2. "Expression did not return a number"

The expression evaluated to `undefined`, a string, or an object instead of a numeric value. Every expression must ultimately produce a number.

**Common causes:**

- **Calling `posterizeTime()` alone.** `posterizeTime(12)` modifies the temporal resolution but returns `undefined`. It must be followed by an expression that produces a value:
  ```js
  // Wrong - returns undefined
  posterizeTime(12)

  // Correct - posterizeTime modifies time, then linear() returns a number
  posterizeTime(12); linear(time, 0, duration, 0, value)
  ```

- **Using `console.log()`.** The sandboxed environment does not provide `console`. There is no logging mechanism inside expressions.

- **Forgetting the return expression.** If your expression is a block of statements, the last statement must evaluate to a number:
  ```js
  // Wrong - the if/else doesn't produce a value as a statement
  // (though in practice, ternary is preferred)

  // Correct - use a ternary
  time < duration / 2 ? linear(time, 0, duration / 2, 0, value) : value
  ```

---

### 3. "Expression returned NaN or Infinity"

The expression produced a value that is not a usable number. This typically comes from invalid arithmetic.

**Common causes:**

- **Division by zero:**
  ```js
  // If time == 0 at the start of the clip, this divides by zero
  value / time

  // Fix: guard the denominator
  time > 0 ? value / time : 0
  ```

- **`Math.log()` of a negative number or zero:**
  ```js
  // Math.log(0) = -Infinity, Math.log(-1) = NaN
  Math.log(time)

  // Fix: clamp the input
  Math.log(clamp(time, 0.001, duration))
  ```

- **Subtracting identical values in a denominator:**
  ```js
  // Always zero denominator
  value / (time - time)
  ```

Use `clamp()` liberally to keep intermediate values in safe ranges, and always verify that denominators cannot be zero.

---

### 4. "linear() requires 3 or 5 arguments" (and similar arity errors)

You passed the wrong number of arguments to a built-in function.

**Function signatures:**

| Function                                         | Arguments |
|--------------------------------------------------|-----------|
| `linear(t, outMin, outMax)`                      | 3         |
| `linear(t, inMin, inMax, outMin, outMax)`        | 5         |
| `clamp(value, min, max)`                         | 3         |
| `posterizeTime(framesPerSecond)`                 | 1         |
| `audioLevel(channel, time)`                      | 2         |
| `wiggle(frequency, amplitude)`                   | 2         |

Double-check the function reference if you are unsure about the expected arguments.

---

### 5. Red status with QuickJS error

A JavaScript syntax error in the expression. QuickJS will report the error with a line/column indicator when possible.

**Common syntax mistakes:**

- Missing closing parenthesis: `linear(time, 0, duration, 0, value`
- Unmatched or mismatched quotes: `audioLevel("Both, time)`
- Stray characters from copy-paste (smart quotes, zero-width spaces)
- Using `=` instead of `==` or `===` in a comparison

If the error message is cryptic, simplify the expression down to the smallest fragment that still triggers the error, then rebuild from there.

---

## Audio Issues

### 1. audioLevel() always returns 0

The `audioLevel()` function reads analyzed audio data from the timeline. If it consistently returns 0, one of the following is the cause:

- **No audio track in the timeline.** There must be at least one audio track with a clip that overlaps temporally with the video clip containing the expression.
- **Audio has not been analyzed.** The audio waveform must be visible in the timeline. If the waveform is not displayed, Kdenlive has not yet analyzed the audio data. Ensure waveform display is enabled and give it a moment to process.
- **Channel mismatch.** If the audio is mono and you request a specific channel that does not exist, the return value may be 0. Use `"Both"` as the channel parameter for the safest default.

### 2. Audio response is too subtle or too strong

Raw `audioLevel()` values are normalized to the 0.0-1.0 range, but real-world audio content rarely spans the full range. Typical music or dialogue sits in the 0.05-0.6 range, with occasional peaks higher.

Use `linear()` to remap the effective range to your desired output range:

```js
// Map the practical audio range (0.05 to 0.5) to a parameter range of 0.0 to 1.0
linear(audioLevel("Both", time), 0.05, 0.5, 0, value)
```

This makes quiet passages (below 0.05) map to 0, moderately loud passages (0.5) map to 1.0, and values outside the input range extrapolate linearly. If you want to hard-cap the output, wrap in `clamp()`:

```js
clamp(linear(audioLevel("Both", time), 0.05, 0.5, 0, value), 0, 1)
```

Adjust the input range (0.05 and 0.5) based on the actual audio content. Start with `audioLevel("Both", time)` alone to observe raw values, then tune accordingly.

---

## Behavior Notes

### 1. Expression doesn't update immediately

There is a **300ms debounce** after the last keystroke before the expression is re-evaluated. This prevents excessive recomputation while typing. Wait for the status indicator to show "Expression valid" (or an error message) before judging the result.

### 2. Expression result differs from preview

The **live preview** shows the computed value at the current playhead position. The expression is a function of time, so its output changes as you move through the clip. Scrub the playhead across the full duration of the clip to see how the expression evolves. Pay particular attention to the start and end boundaries.

### 3. Changes not visible in rendered output

When an expression is applied, it is **baked to dense keyframes** (one keyframe per frame). This baked data is what the render engine uses. In normal operation, baking happens automatically:

- When you confirm the expression, keyframes are generated immediately.
- When you trim or resize the clip, the expression is re-baked to match the new duration.

If the rendered output seems stale or incorrect, toggle the expression off and on again using the **[fx] button** on the effect parameter. This forces a fresh bake.

### 4. Opening project in old Kdenlive

Expressions are stored as an `expression` attribute on `<property>` elements in the project XML. Older versions of Kdenlive that do not support the expression engine will **ignore the unknown attribute** and use the baked keyframe values as a fallback. The effect will render correctly, but it will not update dynamically if the clip is modified. This provides safe backward compatibility at the cost of losing the live expression behavior.

### 5. Performance with long clips

Baking evaluates the expression once per frame. For a 10-minute clip at 25fps, that is 15,000 evaluations. QuickJS is highly optimized and handles this quickly for typical expressions. However, expressions that call `audioLevel()` multiple times per frame or perform heavy `Math` operations may introduce a brief delay on very long clips. If baking takes noticeably long:

- Reduce the number of `audioLevel()` calls per expression (combine channels with `"Both"` instead of querying `"Left"` and `"Right"` separately).
- Simplify nested `linear()` chains where possible.
- Consider splitting very long clips into shorter segments.

---

## Debugging Tips

1. **Use the live preview label.** It displays "Result: X.XX at frame N" and updates as you scrub. This is your primary feedback mechanism.

2. **Check boundary values.** Move the playhead to the **start of the clip** (frame 0) and the **end of the clip** (frame = duration). Verify that the expression returns sensible values at both extremes. Many bugs manifest only at the boundaries (division by zero at frame 0, unexpected extrapolation at the end).

3. **Start simple, then build up.** Test the bare minimum first:
   - `time` -- confirms the variable is available and increasing.
   - `linear(time, 0, duration, 0, 1)` -- confirms linear interpolation over the clip length.
   - Then layer in audio, conditionals, or other complexity one piece at a time.

4. **Use `clamp()` defensively.** Every effect parameter has a valid range. Wrapping your final expression in `clamp(expr, min, max)` prevents out-of-range values from causing unexpected visual artifacts or render errors.

5. **For audio expressions, isolate the audio signal first.** Before building a complex audio-reactive expression, test with just:
   ```js
   audioLevel("Both", time)
   ```
   Scrub through the timeline and observe the raw values in the live preview. Note the typical minimum, maximum, and average. Then wrap in `linear()` to remap to your target range. Only after the remapping looks correct should you integrate it into a larger expression.
