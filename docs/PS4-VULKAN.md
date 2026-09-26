# Native Vulkan checkpoint (2026-09-25)

The standalone PS4 probe renders a background and interpolated triangle through
vulkan-ps4 → OpenGNM, with SPIR-V compiled to GFX7 GCN inside the PS4 process.
No Piglet or Shacc module is linked. This is an experimental driver checkpoint,
not Vulkan conformance or a working OpenGL/Thimbleweed Park backend.

## Reproduce

Use the pinned toolchains and prerequisites in [PS4-OPENGNM.md](PS4-OPENGNM.md),
plus `glslc` and `spirv-val`:

```sh
scripts/build-vulkan-probe.sh
xvfb-run -a -s '-screen 0 1280x720x24' python3 scripts/run-opengnm-probe.py --vulkan
```

The runner uses the installed `.tools/shadps4/Shadps4-sdl.AppImage` and isolated
`.tools/xdg-vulkan/shadPS4/config.toml` profile. On first setup, copy the known
working OpenGNM profile's config there, retaining its F12 screenshot binding.
No game saves are needed. The test title is SCVM00009 and its package is
`dist/ScummVM-PS4-Vulkan-Probe.pkg`, with a SHA-256 sidecar. The regular ScummVM
package is independent.

Evidence is saved in `artifacts/vulkan/`: `triangle-gpu.png`, `pixel-test.log`,
`triangle-result.txt`, `driver.log`, `triangle-emulator.log`, and `imports.txt`.
The pixel oracle checks 149 interpolated triangle samples and five background
samples from shadPS4's actual game framebuffer. Both primitives are GPU draws.
Eight frames exercise acquisition, command recording/reset, pipeline switching,
queue submission, fence polling and presentation. The runner also requires the
post-destruction lifecycle marker; an image alone does not pass.

## Fixes carried as patches

`patches/opengnm/vulkan-runtime.patch` uploads compiled shader instructions to
aligned GPU memory, retains the trailing shader metadata, connects VS exports
to PS inputs, and programs Vulkan depth range and scissor state. Shader uploads
are released on failed creation and pipeline destruction. Device creation does
not finish a presentation frame before VideoOut is registered. Polled EOP labels
do not request unsupported CP interrupts. Queue-idle submission errors propagate.

The compiler patch builds real portable BLAKE3, DXT format conversion and Mesa
logging. The native application no longer links upstream `mesa_stubs.c`, whose
placeholder implementations are unsuitable for runtime shader compilation.
Header and archive dependencies ensure native objects and links are rebuilt.

## Deliberately limited evidence

Validated in shadPS4 only; a physical PS4 run is still required. The probe uses
the driver's direct PS4 swapchain convention (null surface), one viewport,
resource-free vertex/fragment shaders, procedural vertices, and serial GPU
submission. It draws the background rather than testing render-pass clears.

Beyond the checkpoints below, vertex buffers, compute, depth,
asynchronous semaphore submissions and error recovery are not validated here.
The upstream feature advertisement is not evidence of implementation. The
resource checkpoint below adds Vulkan coverage beyond the earlier native GNM tests.

The resource bridge and its bounded contract are described below. The shared
submission epilogue still needs a lifetime review before overlapping submissions
are supported.

Final checkpoint: 154 pixel samples passed, all eight frames completed, and
`PASS Vulkan lifecycle` followed resource destruction. The VideoOut helper now
unregisters the returned attribute group once instead of treating image indices
as registration groups. The host compiler resource regressions and native GNM
triangle pixel test also passed. Stable ScummVM package SHA-256 remains
`c519b288b663d4f7b5db0e86b23a2a7031f1dacc97984edaf9b807f3738191e1`.

## Vulkan resources checkpoint (2026-09-25, following the first triangle)

The resource bridge now passes the real GPU texture oracle through Vulkan:
208 nearest and 369 bilinear RGBA samples, including 333 samples outside the
normalized UV range to check clamp-to-edge, plus five background samples.
This is Vulkan API resource creation and binding, not private GNM descriptor
injection by the test. Vertex UBO data controls geometry; two fragment UBOs
control color, bias and alpha modulation. Nonzero buffer offsets, sparse and
unsorted binding numbers, descriptor writes/copies, freeing the source set,
binding before the pipeline, pipeline switching, pool reset/reallocation and
pool destruction with live sets are exercised.

