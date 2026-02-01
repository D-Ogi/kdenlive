![](data/pics/kdenlive-logo.png)

# Kdenlive — D-Bus Scripting & Expressions Fork

Fork of [KDE/kdenlive](https://github.com/KDE/kdenlive) that adds a **D-Bus scripting API** and a **JavaScript expression engine** for programmatic control of Kdenlive from Python, CLI, or any D-Bus client.

## What this fork adds

### D-Bus Scripting API

108 new `Q_SCRIPTABLE` methods exposed via D-Bus (`org.kde.kdenlive.MainWindow`), covering:

| Category | Examples |
|----------|---------|
| **Project** | Open, save, new, properties, fps, resolution |
| **Media pool** | Import, folders, clip properties, delete, relink |
| **Timeline** | Insert, move, resize, cut, slip, delete clips; track management |
| **Effects** | Add/remove effects, get/set/update keyframes |
| **Compositions** | Add, move, resize, delete, list cross-track compositions |
| **Audio** | Volume get/set, audio fades, audio level analysis |
| **Subtitles** | Add, edit, delete, export; subtitle styles |
| **Markers/guides** | Add, list, delete by frame or category (timeline and clip markers) |
| **Groups** | Group/ungroup clips, query group membership |
| **Selection** | Get/set selection, select all, select by track |
| **Sequences** | List sequences, get/set active sequence |
| **Zones** | Get/set zone in/out points, extract zone |
| **Titles** | Create/read/update title clips (kdenlivetitle XML) |
| **Proxy** | Set, rebuild, delete, query clip proxy status |
| **Playback** | Seek, play, pause, get position |
| **Rendering** | Render timeline/bin frames as thumbnails, scene detection |
| **Undo** | Undo, redo, query undo stack status |

### JavaScript Expression Engine

An embedded QuickJS-based expression engine that allows keyframe parameters to be driven by JavaScript expressions (e.g. `sin(time * 2)`, audio-reactive effects). Includes:

- Expression editor dialog with syntax highlighting
- Built-in function library (math, time, audio, easing)
- Expression template system with a repository of presets
- Expression cache for performance

### libplacebo Effects

Two new GPU-accelerated effects via libplacebo: shader and render.

## Python API

Use with [kdenlive-api](https://github.com/D-Ogi/kdenlive-api) — a DaVinci Resolve-compatible Python wrapper:

```python
from kdenlive_api import Resolve

resolve = Resolve()
project = resolve.GetProjectManager().GetCurrentProject()
timeline = project.GetCurrentTimeline()
```

## Building

Follow the standard Kdenlive [build instructions](dev-docs/build.md). This fork tracks upstream `master`. Additional build requirements:

- **D-Bus**: `-DUSE_DBUS=ON` (enabled by default on Linux, must be explicit on Windows via Craft)
- **QuickJS**: Bundled in `src/expressions/quickjs/` (no external dependency)

## Upstream

For general Kdenlive information, features, and downloads visit [kdenlive.org](https://kdenlive.org).
Upstream source: [KDE Invent](https://invent.kde.org/multimedia/kdenlive) / [GitHub mirror](https://github.com/KDE/kdenlive).
