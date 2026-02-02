# Automask — SAM2-based Roto Brush for Kdenlive

Interactive object segmentation using [SAM2](https://github.com/facebookresearch/sam2) (Segment Anything Model 2). The user paints brush strokes, clicks points, or draws boxes on the clip monitor to select objects, then propagates the mask across the full clip duration.

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│  QML (MonitorAutomask.qml)                               │
│  Canvas-based brush drawing, point/box display,          │
│  progress bar, overlay preview                           │
└────────────┬─────────────────────────────────────────────┘
             │ signals: addControlStroke, addControlPoint,
             │          addControlRect, moveControlPoint,
             │          generateMask, maskUndoAction/Redo
             ▼
┌──────────────────────────────────────────────────────────┐
│  Monitor (monitor.cpp)                                   │
│  Keyboard handling (Ctrl+Z/Shift+Z), brush sampling      │
│  (15px distance threshold), coordinate transform          │
└────────────┬─────────────────────────────────────────────┘
             │ signals: addMonitorControlStroke, etc.
             ▼
┌──────────────────────────────────────────────────────────┐
│  MaskManager (maskmanager.cpp)                           │
│  Zone offset (position -= zone.x), frame export,        │
│  mask tree UI, apply mask to effect stack                │
└────────────┬─────────────────────────────────────────────┘
             │ direct calls
             ▼
┌──────────────────────────────────────────────────────────┐
│  AutomaskHelper (automaskhelper.cpp/hpp)                  │
│  Point/stroke/box storage, undo/redo stack,              │
│  300ms debounce timer, QProcess management,              │
│  stdin/stdout protocol with SAM2 Python process          │
└────────────┬─────────────────────────────────────────────┘
             │ stdin (commands) / stdout (responses)
             ▼
┌──────────────────────────────────────────────────────────┐
│  sam-objectmask.py                                       │
│  SAM2 image predictor (preview) +                        │
│  SAM2 video predictor (propagation)                      │
│  Outputs PNG mask frames                                 │
└──────────────────────────────────────────────────────────┘
```

## Input Methods

All input is in the QML monitor overlay (`MonitorAutomask.qml`):

| Action | Modifier | Description |
|--------|----------|-------------|
| Drag | *(none)* | Brush stroke — selects object under the stroke |
| Alt + Drag | Alt | Exclude stroke — removes region from selection |
| Shift + Click | Shift | Single include point |
| Alt + Click | Alt | Single exclude point |
| Ctrl + Drag | Ctrl | Bounding box selection |

Brush strokes are sampled at 15px intervals in `Monitor::addControlStroke()` to avoid sending thousands of points to SAM2.

## Undo / Redo

`AutomaskHelper` maintains a `QStack<MaskUndoAction>` (max 100 entries) supporting four action types:

| Type | Undo behavior |
|------|---------------|
| `AddPoint` | Remove point; restore previous frame data if non-extend |
| `AddStroke` | Remove all stroke points (batch) |
| `AddBox` | Remove box; restore previous frame data if non-extend |
| `MovePoint` | Restore original position |

Triggered via `Ctrl+Z` / `Ctrl+Shift+Z` in the monitor, relayed through the signal chain.

## Preview Debounce

Rapid point/stroke additions are debounced with a 300ms `QTimer` (`m_previewDebounce`). Each new input restarts the timer. When it fires, `sendPreviewCommand()` writes a `preview=` command to stdin.

## Overlay Modes

Three visualization modes for the mask preview (controlled from `MaskToolBar.qml`):

| Mode | ID | Description |
|------|----|-------------|
| Color Overlay | 0 | Semi-transparent colored fill (default) |
| Alpha Boundary | 1 | Contour outline only |
| Alpha Channel | 2 | White = selected, transparent = background |

The overlay mode is passed to Python via `--overlay-mode N`. During final `render=`, overlay is forced to 0 (color overlay) regardless of preview setting.

## C++ → Python Protocol (stdin/stdout)

`AutomaskHelper` launches `sam-objectmask.py` as a `QProcess` in read-write mode. Communication is line-based over stdin (commands) and stdout (responses).

### Commands (C++ → Python via stdin)

#### `preview=<args>`
Generate a single-frame mask preview.

```
preview=-F 42 -P 42=200,300,400,500 -L 42=1,1 --border 0 --color 255,100,100,180 --overlay-mode 0
```

Arguments follow argparse format:
- `-F <frame>` — frame index (0-based, relative to zone start)
- `-P <frame>=<x1,y1,x2,y2,...>` — point coordinates
- `-L <frame>=<label1,label2,...>` — point labels (1=include, 0=exclude)
- `-B <frame>=<x1,y1,x2,y2>` — bounding box (top-left, bottom-right corners)
- `--border <width>` — border width in pixels
- `--bordercolor <r,g,b,a>` — border color
- `--color <r,g,b,a>` — fill color
- `--overlay-mode <0|1|2>` — visualization mode

#### `render=<output_path>`
Propagate mask across all frames and export PNG sequence.

```
render=C:/Users/user/cache/output-frames
```

Adds all keyframe points/boxes to the video predictor, then calls `propagate_in_video()`. Outputs one PNG per frame.

#### `rerender=<output_path>`
Same as `render=`, but calls `videoPredictor.reset_state()` first. Used for corrections — when the user has edited keyframes after a previous render.

#### `edit=<args>`
Update mask visualization parameters (color, border) without re-running inference.

```
edit=--border 2 --bordercolor 255,0,0,200 --color 0,255,0,128
```

#### `finish`
Clean exit after rendering. Destroys the video predictor and exits the process.

### Responses (Python → C++ via stdout)

| Response | Meaning |
|----------|---------|
| `preview ok <frame>` | Preview PNG written for `<frame>` |
| `frame_done <N>/<total>` | Propagation progress (one per frame) |
| `mask ok` | Full mask render complete |
| `INFO:<message>` | Status message to display in UI |

### Buffered Parsing

`AutomaskHelper` accumulates stdout in `m_stdoutBuffer` and only processes complete `\n`-terminated lines. This prevents partial-line parsing (e.g., `"preview ok "` without a frame number).

## Workflow

### First mask creation

1. User clicks "Add Mask" in MaskManager
2. `MaskManager::exportFrames()` exports clip frames as JPEGs to cache
3. `AutomaskHelper::launchSam()` starts `sam-objectmask.py` with initial args
4. Python loads SAM2 model, waits on stdin
5. User paints strokes / adds points in QML overlay
6. Each input triggers debounced `preview=` command → Python returns `preview ok`
7. QML displays preview PNG overlay
8. User clicks "Generate Mask"
9. C++ sends `render=<path>` → Python propagates, streams `frame_done` progress
10. Python sends `mask ok` → C++ sends `finish` → Python exits
11. `MaskTask` creates final `.mkv` from PNG frames

### Correction workflow

1. User selects existing mask and clicks "Edit"
2. `MaskManager::editMask()` loads saved keyframe data, re-exports frames
3. User adds/moves/removes points on specific frames
4. User clicks "Generate Mask"
5. C++ sends `rerender=<path>` (which calls `reset_state()` for clean propagation)
6. Same completion flow as above

## File Locations

| Content | Path |
|---------|------|
| Source frames (JPEG) | `<cache>/mask/source-frames/00000.jpg` ... |
| Preview PNGs | `<cache>/mask/source-frames/preview-00042.png` |
| Output frames (PNG) | `<cache>/mask/output-frames/00000.png` ... |
| Final mask (MKV) | `<cache>/mask/<name>-<in>-<out>.mkv` |

## Key Files

| File | Role |
|------|------|
| `src/monitor/view/MonitorAutomask.qml` | QML overlay: brush canvas, point display, progress bar |
| `src/monitor/view/MaskToolBar.qml` | Toolbar: brush toggle, overlay mode selector |
| `src/assets/keyframes/model/automask/automaskhelper.hpp` | Data model: points, boxes, undo stack, process management |
| `src/assets/keyframes/model/automask/automaskhelper.cpp` | Core logic: input handling, debounce, stdin/stdout protocol |
| `src/effects/effectstack/view/maskmanager.hpp/cpp` | UI panel: mask list, zone management, signal routing |
| `src/monitor/monitor.h/cpp` | Keyboard shortcuts, brush sampling, signal relay |
| `src/monitor/monitorproxy.h/cpp` | Q_PROPERTY bridge: maskMode, maskProgress, overlayMode |
| `data/scripts/automask/sam-objectmask.py` | SAM2 inference: preview, propagation, mask export |
