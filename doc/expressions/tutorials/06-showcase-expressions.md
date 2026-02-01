# Showcase Expressions

Production-ready templates that combine multiple expression features. Each demonstrates a different class of problems the expression engine solves. Copy these directly into your effect parameters and adjust the marked constants to taste.

---

## How Expression Values Work

Expressions operate at the **MLT level** -- the raw internal values that the video engine uses. The `value` variable contains the current MLT-level base value of the parameter. Your expression's return value is written directly to MLT.

Some effects define a `factor` in their XML, which means the slider display differs from the MLT value:

| Effect parameter | Slider range | MLT range | Factor | MLT default |
|-----------------|-------------|-----------|--------|-------------|
| Brightness `level` | 0-400 | 0.0-4.0 | 100 | 1.0 |
| Box Blur `hradius` | 0-1000 | 0-1000 | none | 1 |
| Hue/Sat `av.sat` | -1.0-+1.0 | -1.0-+1.0 | none | 0.0 |
| Rotation `rotation` | -360-+360 | -360-+360 | none | 0.0 |
| Opacity `opacity` | 0-100 | 0.0-1.0 | 100 | 1.0 |

**Rule of thumb:** Keyframe values, `value`, and expression output are all MLT-level. When an expression says "apply to any parameter", the numeric constants must match the target parameter's MLT range.

---

## 1. Inertial Bounce (Elastic Overshoot)

The most popular After Effects expression, now in Kdenlive. Set two or more keyframes on any parameter -- the expression automatically generates a springy overshoot after each keyframe, as if the value has physical momentum.

**Requires:** At least 2 keyframes on the parameter.

**Apply to:** Any scalar parameter (opacity, scale, rotation, position X/Y, blur amount).

```javascript
// --- INERTIAL BOUNCE --------------------------------------------
// Set keyframes normally. The expression adds elastic overshoot
// after each one. Adjust the 3 constants below.
//
var freq = 3.0;    // Oscillation speed (Hz). Higher = faster bounce.
var decay = 5.0;   // Damping rate. Higher = settles faster.
var nMax = 4;      // Max number of visible bounces.

// Find the most recent keyframe
var n = numKeys;
if (n < 2) {
  value
} else {
  var idx = nearestKey(time).index;
  var kf = key(idx);
  if (time < kf.time && idx > 1) {
    kf = key(idx - 1);
    idx = idx - 1;
  }
  var t = time - kf.time;
  if (t < 0 || idx === n) {
    // Before first keyframe or at last keyframe: use velocity
    var vel = velocityAtTime(kf.time + 0.001);
    var amp = vel / (Math.PI * 2 * freq);
    kf.value + amp * Math.sin(Math.PI * 2 * freq * t) * Math.exp(-decay * t)
  } else {
    // Between keyframes: pass through normally
    value
  }
}
```

### How it works

1. `nearestKey(time)` finds the keyframe closest to the current frame.
2. `velocityAtTime()` measures how fast the parameter was changing at that keyframe -- this becomes the initial "momentum" of the bounce.
3. `Math.sin(freq * t)` creates the oscillation. `Math.exp(-decay * t)` damps it to zero over time.
4. The amplitude is derived from velocity: fast keyframe transitions produce big bounces, small changes produce subtle ones. No manual tuning needed.

### Tuning guide

| Parameter | Effect | Range |
|-----------|--------|-------|
| `freq` | Bounce speed | 1.5 (slow rubber) -- 8 (tight spring) |
| `decay` | How quickly it settles | 2 (long wobble) -- 10 (snaps to rest) |

### Example: brightness pop-in

Apply to brightness `level`. Set two keyframes: frame 0 = 0.0 (black, slider 0), frame 15 = 1.0 (normal brightness, slider 100). The expression makes the clip pop in from black, overshoot to ~1.2 (bright), bounce back through 0.95, and settle at 1.0. All from two keyframes.

Note: keyframe values are MLT-level. When you set the brightness slider to 100, the stored keyframe value is 1.0 (slider / factor 100). The expression reads and outputs these MLT values directly.

---

## 2. Audio-Reactive Beat Pulse with Marker Sync

Combines audio analysis with timeline markers to create precise beat-synced effects. Place markers on your beat points. The expression creates a sharp pulse on each marker, with the pulse intensity scaled by the actual audio energy at that moment. Between beats, the value decays smoothly.

**Requires:** Timeline markers placed on beat positions. Audio track overlapping the clip.

