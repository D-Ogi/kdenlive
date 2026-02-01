# Audio-Reactive Effects

The expression engine can read audio levels in real time and use them to drive any effect parameter. This allows you to create visuals that pulse, shake, or transform in sync with the soundtrack.

## How Audio Integration Works

Kdenlive analyzes the audio track that overlaps with your clip and exposes per-frame amplitude data to the expression engine through two functions: `audioLevel()` and `audioRms()`.

**Prerequisites:**
- There must be an audio track with a clip that overlaps the video clip containing your expression.
- The audio data is read from the timeline mix, so muted tracks are excluded.
- Audio analysis happens during playback and rendering. The expression reads whatever audio is present at each frame.

## Basic Audio Read

```javascript
audioLevel("Both", time)
```

This returns a value between 0.0 (silence) and 1.0 (maximum level) representing the instantaneous peak amplitude at the current frame.

The first argument selects the audio channel:

| Channel   | Description                        |
|-----------|------------------------------------|
| `"Both"`  | Combined level from both channels  |
| `"Left"`  | Left channel only                  |
| `"Right"` | Right channel only                 |

For most use cases, `"Both"` is the right choice.

## The Problem with Raw Audio Levels

Raw `audioLevel()` values are noisy and occupy a narrow useful range. In a typical music track, quiet passages might read 0.02-0.1 and loud passages might read 0.3-0.7. You rarely see values near 0.0 or 1.0.

If you map raw audio directly to a parameter without remapping, the visual result will be jittery and will only use a fraction of the available range.

## Solution: Remap with linear()

```javascript
linear(audioLevel("Both", time), 0.05, 0.8, 0, value)
```

This takes the raw audio level and remaps it:
- Audio at or below 0.05 maps to 0.
- Audio at or above 0.8 maps to `value` (the parameter's maximum normalized value).
- Everything between 0.05 and 0.8 is proportionally mapped to the 0 to `value` range.

The first argument to `linear()` is the input value (the audio level). The second and third arguments define the input range you care about. The fourth and fifth define the output range.

Adjust the input range (0.05 and 0.8) based on your actual audio content. Start by testing `audioLevel("Both", time)` on its own to see what range your track produces, then set the thresholds accordingly.

## Audio-Reactive Brightness Pulse

```javascript
linear(audioLevel("Both", time), 0.05, 0.8, value * 0.3, value)
```

Applied to a brightness parameter: the clip sits at 30% of its brightness range during quiet moments and ramps to the full `value` during loud moments. The visual result is a brightness pulse that follows the music.

The floor of `value * 0.3` means the clip is never completely dark, even during silence. Adjust the floor multiplier to taste.

## Inverse Audio (Quiet = High Value)

```javascript
linear(audioLevel("Both", time), 0.05, 0.8, value, value * 0.2)
```

Swapping the output values inverts the relationship: silence produces a high value (`value`) and loud audio produces a low value (`value * 0.2`). This is useful for effects where you want something to recede during loud passages -- for example, an overlay that becomes more transparent when the music hits.

## Audio-Driven Shake

```javascript
wiggle(6, audioLevel("Both", time) * 0.3)
```

The `wiggle()` function produces smooth random motion. Here, the frequency is fixed at 6 oscillations per second, but the amplitude is controlled by audio level. During quiet passages, the amplitude approaches zero and the image is still. During loud passages, the amplitude grows (up to 30% of the parameter's normalized range) and the image shakes.

This works well on position X/Y parameters for a camera-shake effect that reacts to bass hits or drum beats.

## Smoother Audio with audioRms

```javascript
linear(audioRms("Both", time, 0.2), 0.05, 0.5, 0, value)
```

`audioRms()` computes the root-mean-square level over a time window, which is much smoother than per-frame peak values. The third argument (0.2) specifies the window size in seconds -- in this case, 200 milliseconds.

The difference is significant:
- `audioLevel()` -- per-frame peak, reacts instantly but jitters heavily.
- `audioRms()` with a 0.1-0.3s window -- smoothed average, reacts with a slight lag but produces clean, stable motion.

For visual effects like scale pulsing or color grading shifts, `audioRms()` almost always looks better. For percussive effects where you want instant response to transients (drum hits), `audioLevel()` is more appropriate.

Note that RMS values are generally lower than peak values for the same audio. Adjust your `linear()` input range accordingly -- the example above uses 0.05-0.5 instead of 0.05-0.8.

## Practical Tips

**Start by reading raw values.** Before building a complex expression, set your parameter expression to just `audioLevel("Both", time)` and play the clip. Watch the parameter value in the effect panel to understand the actual range your audio produces. Then wrap it in `linear()` with appropriate thresholds.

**Use a floor value.** Mapping to a range like `value * 0.3` to `value` instead of `0` to `value` prevents the effect from disappearing completely during quiet moments, which usually looks better.

**Combine audio with other functions.** Audio-reactive expressions can be added to other expressions:

```javascript
ease(time, 0, duration, 0, value * 0.8) + linear(audioLevel("Both", time), 0.1, 0.7, 0, value * 0.2)
```

This fades in from 0 to 80% of the parameter's range over the clip duration, with an additional 0-20% range of audio reactivity layered on top.

**Different channels for different effects.** If you are working with a stereo mix, you can drive the X position with the left channel and Y position with the right channel for a spatial audio-reactive effect:

- Position X: `linear(audioLevel("Left", time), 0.05, 0.8, value * -0.2, value * 0.2)`
- Position Y: `linear(audioLevel("Right", time), 0.05, 0.8, value * -0.2, value * 0.2)`
