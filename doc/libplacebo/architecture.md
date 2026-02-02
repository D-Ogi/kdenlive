# Architecture

Technical architecture of the libplacebo MLT module.

## Table of Contents

- [Module Structure](#module-structure)
- [GPU Context Lifecycle](#gpu-context-lifecycle)
- [Backend Selection](#backend-selection)
- [Shader Cache](#shader-cache)
- [Frame Processing Pipeline](#frame-processing-pipeline)
- [Threading Model](#threading-model)
- [Memory Management](#memory-management)
- [Integration Points](#integration-points)

## Module Structure

The module consists of six source files compiled into a single shared library (`mltplacebo.dll` / `mltplacebo.so`):

```
mlt/src/modules/placebo/
  CMakeLists.txt                 Build configuration, dependency resolution
  factory.c                      MLT module entry point, filter registration
  gpu_context.h                  Public API for shared GPU singleton
  gpu_context.c                  GPU lifecycle, shader cache, backend init
  filter_placebo_render.c        placebo.render filter implementation
  filter_placebo_shader.c        placebo.shader filter implementation
  filter_placebo_render.yml      MLT metadata (schema, parameters) for render
  filter_placebo_shader.yml      MLT metadata (schema, parameters) for shader
  mltplacebo_export.h            Generated export macros (CMake)
```

Additionally, Kdenlive-side effect definitions live in:

```
kdenlive/data/effects/placebo/
  placebo_render.xml             UI definition: combo boxes, checkboxes, sliders
  placebo_shader.xml             UI definition: file picker for shader path
```

### File Responsibilities

**`factory.c`** -- Module entry point. The `MLT_REPOSITORY` macro registers both filters and their YAML metadata with the MLT service registry. It also declares the `placebo_gpu_release()` extern for module teardown.

**`gpu_context.h` / `gpu_context.c`** -- Manages the singleton GPU context shared by all filter instances. Provides four public functions:

| Function | Purpose |
|----------|---------|
| `placebo_gpu_get()` | Returns the singleton `pl_gpu`, initializing on first call |
| `placebo_dispatch_get()` | Returns the singleton `pl_dispatch` (for shader filter) |
| `placebo_renderer_get()` | Returns the singleton `pl_renderer` (for render filter) |
| `placebo_gpu_release()` | Destroys all GPU resources; called at module unload |

**`filter_placebo_render.c`** -- Implements `placebo.render`. Reads filter properties (preset, scalers, debanding, dithering, tonemapping), configures `pl_render_params`, and calls `pl_render_image()`.

**`filter_placebo_shader.c`** -- Implements `placebo.shader`. Manages a `shader_private` struct per filter instance that tracks the loaded `.hook` shader, its file path, and modification time for hot-reload.

## GPU Context Lifecycle

The GPU context follows a lazy-initialized singleton pattern. No GPU resources are allocated until the first filter instance actually processes a frame.

### Initialization Sequence

```
First call to placebo_gpu_get()
  |
  +-- LOCK mutex
  |
  +-- Check s_initialized flag
  |     (if already true, return s_gpu immediately)
  |
  +-- Set s_initialized = 1
  |
  +-- init_gpu()
  |     |
  |     +-- Create pl_log (log level: PL_LOG_WARN)
  |     |
  |     +-- Create pl_cache (shader cache)
  |     |
  |     +-- Try D3D11 backend (if PL_HAVE_D3D11)
  |     |     +-- pl_d3d11_create(allow_software=false)
  |     |     +-- On success: s_gpu = s_d3d11->gpu, goto done
  |     |     [Note: UNDEF in current v7.349.0 build, skipped]
  |     |
  |     +-- Try Vulkan backend (if PL_HAVE_VULKAN)
  |     |     +-- LoadLibraryA("vulkan-1.dll")
  |     |     +-- GetProcAddress(vkGetInstanceProcAddr)
  |     |     +-- pl_vk_inst_create(get_proc_addr=vkGetInstanceProcAddr, debug=false)
  |     |     +-- pl_vulkan_create(opt_ptr=vkGetInstanceProcAddr)
  |     |     +-- On success: s_gpu = s_vulkan->gpu, goto done
  |     |     [Note: This is the active backend in current build - WORKING]
  |     |
  |     +-- Try OpenGL backend (if PL_HAVE_OPENGL)
  |     |     +-- pl_opengl_create()
  |     |     +-- On success: s_gpu = s_opengl->gpu, goto done
  |     |
  |     +-- On failure: return 0 (no backend available)
  |     |
  |     done:
  |     +-- pl_gpu_set_cache(s_gpu, s_cache)
  |     +-- load_cache() -- read shader_cache.bin from disk
  |
  +-- UNLOCK mutex
  |
  +-- Return s_gpu
```

The `pl_dispatch` and `pl_renderer` singletons are created on demand by their respective getter functions, also under mutex protection.

### Teardown Sequence

`placebo_gpu_release()` is called when the MLT module is unloaded:

```
placebo_gpu_release()
  |
  +-- LOCK mutex
  |
  +-- save_cache() -- write shader_cache.bin to disk
  |
  +-- pl_renderer_destroy()
  +-- pl_dispatch_destroy()
  +-- pl_cache_destroy()
  +-- pl_d3d11_destroy()  (Windows)
  +-- pl_vulkan_destroy()  (if Vulkan was used)
  +-- pl_log_destroy()
  |
  +-- Reset all static pointers to NULL
  +-- s_initialized = 0
  |
  +-- UNLOCK mutex
```

## Backend Selection

The module uses a cascading backend selection strategy:

| Priority | Backend | Condition | Notes |
|----------|---------|-----------|-------|
| 1 | D3D11 | `PL_HAVE_D3D11` defined | Hardware-only (`allow_software=false`). Preferred on Windows for driver stability and broad GPU support. |
| 2 | Vulkan | `PL_HAVE_VULKAN` defined | Cross-platform. Dynamic loader (`vulkan-1.dll`) on Windows. Confirmed working with NVIDIA RTX 4090. |
| 3 | OpenGL | `PL_HAVE_OPENGL` defined | Final fallback when D3D11 and Vulkan unavailable. |
| -- | None | All fail | Filter becomes a passthrough (frames unmodified). |

**Current build status**: The Craft-built libplacebo v7.349.0 has `PL_HAVE_D3D11` undefined and `PL_HAVE_VULKAN=1`. This means the module **uses Vulkan as the GPU backend on Windows**. Vulkan backend is fully functional with dynamic runtime loading of `vulkan-1.dll`.

### Why Vulkan on this build?

The original plan was to use D3D11 as the primary backend on Windows, but the current libplacebo build does not have D3D11 support enabled. The module code is ready for D3D11 (with proper `#ifdef PL_HAVE_D3D11` guards), but it requires rebuilding libplacebo with D3D11 support.

### Backend initialization logic

The code in `gpu_context.c` tries backends in order:

1. **D3D11** (if `PL_HAVE_D3D11` is defined): Calls `pl_d3d11_create(allow_software=false)`. This ensures hardware GPU acceleration. If initialization fails (no hardware GPU, remote desktop session), the module falls back to Vulkan.

2. **Vulkan** (if `PL_HAVE_VULKAN` is defined): Dynamically loads `vulkan-1.dll` via `LoadLibraryA` on Windows, retrieves `vkGetInstanceProcAddr` via `GetProcAddress`, and passes it to `pl_vk_inst_create()` and `pl_vulkan_create()`. Validation layers are disabled (`.debug = false`) to prevent hangs on systems without Vulkan SDK. Confirmed working with NVIDIA RTX 4090.

3. **OpenGL** (if `PL_HAVE_OPENGL` is defined): Calls `pl_opengl_create()` as a final fallback.

4. **None**: If all backends fail, an error is logged and all filter instances pass frames through unmodified -- no crash, no visual corruption, just no GPU processing.

### Future improvement

To enable D3D11 support, libplacebo must be rebuilt with D3D11 enabled. Once available, the module will automatically prefer D3D11 over Vulkan on Windows because:

- It requires no user-installed runtime (D3D11 ships with Windows 7+)
- Driver support is effectively universal on Windows
- D3D11 interop avoids Vulkan's more complex instance/device setup

## Shader Cache

Compiled GPU shader programs are cached to disk to avoid recompilation on subsequent runs.

### Cache Location

| Platform | Path |
|----------|------|
| Windows | `%APPDATA%\kdenlive\shader_cache.bin` |
| Linux | `~/.local/share/kdenlive/shader_cache.bin` |

### Cache Lifecycle

1. **Load**: Called once during `init_gpu()`, after the GPU and `pl_cache` are created. The file is read entirely into memory and passed to `pl_cache_load()`.

2. **Use**: The cache is attached to the GPU via `pl_gpu_set_cache()`. libplacebo transparently reads from and writes to this cache during shader compilation.

3. **Save**: Called during `placebo_gpu_release()`. `pl_cache_save()` is called twice -- first with `NULL` to query the required buffer size, then with an allocated buffer to retrieve the data. The buffer is written to disk.

The directory is created if it does not exist (Windows: `CreateDirectoryA`; Linux: assumed to exist).

## Frame Processing Pipeline

Both filters share the same fundamental pipeline. The difference is in how they configure `pl_render_params`.

### Step-by-Step Pipeline

```
MLT calls filter_get_image()
  |
  (1) Acquire GPU + renderer/dispatch singletons
  |   - If unavailable, fall back to passthrough
  |
  (2) Request RGBA frame from upstream
  |   - *format = mlt_image_rgba
  |   - mlt_frame_get_image(frame, image, format, width, height, writable=1)
  |
  (3) Create source texture (pl_tex_create)
  |   - Format: rgba8
  |   - Flags: sampleable=true, host_writable=true
  |
  (4) Upload CPU -> GPU (pl_tex_upload)
  |   - row_pitch = width * 4
  |   - ptr = *image (the RGBA buffer)
  |
  (5) Create destination texture (pl_tex_create)
  |   - Format: rgba8
  |   - Flags: renderable=true, host_readable=true
  |
  (6) Build pl_frame structs for src and dst
  |   - 1 plane, 4 components (RGBA), component_mapping = {0,1,2,3}
  |   - Color repr: pl_color_repr_rgb
  |   - Color space: pl_color_space_srgb
  |
  (7) Configure pl_render_params
  |   [placebo.render]: preset + scalers + deband + dither + tonemap
  |   [placebo.shader]: default params + hooks array
  |
  (8) Render: pl_render_image(renderer, &src, &dst, &params)
  |
  (9) Download GPU -> CPU (pl_tex_download)
  |   - Writes back to *image (same buffer)
  |
  (10) Destroy textures (pl_tex_destroy for src and dst)
  |
  Return 0 (success)
```

### placebo.render Configuration (Step 7)

The render filter reads these properties from the MLT filter and maps them to libplacebo structs:

```c
// Preset selection
"fast"         -> pl_render_fast_params
"default"      -> pl_render_default_params
"high_quality" -> pl_render_high_quality_params

// Scaler lookup (applied to both upscaler and downscaler)
"bilinear"    -> pl_filter_bilinear
"catmull_rom" -> pl_filter_catmull_rom
"mitchell"    -> pl_filter_mitchell
"lanczos"     -> pl_filter_lanczos
"ewa_lanczos" -> pl_filter_ewa_lanczos
"spline36"    -> pl_filter_spline36

// Debanding
deband=1      -> params.deband_params = &deband_params (with configurable iterations)
deband=0      -> params.deband_params = NULL

// Dithering
"blue"        -> PL_DITHER_BLUE_NOISE
"ordered_lut" -> PL_DITHER_ORDERED_LUT
"white"       -> PL_DITHER_WHITE_NOISE
"none"        -> params.dither_params = NULL

// Tonemapping
"auto"        -> pl_tone_map_auto
"clip"        -> pl_tone_map_clip
"mobius"      -> pl_tone_map_mobius
"reinhard"    -> pl_tone_map_reinhard
"hable"       -> pl_tone_map_hable
"bt2390"      -> pl_tone_map_bt2390
"spline"      -> pl_tone_map_spline
```

### placebo.shader Configuration (Step 7)

The shader filter manages a `shader_private` struct stored as MLT property data (`_shader_priv`). Before rendering each frame, `ensure_shader()` checks:

1. If `shader_path` is set, compare the current file's mtime against the stored mtime. If changed (or first load), re-read and re-parse via `pl_mpv_user_shader_parse()`.
2. If `shader_text` is set (inline shader), compare against stored text. If different, re-parse.
3. If neither is configured, return passthrough.

The parsed hook is stored in `priv->hooks[1]` and passed to `pl_render_params.hooks`.

## Threading Model

MLT uses worker threads for frame processing. Multiple frames may be processed concurrently by different filter instances.

### Synchronization Strategy

The GPU context uses a **coarse-grained mutex** protecting all singleton access:

| Platform | Mechanism |
|----------|-----------|
| Windows | `CRITICAL_SECTION` (initialized lazily via `ensure_mutex()`) |
| POSIX | `pthread_mutex_t` (statically initialized with `PTHREAD_MUTEX_INITIALIZER`) |

The mutex is held for:

- Checking/setting `s_initialized` in `placebo_gpu_get()`
- Creating `s_dispatch` in `placebo_dispatch_get()`
- Creating `s_renderer` in `placebo_renderer_get()`
- The entire teardown sequence in `placebo_gpu_release()`

The mutex is **not** held during the actual frame processing (`pl_render_image`, `pl_tex_upload`, `pl_tex_download`). This is safe because:

- Each filter invocation creates its own `pl_tex` objects (no sharing)
- `pl_renderer` and `pl_dispatch` are thread-safe in libplacebo (they use internal synchronization)
- The singletons are read-only after initialization (no writes to `s_gpu`, `s_renderer`, etc. during processing)

### Thread Safety Considerations

- `shader_private` data is per-filter-instance (stored on `MLT_FILTER_PROPERTIES`), so there are no cross-instance races on shader state.
- The `loaded_mtime` check in `ensure_shader()` is not atomic, but this is acceptable: a missed mtime update simply delays hot-reload by one frame.

## Memory Management

### GPU Textures

Textures are **created and destroyed per frame** in both filters:

```c
pl_tex src_tex = pl_tex_create(gpu, ...);  // Allocate on GPU
pl_tex dst_tex = pl_tex_create(gpu, ...);
// ... process ...
pl_tex_destroy(gpu, &src_tex);             // Free GPU memory
pl_tex_destroy(gpu, &dst_tex);
```

This approach trades some overhead (allocation/deallocation per frame) for simplicity and safety. There is no texture pooling or caching at the filter level -- libplacebo's internal allocator handles efficient GPU memory reuse.

### CPU Frame Buffer

The filter writes the processed result **back into the same buffer** provided by MLT (`*image`). No additional CPU-side allocation is needed for the output. The `writable=1` flag in `mlt_frame_get_image()` ensures the buffer is writable.

### Shader Private Data

The `shader_private` struct in `placebo.shader` is allocated with `calloc` and registered as MLT property data with a destructor. Cleanup happens via:

1. `filter_close()` -- called when the filter is destroyed, frees the hook via `pl_mpv_user_shader_destroy()` and releases path/text strings.
2. MLT's property destructor -- frees the `shader_private` struct itself.

### Shader Cache Memory

The shader cache (`pl_cache`) holds compiled shader binaries in memory. It is bounded by libplacebo's internal limits. The cache is serialized to disk on teardown and deserialized on startup.

## Integration Points

### MLT Framework

The module integrates with MLT through:

1. **Module registration** (`factory.c`): `MLT_REGISTER` and `MLT_REGISTER_METADATA` macros in the `MLT_REPOSITORY` block. MLT discovers the module at runtime by loading `mltplacebo.dll`.

2. **Filter lifecycle**: Each filter implements `filter_process()` (which pushes a callback onto the frame stack) and `filter_get_image()` (which does the actual work when the frame is consumed).

3. **YAML metadata**: `.yml` files declare the filter schema (parameters, types, defaults, descriptions) that MLT uses for service introspection.

4. **Property system**: Filter parameters are read via `mlt_properties_get()` and `mlt_properties_get_int()` from `MLT_FILTER_PROPERTIES(filter)`.

### libplacebo API

Key libplacebo types and functions used:

| Type/Function | Used In | Purpose |
|---------------|---------|---------|
| `pl_gpu` | All | Abstract GPU handle |
| `pl_log` | gpu_context.c | Logging (PL_LOG_WARN level) |
| `pl_d3d11` | gpu_context.c | D3D11 backend wrapper |
| `pl_vulkan` | gpu_context.c | Vulkan backend wrapper |
| `pl_cache` | gpu_context.c | Shader cache persistence |
| `pl_renderer` | render, shader | High-level rendering pipeline |
| `pl_dispatch` | gpu_context.c | Low-level shader dispatch |
| `pl_tex_create/destroy` | render, shader | GPU texture management |
| `pl_tex_upload/download` | render, shader | CPU-GPU data transfer |
| `pl_render_image()` | render, shader | Execute the render pipeline |
| `pl_mpv_user_shader_parse()` | shader | Parse mpv `.hook` shader format |
| `pl_render_params` | render, shader | Configuration struct for rendering |
| `pl_deband_params` | render | Debanding configuration |
| `pl_dither_params` | render | Dithering configuration |
| `pl_color_map_params` | render | Tonemapping configuration |
| `pl_filter_*` | render | Scaling algorithm definitions |
| `pl_tone_map_*` | render | Tonemapping function definitions |

### Kdenlive Effect System

Kdenlive effect XML files (`placebo_render.xml`, `placebo_shader.xml`) define:

- The MLT filter tag to use (`tag="placebo.render"`)
- UI widgets for each parameter (list/combo, bool/checkbox, constant/slider, url/fileopen)
- Default values and valid option lists
- Display names for UI labels

When a user applies the effect in Kdenlive, the XML drives the Effect Stack UI, and parameter changes are written to MLT filter properties which the C code reads during `filter_get_image()`.