```sh
scripts/build-vulkan-probe.sh --resources
xvfb-run -a -s '-screen 0 1280x720x24' python3 scripts/run-opengnm-probe.py --vulkan --resources
```

The package is `dist/ScummVM-PS4-Vulkan-Resources.pkg`; it shares diagnostic title
SCVM00009 with the basic probe. The last built mode determines `eboot.bin`.
Resource run evidence is in `artifacts/vulkan/resources/`; build logs and import
checks remain in `artifacts/vulkan/`. Both modes require the lifecycle marker.

Each descriptor set owns a GPU-visible table until free/reset/pool destruction.
Writes and copies publish to that table. Commands remember the graphics set and
emit its pointer for each consuming VS/PS stage at draw time. Pipeline compilation
receives per-stage masks of declared UBO and combined-sampler bindings, checking
them against the compiler's bounded set-0 ABI. Existing pipeline-cache keys omit
layout details, so cache reuse is disabled pending a correct key. Linear sampled
RGBA8 images now use linear GPU layout and report the actual padded row pitch.

The supported resource contract is deliberately small: set 0, binding numbers
0–15, one descriptor per binding, ordinary UBOs in VS/FS, combined float sampler2D
in FS. Descriptor arrays, dynamic UBOs, additional sets, immutable samplers,
push constants, indexing/update-after-bind flags and other resource types are
rejected. The layout-support query uses the same validation as creation.
Negative native checks cover arrays, dynamic UBOs, multiple sets, exhausted pool
capacity and a resource shader compiled against an empty pipeline layout.
Linear image support here is a single sampled RGBA8 mip/layer. Normal Vulkan
synchronization and object-lifetime requirements still apply; updating descriptors
referenced by recorded/pending commands is not an update-after-bind feature.

This supersedes the earlier statement that Vulkan textures/UBOs were unvalidated.
It does not establish support for arbitrary Vulkan resources, arbitrary optimal-image
uploads, depth or overlapping submissions. Later checkpoints below cover bounded
uploads and selected render-to-texture/blending cases. Physical
PS4 remains untested. The upload checkpoint below adds GPU buffer-to-image transfer and conservative
barriers. Next: blending/render-target sampling and a feature audit against the
OpenGL translator before wiring it into ScummVM.

Final regression: the original Vulkan triangle still passes all 154 pixel
checks and lifecycle cleanup with this driver/compiler. The standalone compiler
resource fixtures pass as well. The runnable `eboot.bin` is restored to the
resource probe after the basic-triangle regression.

## GPU tiled upload checkpoint

The `--upload` variant creates an optimal-tiling 37×23 RGBA8 image, then uploads
it through `vkCmdCopyBufferToImage`. Two unequal subregions use a nonzero buffer
offset, a 43-pixel source row stride and a nonzero destination X offset. The
staging buffer is cleared before recording every frame and populated only after
`vkEndCommandBuffer`: the image must be copied by the GPU when submitted.
The CPU oracle models all 37×23 texels, including nearest/bilinear filtering and
clamping, independently of the driver's tiling code.

```sh
scripts/build-vulkan-probe.sh --upload
xvfb-run -a -s '-screen 0 1280x720x24' python3 scripts/run-opengnm-probe.py --vulkan --upload
```

Output: `dist/ScummVM-PS4-Vulkan-Upload.pkg` and SHA-256 sidecar; runtime reports
and screenshot: `artifacts/vulkan/upload/`. This uses the same SCVM00009 diagnostic
title. The basic and resource modes are still available.

The driver computes tiled destination addresses using OpenGNM GpuAddr and emits
CP DMA copies, coalescing adjacent source/destination spans. No source pixels are
read during recording. Transfer-to-sampling barriers use a command-buffer-owned
GPU completion label, EOP flush/wait and texture-cache acquisition. The label is
reset by GPU commands on every execution, including resubmission. Recording a
barrier no longer changes the global image layout before submission. Labels are
released with their owning command buffers.

