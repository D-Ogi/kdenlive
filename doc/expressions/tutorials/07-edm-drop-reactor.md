# EDM Drop Reactor

A coordinated multi-effect expression system for electronic music videos. Four expressions work together on a single clip to create beat-synced flash, shake, blur, and saturation shifts. The entire system reacts to audio energy and timeline markers -- you place markers on beats and drops, and the expressions do the rest.

**End result:** Your clip flashes white on every beat, shakes on heavy drops, blurs momentarily on bass hits, and shifts warm/cool saturation in sync with the energy of the track. No manual keyframing.

---

## Important: How Expression Values Work

Expressions operate at the **MLT level** -- the raw internal values that the video engine uses. Each effect parameter has its own range. The `value` variable contains the current MLT-level base value of the parameter (what you set with the slider before adding the expression). Your expression's return value is written directly to MLT.

The slider display and the MLT value differ when the effect XML defines a `factor`:

| Parameter | Slider range | Slider default | MLT range | MLT default | Factor |
|-----------|-------------|---------------|-----------|-------------|--------|
| Brightness `level` | 0-400 | 100 | 0.0-4.0 | 1.0 | 100 |
| Box Blur `hradius` | 0-1000 | 1 | 0-1000 | 1 | none |
| Hue/Sat `av.sat` | -1.0-+1.0 | 0.0 | -1.0-+1.0 | 0.0 | none |
| Rotation `rotation` | -360-+360 | 0 | -360-+360 | 0.0 | none |

**Brightness** is the odd one out: its slider shows 0-400, but MLT stores 0.0-4.0 (slider / 100). At the default slider position of 100, `value` = 1.0. All other parameters here have no factor -- their slider values equal their MLT values.

**Rule of thumb:** Leave sliders at their defaults before adding expressions. Then `value` will be 1.0 for brightness, 0 for everything else.

---

## Prerequisites

- A video clip on your timeline
- An audio track (your EDM track) that overlaps the video clip
- Timeline markers placed on beat positions (see Marker Workflow below)

## Marker Workflow

The system uses **marker colors** to distinguish beat types:

| Marker color | Meaning | When to place |
|-------------|---------|---------------|
| **Purple** | Regular beat | Every kick/snare hit in the main section |
| **Red** | Drop / heavy hit | The moment the bass drops, breakdown hits, build-up climax |
| **Yellow** | Accent | Synth stabs, vocal hits, any moment you want extra emphasis |

### Placing markers quickly

1. Play the timeline and tap `M` on every beat to place purple markers.
2. Go back and recolor the drop/climax markers to red (right-click marker, change color).
3. Optionally add yellow accent markers.

The expressions read ALL markers regardless of color. The color coding is for your visual reference while editing.

---

## Setup: Effects Stack

Add these four effects to your video clip, in this order:

| # | Effect | Effect ID | Parameter with expression |
|---|--------|-----------|--------------------------|
| 1 | Brightness | `brightness` | `level` |
| 2 | Box Blur | `box_blur` | `hradius` and `vradius` (same expression on both) |
| 3 | Hue/Saturation | `avfilter.huesaturation` | `av.sat` |
| 4 | Transform | `qtblend` | `rotation` |

Leave all sliders at their defaults before adding expressions. The expressions reference `value` to read the base position.

---

## Expression 1: Beat Flash (Brightness)

**Apply to:** Brightness `level` parameter (leave slider at default 100 -> `value` = 1.0 in MLT).

Creates a sharp white flash on every beat marker, with intensity proportional to the audio energy at that moment. Drops produce blinding flashes; quiet beats produce subtle pulses.

