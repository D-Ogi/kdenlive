# Roto Brush AE-quality UX — Implementation Status

## Implementation Order (per plan)

| # | Step | Status | Files | Notes |
|---|------|--------|-------|-------|
| 1 | **1.3** Debug cleanup | DONE | MonitorAutomask.qml, automaskhelper.cpp, maskmanager.cpp, monitorproxy.cpp, sam-objectmask.py | Removed console.log, qDebug spam; fixed Python edit= bug |
| 2 | **1.1** Brush strokes | DONE | MonitorAutomask.qml, MaskToolBar.qml, automaskhelper.hpp/cpp, monitor.h/cpp, qmlmanager.cpp, maskmanager.hpp/cpp | Canvas-based brush drawing, 15px sampling, full signal chain |
| 3 | **1.2** Live preview | DONE | automaskhelper.hpp/cpp, sam-objectmask.py, MonitorAutomask.qml | 300ms debounce timer, pulsing indicator, --preview-scale support |
| 4 | **2.3** Overlay modes | DONE | MaskToolBar.qml, monitorproxy.h/cpp, automaskhelper.cpp, sam-objectmask.py | 3 modes: Color Overlay, Alpha Boundary, Alpha Channel |
| 5 | **2.4** Undo stack | DONE | automaskhelper.hpp/cpp, maskmanager.hpp/cpp, monitor.h/cpp | MaskUndoAction struct, QStack undo/redo, Ctrl+Z/Ctrl+Shift+Z |
| 6 | **2.1** Span propagation | DONE | sam-objectmask.py, automaskhelper.cpp, MonitorAutomask.qml, monitorproxy.h/cpp, maskmanager.cpp | Streaming per-frame progress, green progress bar in ruler |
| 7 | **2.2** Corrections | DONE | sam-objectmask.py, automaskhelper.cpp | rerender= command with reset_state, finish command for clean exit |

## Current Phase: COMPLETE + AUDIT FIX PASS + EDGE CASE FIXES

### Verification Checklist
- [x] Codebase explored — all key files read and understood
- [x] Step 1.3 — debug cleanup
- [x] Step 1.1 — brush strokes
- [x] Step 1.2 — live preview
- [x] Step 2.3 — overlay modes
- [x] Step 2.4 — undo stack
- [x] Step 2.1 — span propagation
- [x] Step 2.2 — corrections
- [x] Final review — KDE code style, REUSE licensing, i18n
- [x] Audit fix pass 1 — HIGH + MEDIUM issues (code review, internal consistency, pattern consistency)
- [x] Audit fix pass 2 — LOW issues (enum class, Doxygen, const-correctness, false positive triage)
- [x] Edge case fixes — 10 bugs (2 our code, 3 C++ pre-existing, 5 Python pre-existing)

## Decisions Log

| Date | Decision | Rationale |
|------|----------|-----------|
| 2026-02-02 | Start with Step 1.3 (cleanup) | No dependencies, establishes clean baseline |
| 2026-02-02 | Brush tool as default, Shift=point, Ctrl=box, Alt=exclude | Matches AE UX where brush is primary tool |
| 2026-02-02 | 15px distance sampling for brush strokes | Balance between SAM2 point density and performance |
| 2026-02-02 | 300ms debounce for preview generation | Batches rapid stroke additions without perceptible delay |
| 2026-02-02 | Undo via Monitor keyPressEvent signal chain | Follows existing signal pattern (Monitor → MaskManager → AutomaskHelper) |
| 2026-02-02 | Streaming propagation (single-pass) | Eliminates memory usage spike from collecting all frames before export |
| 2026-02-02 | rerender= for corrections (reset_state + re-propagate) | SAM2 requires clean state for accurate re-propagation with new keyframes |
| 2026-02-02 | render= no longer exits process, finish command added | Allows process reuse for potential future iterative corrections |

## Audit Fix Pass

Three parallel audits (code review, internal consistency, Kdenlive pattern consistency) were performed. All HIGH and MEDIUM issues were fixed, followed by a second pass addressing LOW issues.

### HIGH — Fixed