Scope: single-mip, single-layer, single-sample 2D RGBA8 UNORM/SRGB uploads to
sampled textures. Unsupported shapes and source bounds errors fail command
recording rather than silently using linear addressing. Cross-queue ownership
and simultaneous use of one command buffer are explicitly unsupported. Packet
buffer exhaustion is reported; per-span DMA is a correctness bridge, not yet a
high-throughput upload solution for large textures. Compressed formats, mip-chain
uploads, 3D/array images, image readback and image-to-image/blit operations remain
unvalidated. This is still emulator evidence, not physical PS4 verification.

The upload test also rejects unsupported mip uploads, out-of-range source reads,
queue ownership transfer and simultaneous command-buffer use. The last frame
resubmits the same recorded command buffer to exercise completion-label reuse.

Validation: the upload mode passed 582 pixel checks, all negative recording
checks, command-buffer resubmission and lifecycle cleanup in shadPS4. The basic
Vulkan triangle (154 pixel checks) and original resource mode (582) also passed
with the new barrier implementation. The upload executable was restored after
these regression runs. The stable ScummVM package hash remains unchanged.

## Offscreen composition and blending checkpoint

```sh
scripts/build-vulkan-probe.sh --composite
xvfb-run -a -s '-screen 0 1280x720x24' python3 scripts/run-opengnm-probe.py --vulkan --composite
```

This exercises the existing driver implementation with a new independent pixel
oracle. An 80×60 RGBA8 UNORM color attachment is drawn in one pass, transitioned
from color-write to shader-read, then sampled in a second 1280×720 sRGB pass.
No CPU readback or CPU-created replacement pixels are involved. The source image
contains four spatially distinct regions, testing:

- SRC_ALPHA / ONE_MINUS_SRC_ALPHA color blending.
- ONE / ONE_MINUS_SRC_ALPHA separate alpha blending.
- Additive color with a red-only write mask; other channels must survive.
- Unblended data preservation and full RGBA write-mask restoration afterward.
- Scissor/viewport changes between unequal framebuffer sizes.
- Sampled attachment alpha influencing the final RGB output.

The oracle accounts for quantization into the intermediate UNORM attachment,
blend equations and final sRGB conversion. It checks 1,230 interior pixels
across all four regions. Eight render/sample iterations and complete resource
cleanup also pass in shadPS4. This validates these blend cases and render-target
sampling; it is not coverage of every blend factor, format, MRT, depth mode or
implicit render-pass dependency. Explicit barriers separate the passes.

Package: `dist/ScummVM-PS4-Vulkan-Composite.pkg` with SHA-256 sidecar. Reports:
`artifacts/vulkan/composite/`. This remains a separate SCVM00009 diagnostic.

## TWP source requirements and the next integration gate

`python3 scripts/audit-twp-gl.py` inventories the current local renderer and
shared shader wrapper, recording source hashes and call locations in
`artifacts/vulkan/twp-gl-inventory.json`. It finds 59 distinct GL call names in
three source files; conditional paths are included, so this is not a runtime
trace. Relevant current source:

- `engines/twp/gfx.cpp`: texture upload/FBO/readback at lines 71–131; GLSL 110
  shader setup and vertex attributes at 140–147; dynamic VBO/EBO uploads and
  array/indexed drawing at 247–317; target/viewport switches at 387–393.
- `engines/twp/shaders.cpp`: transform matrix, position/color/UV attributes,
  varying interpolation, one/two samplers, scalar/vector uniforms, and the
  `RandomValue[5]` uniform array used by effects.
- `graphics/opengl/shader.cpp` and `.h`: compile/link/reflection, uniforms,
  vertex-attribute setup and program lifetime.

The standard TWP blend function matches the new straight-alpha test exactly.
Texture sampling, explicit synchronization and render-target composition now
have bounded native Vulkan evidence. However, TWP also needs streamed vertex
and uint32 index buffers, matrix transforms, clears, readback and legacy GLSL
translation/reflection. The probes use prebuilt SPIR-V; they do not yet provide these GL-facing facilities.
The indexed vertex-buffer checkpoint below now covers the native buffer/matrix path.