```javascript
// --- BEAT FLASH ------------------------------------------------
// Brightness flash synced to markers + audio energy.
// Apply to: brightness > level
//
// MLT range: 0.0 = black, 1.0 = normal, 4.0 = max.
// At default slider (100), value = 1.0.
//
var attackMs = 15;       // Flash rise time (ms). Lower = sharper.
var decayMs = 200;       // Flash fall time (ms). Lower = snappier.
var flashBoost = 2.0;    // Max boost above value (1.0 + 2.0 = 3.0 = very bright).
var audioWindow = 0.08;  // RMS analysis window (seconds).
var audioFloor = 0.08;   // Audio below this = silence.
var audioCeil = 0.65;    // Audio above this = max energy.

var attack = attackMs / 1000;
var decay = decayMs / 1000;

var nM = marker.numKeys;
if (nM === 0) {
  // No markers: gentle audio pulse as fallback
  var e = linear(audioRms("Both", time, audioWindow), audioFloor, audioCeil, 0, 1);
  value + e * flashBoost * 0.3
} else {
  // Find most recent marker
  var mk = marker.nearestKey(time);
  var dist = time - mk.time;
  if (dist < -0.001 && mk.index > 1) {
    mk = marker.key(mk.index - 1);
    dist = time - mk.time;
  }

  if (dist < 0) {
    value
  } else {
    // Audio energy at beat moment (floor 0.2 so even quiet beats flash)
    var energy = clamp(
      linear(audioRms("Both", mk.time, audioWindow), audioFloor, audioCeil, 0.2, 1),
      0, 1
    );

    // Envelope: sharp attack, exponential decay
    var env;
    if (dist < attack) {
      env = ease(dist, 0, attack, 0, 1);
    } else {
      env = Math.exp(-(dist - attack) / decay * 4.0);
    }

    // Boost above base: value + up to flashBoost (clamped to MLT max 4.0)
    clamp(value + env * energy * flashBoost, 0, 4.0)
  }
}
```

### What you see

At rest, the clip is at normal brightness (`value` = 1.0). When a beat marker passes, brightness spikes toward 3.0 (bright white) and decays back to 1.0 over 200ms. Louder beats produce brighter flashes. The result is clamped to 4.0 (MLT maximum).

### Tuning

- **Brighter flash?** Increase `flashBoost` (try 3.0 for extreme peaks up to 4.0).
- **Dimmer resting state?** Lower the slider before adding the expression. `value` captures whatever the slider is set to.
- **Flash too long?** Lower `decayMs` (try 100 for a snappier pop).

---

## Expression 2: Drop Blur (Box Blur)

**Apply to:** Box Blur `hradius` AND `vradius` (paste the same expression into both). Set slider to 0 first -> `value` = 0.

Creates a momentary blur pulse on each beat. The blur is barely visible on regular beats but hits hard on bass-heavy drops. Simulates the "bass face" focus loss effect.

```javascript
// --- DROP BLUR --------------------------------------------------
// Blur pulse synced to markers + bass energy.
// Apply to: box_blur > hradius AND vradius (same expression)
//
// MLT range: 0 = sharp, 1000 = maximum blur.
// Useful range for this effect: 0-40.
//
var attackMs = 10;       // Blur rise time (ms).
var decayMs = 180;       // Blur fall time (ms).
var maxBlur = 30;        // Max blur radius on loudest hits (MLT units).
var audioWindow = 0.06;  // Short window to catch transients.
var audioFloor = 0.15;   // Only trigger on reasonably loud audio.
var audioCeil = 0.7;     // Above this = max blur.
var threshold = 0.3;     // Energy threshold: below this, no blur at all.

var attack = attackMs / 1000;
var decay = decayMs / 1000;

var nM = marker.numKeys;
if (nM === 0) {
  value
} else {
  var mk = marker.nearestKey(time);
  var dist = time - mk.time;
  if (dist < -0.001 && mk.index > 1) {
    mk = marker.key(mk.index - 1);
    dist = time - mk.time;
  }

  if (dist < 0) {
    value
  } else {
    var energy = clamp(
      linear(audioRms("Both", mk.time, audioWindow), audioFloor, audioCeil, 0, 1),
      0, 1
    );

    // Only blur if energy exceeds threshold (skip weak beats)
    if (energy < threshold) {
      value
    } else {
      var env;
      if (dist < attack) {
        env = ease(dist, 0, attack, 0, 1);
      } else {
        env = Math.exp(-(dist - attack) / decay * 5.0);
      }

      // Quadratic scaling: bigger difference between medium and heavy hits
      value + env * energy * energy * maxBlur
    }
  }
}
```

### What you see

Most beats produce no visible blur (energy below threshold). Heavy bass drops produce a 1-2 frame blur burst that snaps back to sharp. The effect is subliminal -- you feel the impact more than you see it.

### Tuning

- **More blur?** Increase `maxBlur` (try 60 for a heavier feel, 100 for a dramatic out-of-focus slam).
- **Blur on lighter beats?** Lower `threshold` (try 0.15).
- **Blur lingers too long?** Lower `decayMs` (try 100).

---

## Expression 3: Energy Saturation (Hue/Saturation)