**Apply to:** Any scalar parameter. Adjust `floor` and `ceiling` to match the target parameter's MLT range (see table above).

```javascript
// --- BEAT PULSE -------------------------------------------------
// Place markers on beats. Expression creates pulses synced to
// markers, with intensity driven by actual audio energy.
//
// IMPORTANT: floor/ceiling are MLT-level values. Set them to
// match your target parameter. Examples:
//   Brightness level:  floor = 1.0 (normal), ceiling = 3.0 (bright flash)
//   Box Blur hradius:  floor = 0 (sharp),    ceiling = 40 (blurry)
//   Hue/Sat av.sat:    floor = 0.0 (normal), ceiling = 0.5 (vivid)
//   Opacity:           floor = 0.3 (faded),  ceiling = 1.0 (fully visible)
//
var attackSec = 0.02;  // Pulse rise time in seconds (instant feel).
var decaySec = 0.35;   // Pulse fall time in seconds.
var floor = 1.0;       // Resting MLT value between beats. (brightness: 1.0 = normal)
var ceiling = 3.0;     // Peak MLT value at full audio energy. (brightness: 3.0 = bright)
var audioSmooth = 0.1; // RMS window in seconds (smoothing).
var audioLo = 0.05;    // Audio threshold: below this = silence.
var audioHi = 0.6;     // Audio threshold: above this = full energy.

// Find nearest marker
var nMarkers = marker.numKeys;
if (nMarkers === 0) {
  // No markers: fall back to pure audio reactivity
  linear(audioRms("Both", time, audioSmooth), audioLo, audioHi, floor, ceiling)
} else {
  var mk = marker.nearestKey(time);
  var dist = time - mk.time;

  // Only pulse on markers we have passed (not future ones)
  if (dist < 0 && mk.index > 1) {
    mk = marker.key(mk.index - 1);
    dist = time - mk.time;
  }

  // Audio energy at the marker moment (how loud was the beat?)
  var energy = clamp(
    linear(audioRms("Both", mk.time, audioSmooth), audioLo, audioHi, 0, 1),
    0, 1
  );

  // Envelope: sharp attack, exponential decay
  var env;
  if (dist < 0) {
    env = 0;
  } else if (dist < attackSec) {
    env = ease(dist, 0, attackSec, 0, 1);
  } else {
    env = Math.exp(-(dist - attackSec) / decaySec * 3.0);
  }

  // Final: envelope shaped by audio energy, mapped to floor..ceiling range
  linear(env * energy, 0, 1, floor, ceiling)
}
```

### How it works

1. `marker.nearestKey(time)` finds the closest beat marker. We step back one marker if we haven't reached the nearest one yet -- we only pulse on beats that have already occurred.
2. `audioRms("Both", mk.time, 0.1)` samples the actual audio energy at the marker's exact position. Loud beats produce stronger pulses than quiet ones.
3. The envelope uses `ease()` for a quick attack (20ms) and `Math.exp()` for a natural-sounding exponential decay (350ms).
4. `linear()` maps the result to the desired output range.

### Tuning guide

| Parameter | Effect | Range |
|-----------|--------|-------|
| `attackSec` | Pulse sharpness | 0.01 (percussive) -- 0.1 (soft) |
| `decaySec` | Pulse tail length | 0.1 (staccato) -- 1.0 (sustained) |
| `audioSmooth` | Audio analysis window | 0.05 (reactive) -- 0.3 (averaged) |
| `audioLo/Hi` | Audio sensitivity range | Adjust based on your track's dynamics |
| `floor/ceiling` | Output range (MLT values) | Must match target parameter's range (see comments in code) |

### Variant: strobe on beats

For a hard on/off strobe instead of smooth decay, replace the envelope section:

```javascript
  var env = dist >= 0 && dist < 0.08 ? 1 : 0;
```

---

## 3. Cinematic Handheld Drift

Simulates organic, handheld camera movement by layering multiple noise frequencies. The motion has a slow, breathing base drift with faster micro-jitter on top. The intensity ramps in and out smoothly at clip boundaries so it never starts or ends abruptly.

**Requires:** Nothing (self-contained). Works best on position X/Y or rotation.

**Apply to:** Position X, Position Y (use different `seedRandom` seeds for each), or rotation with smaller amplitude.