| ID | Issue | Fix |
|----|-------|-----|
| **F8** | `redoAction()` AddPoint doesn't clear existing data when `hadPreviousData=true` | Added non-extend clear logic in redo path |
| **F9** | `redoAction()` AddBox same problem | Same fix as F8 |
| **H4** | Lambda connections in `launchSam()` accumulate on repeated calls | Added 3x `disconnect()` before `connect()` calls |
| **H3** | `moveMonitorControlPoint` uses `ix` instead of `adjustedIx` for include mutation | Changed to `adjustedIx` |
| **H1** | `loadData()` passes QString by value instead of `const QString &` | Changed to const reference in declaration and definition |

### MEDIUM — Fixed

| ID | Issue | Fix |
|----|-------|-----|
| **M1** | QML overlay refresh duplicated 5x | Replaced with `refreshQmlOverlay()` calls in all 5 sites |
| **M5** | `m_maskOverlayMode`, `m_maskProgress` in protected instead of private | Moved to private section (friend class grants access) |
| **M6** | Bare `MaskModeType` in QML, numeric literal `== 2` | Added namespace qualifiers (`Kdenlive.`/`K.`), replaced magic numbers |
| **M8** | `Q_EMIT m_maskHelper->showMessage(...)` | Removed `Q_EMIT` (emitting another object's signal) |
| **M10** | `!= None` / `== None` in Python | Changed to `is not None` / `is None` (PEP 8) |
| **F2** | `render=`/`rerender=` uses stale overlay_mode from preview | Force `overlay_mode = 0` before final mask rendering |

### LOW — Fixed (second pass)

| ID | Issue | Fix |
|----|-------|-----|
| **L1** | `MaskUndoAction::Type` is `enum` not `enum class` | Changed to `enum class Type`. All 12 usage sites already fully qualified — zero collateral changes. |
| **L2** | Stale Doxygen on AutomaskHelper constructor | Replaced copied `@param init_value/model/index` with accurate description. |
| **L3** | Range-for loops missing `const` on iterators | All 9 `for (auto &p :` → `for (const auto &p :` in automaskhelper.cpp. |
| **L8/L9** | Magic numbers in MonitorAutomask.qml | Already covered by M6 fix. Verified: zero remaining numeric maskMode comparisons. |

### LOW — Not Fixed (with rationale)

| ID | Issue | Rationale |
|----|-------|-----------|
| **L5** | Old-style SIGNAL/SLOT in qmlmanager.cpp | Intentional: `root` is `QObject*` from QML (dynamic type). New-style syntax requires compile-time type. Entire file uses old-style consistently. |
| **L6** | Python code outside `if __name__` block | Pre-existing structural pattern (~250 lines at module level). Refactoring = major risk for no behavioral gain. |
| **L7** | `np.fromstring` | **False positive.** Both calls use `sep=','` (text mode), which is NOT deprecated. Only binary mode was removed in NumPy 2.3. |
| **M2** | O(n) QList::contains for stroke dedup | Acceptable: 10-50 points max. QSet would need QPoint hash (not in Qt). |
| **M4** | maskOverlayMode is int not Q_ENUM | Would require C++ enum def, Q_ENUM registration, Q_PROPERTY type change, QML updates. Significant refactor for minor type safety on a 3-value int. |
| **H5** | Direct member access via `friend class AutomaskHelper` | Pre-existing Kdenlive pattern. Replacing with getters/setters across files for no behavioral gain. |

## Edge Case Fixes (post-audit)

10 bugs found by triple-agent edge-case audit, all fixed:

| ID | Severity | Fix |
|----|----------|-----|
| OUR-1 | UI_GLITCH | `isBrushing` state leak on maskMode change mid-stroke → reset in `onMaskModeChanged` |
| OUR-2 | UI_GLITCH | Brush canvas ghost on frame seek → clear in `onDisplayFrameChanged` |
| CPP-1 | CRASH | `moveMonitorControlPoint` out-of-bounds index → bounds check added |
| CPP-3 | UI_GLITCH | Partial stdout line parsing → buffered line accumulation |
| CPP-4 | UI_GLITCH | `generateMask` doesn't stop debounce timer → `m_previewDebounce.stop()` added |
| PY-1 | CRASH | stdin EOF infinite busy loop → exit on empty readline |
| PY-2 | DATA_CORRUPTION | Double `render=` without reset → `reset_state()` added |
| PY-4 | CRASH | `predict()` returns empty masks → guard with blank mask fallback |
| PY-5 | SILENT_FAILURE | `predictor` not restored globally after render → use `global predictor` |
| PY-6 | DATA_CORRUPTION | Alpha Channel mode: all-zero mask → opaque black → use mask as alpha |

## Known Issues Found During Exploration

1. **Python `edit=` bug** (FIXED): `sam-objectmask.py` used `args.border` instead of `inArgs.border` — stdin edits were silently ignored
2. `console.log` debug statements in MonitorAutomask.qml (FIXED — removed)
3. Excessive `qDebug()` in automaskhelper.cpp and maskmanager.cpp (FIXED — removed)

## Files Modified (all changes)

| File | Changes |
|------|---------|
| `src/monitor/view/MonitorAutomask.qml` | Brush properties, Canvas overlay, stroke signals, pulsing indicator, info label text, progress bar |
| `src/monitor/view/MaskToolBar.qml` | Brush tool toggle, overlay mode button |
| `src/assets/keyframes/model/automask/automaskhelper.hpp` | MaskUndoAction struct, debounce timer, undo stacks, new slots/signals |
| `src/assets/keyframes/model/automask/automaskhelper.cpp` | Stroke handling, debounce, overlay mode, undo/redo, streaming output parsing, rerender command, finish command |
| `src/monitor/monitor.h` | addControlStroke slot, stroke/undo/redo signals |
| `src/monitor/monitor.cpp` | addControlStroke impl (sampling), Ctrl+Z/Shift+Z mask undo/redo |
| `src/monitor/qmlmanager.cpp` | QML addControlStroke signal connection |
| `src/monitor/monitorproxy.h` | maskOverlayMode + maskProgress Q_PROPERTYs |
| `src/monitor/monitorproxy.cpp` | maskOverlayMode + maskProgress getters/setters |
| `src/effects/effectstack/view/maskmanager.hpp` | addControlStroke, undoMaskAction, redoMaskAction slots |
| `src/effects/effectstack/view/maskmanager.cpp` | Stroke/undo/redo relay, signal connect/disconnect, progress forwarding, cleanup on crash/finish |
| `data/scripts/automask/sam-objectmask.py` | 3 overlay modes, preview-scale, streaming render, rerender command, finish command, fixed edit= bug |

## Triple Agent Audit (post-fix verification)

Three independent audits ran in parallel after all fixes were applied:

1. **Re-verification** — point-by-point checklist of all 7 files: **ALL PASS**
2. **Internal consistency** — signal chains, undo/redo symmetry, data flow: **CLEAN** (our code)
3. **Kdenlive pattern consistency** — KDE code style, conventions: **CONSISTENT** (3 minor LOW deviations)

### Pre-existing bugs discovered and fixed

| ID | Issue | Severity | Fix |
|----|-------|----------|-----|
| PRE-1 | Box serialization: save uses `right()/bottom()`, load used `QRect(x,y,w,h)` constructor — round-trip corrupted box coordinates | HIGH | Changed to `QRect(QPoint, QPoint)` corner constructor. Round-trip verified. |
| PRE-2 | `refreshQmlOverlay()` builds keyframe list from points only, not boxes — box-only frames missing from ruler | MEDIUM | Added m_boxes iteration with dedup to keyframe list building |
| PRE-3 | `cleanup()`, `abortJob()`, `terminate()` did not stop `m_previewDebounce` timer | MEDIUM | Added `m_previewDebounce.stop()` to all 3 exit paths |

## Architecture Summary

### Signal Chains

**Brush stroke**: QML Canvas drag → `addControlStroke(var,bool)` signal → `Monitor::addControlStroke()` (15px sampling) → `Monitor::addMonitorControlStroke` signal → `MaskManager::addControlStroke()` (zone offset) → `AutomaskHelper::addMonitorControlStroke()` (store points + debounce preview)

**Undo/Redo**: Ctrl+Z in `Monitor::keyPressEvent()` → `Monitor::maskUndoAction()` signal → `MaskManager::undoMaskAction()` → `AutomaskHelper::undoAction()` (pop stack, restore state, refresh QML, regenerate preview)

**Propagation progress**: Python `frame_done N/total` → `AutomaskHelper` stdout handler → `updateProgress(percent)` signal → `MaskManager` handler → `MonitorProxy::setMaskProgress()` → QML `controller.maskProgress` → progress bar

**Correction re-propagation**: User edits mask → adds corrections → "Generate Mask" → `rerender=` command → Python `reset_state()` + re-add all keyframes + `propagate_in_video()` → streaming output → `mask ok` → `finish` → process exits