**Apply to:** Hue/Saturation `av.sat` parameter (leave slider at default 0 -> `value` = 0.0).

Continuously modulates saturation based on audio energy. Quiet sections are slightly desaturated (moody), loud sections are punchy and vivid. A slow sine wave adds drift so the color never feels static.

```javascript
// --- ENERGY SATURATION ------------------------------------------
// Audio-driven saturation: quiet = muted, loud = vivid.
// Apply to: avfilter.huesaturation > av.sat
//
// MLT range: -1.0 = fully desaturated, 0.0 = normal, +1.0 = double saturation.
// No factor -- slider values equal MLT values.
//
var audioWindow = 0.25;  // Longer window for smooth response.
var audioFloor = 0.05;
var audioCeil = 0.55;
var satLow = -0.15;      // Saturation in quiet parts (below normal).
var satHigh = 0.4;       // Saturation in loud parts (above normal).
var driftAmp = 0.05;     // Slow sine drift amplitude.
var driftFreq = 0.12;    // Drift frequency in Hz.

// Smooth audio energy
var energy = clamp(
  linear(audioRms("Both", time, audioWindow), audioFloor, audioCeil, 0, 1),
  0, 1
);

// Map energy to saturation value (absolute, not offset from value)
var sat = linear(energy, 0, 1, satLow, satHigh);

// Add slow organic drift so it never looks mechanical
var drift = Math.sin(time * Math.PI * 2 * driftFreq) * driftAmp;

// Smooth the combined result, clamp to MLT range
clamp(smooth(0.2, sat + drift), -1.0, 1.0)
```

### What you see

During a quiet intro, the clip looks slightly washed out and moody (saturation at -0.15). As the track builds, colors gradually intensify. At the drop, saturation jumps to +0.4 -- vivid, punchy, alive. The slow sine drift adds organic variation so consecutive loud moments don't look identical.

### Tuning

- **More vivid drops?** Increase `satHigh` (try 0.6 -- remember max is 1.0).
- **Moodier quiet sections?** Decrease `satLow` (try -0.3).
- **Smoother transitions?** Increase `audioWindow` (try 0.4).

---

## Expression 4: Impact Shake (Rotation)

**Apply to:** Transform `rotation` parameter (leave slider at default 0 -> `value` = 0.0).

Adds a sharp rotational jitter on beat markers that decays to stillness. The shake direction alternates on each beat. Combined with the flash and blur, this completes the physical "impact" feel.

```javascript
// --- IMPACT SHAKE -----------------------------------------------
// Rotational jitter synced to markers.
// Apply to: qtblend > rotation
//
// MLT range: -360 to +360 degrees. No factor.
// value = 0.0 means no rotation.
//
var decayMs = 250;       // Shake duration (ms).
var maxDeg = 1.5;        // Max rotation in degrees.
var oscFreq = 18;        // Oscillation frequency (Hz). Higher = tighter shake.
var audioWindow = 0.06;
var audioFloor = 0.1;
var audioCeil = 0.65;
var threshold = 0.25;    // Only shake on hits above this energy.

var decay = decayMs / 1000;

var nM = marker.numKeys;
if (nM === 0) {
  value
} else {
  var mk = marker.nearestKey(time);
  var dist = time - mk.time;
  if (dist < -0.001 && mk.index > 1) {
    mk = marker.key(mk.index - 1);
    dist = time - mk.time;
  }

  if (dist < 0) {
    value
  } else {
    var energy = clamp(
      linear(audioRms("Both", mk.time, audioWindow), audioFloor, audioCeil, 0, 1),
      0, 1
    );

    if (energy < threshold) {
      value
    } else {
      // Damped oscillation: sine wave with exponential decay
      var envelope = Math.exp(-dist / decay * 4.0);
      var oscillation = Math.sin(dist * Math.PI * 2 * oscFreq);

      // Direction seed from marker index (alternates left/right)
      var direction = mk.index % 2 === 0 ? 1 : -1;

      // Offset from center: value is 0 degrees, add up to +/-maxDeg
      value + direction * oscillation * envelope * energy * maxDeg
    }
  }
}
```

### What you see

On heavy beats, the frame jolts 0.5-1.5 degrees and oscillates rapidly back to center. The oscillation decays within 250ms. Light beats produce no shake (below threshold). The alternating direction from `mk.index % 2` prevents the shake from always starting the same way.

### Tuning

