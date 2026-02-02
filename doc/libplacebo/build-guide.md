# Build Guide

How to build the libplacebo MLT module and integrate it with Kdenlive.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Build Steps](#build-steps)
- [Verification](#verification)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### Required Software

| Software | Version | Purpose |
|----------|---------|---------|
| [KDE Craft](https://community.kde.org/Craft) | Latest | Build system for KDE applications on Windows |
| CMake | 3.16+ | Build configuration (bundled with Craft) |
| MSVC / Visual Studio | 2019+ | C compiler (bundled with Craft on Windows) |
| pkg-config | Any | Dependency resolution for libplacebo |

### Required Libraries

| Library | Version | How to Install |
|---------|---------|----------------|
| libplacebo | 6.x+ (tested with v7.349.0) | `craft libplacebo` |
| MLT Framework | 7.x+ | Included in Kdenlive Craft build |
| Vulkan Headers | Any | Included in libplacebo source tree (see note below) |
| shell32 + ole32 | Windows SDK | Ships with Visual Studio / Windows SDK |

**Note on Vulkan backend**: The Craft-built libplacebo v7.349.0 has `PL_HAVE_VULKAN=1` but `PL_HAVE_D3D11` is undefined. This means the module uses **Vulkan as the GPU backend** on Windows, not D3D11. Vulkan backend is fully functional with dynamic runtime loading of `vulkan-1.dll` and confirmed working on NVIDIA RTX 4090. Vulkan headers are required at compile time. Copy them from libplacebo's source tree:

```powershell
# Copy Vulkan headers from libplacebo's 3rdparty submodule
copy -Recurse C:\CraftRoot\download\git\libs\libplacebo\3rdparty\Vulkan-Headers\include\vulkan C:\CraftRoot\include\vulkan
```

No Vulkan SDK installation is required at runtime — the module dynamically loads `vulkan-1.dll` from the NVIDIA driver.

### Optional Libraries (Future)

| Library | Condition | Purpose |
|---------|-----------|---------|
| D3D11 SDK | `PL_HAVE_D3D11` | D3D11 backend (requires rebuilding libplacebo with D3D11 enabled) |

D3D11 support is currently disabled in the Craft libplacebo build. To enable it, libplacebo must be rebuilt with D3D11 enabled. Once available, the module will prefer D3D11 over Vulkan on Windows for better driver compatibility.

## Build Steps

### Step 1: Install libplacebo

```powershell
craft libplacebo
```

This installs libplacebo headers and libraries into the Craft prefix (`C:\CraftRoot`). Verify installation:

```powershell
ls C:\CraftRoot\lib\pkgconfig\libplacebo.pc
```

### Step 1a: Install Vulkan Headers

The current libplacebo build (v7.349.0) uses Vulkan as the backend on Windows. Copy Vulkan headers from libplacebo's source tree:

```powershell
# Ensure the libplacebo source has the 3rdparty submodule initialized
cd C:\CraftRoot\download\git\libs\libplacebo
git submodule update --init 3rdparty/Vulkan-Headers

# Copy headers to Craft include directory
copy -Recurse .\3rdparty\Vulkan-Headers\include\vulkan C:\CraftRoot\include\vulkan
```

Verify the headers are in place:

```powershell
ls C:\CraftRoot\include\vulkan\vulkan.h
```

### Step 2: Add the Placebo Module to MLT

The module source must be placed at:

```
C:\CraftRoot\download\git\libs\mlt\src\modules\placebo\
```

This directory should contain:

```
CMakeLists.txt
factory.c
gpu_context.h
gpu_context.c
filter_placebo_render.c
filter_placebo_shader.c
filter_placebo_render.yml
filter_placebo_shader.yml
```

### Step 3: Enable the Module in MLT's Build

Edit MLT's top-level module list to include the `placebo` module. In `mlt/src/modules/CMakeLists.txt`, add:

```cmake
if(MOD_PLACEBO)
  add_subdirectory(placebo)
endif()
```

Then configure the MLT build with:

```powershell
craft --configure mlt -- -DMOD_PLACEBO=ON
```

Or, if rebuilding MLT from scratch:

```powershell
craft mlt
```

Make sure the CMake output shows:

```
-- Found libplacebo
-- Building module: placebo
```

### Step 4: Build MLT

```powershell
craft --compile mlt
```

The compiled module will be at:

```
C:\_\<hash>\build\lib\mlt-7\mltplacebo.dll
```

### Step 5: Deploy MLT Module

Copy the compiled module to the Craft runtime directory:

```powershell
copy C:\_\<hash>\build\lib\mlt-7\mltplacebo.dll C:\CraftRoot\lib\mlt-7\mltplacebo.dll
```

Also copy the YAML metadata files:

```powershell
mkdir C:\CraftRoot\share\mlt-7\placebo
copy C:\CraftRoot\download\git\libs\mlt\src\modules\placebo\filter_placebo_render.yml C:\CraftRoot\share\mlt-7\placebo\
copy C:\CraftRoot\download\git\libs\mlt\src\modules\placebo\filter_placebo_shader.yml C:\CraftRoot\share\mlt-7\placebo\
```

### Step 6: Install Kdenlive Effect XMLs

Copy the effect XML files to Kdenlive's data directory:

```powershell
mkdir C:\CraftRoot\bin\data\kdenlive\effects\placebo
copy fork\kdenlive\data\effects\placebo\placebo_render.xml C:\CraftRoot\bin\data\kdenlive\effects\placebo\
copy fork\kdenlive\data\effects\placebo\placebo_shader.xml C:\CraftRoot\bin\data\kdenlive\effects\placebo\
```

Alternatively, rebuild Kdenlive to deploy them automatically:

```powershell
craft --compile kdenlive
copy C:\_\3377f5a\build\bin\kdenlive.exe C:\CraftRoot\bin\kdenlive.exe
```

### Step 7: Restart Kdenlive

Close Kdenlive entirely and relaunch:

```powershell
C:\CraftRoot\bin\kdenlive.exe
```

## Verification

### Verify Module Loading

Run MLT's module listing tool:

```powershell
C:\CraftRoot\bin\melt.exe -query filters
```

Look for:

```
  - placebo.render
  - placebo.shader
```

### Verify GPU Initialization

Apply the GPU Render effect to any clip. Check the console output (stderr) for:

```
[placebo] GPU initialized via Vulkan
```

or (if using a future D3D11-enabled libplacebo build):

```
[placebo] GPU initialized via D3D11
```

or (if both D3D11 and Vulkan fail):

```
[placebo] GPU initialized via OpenGL
```

**Note**: The current Craft libplacebo v7.349.0 has `PL_HAVE_D3D11` undefined, so the module uses Vulkan on Windows. Vulkan backend is confirmed working with NVIDIA RTX 4090 via dynamic loading of `vulkan-1.dll`.

### Verify Shader Cache

After the first use, check that the shader cache was created:

```powershell
ls $env:APPDATA\kdenlive\shader_cache.bin
```

### Verify Effect UI in Kdenlive

1. Open Kdenlive
2. Search for "GPU Render" in the Effects panel
3. The effect should appear with "libplacebo" in the description
4. Dragging it onto a clip should show parameter controls: Quality Preset, Upscaler, Downscaler, Debanding, Dithering, Tone Mapping

## Troubleshooting

### pkg-config cannot find libplacebo

**Symptom**: CMake error `pkg_check_modules(libplacebo REQUIRED IMPORTED_TARGET libplacebo)` fails.

**Solution**: Ensure `PKG_CONFIG_PATH` includes the Craft prefix:

```powershell
$env:PKG_CONFIG_PATH = "C:\CraftRoot\lib\pkgconfig"
```

Or verify the `.pc` file exists:

```powershell
ls C:\CraftRoot\lib\pkgconfig\libplacebo.pc
```

If missing, reinstall libplacebo: `craft libplacebo`.

### Missing libplacebo headers

**Symptom**: Compiler errors like `fatal error: libplacebo/gpu.h: No such file or directory`.

**Solution**: Check that headers are installed:

```powershell
ls C:\CraftRoot\include\libplacebo\gpu.h
```

If missing, run `craft libplacebo` to rebuild and install.

### Missing Vulkan headers

**Symptom**: Compiler errors like `fatal error: vulkan/vulkan.h: No such file or directory`.

**Solution**: The current libplacebo build (v7.349.0) uses Vulkan as the backend. Copy Vulkan headers from libplacebo's 3rdparty submodule:

```powershell
cd C:\CraftRoot\download\git\libs\libplacebo
git submodule update --init 3rdparty/Vulkan-Headers
copy -Recurse .\3rdparty\Vulkan-Headers\include\vulkan C:\CraftRoot\include\vulkan
```

### SHGetFolderPathA linker errors

**Symptom**: `LNK2019: unresolved external symbol SHGetFolderPathA`.

**Solution**: The shader cache code uses Windows shell functions. Ensure `shell32` and `ole32` are linked:

```cmake
if(WIN32)
  target_link_libraries(mltplacebo PRIVATE shell32 ole32)
endif()
```

This is already in the `CMakeLists.txt`, but if you're using a custom build, verify these libraries are linked.

### D3D11 support (future)

**Current status**: The Craft libplacebo v7.349.0 has `PL_HAVE_D3D11` undefined, so D3D11 backend is not available. The module uses Vulkan on Windows.

**Future improvement**: To enable D3D11 support, libplacebo must be rebuilt with D3D11 enabled. Once available, the module will automatically prefer D3D11 over Vulkan for better driver compatibility on Windows.

### PThreads4W not found (MSVC)

**Symptom**: Linker errors related to pthread functions when building with MSVC.

**Solution**: The `CMakeLists.txt` links PThreads4W on MSVC:

```cmake
if(MSVC)
  target_link_libraries(mltplacebo PRIVATE PThreads4W::PThreads4W)
endif()
```

Install PThreads4W via Craft: `craft pthreads4w`.

### Module not appearing in Kdenlive

**Symptom**: The effects "GPU Render" and "GPU Shader" do not appear in Kdenlive's Effects panel.

**Checklist**:

1. Is `mltplacebo.dll` in `C:\CraftRoot\lib\mlt-7\`?
2. Are the `.yml` files in `C:\CraftRoot\share\mlt-7\placebo\`?
3. Are the `.xml` files in the Kdenlive effects directory?
4. Did you restart Kdenlive after copying files?

Run `melt -query filters` to confirm MLT can find the filters independently of Kdenlive.

### GPU initialization fails silently

**Symptom**: Effects apply but frames look identical (no processing).

**Cause**: All backends (D3D11, Vulkan, OpenGL) failed to initialize. The filter falls back to passthrough mode.

**Check**: Run Kdenlive from a terminal and look for:

```
[placebo] No GPU backend available
```

**Common causes**:

- Remote Desktop session (D3D11 often unavailable)
- No discrete GPU (and `allow_software=false` blocks software rasterization)
- Vulkan runtime DLLs not in PATH (see below)
- Driver too old to support the required D3D11 feature level or Vulkan version

**Note on Vulkan runtime**: The Vulkan backend (v1.1.0+) works correctly on NVIDIA RTX 4090 by dynamically loading `vulkan-1.dll` from the driver. However, when running `melt.exe` or Kdenlive outside the Craft environment, ensure `C:\CraftRoot\mingw64\bin` is in PATH for MinGW runtime DLLs. The Craft environment (`craftenv.ps1`) handles this automatically.

### Craft PATH conflicts

**Symptom**: Build fails with unexpected compiler or linker found (e.g., MinGW `gcc` instead of MSVC `cl`).

**Solution**: Craft requires a clean PATH. Use the Craft environment:

```powershell
C:\CraftRoot\craft\craftenv.ps1
```

Or manually strip Git's `sh.exe` and MinGW from PATH before building.

### CMake complains about mismatched source directory

**Symptom**: CMake error about source directory not matching previous configuration.

**Solution**: Delete the build directory and retry:

```powershell
Remove-Item -Recurse -Force C:\_\<hash>\build
craft --compile mlt
```

### Vulkan initialization fails: "No vkGetInstanceProcAddr function provided"

**Symptom**: Runtime error when using placebo effects. Console output shows Vulkan init failing with a message about missing `vkGetInstanceProcAddr`.

**Cause**: The Craft libplacebo v7.349.0 is built with `PL_HAVE_VULKAN=1` but `PL_HAVE_VK_PROC_ADDR=undef`. This means libplacebo was not linked against the Vulkan loader at compile time. The module must dynamically load `vulkan-1.dll` and obtain the `vkGetInstanceProcAddr` function pointer at runtime, then pass it to `pl_vk_inst_create()`.

**Solution**: This is already implemented in `gpu_context.c`. The code uses `LoadLibraryA("vulkan-1.dll")` and `GetProcAddress()` to obtain the function pointer and pass it via the `opt_ptr` parameter. No user action required.

**Technical details**: When libplacebo is built without linking against the Vulkan loader, it expects the caller to provide the proc address function. The module handles this by dynamically loading the Vulkan loader DLL and extracting the required function pointer.

### Vulkan initialization fails at runtime from bare shell

**Symptom**: Running `melt.exe` or Kdenlive from a plain `cmd.exe` or PowerShell session (not the Craft environment) causes Vulkan init to fail silently. The console may show "No GPU backend available" or a DLL load error.

**Cause**: The Vulkan loader (`vulkan-1.dll`) and libplacebo depend on MinGW runtime DLLs. When running from a bare shell, these DLLs are not in `PATH`. The Craft environment (`craftenv.ps1`) sets up `PATH` to include `C:\CraftRoot\mingw64\bin` automatically, but running outside the Craft environment does not.

**Solution**: When running MLT or Kdenlive tools from a bare shell, ensure the MinGW bin directory is in `PATH`:

```powershell
$env:PATH = "C:\CraftRoot\mingw64\bin;$env:PATH"
C:\CraftRoot\bin\melt.exe -query filters
```

Alternatively, always use the Craft environment:

```powershell
C:\CraftRoot\craft\craftenv.ps1
melt -query filters
```

**Additional note**: On this system, the NVIDIA Vulkan ICD is registered via the GPU device class registry (`HKLM\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}\XXXX\VulkanDriverName`) rather than the standard Khronos registry key (`HKLM\SOFTWARE\Khronos\Vulkan\Drivers`). This is a driver-specific detail and does not affect module functionality as long as the Vulkan loader can find the ICD.

### D3D11 backend not available

**Symptom**: The module always uses Vulkan, never D3D11, even on Windows systems where D3D11 should be preferred.

**Cause**: The installed libplacebo (Craft v7.349.0) was built with `PL_HAVE_D3D11=undef`. The module's D3D11 backend code is guarded by `#ifdef PL_HAVE_D3D11`, so it is never compiled.

**Solution**: To use D3D11, libplacebo must be rebuilt with D3D11 support enabled. This requires modifying the Meson build configuration for libplacebo in the Craft blueprint. Once libplacebo is rebuilt with D3D11 support, the module will automatically prefer D3D11 over Vulkan on Windows (no code changes required).

**Current status**: The module uses Vulkan as the primary backend on Windows. This works correctly on systems with Vulkan drivers (NVIDIA, AMD, Intel Arc). D3D11 support is planned for better compatibility with older systems or Remote Desktop sessions.

### OpenGL backend fails in headless mode

**Symptom**: When both D3D11 and Vulkan fail, the module falls back to OpenGL but still reports "No GPU backend available".

**Cause**: `pl_opengl_create()` with empty parameters requires a current OpenGL context (window + WGL on Windows, or GLX on Linux). The module does not create a window or OpenGL context, so the OpenGL backend cannot initialize in headless mode (e.g., when running `melt.exe` from a terminal).

**Current behavior**: The module attempts OpenGL as a final fallback, but it will only succeed if the calling application has already set up an OpenGL context. In practice, this means OpenGL works in Kdenlive (which has a Qt OpenGL context) but not in standalone MLT tools like `melt`.

**Workaround**: If Vulkan is unavailable and OpenGL is needed for headless rendering, you must create an off-screen OpenGL context before initializing the GPU. This is not currently implemented in the module.
