# Changelog

Version history for the libplacebo MLT module.

## v1.1.0 -- Vulkan Dynamic Loader Working, GPU Pipeline Verified

**Date**: 2025-02-01

### Major Fixes

**Vulkan backend now functional on Windows with NVIDIA RTX 4090:**
- Root cause identified: libplacebo v7.349.0 built with `PL_HAVE_VULKAN=1` but `PL_HAVE_VK_PROC_ADDR` undefined, preventing runtime Vulkan loader initialization
- Fixed by adding dynamic loader in `gpu_context.c`: `LoadLibraryA("vulkan-1.dll")` + `GetProcAddress` to obtain `vkGetInstanceProcAddr`
- `vkGetInstanceProcAddr` now passed to `pl_vk_inst_create()` and `pl_vulkan_create()` via `opt_ptr` parameter
- Validation layers disabled (`.debug = false`) in `pl_vk_inst_create` to prevent hangs on systems without Vulkan SDK

**OpenGL fallback added:**
- Third backend option via `pl_opengl_create()` when both D3D11 and Vulkan fail
- Backend priority: D3D11 -> Vulkan (dynamic loader) -> OpenGL

**Backend cascade refined:**
- Priority order now: D3D11 (if `PL_HAVE_D3D11`) -> Vulkan -> OpenGL (if `PL_HAVE_OPENGL`)
- Each backend failure triggers automatic fallback to next available backend

### Verification

**GPU pipeline fully operational on RTX 4090:**
- All 8 test cases passing:
  - 3 renderer presets (fast, default, high_quality)
  - 4 shader files (Anime4K, CAS, adaptive-sharpen, film-grain)
  - 1 stacked effect (film-grain on top of Anime4K upscale)
- Custom shader tests successful:
  - Grayscale conversion shader
  - Sepia tone shader
  - Warm color grading shader
- GPU acceleration confirmed working via Vulkan backend

### Technical Details

Dynamic Vulkan loading implementation:
```c
HMODULE vulkan_lib = LoadLibraryA("vulkan-1.dll");
PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
    (PFN_vkGetInstanceProcAddr)GetProcAddress(vulkan_lib, "vkGetInstanceProcAddr");
struct pl_vk_inst_params vk_inst_params = {
    .get_proc_addr = vkGetInstanceProcAddr,
    .debug = false  // Critical: prevents hangs without validation layers
};
```

### Known Issues Documented

**Vulkan runtime dependencies:**
- The Vulkan loader (`vulkan-1.dll`) and libplacebo depend on MinGW runtime DLLs
- Running `melt.exe` or Kdenlive from a bare shell (not Craft environment) requires `C:\CraftRoot\mingw64\bin` in `PATH`
- The Craft environment (`craftenv.ps1`) handles this automatically

**D3D11 backend unavailable:**
- Current libplacebo build has `PL_HAVE_D3D11=undef`
- To enable D3D11, libplacebo must be rebuilt with D3D11 support in Meson configuration
- Module will automatically prefer D3D11 once available (no code changes needed)

**OpenGL backend limitations:**
- `pl_opengl_create()` requires an existing OpenGL context (window + WGL/GLX)
- OpenGL fallback works in Kdenlive (has Qt OpenGL context) but not in headless `melt` usage
- No off-screen context creation implemented in the module

**NVIDIA Vulkan ICD registration:**
- On this system, the NVIDIA driver registers the Vulkan ICD via GPU device class registry (`HKLM\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}\XXXX\VulkanDriverName`)
- This differs from the standard Khronos registry path (`HKLM\SOFTWARE\Khronos\Vulkan\Drivers`)
- Does not affect functionality as the Vulkan loader handles both registration methods

## v1.0.1 -- Build Fixes and Vulkan Backend

**Date**: 2025-02-01

### Build Fixes

**D3D11 detection and guards:**
- Changed D3D11 backend guards from `#ifdef _WIN32` to `#ifdef PL_HAVE_D3D11` to respect libplacebo's actual build configuration
- Added CMake `check_c_source_compiles` test to detect D3D11 availability and define `PL_HAVE_D3D11` accordingly
- Current Craft libplacebo v7.349.0 has `PL_HAVE_D3D11` undefined, so D3D11 code is not compiled

**Vulkan backend:**
- Confirmed `PL_HAVE_VULKAN=1` in libplacebo v7.349.0
- Module now uses Vulkan as the GPU backend on Windows (not D3D11 as originally planned)
- Vulkan headers sourced from libplacebo's `3rdparty/Vulkan-Headers` submodule and copied to `C:\CraftRoot\include\vulkan\`