The native transfer-clear/readback checkpoint is recorded below. Remaining gates
include partial attachment clears and actual translated shader effects before
enabling an OpenGL context in ScummVM.
No general OpenGL translator has been integrated by these probes, and no TWP
startup on this open stack is claimed. Physical PS4 verification remains open.

Composite regression: the basic triangle (154 samples) and optimal-upload mode
(582 samples, negative checks and resubmission) pass with the shared probe changes.
The runnable composite executable is restored afterward. Stable ScummVM package
SHA-256 is unchanged; this milestone adds verified coverage of existing driver
paths rather than enabling a new game backend.

## Indexed vertex inputs and matrix transforms

The `--sprites` diagnostic exercises TWP's 32-byte interleaved layout: float2
position at byte 0, float4 color at byte 8 and float2 UV at byte 24. An indexed
textured triangle deliberately reuses the texture oracle, now with interpolated
UVs and vertex color instead of fragment-coordinate-generated UVs. Its nonidentity
mat4 UBO scales and flips the input positions. It uses a 32-byte vertex binding
offset, 160-byte uint32 index binding offset and firstIndex=1, with poisoned
padding to catch offset mistakes. The vertex buffer is bound before the pipeline.
A second, offscreen draw changes the vertex binding offset; it must not mutate
the first draw's descriptor table.

The compiler now enables the external vertex fetch prolog, emits its actual
SGPR address, reserves a full 64-bit vertex-table pointer, assigns fragment
input locations and preserves the real VS-export/PS-input semantic mapping.
Previously multiple fragment attributes silently shared interpolation slot zero.
The driver allocates fetch code and immutable per-draw descriptor snapshots in
GPU-visible memory, and derives each attribute's format, offset and record count
from the pipeline and bound buffer. A command buffer has a 64-KiB descriptor
arena, reset only when recording is reset; exhaustion is reported rather than
wrapping over descriptors still referenced by recorded draws.

Supported vertex scope is per-vertex float1/2/3/4 attributes, nonzero stride,
one instance, zero base vertex/firstVertex/firstInstance. Other vertex draw
parameters report an error at EndCommandBuffer. This is bounded native coverage,
not a complete Vulkan implementation, GL translator, TWP startup or physical-PS4
validation. Buffer contents and descriptor sets still obey normal caller-managed
GPU lifetime/synchronization rules.

Reproduce with `scripts/build-vulkan-probe.sh --sprites`, then
`xvfb-run -a -s '-screen 0 1280x720x24' python3 scripts/run-opengnm-probe.py --vulkan --sprites`.
Reports are under `artifacts/vulkan/sprites/`; the separate diagnostic package is
`dist/ScummVM-PS4-Vulkan-Sprites.pkg` (SCVM00009).

Validation: 577 texture samples plus 5 background checks and complete eight-frame
lifecycle pass in shadPS4, including both draws. The basic triangle (154 checks),
optimal upload (582 checks and resubmission) and composite (1,230 checks) regressions
pass with the same driver/compiler changes. Host compiler tests include both new
sprite shaders. Stable ScummVM 1.17 retains SHA-256
`c519b288b663d4f7b5db0e86b23a2a7031f1dacc97984edaf9b807f3738191e1`.

## Transfer clears and GPU-to-CPU readback

The independent `--readback` mode extends the offscreen blend probe with
`vkCmdClearColorImage` and `vkCmdCopyImageToBuffer`. Transfer clear is now a
bounded, checked path for full single-mip/single-layer RGBA8 UNORM images. It
clamps components before quantization and fills the entire allocated surface,
including tile padding. A constant 32-bit pixel is invariant under tiling;
filling only width*height bytes would leave holes in padded tiled surfaces.
Unsupported formats/ranges return an error rather than an approximate clear.
This transfer checkpoint does **not** establish render-pass loadOp clear,
depth/stencil clear or sRGB clear support. Partial color attachment clears are
covered by the later checkpoint below.

