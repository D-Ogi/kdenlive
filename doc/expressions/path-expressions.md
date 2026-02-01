# Path Expressions (`createPath`)

Generate procedural mask shapes per-frame using JavaScript expressions, compatible with After Effects' `createPath()` API.

## API

```javascript
createPath(points, inTangents, outTangents, isClosed)
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `points` | `[[x,y], ...]` | required | Array of vertex positions (normalized 0.0-1.0) |
| `inTangents` | `[[dx,dy], ...]` | `[]` | In-handle offsets from each point (empty = all linear) |
| `outTangents` | `[[dx,dy], ...]` | `[]` | Out-handle offsets from each point (empty = all linear) |
| `isClosed` | `boolean` | `true` | Whether the path is closed |

Returns a path object that the expression engine bakes into roto-spline JSON.

## Coordinate System

- **Normalized**: All coordinates are 0.0 to 1.0, relative to the frame dimensions.
  - `(0, 0)` = top-left corner
  - `(1, 1)` = bottom-right corner
  - `(0.5, 0.5)` = center of frame
- Use `thisClip.width` and `thisClip.height` if you need pixel-based calculations:
  ```javascript
  var px = 100;  // 100 pixels from left
  var x = px / thisClip.width;
  ```

## Tangent Convention

Tangents are **offsets from the point**, not absolute positions.

- `[0, 0]` = linear segment (sharp corner)
- `[0.05, 0]` = smooth handle extending 5% of frame width to the right
- If `inTangents` or `outTangents` arrays are shorter than `points`, missing entries default to `[0, 0]` (linear).

The conversion to Kdenlive's internal BPoint format:
```
h1 (in-handle)  = [point.x + inTangent.x,  point.y + inTangent.y]
p  (vertex)      = [point.x, point.y]
h2 (out-handle) = [point.x + outTangent.x, point.y + outTangent.y]
```

## Available Globals

All standard expression globals work inside path expressions:

| Variable | Type | Description |
|----------|------|-------------|
| `time` | number | Current time in seconds (clip-relative) |
| `frame` | number | Current frame number (0-based, clip-relative) |
| `duration` | number | Clip duration in seconds |
| `fps` | number | Project FPS |
| `thisClip.width` | number | Source image width in pixels |
| `thisClip.height` | number | Source image height in pixels |
| `Math.*` | — | Full JavaScript Math library |

Audio functions (`audioLevel`, `audioRms`) and marker functions (`marker.key`, `marker.nearestKey`) also work.

## Usage

1. Add a **Rotoscoping** effect to a clip
2. Open the expression editor on the `spline` parameter
3. Write a JS expression that ends with a `createPath(...)` call
4. The expression is evaluated for every frame and baked into the roto-spline

## Examples

### Static Circle

```javascript
var n = 32;
var cx = 0.5, cy = 0.5, r = 0.2;
var pts = [], inT = [], outT = [];
var k = r * 0.5522847498;  // kappa for cubic bezier circle

for (var i = 0; i < n; i++) {
    var a = i * 2 * Math.PI / n;
    var aNext = (i + 1) * 2 * Math.PI / n;
    var aPrev = (i - 1) * 2 * Math.PI / n;
    pts.push([cx + r * Math.cos(a), cy + r * Math.sin(a)]);

    // Tangent perpendicular to radius, length proportional to arc segment
    var tLen = r * Math.tan(Math.PI / n) * 0.5522847498;
    inT.push([-tLen * Math.sin(a), tLen * Math.cos(a)]);
    outT.push([tLen * Math.sin(a), -tLen * Math.cos(a)]);
}
createPath(pts, inT, outT, true);
```

### Rotating Gear

```javascript
var n = 20;
var cx = 0.5, cy = 0.5;
var r1 = 0.15, r2 = 0.25;
var angle = time * 2 * Math.PI / 4;  // full rotation every 4 seconds
var pts = [];
for (var i = 0; i < n * 2; i++) {
    var a = angle + i * Math.PI / n;
    var r = (i % 2 === 0) ? r2 : r1;
    pts.push([cx + r * Math.cos(a), cy + r * Math.sin(a)]);
}
createPath(pts, [], [], true);
```

### Pulsing Star (Audio-Reactive)

```javascript
var n = 5;
var cx = 0.5, cy = 0.5;
var level = audioLevel("Both", time);
var r1 = 0.1 + level * 0.1;   // inner radius pulses with audio
var r2 = 0.2 + level * 0.15;  // outer radius pulses more
var pts = [];
for (var i = 0; i < n * 2; i++) {
    var a = i * Math.PI / n - Math.PI / 2;
    var r = (i % 2 === 0) ? r2 : r1;
    pts.push([cx + r * Math.cos(a), cy + r * Math.sin(a)]);
}
createPath(pts, [], [], true);
```

### Horizontal Wipe

```javascript
var progress = time / duration;  // 0 to 1 over clip length
var x = progress;
createPath(
    [[0,0], [x,0], [x,1], [0,1]],
    [], [], true
);
```

## Limitations

- Path expressions bake every frame — no keyframe interpolation between frames.
- Monitor overlay handles are read-only — the baked spline displays BPoint control points on the monitor for the current frame, but manually dragging them will be overwritten on the next expression re-bake.
- Path method accessors (`.points()`, `.inTangents()`) for reading existing paths are not yet supported.
- Expressions that return a scalar number (not `createPath()`) on a `Roto_spline` parameter will be ignored.
