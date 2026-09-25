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

Beyond the resource checkpoint below, vertex buffers, compute, blending, depth,
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
It does not establish support for arbitrary Vulkan resources, optimal-image
uploads, render-to-texture, blending/depth or overlapping submissions. Physical
PS4 remains untested. Next: GPU buffer-to-image upload and image transitions,
then blending/render-target sampling and a feature audit against the OpenGL
translator before wiring it into ScummVM.

Final regression: the original Vulkan triangle still passes all 154 pixel
checks and lifecycle cleanup with this driver/compiler. The standalone compiler
resource fixtures pass as well. The runnable `eboot.bin` is restored to the
resource probe after the basic-triangle regression.