Readback supports single-sample 2D RGBA8 UNORM/sRGB images, one mip/layer,
including render targets. It first copies the full physical image surface on
the GPU, then uses GPA surface addresses to copy contiguous spans from that
snapshot into the requested linear buffer rows. This supports buffer offsets,
image offsets and both explicit and tightly packed row pitches. No source pixel
is read on the CPU while recording commands. Scratch allocations are retained
per command buffer, allocated to the actual surface size with 64-KiB alignment;
there are at most 16 scratch-backed transfer/clear operations per recording,
each bounded to a 16-MiB surface. Reset/re-record and destruction follow the command-buffer GPU lifetime.
Explicit barriers and a completed fence precede the CPU oracle.

The test uses an 80x60 tiled UNORM render target and a 64-KiB host-coherent buffer.
It checks alternating clamped clear colors, the rendered/blended full image,
a pitched crop across all four quadrants and a tightly packed crop. Every byte
outside the copies must retain its sentinel. Invalid row pitch, destination
bounds, mip selection, image bounds and clear ranges are rejected. Frame 7 is
submitted a second time without re-recording after poisoning the destination.
The existing 1,230-point screenshot oracle still verifies the presented image.

Reproduce with `scripts/build-vulkan-probe.sh --readback` followed by
`xvfb-run -a -s '-screen 0 1280x720x24' python3 scripts/run-opengnm-probe.py --vulkan --readback`.
Package: `dist/ScummVM-PS4-Vulkan-Readback.pkg`; reports:
`artifacts/vulkan/readback/`. This remains the separate SCVM00009 diagnostic.

The runner enables `GPU.readbacks_mode=2` (Precise) only in the isolated diagnostic
profile `.tools/xdg-vulkan`. The tested shadPS4 v0.18.0 revision is
`e3ce810f3a653f43ac64ebab63023de281a4103a`; mode 0 disables CPU visibility of GPU
writes. Source inspection at that revision also showed that image-to-buffer
alias synchronization expects the image base address, motivating the full-surface
snapshot before copying tiled spans. Direct small-span copies failed this test.
These are emulator results; physical PS4 cache visibility is not yet validated.

Next: the GL translation/reflection boundary, with partial attachment clears
covered by the checkpoint below.
The native full-image transfer operations can now underpin screenshot/readPixels
work, but ScummVM does not yet have an OpenGL context on this stack.

Readback validation: all six rejection cases return their expected errors; nine
64-KiB CPU comparisons (eight frames plus resubmission), the 1,230-point screenshot
oracle and complete resource cleanup pass. This tests GPU-produced pixels, not
just submission success. Scratch capacity exhaustion is included in the rejection
cases. The diagnostic leaves stable ScummVM package 1.17 unchanged.

Regression results with the same driver: basic triangle, optimal texture upload
(including resubmission), offscreen blending and indexed vertex/matrix probes all
pass their pixel oracles and lifecycle cleanup. The runnable readback diagnostic
is restored after these checks.

## Partial color attachment clears

`--rectclear` verifies `vkCmdClearAttachments` on the same tiled 80x60 target.
The previous implementation wrote linear rows into tiled images; its depth path
also treated over-clearing as acceptable. The new bounded color path rejects
unsupported aspects and uses a GPU snapshot of the complete backing surface,
GPA-addressed constant fills for only the requested rectangles, then a GPU copy
back with cache synchronization. It preserves unselected pixels, all RGBA
components, and the bound pipeline, blending, viewport and scissor state.
No CPU pixel copying and no game/room-specific exceptions are involved.

The test binds the left-half scissor and alpha-blend pipeline, clears rectangles
on both sides, adds an overlapping rectangle with a different color, then draws
without re-binding the pipeline or scissor. Clears ignore those draw states;
subsequent drawing must still use them. The CPU oracle checks every pixel of the
result plus pitched/tight crops and untouched destination guards. The 1,230-point
screenshot oracle includes the cleared areas. Eight frames plus re-submission
exercise cache reuse and ordering. Four additional recording-only rejection tests
cover depth aspect, unsupported array layer and rectangles outside the render area.

This test also exposed a shadPS4 v0.18.0 cache-coherence gap in direct GPU fills
of an image after GPU buffer copies. Full transfer clear now fills a temporary
GPU surface and copies it into the image, using the same image-invalidation path
as partial clears. The change is exercised across alternating clear colors.

