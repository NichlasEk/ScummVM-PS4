# VideoOut attachment clears and first launcher image (2026-09-26)

The real ScummVM launcher now displays through the native OpenGNM Vulkan bridge
in shadPS4. The inspected capture shows its logo, empty game list, search field,
Add Game, options and Quit controls. This validates menu rendering, not input,
game startup or physical PS4 operation.

## Driver correction

`VkPs4Image` now records the borrowed VideoOut buffer address and allocation size
when swapchain images are constructed. `CmdClearAttachments` accepts that storage
without inventing an owning VkDeviceMemory object. Normal images retain their
existing memory-offset bounds checks. Both paths validate render-target dimensions,
format, sample/layer count, clear rectangle and calculated byte size before copying.
VideoOut still owns and releases its buffers.

RGBA8_SRGB clears convert linear RGB to sRGB bytes; alpha stays linear. UNORM
behavior is preserved. Before copying the render target into scratch storage, an
explicit color-target cache synchronization complements the existing pipeline
barrier. Without it, a partial clear copied stale pixels and erased earlier draws.
The copy/fill/copy path preserves all pixels outside the requested rectangles.
The change is included in the reproducible pinned `vulkan-runtime.patch`.

## Validation and limitations

The SDL native fixture exercises full default-buffer clear, post-draw partial
clears with fractional and low-linear sRGB values, unaffected surrounding pixels,
existing offscreen UNORM clears, frame replay and resource release. The pixel
oracle checks 2,553 samples. One development run passed pixels but hit an
intermittent shadPS4 GPU-thread access violation during display destruction;
a subsequent run passed both pixels and full cleanup. This emulator teardown
observation is not dismissed as a proven driver fix.

The real application records two successful presented frames instead of the
previous command-buffer failure (-8). A settled X-window capture shows the menu.
The emulator's F12 capture initially saved the first black frame, so it must not
be used alone to judge the idle launcher's visible state. Two early texture-filter
warnings remain in the application log. Application runs end by diagnostic
timeout; input and graceful application shutdown remain unverified.

Reproduce the diagnostic:

```
scripts/build-vulkan-probe.sh --sdlgl
xvfb-run -a -s '-screen 0 1280x720x24' python3 scripts/run-opengnm-probe.py --vulkan --sdlgl
```

Reproduce the real application from source:

```
scripts/build-scummvm-vulkan.sh
xvfb-run -a -s '-screen 0 1280x720x24' python3 scripts/capture-scummvm-vulkan.py
```

The capture helper requires ImageMagick's `import`, waits for two successful
presents, captures the X window and records that termination is by timeout.
Evidence: `artifacts/vulkan/sdlgl/` and
`artifacts/scummvm-vulkan/launcher-window.png`, `launcher-bootstrap.log`,
`launcher-capture.json`.

Next: connect input to the experimental SDL video backend, verify real menu
interaction and a clean exit, investigate the intermittent emulator teardown
failure, then enable/test game capabilities incrementally.
