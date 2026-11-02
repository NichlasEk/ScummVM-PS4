# PS4 2D image filters

The ScummVM 2026.4.0 Vulkan build includes **30 selectable 2D filters** in
Global Options → Graphics → Shader. The standard chooser lists `PS4/*.glslp`;
choose a preset and apply the settings. Clear the selection with the **X** next
to Shader (displayed as **None**) to disable filtering. Per-game overrides are
available under Edit Game → Graphics.

The entire collection is bundled in `/app0/data/shaders.dat`; no download or
network access is needed. [Full catalog](PS4-SHADER-CATALOG.md):

- 10 CRT styles: scanlines, light, soft, curved, glow, aperture grille, slot
  mask, shadow mask, warm television and curved glow.
- Three monochrome monitors: green, amber and white phosphors.
- Five LCD styles: pixel grid, RGB subpixels, green/gray handheld and backlit.
- Four scaling filters: smooth, sharp bilinear, soft and sharpen.
- Three analog effects: composite colour bleed, RGB fringe and static tape.
- Five colour filters: grayscale, sepia, vivid, posterization and dithering.

These are original single-pass GLSL filters, with bounded texture sampling and
no downloaded shader dependencies. Tape grain is static, not temporal VHS
simulation. The GUI overlay is rendered after the filter. Filtering defaults to
off; the selected preference persists in ScummVM's existing configuration.

3D rendering bypasses the filters while retaining the preference.

External `.glslp` presets are now accepted through **Pick file instead**, without
a filename allowlist. Copy the entire shader folder, including the referenced
`.glsl` files and any LUT images, keeping relative paths intact. For example,
place it under `/data/scummvm/shaders/`, then select the `.glslp` and apply.
A `.glslp` is a preset referencing other files; copying it alone is insufficient.

External single-pass, multipass and LUT presets use the same parser, compiler
and rendering pipeline as bundled presets. This does not guarantee compatibility
with every RetroArch shader. Missing files and compiler errors disable the filter and retain a working
unfiltered game image. The PS4 backend explicitly rejects mipmap input and floating-point or
sRGB framebuffer requests, which it cannot render correctly. See `scummvm.log`
for the failing path, pass or compiler diagnostic. Physical PS4 validation of
external presets is pending; no console test was performed during the TV pause.

Version 0.22 has the old bundled-only restriction. External loading is enabled
in version 0.24.

## Implementation

The PS4 bridge already supports color texture framebuffers. The port enables
ScummVM's Libretro pipeline using this bounded capability, without advertising
general depth/renderbuffer support. Backbuffer activation explicitly rebinds
framebuffer zero. Each filter pass owns a packed VBO for its quad and texture
coordinates; CPU-pointer vertex attributes are unsupported by the bridge.
Attributes are reapplied before drawing, and buffers are freed with the preset.

The initial probe used CPU pointers and produced an upside-down image with GL
errors. That probe is superseded by the VBO implementation. Its orientation is
now correct and was confirmed visually by the user in shadPS4.

Rebuild incrementally with the existing configured 2026.4.0 Vulkan tree:

```sh
make -C build/scummvm-2026.4.0-vulkan -j8
SCUMMVM_APP_VERSION=0.22 \
 SCUMMVM_STAGE_DIR="$PWD/build/scummvm-ps4-shaders-022-pkg" \
 SCUMMVM_DIST_DIR="$PWD/dist/ps4-shaders-022" \
 bash experiments/scummvm-2026.4.0/package-vulkan.sh
```

Prepared-source changes are maintained in `vulkan.patch`. The shader source is
under `assets/ps4-shaders`, and `scripts/package-ps4-shaders.py` creates its
reproducible ZIP archive. Preserve the existing build's configuration, including
its heap and engine options, when rebuilding from scratch.

## Build and validation

`assets/ps4-shaders/catalog.json` is the complete preset catalog.
`scripts/generate-ps4-filters.py` regenerates the new shader sources and native
preset catalog (`compat/ps4gl/scummvm-filter-presets.inc`). The source snapshot's
`backends/graphics/opengl/ps4-filter-presets.h` must match this catalog;
`vulkan.patch` preserves it for clean builds. The original Scanlines and CRT
Light paths remain valid for existing configurations.

Tests:

```sh
python3 tests/ps4/check-filter-catalog.py
bash scripts/check-ps4-filters.sh
```

The compiler check reuses `build/glslang-host` from
`scripts/check-ps4-glslang.sh` and links every filter through the production
legacy GLSL-to-SPIR-V bridge under ASan/UBSan.

Evidence for 0.22 is under `artifacts/ps4-shaders-022/`. The temporary cycle
probe swaps filters during DOTT gameplay, recording transaction results and
one screenshot per selection. The probe code is excluded from the production
source, maintained patch and packaged executable.

Validated for 0.22:

- All 30 presets compile/link through the production legacy GLSL bridge under
  ASan/UBSan. Catalog, allowlist, preset references and ZIP integrity pass.
- Vulkan patch replay matches all 46 modified/added files against clean upstream.
- In shadPS4, all 30 filters were applied during DOTT with successful graphics
  transactions, upright visible images and no unexpected ScummVM warnings.
- Disabling filtering after the complete sweep succeeds. Render-target memory
  returns from 3,017,920 bytes to zero and texture memory returns to baseline.
- Package digests/signatures pass; all 241 extracted payloads match staging.
- The extracted final PKG boots DOTT with CRT-Curved-Glow, correct orientation
  and no ScummVM warnings in a separate cold-start profile.
- Test-cycle instrumentation is absent from the production ELF and source patch.

Package: `dist/ps4-shaders-022/ScummVM-2026.4.0-Vulkan-Test.pkg`.
Title ID `SCVM00265`, APP_VER `0.22`; updates the existing 2026.4.0 test title.
No games included. Physical PS4 performance and acceptance remain pending.

SHA-256: `ad83b7c46f13379d30e6fb099b28533773203d4be7e8e4700c87560532068922`


### External preset validation (0.24)

The filename gate from0.22 is removed. Shaders are parsed and compiled regardless
of preset name or selected filesystem path. Actual unsupported mipmap-input,
float-FBO and sRGB-FBO requests are rejected explicitly instead of silently
changing their rendering semantics.

Emulator regression: bundled baseline; external single pass; external two-pass;
external PNG LUT; missing referenced GLSL; invalid GLSL; float FBO; sRGB FBO;
mipmap input; shader OFF. All10 expected outcomes passed, including continuing
with unfiltered rendering after rejected presets. Positive cases have captured
screenshots. This is bounded compatibility coverage, not proof for every
third-party shader. All30 bundled filters also pass the host compiler/link check.
Evidence: artifacts/ps4-external-shaders/. No physical console tests in this run.


Package0.24: `dist/ps4-external-shaders-024/ScummVM-2026.4.0-Vulkan-Test.pkg`,
250216448 bytes, SHA256
`da83df1d92b34cc3509ba5f7de12e403d0639f24320122fbbf3907463c0da0f9`.
PKG signatures/hashes and all241 extracted payload files verified. Package
contains production graphics code, without the filter-cycle or MT32/intro probes.
