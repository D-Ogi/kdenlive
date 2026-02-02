# Filter Reference

Complete parameter reference for both libplacebo filters.

## Table of Contents

- [placebo.render -- GPU Render](#placeborender----gpu-render)
  - [Parameters](#render-parameters)
  - [Quality Presets](#quality-presets)
  - [Scaling Algorithms](#scaling-algorithms)
  - [Debanding](#debanding)
  - [Dithering](#dithering)
  - [Tone Mapping](#tone-mapping)
- [placebo.shader -- GPU Shader](#placeboshader----gpu-shader)
  - [Parameters](#shader-parameters)
  - [Hot-Reload Behavior](#hot-reload-behavior)
  - [Finding and Installing Shaders](#finding-and-installing-shaders)

---

## placebo.render -- GPU Render

**MLT identifier**: `placebo.render`
**Kdenlive name**: GPU Render (libplacebo)
**Source**: `filter_placebo_render.c`
**Metadata**: `filter_placebo_render.yml`
**Effect XML**: `placebo_render.xml`

A batteries-included GPU rendering filter that processes every frame through libplacebo's `pl_renderer`. Provides configurable scaling, debanding, dithering, and tonemapping in a single effect.

### Render Parameters

| Parameter | Type | Default | Values | Description |
|-----------|------|---------|--------|-------------|
| `preset` | string | `default` | `fast`, `default`, `high_quality` | Overall quality preset (see below) |
| `upscaler` | string | `ewa_lanczos` | `bilinear`, `catmull_rom`, `mitchell`, `lanczos`, `ewa_lanczos`, `spline36` | Scaling algorithm for upscaling |
| `downscaler` | string | `mitchell` | `bilinear`, `catmull_rom`, `mitchell`, `lanczos`, `ewa_lanczos`, `spline36` | Scaling algorithm for downscaling |
| `deband` | integer | `0` | `0` (off), `1` (on) | Enable debanding |
| `deband_iterations` | integer | `1` | `1` -- `4` | Number of debanding iterations |
| `dithering` | string | `blue` | `blue`, `ordered_lut`, `white`, `none` | Dithering method |
| `tonemapping` | string | `auto` | `auto`, `clip`, `mobius`, `reinhard`, `hable`, `bt2390`, `spline` | HDR-to-SDR tone mapping function |

All parameters are mutable -- they can be changed at any time and take effect on the next frame.

### Quality Presets

Presets map to libplacebo's built-in `pl_render_params` configurations. They set sensible defaults for all internal rendering options (not just the parameters exposed in the UI).

#### fast

Maps to `pl_render_fast_params`.

Minimizes GPU workload. Disables most enhancements. Suitable for real-time preview on low-end GPUs or when the filter chain is complex.

- No debanding
- Simple dithering
- Basic scaling
- Minimal color management overhead

#### default

Maps to `pl_render_default_params`.

Balanced quality and performance. A reasonable starting point for most workflows. Enables standard color management and scaling quality.

#### high_quality

Maps to `pl_render_high_quality_params`.

Enables all libplacebo quality enhancements. Uses the highest-quality scaling, dithering, and color processing. Suitable for final renders where GPU cost is not a concern.

- Full debanding (if enabled)
- High-quality dithering
- Anti-aliasing on scalers
- Maximum color accuracy

**Note**: The `upscaler`, `downscaler`, `deband`, `dithering`, and `tonemapping` parameters override the corresponding preset defaults. For example, setting `preset=fast` but `upscaler=ewa_lanczos` uses the fast preset as a base but with EWA Lanczos scaling.

### Scaling Algorithms

Scaling algorithms are used when the source frame resolution differs from the output resolution (e.g., when the project profile differs from the clip's native resolution, or when transform/crop effects change the effective size).

#### bilinear

Linear interpolation between the four nearest pixels. Fastest algorithm with minimal GPU cost. Produces soft, slightly blurry results on upscaling. Adequate for downscaling when speed matters more than quality.

- **Quality**: Low
- **Speed**: Maximum
- **Use case**: Real-time preview, low-end GPUs

#### catmull_rom

Cubic interpolation using the Catmull-Rom spline (equivalent to "Bicubic" in many applications with B=0, C=0.5). Sharper than bilinear with mild ringing on high-contrast edges.

- **Quality**: Medium
- **Speed**: Fast
- **Use case**: General-purpose scaling with decent sharpness

#### mitchell

Mitchell-Netravali filter (B=1/3, C=1/3). A compromise between sharpness and smoothness, specifically designed to minimize ringing artifacts while maintaining good detail. The default downscaler for good reason.

- **Quality**: Medium-High
- **Speed**: Fast
- **Use case**: Downscaling (default), general-purpose

#### lanczos

Windowed sinc function (Lanczos3 by default). Produces sharp results with well-defined edges. Some ringing may be visible around high-contrast transitions.

- **Quality**: High
- **Speed**: Medium
- **Use case**: Upscaling where sharpness matters, non-EWA alternative

#### ewa_lanczos

Elliptical Weighted Average (EWA) Lanczos filter, also known as Jinc-windowed Jinc. The highest-quality general-purpose scaler in libplacebo. Handles diagonal edges and fine detail better than non-EWA alternatives. The default upscaler.

- **Quality**: Highest
- **Speed**: Slowest
- **Use case**: Final renders, maximum upscaling quality

#### spline36

36-tap spline interpolation. Very sharp with controlled ringing. A popular choice in the video playback community (mpv default for years). Slightly faster than EWA Lanczos with nearly comparable quality.

- **Quality**: High
- **Speed**: Medium
- **Use case**: High-quality scaling with lower GPU cost than EWA Lanczos

### Debanding

Color banding (also called posterization) appears as visible steps between color gradients, most noticeably in dark scenes, skies, and gradual color transitions. It is caused by limited bit depth (typically 8-bit per channel) and lossy compression.

Debanding adds a controlled amount of noise to smooth these transitions, making them perceptually invisible.

#### How It Works

libplacebo's debanding algorithm (`pl_shader_deband`) operates in multiple iterations:

1. For each pixel, sample a random neighbor within a configurable radius
2. If the difference between the pixel and its neighbor is below a threshold, blend them
3. Repeat for the configured number of iterations (each iteration uses a smaller radius)
4. Apply a final grain pass to mask remaining artifacts

#### Iteration Count Tradeoffs

| Iterations | Effect | Performance |
|------------|--------|-------------|
| 1 | Light debanding. Sufficient for mild banding in well-encoded content. | Fastest |
| 2 | Moderate debanding. Good balance for most compressed video. | Low overhead |
| 3 | Aggressive debanding. Effective on heavily compressed or 8-bit content. | Moderate overhead |
| 4 | Maximum debanding. May introduce visible softening in textured areas. | Highest overhead |

For most content, 1-2 iterations are sufficient. Increase to 3-4 only for severely banded material (e.g., heavily compressed streaming content, 8-bit HDR converted to SDR).

### Dithering

Dithering adds imperceptible noise patterns to reduce quantization error when converting between bit depths or color spaces. It is the final step in the rendering pipeline, applied after all other processing.

#### blue

**Blue noise dithering** (`PL_DITHER_BLUE_NOISE`). Default.

Uses a precomputed blue noise texture to distribute quantization error. The noise pattern has no visible structure -- it appears as uniform, high-frequency grain. Produces the most visually pleasing results.

- **Quality**: Best
- **Speed**: Moderate (texture lookup)
- **Artifacts**: None visible at normal viewing distances

#### ordered_lut

**Ordered dithering with LUT** (`PL_DITHER_ORDERED_LUT`).

Uses a Bayer matrix or similar ordered pattern stored as a lookup table. Slightly faster than blue noise because the pattern is deterministic. May produce faint grid-like patterns on very close inspection.

- **Quality**: Good
- **Speed**: Fastest
- **Artifacts**: Faint grid pattern visible under magnification

#### white

**White noise dithering** (`PL_DITHER_WHITE_NOISE`).

Uses randomly generated noise per pixel per frame. Introduces temporal flickering (scintillation) that can be distracting on static content. Not recommended for most workflows.

- **Quality**: Acceptable
- **Speed**: Fast (PRNG per pixel)
- **Artifacts**: Temporal scintillation on static frames

#### none

Disables dithering entirely. Sets `params.dither_params = NULL`.

Not recommended for final output -- quantization banding may become visible, especially after tonemapping or color space conversion. Useful for debugging or when the downstream pipeline handles dithering.

### Tone Mapping

Tone mapping compresses the dynamic range of High Dynamic Range (HDR) content to fit within the Standard Dynamic Range (SDR) display range. Only relevant when processing HDR source material (e.g., HDR10, HLG, Dolby Vision) for SDR output.

For SDR-to-SDR workflows (the common case in Kdenlive), tone mapping has no visible effect -- the color pipeline detects matching source and destination color spaces and skips the mapping.

#### auto

`pl_tone_map_auto`. Default.

Automatically selects the best tone mapping function based on the source and destination characteristics. Delegates to libplacebo's internal heuristics, which typically choose `spline` or `bt2390` depending on the content.

- **Use case**: General purpose, recommended default

#### clip

`pl_tone_map_clip`.

Simply clips values that exceed the destination range. No compression or rolloff. Bright highlights are hard-clipped to white.

- **Use case**: Content that is already within SDR range, or when preserving exact color values below the clip point is critical
- **Drawback**: Harsh highlight clipping, loss of detail in bright areas

#### mobius

`pl_tone_map_mobius`.

Applies a Mobius (bilinear) transformation. Smoothly compresses highlights while leaving shadows and midtones largely unchanged. Controlled by a single "knee" parameter.

- **Use case**: Gentle HDR-to-SDR conversion with natural highlight rolloff
- **Character**: Soft, film-like highlight compression

#### reinhard

`pl_tone_map_reinhard`.

Classic Reinhard global tone mapping operator. Maps the entire luminance range through a simple curve: `L / (1 + L)`. Never clips, but compresses contrast across the entire range.

- **Use case**: Broadly compatible, well-understood behavior
- **Character**: Global contrast reduction; shadows may appear slightly lifted

#### hable

`pl_tone_map_hable`.

John Hable's "Uncharted 2" filmic tone mapping curve. Designed to emulate the S-curve response of photographic film. Preserves shadow detail and rolls off highlights naturally.

- **Use case**: Cinematic content where a filmic look is desired
- **Character**: Strong S-curve; rich shadows, soft highlights

#### bt2390

`pl_tone_map_bt2390` (also accepted as `bt.2390`).

Implements the ITU-R BT.2390-8 EETF (Electrical-Electrical Transfer Function). The broadcast standard for HDR-to-SDR conversion. Uses a spline-based knee function calibrated to perceptual uniformity.

- **Use case**: Broadcast compliance, standards-conformant conversion
- **Character**: Perceptually uniform, industry-standard

#### spline

`pl_tone_map_spline`.

libplacebo's custom spline-based tone mapping. Attempts to produce the most perceptually natural result by optimizing a spline curve for the specific source and destination parameters.

- **Use case**: High-quality HDR-to-SDR when `auto` is not satisfactory
- **Character**: Smooth, optimized for perceptual quality

---

## placebo.shader -- GPU Shader

**MLT identifier**: `placebo.shader`
**Kdenlive name**: GPU Shader (libplacebo)
**Source**: `filter_placebo_shader.c`
**Metadata**: `filter_placebo_shader.yml`
**Effect XML**: `placebo_shader.xml`

Loads and applies a custom mpv/libplacebo `.hook` shader for GPU-accelerated processing. Supports the full range of mpv user shaders, including neural network upscalers, chroma processing, film grain overlays, and custom color correction.

### Shader Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `shader_path` | string | `""` | Absolute path to a `.hook` or `.glsl` shader file |
| `shader_text` | string | `""` | Inline shader source code (alternative to file) |

If both `shader_path` and `shader_text` are set, `shader_path` takes priority.

If neither is set, the filter acts as a passthrough (frames are not uploaded to the GPU).

#### shader_path

The primary way to use this filter. Point it to a `.hook` or `.glsl` file on disk. The shader is loaded, parsed via `pl_mpv_user_shader_parse()`, and applied as a hook in `pl_render_params`.

The Kdenlive effect XML provides a file picker widget (`<parameter type="url">`) for convenience.

Supported file extensions are `.hook` and `.glsl` by convention, but the parser only cares about the file's content format (mpv user shader syntax), not the extension.

#### shader_text

For programmatic use or when embedding shaders directly. Pass the full shader source as a string. Useful when integrating with other tools or scripts that generate shader code dynamically.

Changes to `shader_text` are detected by string comparison and trigger a re-parse.

### Hot-Reload Behavior

The filter monitors the loaded shader file for changes using filesystem modification time (`mtime`). On each frame processed:

1. `ensure_shader()` calls `stat()` on `shader_path` to read the current `mtime`
2. If `mtime` differs from the stored `loaded_mtime`, the file is re-read and re-parsed
3. The old `pl_hook` is destroyed and replaced with the new one
4. `loaded_mtime` is updated

This allows iterative shader development: edit the `.hook` file in a text editor, save, and the result appears in Kdenlive's preview within one frame cycle.

**Limitations**:

- Hot-reload checks `mtime` granularity, which is 1-second resolution on many filesystems. Rapid saves within the same second may not trigger a reload.
- If the re-parsed shader fails (`pl_mpv_user_shader_parse()` returns `NULL`), an error is logged and the filter falls back to passthrough until a valid shader is saved.
- Only one hook shader is supported per filter instance. To chain multiple shaders, stack multiple `placebo.shader` filter instances.

### Finding and Installing Shaders

mpv user shaders are plain text files with a specific format (see the [Shader Guide](shader-guide.md) for the full syntax). Popular sources:

| Source | URL | Description |
|--------|-----|-------------|
| mpv User Shaders Wiki | `https://github.com/mpv-player/mpv/wiki/User-Scripts#user-shaders` | Curated list of community shaders |
| Anime4K | `https://github.com/bloc97/Anime4K` | Real-time anime upscaling |
| FSRCNNX | `https://github.com/igv/FSRCNN-TensorFlow` | Neural network upscaling |
| KrigBilateral | (bundled with mpv shader collections) | Chroma upscaling using bilateral filtering |

To use a shader:

1. Download the `.glsl` or `.hook` file
2. Place it in a convenient location (e.g., `C:\Users\<you>\shaders\`)
3. Apply the "GPU Shader (libplacebo)" effect to a clip
4. Browse to the shader file using the file picker

No compilation or installation step is required -- shaders are parsed and compiled to GPU code at runtime by libplacebo.