```javascript
// --- CINEMATIC HANDHELD DRIFT -----------------------------------
// Layered organic motion: slow breathing + medium wander + fast jitter.
// Apply to Position X and Position Y separately (change the seed).
//
var seed = 1;           // Change per axis (1 for X, 2 for Y, 3 for rotation).
var breathAmp = 15.0;   // Slow drift amplitude (MLT units: pixels for position, degrees for rotation).
var breathFreq = 0.15;  // Slow drift frequency in Hz.
var wanderAmp = 6.0;    // Medium wander amplitude (MLT units).
var wanderFreq = 0.7;   // Medium wander frequency.
var jitterAmp = 2.0;    // Fast micro-jitter amplitude (MLT units).
var jitterFreq = 4.0;   // Fast micro-jitter frequency.
var fadeIn = 0.5;        // Seconds to ramp in at clip start.
var fadeOut = 0.3;       // Seconds to ramp out before clip end.

seedRandom(seed, false);

// Three octaves of wiggle at different timescales
var breath = wiggle(breathFreq, breathAmp);
var wander = wiggle(wanderFreq, wanderAmp);
var jitter = wiggle(jitterFreq, jitterAmp);

// Combine: breath is the base, wander and jitter add detail
var raw = breath + wander * 0.5 + jitter * 0.25;

// Smooth the combined signal to remove any harsh discontinuities
var smoothed = smooth(0.15, raw);

// Intensity envelope: fade in/out at clip boundaries
var rampIn = clamp(time / fadeIn, 0, 1);
var rampOut = clamp((duration - time) / fadeOut, 0, 1);
var envelope = ease(rampIn * rampOut, 0, 1, 0, 1);

value + smoothed * envelope
```

### How it works

1. Three `wiggle()` calls at different frequencies create layered Perlin-like motion:
   - **Breath** (0.15 Hz, 15px): slow, sweeping drift that takes 6-7 seconds per cycle.
   - **Wander** (0.7 Hz, 6px): medium motion, multiplied by 0.5 so it doesn't dominate.
   - **Jitter** (4 Hz, 2px): fast micro-tremor, multiplied by 0.25 for subtlety.
2. `smooth(0.15, raw)` applies temporal smoothing to the combined signal, preventing any frame-to-frame jumps.
3. `seedRandom(seed)` ensures each axis (X, Y, rotation) gets independent noise. Without different seeds, X and Y would move in lockstep.
4. The fade envelope uses `clamp()` + `ease()` to smoothly ramp the effect in/out over the first and last frames of the clip. This prevents the visible "snap" that happens when handheld drift starts or stops suddenly.

### Tuning guide

| Parameter | Effect | Range |
|-----------|--------|-------|
| `breathAmp` | Overall drift range | 5 (subtle) -- 30 (dramatic) |
| `breathFreq` | Base drift speed | 0.05 (glacial) -- 0.3 (noticeable sway) |
| `jitterAmp` | Handheld shake intensity | 0 (smooth dolly) -- 5 (nervous handheld) |
| `jitterFreq` | Shake speed | 2 (relaxed) -- 8 (tense) |
| `fadeIn/Out` | Envelope ramp duration | 0.2 (quick) -- 2.0 (very gradual) |

### Variant: documentary style

For a more restrained documentary look, reduce all amplitudes and increase smoothing:

```javascript
var breathAmp = 4.0;
var wanderAmp = 2.0;
var jitterAmp = 0.5;
var smoothed = smooth(0.3, raw);
```

### Variant: rotation only

For rotation (applied to the rotation parameter, values in degrees):

```javascript
var seed = 3;
var breathAmp = 0.3;    // degrees
var wanderAmp = 0.15;
var jitterAmp = 0.05;
```

---

## Feature Coverage

These three expressions collectively demonstrate:

| Feature | Expression 1 | Expression 2 | Expression 3 |
|---------|:---:|:---:|:---:|
| `numKeys` / `key()` / `nearestKey()` | x | | |
| `velocityAtTime()` | x | | |
| `value` (base parameter) | x | | x |
| `audioRms()` | | x | |
| `marker.numKeys` / `marker.nearestKey()` / `marker.key()` | | x | |
| `wiggle()` | | | x |
| `smooth()` | | | x |
| `seedRandom()` | | | x |
| `ease()` | | x | x |
| `linear()` | | x | |
| `clamp()` | | x | x |
| `Math.exp()` (spring/decay) | x | x | |
| `Math.sin()` (oscillation) | x | | |
| Multi-statement (`var`) | x | x | x |
| Conditional logic | x | x | |