Supported attachment scope is RGBA8 UNORM, single-sample 2D, one mip/layer and
ordinary framebuffers. Multiple/overlapping rectangles are supported; depth,
stencil, sRGB, imageless framebuffers and other layouts are explicitly outside
this path. Render-pass loadOp clears are still unverified. Readback, transfer
clear and each cleared attachment share the command buffer's 16 retained scratch
slots, each capped at 16 MiB. A clear copies the full backing surface twice, so
this is a correctness-first path; its bandwidth cost has not been benchmarked.
Physical PS4 behavior remains unverified.

Reproduce with `scripts/build-vulkan-probe.sh --rectclear`, then
`xvfb-run -a -s '-screen 0 1280x720x24' python3 scripts/run-opengnm-probe.py --vulkan --rectclear`.
Package: `dist/ScummVM-PS4-Vulkan-RectClear.pkg`; reports:
`artifacts/vulkan/rectclear/`. The package is separate from stable ScummVM 1.17.
The next integration step is translating and reflecting TWP's actual shaders,
then wiring those operations through a GL-facing context.

Partial-clear validation: all ten rejection checks, nine 64-KiB CPU comparisons,
the 1,230-point screenshot oracle and lifecycle cleanup pass in shadPS4.
Regressions for the basic triangle, optimal texture upload, offscreen blending,
indexed vertices/matrices and standalone transfer-clear/readback all pass. The
runnable partial-clear diagnostic is restored afterward; stable ScummVM retains
SHA-256 `c519b288b663d4f7b5db0e86b23a2a7031f1dacc97984edaf9b807f3738191e1`.

## Actual TWP shader translation and register-budget correction

`python3 scripts/prepare-twp-shaders.py` extracts the six original GLSL bodies
from the local ScummVM checkout: shared vertex shader, default texture shader,
grayscale, ghost, sepia and two-texture fade. It translates the bounded legacy
interface to Vulkan GLSL 450, validates SPIR-V, reflects compiled decorations,
and compiles each to a PS4 GCN binary with structural and CRC verification.
`python3 tests/ps4/check-twp-shaders.py` checks the resulting interface contracts
and rejection of declarations outside the supported subset.

The bridge reserves set 0 binding 0 for the vertex uniform block, binding 1 for
the fragment uniform block, and bindings 2/3 for the two combined samplers.
Position/color/UV locations are 0/1/2; color/UV varyings are 0/1 in both stages,
even when source declarations appear in a different order. Uniform block sizes,
member offsets, matrix stride and array stride come from SPIR-V decorations and
types. Notably, `RandomValue[5]` starts at byte 16 with stride 16, and `TimeLapse`
is at byte 96 in a 112-byte block. The transform block is 64 bytes, ghost block
64 bytes, and fade block 16 bytes. The manifest records source and SPIR-V hashes.
Reports and generated GLSL/SPIR-V/GCN are under `artifacts/vulkan/twp-shaders/`.
Generated shader sources retain ScummVM's licensing; the original source files
are neither modified nor copied into the tracked diagnostic fixtures.

The first real grayscale shader aborted in ACO spilling. The standalone
compiler's GFX7 target incorrectly advertised only 12 scalar registers and 64
vector registers per wave, with 120 physical scalar registers per SIMD. Its
GCN budgets now match the vendored Mesa `ac_fill_compiler_info`: GFX7 has 512
physical SGPRs per SIMD, allocations of 8..104 in units of 8, and 256 VGPRs.
The GFX8/Tonga target follows Mesa's fixed 96-SGPR allocation for SGPRInitBug,
800 physical SGPRs and 256 VGPRs. Both targets have four SIMDs per CU. PS5
settings are unchanged. All six actual shaders compile and pass binary checks
for both base PS4 and Neo on the host. The correction also cross-compiles in
the native PS4 compiler library; it is persisted in `psbc-build.patch`.

This is an offline shader bridge, not runtime `glCompileShader`/`glLinkProgram`,
GL uniform locations, a complete reflection API, or rendered TWP effects.
Only the declarations present in the six sources are supported. It preserves
shader body math; GL-to-Vulkan clip/depth conventions still belong to the future
GL-facing backend. There is no SDL GL context and no game-start claim. Next:
feed these exact SPIR-V shaders and reflected uniform layouts to a native draw
probe, compare effect pixels, then connect GL state and context handling.

