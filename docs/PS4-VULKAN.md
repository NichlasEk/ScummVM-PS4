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

Vulkan texture/UBO descriptors, vertex buffers, compute, blending, depth,
asynchronous semaphore submissions and error recovery are not validated here.
The upstream feature advertisement is not evidence of implementation. The
previous native GNM texture/UBO probes do not establish Vulkan resource support.

The next step is the Vulkan descriptor bridge: connect pipeline layout and
`vkCmdBindDescriptorSets` to the compiler's bounded resource-table ABI, with
GPU-visible descriptor storage and correct lifetime handling. Then run a Vulkan
texture/UBO pixel test before attempting the OpenGL translation layer. In
particular, the existing binding switch does not handle the compiler's indirect
resource-table usage, and the shared submission epilogue needs a lifetime review
before overlapping submissions are supported.

Final checkpoint: 154 pixel samples passed, all eight frames completed, and
`PASS Vulkan lifecycle` followed resource destruction. The VideoOut helper now
unregisters the returned attribute group once instead of treating image indices
as registration groups. The host compiler resource regressions and native GNM
triangle pixel test also passed. Stable ScummVM package SHA-256 remains
`c519b288b663d4f7b5db0e86b23a2a7031f1dacc97984edaf9b807f3738191e1`.
