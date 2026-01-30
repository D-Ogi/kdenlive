![](data/pics/kdenlive-logo.png)

# Kdenlive — D-Bus Scripting Fork

Fork of [KDE/kdenlive](https://github.com/KDE/kdenlive) that adds a **D-Bus scripting API** (`org.kde.kdenlive.scripting`) for programmatic control of Kdenlive from Python, CLI, or any D-Bus client.

## What this fork adds

One commit on top of upstream Kdenlive, exposing ~40 `Q_SCRIPTABLE` methods via D-Bus:

| Category | Methods |
|----------|---------|
| **Project** | Open, save, properties, fps, resolution |
| **Media pool** | Import, folders, clip properties, delete |
| **Timeline** | Insert/move/resize/delete clips, track management |
| **Transitions** | Same-track mixes, cross-track compositions |
| **Markers/guides** | Add, list, delete by frame or category |
| **Playback** | Seek, play, pause, position |

Changed files: `src/mainwindow.h`, `src/mainwindow.cpp`, `src/org.kdenlive.MainWindow.xml`

## Python API

Use with [kdenlive-api](https://github.com/D-Ogi/kdenlive-api) — a DaVinci Resolve-compatible Python wrapper:

```python
from kdenlive_api import Resolve

resolve = Resolve()
project = resolve.GetProjectManager().GetCurrentProject()
timeline = project.GetCurrentTimeline()
```

## Building

Follow the standard Kdenlive [build instructions](dev-docs/build.md). This fork tracks upstream `master` and should build identically — the only addition is the D-Bus interface.

## Upstream

For general Kdenlive information, features, and downloads visit [kdenlive.org](https://kdenlive.org).
Upstream source: [KDE Invent](https://invent.kde.org/multimedia/kdenlive) / [GitHub mirror](https://github.com/KDE/kdenlive).