Validation for this checkpoint: existing compiler acceptance/rejection tests
pass, as do all six native Vulkan regressions in shadPS4 (basic, upload,
composition, sprites, readback and rectangle clear). See
`artifacts/vulkan/twp-regression-*.log` and `twp-compiler-regression.log`.
Those GPU tests exercise existing diagnostic shaders with the corrected compiler;
they do not render the six TWP effects yet. The prior runnable diagnostic is
restored after the tests. No package is replaced by this offline milestone;
stable ScummVM retains the SHA-256 recorded above. Physical hardware is untested.

## Native original TWP effect draws (2026-09-26)

The `--twp` diagnostic now feeds the six translated original SPIR-V modules to
`vkCreateShaderModule` and compiles five pipelines inside the PS4 executable.
It draws the original texture, grayscale, ghost, sepia and fade fragment bodies
using TWP's vertex shader and interleaved attributes. Two independent 2x2 RGBA
textures, ten descriptor sets and two rows of different uniform/UV values test
resource selection. Uniform payloads are packed by the reflected member offsets,
array strides and types from the previous checkpoint, not C struct assumptions.

The screenshot oracle independently computes RGB for each tile, including
vertex tint, grayscale luminance, ghost shadow/midtone/highlight weights, sepia
flicker and both fade branches (ordinary second texture versus sepia second
texture), followed by sRGB output encoding. All 90 samples pass within 2/255.
The eight-frame draw loop and destruction of pipelines, buffers, descriptors,
textures, device and instance complete successfully in shadPS4.

The first native attempt exposed missing `S_OR_SAVEEXEC_B64` support in the
pinned shadPS4 recompiler. The vendored ACO WQM transition lowering now emits
`S_MOV_B64` to save EXEC followed by `S_OR_B64` for GFX7/GFX8, but only when the
saved mask and source mask do not overlap. EXEC, saved-mask and SCC semantics
are preserved; overlapping registers retain the original instruction. Other
OR_SAVEEXEC emission paths are unchanged. This is a compiler compatibility
change for the emulator, not a claim that PS4 hardware lacks that instruction.
No shader effect math is changed or replaced with constants.

Reproduce:

```sh
scripts/build-vulkan-probe.sh --twp
xvfb-run -a -s '-screen 0 1280x720x24' python3 scripts/run-opengnm-probe.py --vulkan --twp
```

Package: `dist/ScummVM-PS4-Vulkan-TWP.pkg` (SCVM00009, separate from ScummVM).
Reports: `artifacts/vulkan/twp/`; translated shader provenance and binaries:
`artifacts/vulkan/twp-shaders/`. The generated fixture lives in
`build/vulkan-probe/`, including the reflected uniform byte payload.

Coverage remains deliberately bounded: the oracle checks RGB, not target alpha;
UVs are constant per tile; wobble/movement are zero; sepia scratches are disabled
by RandomValue[0]=0.8; both sampled UVs lie in the vignette's full-intensity area.
This validates actual shader execution with selected nonzero effect parameters,
not every animated/noise/derivative branch. It does not implement GL context,
GL state tracking, runtime GLSL translation or TWP startup. Physical PS4 remains
untested. Next integration gate is the GL-facing context/state adapter, with
broader animated-effect validation still needed alongside it.

TWP diagnostic package SHA-256:
`7d93d4509e55807e3a7a37febb051b8bd1b7ab65513e273b5ab02362731cfb4f`.
A complete `build-vulkan-probe.sh --twp` rebuild and subsequent GPU rerun pass
with the persisted compiler patch. Stable ScummVM's package hash is unchanged.
All six existing native Vulkan regressions (basic, upload, composition, sprites,
readback and rectangle clear), plus the host compiler acceptance/rejection and
TWP reflection checks, pass with the WQM change. Regression logs are
`artifacts/vulkan/twp-regression-*.log` and
`artifacts/vulkan/twp-effect-compiler-regression.log`. The TWP diagnostic
executable is restored after those serial tests.
