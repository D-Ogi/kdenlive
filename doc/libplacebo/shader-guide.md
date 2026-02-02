# Shader Authoring Guide

How to write, use, and find custom `.hook` shaders for the `placebo.shader` filter.

## Table of Contents

- [mpv User Shader Format](#mpv-user-shader-format)
- [Format Specification](#format-specification)
- [Example: Minimal Passthrough Shader](#example-minimal-passthrough-shader)
- [Example: Simple Color Correction](#example-simple-color-correction)
- [Example: Vignette Effect](#example-vignette-effect)
- [Popular Community Shaders](#popular-community-shaders)
- [Where to Find Shaders](#where-to-find-shaders)
- [Performance Considerations](#performance-considerations)

## mpv User Shader Format

The `placebo.shader` filter uses the mpv/libplacebo user shader format, also known as `.hook` shaders. This is the same format used by the mpv media player for GPU-based video processing.

A `.hook` shader is a plain text file containing metadata directives (lines starting with `//!`) followed by GLSL shader code. libplacebo parses these directives to determine when and how to inject the shader into the rendering pipeline.

The shader code runs on the GPU, processing pixels in parallel. Each invocation of the shader's `hook()` function processes a single pixel (or texel) and returns the modified color value.

## Format Specification

A `.hook` shader file has two parts: directives and GLSL code.

### Directives

Directives are comment lines at the top of the file, prefixed with `//!`:

```glsl
//! HOOK <texture_name>
//! BIND <texture_name>
//! DESC <description>
//! WIDTH <expression>
//! HEIGHT <expression>
//! WHEN <condition>
//! OFFSET <x> <y>
//! COMPONENTS <n>
//! SAVE <texture_name>
//! COMPUTE <bw> <bh>
```

#### Required Directives

| Directive | Description |
|-----------|-------------|
| `HOOK` | Which texture to hook into. Common values: `MAIN` (the main video frame), `LUMA`, `CHROMA`, `NATIVE`, `OUTPUT`. Multiple `HOOK` lines can be specified to apply the shader at multiple stages. |
| `BIND` | Which textures the shader needs access to. At minimum, `BIND HOOKED` gives access to the texture being processed. Multiple `BIND` lines can be specified. |

#### Optional Directives

| Directive | Description |
|-----------|-------------|
| `DESC` | Human-readable description, shown in log output. |
| `WIDTH` / `HEIGHT` | Output dimensions as expressions. Can reference `HOOKED.w`, `HOOKED.h`, or use arithmetic (e.g., `HOOKED.w * 2` for 2x upscaling). If omitted, output size matches input. |
| `WHEN` | Conditional execution expression. Shader is skipped if the expression evaluates to 0. |
| `OFFSET` | Pixel offset for the output. Used for alignment when the shader changes dimensions. |
| `COMPONENTS` | Number of color components the shader outputs (1-4). Default is 4 (RGBA). |
| `SAVE` | Save the output to a named texture instead of replacing the hooked texture. Used for multi-pass shaders. |
| `COMPUTE` | Block dimensions for compute shaders. Enables compute shader mode instead of fragment shader mode. |

### GLSL Code

After the directives, the file contains a `hook()` function:

```glsl
vec4 hook() {
    // Your shader code here
    return HOOKED_texOff(0);  // Sample the input texture at the current position
}
```

#### Built-in Functions and Variables

For each texture bound via `BIND`, libplacebo provides:

| Function/Variable | Description |
|-------------------|-------------|
| `TEXTURENAME_pos` | Current texture coordinate (vec2, normalized 0.0-1.0) |
| `TEXTURENAME_size` | Texture dimensions in pixels (vec2) |
| `TEXTURENAME_pt` | Pixel size = 1.0 / size (vec2) |
| `TEXTURENAME_tex(pos)` | Sample the texture at normalized coordinates |
| `TEXTURENAME_texOff(offset)` | Sample relative to current position (in pixels) |
| `TEXTURENAME_gather(pos, comp)` | Gather 4 neighboring texels (for performance) |

The special name `HOOKED` refers to whichever texture is being processed (as specified by the `HOOK` directive).

## Example: Minimal Passthrough Shader

The simplest possible shader that reads each pixel and writes it back unchanged:

```glsl
//! HOOK MAIN
//! BIND HOOKED
//! DESC Passthrough (no-op)

vec4 hook() {
    return HOOKED_texOff(0);
}
```

Save this as `passthrough.hook` and load it in the `placebo.shader` filter. Every frame is uploaded to the GPU, processed (identity operation), and downloaded back. Useful as a starting template or for benchmarking GPU upload/download overhead.

## Example: Simple Color Correction

A shader that adjusts brightness, contrast, and saturation:

```glsl
//! HOOK MAIN
//! BIND HOOKED
//! DESC Simple color correction

vec4 hook() {
    vec4 color = HOOKED_texOff(0);

    // Parameters (edit these values)
    float brightness = 0.05;   // -1.0 to 1.0
    float contrast   = 1.1;    // 0.0 to 3.0 (1.0 = no change)
    float saturation = 1.2;    // 0.0 to 3.0 (1.0 = no change)

    // Apply brightness
    color.rgb += brightness;

    // Apply contrast (around midpoint 0.5)
    color.rgb = (color.rgb - 0.5) * contrast + 0.5;

    // Apply saturation
    float luma = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
    color.rgb = mix(vec3(luma), color.rgb, saturation);

    // Clamp to valid range
    color.rgb = clamp(color.rgb, 0.0, 1.0);

    return color;
}
```

## Example: Vignette Effect

A shader that darkens the edges of the frame:

```glsl
//! HOOK MAIN
//! BIND HOOKED
//! DESC Vignette

vec4 hook() {
    vec4 color = HOOKED_texOff(0);

    // Vignette parameters
    float strength = 0.5;   // 0.0 = no vignette, 1.0 = strong
    float radius   = 0.75;  // Inner radius where darkening begins

    // Calculate distance from center (normalized)
    vec2 uv = HOOKED_pos - 0.5;
    float dist = length(uv * vec2(HOOKED_size.x / HOOKED_size.y, 1.0));

    // Smooth falloff
    float vignette = smoothstep(radius, radius + 0.5, dist);
    color.rgb *= 1.0 - (vignette * strength);

    return color;
}
```

## Popular Community Shaders

### Anime4K

**Repository**: `https://github.com/bloc97/Anime4K`

A collection of real-time anime upscaling and restoration shaders. Designed specifically for anime content, exploiting the flat-shaded aesthetic to achieve sharper upscaling than general-purpose algorithms.

Key shaders:

| Shader | Description |
|--------|-------------|
| `Anime4K_Restore_CNN_VL` | Neural network artifact restoration (very large model) |
| `Anime4K_Upscale_CNN_x2_VL` | 2x neural network upscaling |
| `Anime4K_Clamp_Highlights` | Prevent ringing artifacts from other shaders |
| `Anime4K_Darken` / `Anime4K_Thin` | Line art enhancement |

Typical usage: stack `Clamp_Highlights` + `Restore_CNN` + `Upscale_CNN_x2` as three separate `placebo.shader` instances on the same clip.

### FSRCNNX

**Repository**: `https://github.com/igv/FSRCNN-TensorFlow`

Fast Super-Resolution Convolutional Neural Network (extended). A general-purpose neural network upscaler that works well on both live-action and animated content.

| Shader | Description |
|--------|-------------|
| `FSRCNNX_x2_8-0-4-1.glsl` | 2x upscale, 8-0-4-1 architecture (fast) |
| `FSRCNNX_x2_16-0-4-1.glsl` | 2x upscale, 16-0-4-1 architecture (quality) |

These shaders double the frame resolution. The filter's output resolution depends on the `WIDTH` and `HEIGHT` directives in the shader (e.g., `HOOKED.w * 2`).

### KrigBilateral

A chroma upscaling shader that uses bilateral filtering guided by luma information to produce sharper, more accurate chroma planes. Particularly effective for 4:2:0 subsampled content.

| Shader | Description |
|--------|-------------|
| `KrigBilateral.glsl` | Luma-guided chroma upscaling |

Hooks into `CHROMA` rather than `MAIN`, so it processes chroma planes specifically.

### Film Grain

Various shaders that overlay synthetic film grain on the video frame. Useful for adding a cinematic look or matching the grain characteristics of film-originated content.

| Shader | Description |
|--------|-------------|
| `noise_static.hook` | Static per-frame noise overlay |
| `filmgrain.glsl` | Animated film grain with configurable intensity |

### SSimDownscaler

An SSIM-optimized downscaler that produces perceptually sharper results than traditional downscaling algorithms. Hooks into the `OUTPUT` stage.

| Shader | Description |
|--------|-------------|
| `SSimDownscaler.glsl` | SSIM-optimized downscaling |

## Where to Find Shaders

### Primary Sources

1. **mpv User Shaders Wiki**
   `https://github.com/mpv-player/mpv/wiki/User-Scripts#user-shaders`
   Curated list maintained by the mpv community. The most comprehensive index of available shaders.

2. **Anime4K GitHub**
   `https://github.com/bloc97/Anime4K/tree/master/glsl`
   Official releases with versioned shader packs.

3. **igv's shader collection**
   `https://gist.github.com/igv`
   FSRCNNX, KrigBilateral, SSimDownscaler, and other high-quality shaders.

### Community Collections

4. **mpv-prescalers**
   `https://github.com/bjin/mpv-prescalers`
   RAVU (Rapid and Accurate Video Upscaling) shaders in multiple quality tiers.

5. **libplacebo test shaders**
   The libplacebo repository includes test shaders in its test suite that can serve as additional examples.

### Compatibility Notes

- All mpv-compatible `.hook` / `.glsl` user shaders work with this filter. The parsing is handled by libplacebo's `pl_mpv_user_shader_parse()`, which is the same parser used by mpv itself.
- Shaders designed for mpv's `vo_gpu` or `vo_gpu-next` backends are compatible.
- Compute shaders (`//! COMPUTE`) require GPU compute support. D3D11 supports this on Feature Level 11.0+.

## Performance Considerations

### GPU Upload/Download Overhead

Every frame goes through a CPU-to-GPU upload and GPU-to-CPU download cycle. This round-trip has a fixed cost regardless of shader complexity. For simple shaders, this overhead may dominate the total processing time.

Measured overhead for a 1920x1080 RGBA frame (typical):

- Upload (`pl_tex_upload`): ~0.5-1.5 ms
- Download (`pl_tex_download`): ~0.5-2.0 ms
- Total round-trip (no shader work): ~1-3 ms per frame

### Shader Complexity

| Category | Examples | Typical Cost (1080p) |
|----------|----------|---------------------|
| Trivial | Passthrough, brightness, contrast | < 0.1 ms |
| Simple | Color correction, vignette, grain | 0.1-0.5 ms |
| Medium | Debanding, simple upscaling | 0.5-2 ms |
| Complex | Neural network upscaling (FSRCNNX 8-0-4-1) | 2-10 ms |
| Very complex | Neural network (FSRCNNX 16-0-4-1, Anime4K VL) | 10-50+ ms |

### Multi-Shader Stacking

Each `placebo.shader` instance is independent -- it creates its own textures and performs its own upload/download cycle. Stacking N shader instances incurs N round-trips.

For maximum efficiency when using multiple shaders, consider:

- Combining related operations into a single shader file (if feasible)
- Using the `SAVE` directive for multi-pass shaders within a single file
- Placing the most expensive shader last in the chain (to process fewer frames if earlier filters reduce the pipeline)

### Resolution and Format

- Processing time scales linearly with pixel count. A 4K frame (3840x2160) takes approximately 4x longer than 1080p.
- The filter operates on RGBA8 (8 bits per channel, 4 channels). Higher bit depths would require modifications to the texture format in the C code.
- Neural network upscaling shaders that output a larger resolution (e.g., `WIDTH HOOKED.w * 2`) allocate proportionally larger destination textures.

### Shader Cache Impact

The first time a shader runs, the GPU driver compiles the GLSL code to native GPU instructions. This compilation can take several hundred milliseconds. The shader cache (`shader_cache.bin`) stores compiled shaders to eliminate this cost on subsequent runs.

If you experience a noticeable stutter on the first frame after loading a new shader, this is expected -- it is the one-time compilation cost. Subsequent frames use the cached compiled shader.