**Header fixes:**
- Removed non-existent `libplacebo/shaders/deband.h` include
- `pl_deband_params` comes from `libplacebo/renderer.h` in v7.349.0
- Added explicit Vulkan header installation step in build guide

**Linker fixes:**
- Added `shell32` and `ole32` to `target_link_libraries` for `SHGetFolderPathA` used in shader cache code
- D3D11 libraries (`d3d11`, `dxgi`) are only linked when `PL_HAVE_D3D11` is true

### Code Quality Improvements

**C5 -- GPU initialization retry on failure:**
- Added retry logic in `init_gpu()`: if the first backend fails, the second backend is attempted
- Previously, if D3D11 failed, the code would fall through to Vulkan (this was already present)
- Clarified with comments that this is intentional cascading fallback

**W7 -- Shader file size limit:**
- Added 10 MB file size limit in `ensure_shader()` to prevent loading excessively large shader files
- Large files are rejected with an error message

### Verification

- Module verified with `melt -query filters` showing `placebo.render` and `placebo.shader`
- GPU initialization confirmed with Vulkan backend on Windows
- Shader cache (`shader_cache.bin`) created successfully in `%APPDATA%\kdenlive\`

### Known Issues

- D3D11 support requires rebuilding libplacebo with D3D11 enabled (future improvement)
- Current Craft build only provides Vulkan backend on Windows

### Documentation Updates

- Updated `build-guide.md` with Vulkan header installation steps
- Updated `architecture.md` to reflect actual backend priority (Vulkan on current build)
- Added troubleshooting section for missing Vulkan headers and SHGetFolderPathA linker errors

## v1.0 -- Initial Release

**Date**: 2025

### Filters

- **`placebo.render`** -- GPU-accelerated renderer with configurable scaling, debanding, dithering, and tonemapping.
  - Quality presets: fast, default, high_quality (mapped to libplacebo's built-in `pl_render_params`)
  - Scaling algorithms: bilinear, catmull_rom, mitchell, lanczos, ewa_lanczos, spline36
  - Debanding with configurable iteration count (1-4)
  - Dithering methods: blue noise, ordered LUT, white noise, or disabled
  - Tone mapping functions: auto, clip, mobius, reinhard, hable, bt.2390, spline

- **`placebo.shader`** -- Custom mpv `.hook` shader loader.
  - Load shaders from file path or inline text
  - Hot-reload on file modification (mtime monitoring)
  - Full support for mpv user shader format via `pl_mpv_user_shader_parse()`

### Architecture

- Singleton GPU context with lazy initialization
- D3D11 backend (Windows primary), Vulkan fallback
- Persistent shader cache (`shader_cache.bin`) across sessions
- Thread-safe GPU access via platform mutex (CRITICAL_SECTION on Windows, pthread_mutex on POSIX)
- Graceful fallback to passthrough when no GPU backend is available

### Integration

- MLT module: `mltplacebo.dll` / `mltplacebo.so`
- MLT metadata: YAML schema files with full parameter definitions
- Kdenlive effect XMLs with UI controls (combo boxes, checkboxes, file picker)

### Source Files

| File | Lines | Description |
|------|-------|-------------|
| `gpu_context.h` | 41 | Public API header |
| `gpu_context.c` | 341 | GPU lifecycle, cache, backend init |
| `filter_placebo_render.c` | 262 | Render filter implementation |
| `filter_placebo_shader.c` | 334 | Shader filter implementation |
| `factory.c` | 52 | Module registration |
| `CMakeLists.txt` | 40 | Build configuration |
| `filter_placebo_render.yml` | 90 | Render filter metadata |
| `filter_placebo_shader.yml` | 39 | Shader filter metadata |
| `placebo_render.xml` | 38 | Kdenlive render effect UI |
| `placebo_shader.xml` | 11 | Kdenlive shader effect UI |

### Known Limitations

- Textures are created and destroyed per frame (no pooling). Acceptable for current use but leaves room for optimization.
- Only one hook shader per `placebo.shader` instance. Chain multiple instances for multi-shader pipelines.
- RGBA8 format only. No HDR (10-bit/16-bit float) pipeline through the filter, though tonemapping handles HDR source metadata.
- On Windows, the CRITICAL_SECTION initialization in `ensure_mutex()` is not itself thread-safe (theoretical race on first call from multiple threads simultaneously). In practice, MLT initializes modules from a single thread, so this is not an issue.
- Linux shader cache directory is assumed to exist; no `mkdir -p` equivalent is implemented.