- **Stronger shake?** Increase `maxDeg` (try 3.0 for dramatic, 5.0 for extreme).
- **Slower oscillation?** Lower `oscFreq` (try 10 for a heavier, slower wobble).
- **Shake on lighter beats?** Lower `threshold`.

---

## Complete Setup Walkthrough

### Step 1: Prepare audio

1. Import your EDM track and place it on Audio Track 1.
2. Import your video clip and place it on Video Track 1, aligned with the audio.
3. Play the timeline to ensure audio and video are in sync.

### Step 2: Place markers

1. Play the timeline from the beginning.
2. Tap `M` on every main beat (kick drum hit) to place purple markers.
3. After placing all markers, go back and find the **drop moments** -- right-click those markers and change their color to red.
4. This step is the only manual work. Everything else is automatic.

### Step 3: Add effects to the video clip

Double-click the video clip to open the effect stack. Add:

1. **Brightness** (search "brightness" in the effects panel)
2. **Box Blur** (search "box blur")
3. **Hue/Saturation** (search "huesaturation")
4. **Transform** (search "transform" or "qtblend")

### Step 4: Apply expressions

For each effect parameter:

1. Right-click the parameter name (e.g., "Level" in Brightness).
2. Select **Add expression**.
3. Paste the corresponding expression from above.
4. The expression is immediately active -- play the timeline to preview.

Apply in order:
- Brightness > Level: paste **Expression 1** (Beat Flash)
- Box Blur > Horizontal: paste **Expression 2** (Drop Blur)
- Box Blur > Vertical: paste **Expression 2** (Drop Blur) -- same expression
- Hue/Saturation > Saturation: paste **Expression 3** (Energy Saturation)
- Transform > Rotation: paste **Expression 4** (Impact Shake)

### Step 5: Tune

Play through the drop section. Adjust constants in each expression:

- **Too subtle?** Increase `flashBoost`, `maxBlur`, `maxDeg`.
- **Too aggressive?** Decrease them, or raise `threshold`.
- **Blur on weak beats?** Raise the blur `threshold` (default 0.3).
- **Flash timing off?** Adjust `decayMs`.
- **Audio not triggering?** Lower `audioFloor`, raise `audioCeil`. Test with raw `audioRms("Both", time, 0.1)` first to see your track's actual range.

---

## Variations

### Minimal (flash only)

For a cleaner look, use only Expression 1 (Beat Flash) and Expression 3 (Energy Saturation). Skip blur and shake. This works well for lyric videos and calmer electronic genres.

### Hard strobe

Replace the exponential decay in Expression 1 with a hard on/off gate:

```javascript
    var env = dist < 0.04 ? 1 : 0;
```

This produces a 1-frame white flash instead of a smooth decay. Very aggressive -- works for hardstyle/gabber.

### Slow build

For tracks with a long build-up before the drop, multiply all effect intensities by a ramp:

```javascript
var buildStart = 30.0;    // Build starts at 30 seconds
var buildDuration = 16.0; // Build lasts 16 seconds
var buildRamp = clamp((time - buildStart) / buildDuration, 0, 1);
```

Multiply the offset portion (the part added to `value`) by `buildRamp` to make effects gradually intensify during the build and reach full power at the drop.

### Position shake instead of rotation

If you prefer translational shake over rotation, apply a modified Expression 4 to the position X/Y parameters of `qtblend`. Use `maxDeg` as a pixel offset instead (e.g., 10 for ~10px shift) and use different direction logic for each axis:

**Position X:**
```javascript
var direction = mk.index % 3 === 0 ? 1 : -1;
```

**Position Y:**
```javascript
var direction = mk.index % 3 === 1 ? 1 : -1;
```

---

## Feature Coverage

This tutorial demonstrates the following expression engine features working together:

| Feature | Expr 1 | Expr 2 | Expr 3 | Expr 4 |
|---------|:---:|:---:|:---:|:---:|
| `marker.numKeys` | x | x | | x |
| `marker.nearestKey()` | x | x | | x |
| `marker.key()` | x | x | | x |
| `audioRms()` | x | x | x | x |
| `linear()` | x | x | x | |
| `ease()` | x | x | | |
| `clamp()` | x | x | x | x |
| `smooth()` | | | x | |
| `Math.exp()` | x | x | | x |
| `Math.sin()` | | | x | x |
| `value` (baseline) | x | x | x | x |
| Multi-statement `var` | x | x | x | x |
| Conditional logic | x | x | | x |
| Threshold gating | | x | | x |
