# libplacebo MLT Module for Kdenlive

GPU-accelerated video processing for Kdenlive, powered by [libplacebo](https://code.videolan.org/videolan/libplacebo).

## Overview

The `placebo` MLT module brings libplacebo's GPU rendering pipeline into Kdenlive as native video effects. It provides two filters:

| Filter | Purpose |
|--------|---------|
| `placebo.render` | Batteries-included GPU renderer: scaling, debanding, dithering, tonemapping |
| `placebo.shader` | Custom `.hook` shader loader for community shaders (Anime4K, FSRCNNX, film grain, etc.) |

Both filters offload processing to the GPU via D3D11 (Windows preferred), Vulkan (Windows/Linux, confirmed working on NVIDIA RTX 4090), or OpenGL (fallback), with automatic backend selection and a persistent shader cache.

## Architecture

```
+------------------+     +-------------------+     +--------------------------+
|   Kdenlive UI    |     |    MLT Framework   |     |    libplacebo            |
|                  |     |                    |     |                          |
|  Effect XML      +---->+  mlt_filter        +---->+  pl_renderer / pl_dispatch|
|  (parameters)    |     |  (get_image)       |     |  (GPU shaders)           |
+------------------+     +--------+-----------+     +------------+-------------+
                                  |                              |
                                  |  CPU frame (RGBA)            |  GPU textures
                                  |                              |
                         +--------v-----------+     +------------v-------------+
                         |  pl_tex_upload()    +---->+  GPU Processing          |
                         |                    |     |  (D3D11 or Vulkan)       |
                         |  pl_tex_download() <-----+                          |
                         +--------------------+     +--------------------------+
```

### Data Flow

1. Kdenlive effect parameters are set via XML-defined UI controls
2. MLT calls `filter_get_image()` during timeline playback or rendering
3. The filter requests an RGBA frame from the upstream producer
4. CPU frame data is uploaded to a GPU texture (`pl_tex_upload`)
5. libplacebo processes the frame on the GPU (scaling, debanding, shaders, etc.)
6. The result is downloaded back to CPU memory (`pl_tex_download`)
7. MLT continues the filter chain with the processed frame

## Quick Start

### Using GPU Render (placebo.render)

1. Open Kdenlive with the patched build (see [Build Guide](build-guide.md))
2. In the Effects panel, search for **GPU Render (libplacebo)**
3. Drag it onto a clip on the timeline
4. Configure parameters in the Effect Stack:
   - **Quality Preset**: Fast / Default / High Quality
   - **Upscaler**: Algorithm for upscaling (default: EWA Lanczos)
   - **Downscaler**: Algorithm for downscaling (default: Mitchell)
   - **Debanding**: Toggle on to reduce color banding artifacts
   - **Dithering**: Blue Noise (best quality) / Ordered LUT (fastest) / None
   - **Tone Mapping**: Auto / Clip / Mobius / Reinhard / Hable / BT.2390 / Spline

### Using GPU Shader (placebo.shader)

1. Search for **GPU Shader (libplacebo)** in the Effects panel
2. Drag it onto a clip
3. Click the file picker and select a `.hook` or `.glsl` shader file
4. The shader is applied in real-time; editing the file on disk triggers automatic hot-reload

### Verifying GPU Initialization

Check the MLT log output (stderr) for one of these messages on first use:

```
[placebo] GPU initialized via D3D11
[placebo] GPU initialized via Vulkan
[placebo] GPU initialized via OpenGL
```

The current Craft build uses Vulkan on Windows (D3D11 support requires rebuilding libplacebo). Vulkan backend is confirmed working with NVIDIA RTX 4090.

If you see `[placebo] No GPU backend available`, the module will pass frames through unmodified. See [Build Guide](build-guide.md) for troubleshooting.

## Documentation

| Document | Description |
|----------|-------------|
| [Architecture](architecture.md) | Module structure, GPU lifecycle, threading model, memory management |
| [Build Guide](build-guide.md) | Prerequisites, step-by-step build, troubleshooting |
| [Filter Reference](filters.md) | Complete parameter reference for both filters |
| [Shader Guide](shader-guide.md) | Writing and using custom `.hook` shaders |
| [Changelog](changelog.md) | Version history |

## Source Locations

| Component | Path |
|-----------|------|
| MLT module source | `mlt/src/modules/placebo/` |
| Kdenlive effect XMLs | `kdenlive/data/effects/placebo/` |
| Shader cache (Windows) | `%APPDATA%\kdenlive\shader_cache.bin` |
| Shader cache (Linux) | `~/.local/share/kdenlive/shader_cache.bin` |

## License

LGPL v2.1 or later. See individual source files for copyright headers.
