# Experimental SDL GL adapter checkpoint (2026-09-26)

The frame owner and numeric queries are implemented below. For the subsequent
ScummVM startup audit and constant vertex attributes, see
[PS4-SCUMMVM-GL-STARTUP.md](PS4-SCUMMVM-GL-STARTUP.md).

A separate SDL archive now routes real SDL video/GL entrypoints to the bounded
OpenGNM frontend. This is a diagnostic integration checkpoint, not a complete
SDL graphics backend for ScummVM or a real-game boot result.

`build-ps4-sdlgl.sh` starts with the input-patched SDL archive and replaces
`SDL_ps4video.c.o` plus the generic `SDL_video.c.o`. The SDK build disables GL
attribute setters at compile time; the isolated generic video object is rebuilt
with `SDL_VIDEO_OPENGL=1`, enabling the actual attribute API instead of its stubs. Internal headers come from the pinned SDL-PS4 revision
`7774cf3e7d04448c44d569c50d1f9a4532a2efe7`, matching the input build workflow.
The output is `build/ps4-sdlgl/libSDL2.a`; neither the SDK SDL archive nor the
normal input-patched archive is overwritten. The new package includes SDL's
license. The bootstrap never opens its own VideoOut handle or allocates SDL's
software framebuffers: native Vulkan continues to own the display.

## Ownership contract

Before SDL video initialization, the application supplies a `PS4GLSDLOwner` via
`ps4glSDLSetOwner`. It contains borrowed physical-device/device/render-pass
handles, fixed drawable dimensions, presentation and quiesce callbacks, and
caller-owned callback data. All remain alive until SDL video is shut down.
Owner replacement while video, a window or a context is live is rejected.

SDL creates and owns a heap-allocated PS4GL context and the single-threaded
runtime GLSL compiler lifetime. `SDL_GL_MakeCurrent` updates the frontend's
current context, including detach/reattach. `SDL_GL_GetProcAddress` resolves the
existing bounded GL functions; unknown names return NULL and set an SDL error.
No GL version or additional unsupported GL procedures are advertised.

This slice deliberately **borrows the Vulkan frame owner**. That owner still
acquires images, begins/ends command recording and managed GL frames, submits
work and waits on fences. Only then may `SDL_GL_SwapWindow` call its presentation
callback. The callback's failure becomes an SDL error without pretending that a
frame was presented. The probe clears/checks SDL errors because SDL2's public
swap API returns void.

Context deletion requires ended recording. It calls the owner's quiesce
callback, which must wait for GPU completion **and discard every command buffer
that retains references to this context's resources**. Waiting alone is not
sufficient. After success, snapshots, FBOs, textures, buffers, native programs
and compiler state are released. On quiesce failure the context/resources are
retained for retry; retry deletion before destroying the SDL window/video owner.
The SDL current context is detached by SDL's public deletion function even when
the backend must retain the resource object, so reattach explicitly before use.

The current constraints are one window, one unshared context, one calling
thread, fixed dimensions, no resize/fullscreen mode switching and FIFO interval
1. GL context requests are limited to 2.0/2.1 without a profile/flags, depth,
stencil or MSAA; this accepts a diagnostic request, not a claim of complete GL
2.x conformance. SDL capability queries needed by the real renderer, frame-owner
extraction/lazy recording, context recreation/resize policy, and event/input
integration remain separate work. The event pump is intentionally empty here.

## Reproduction and evidence

```sh
scripts/build-vulkan-probe.sh --sdlgl
xvfb-run -a -s '-screen 0 1280x720x24' \
  python3 scripts/run-opengnm-probe.py --vulkan --sdlgl
```

The native test uses SDL_Init, CreateWindow, GL_CreateContext, GetCurrentContext,
GetCurrentWindow, GetDrawableSize, GetProcAddress, MakeCurrent, swap interval,
SwapWindow, DeleteContext, DestroyWindow and Quit. It rejects an unsupported
depth request, a second context, unknown procedure, invalid context and changing
the owner while live. Injected presentation failure must not advance the frame
count; injected quiesce failure must retain a context that can be reattached and
subsequently deleted successfully.

The framebuffer scene still uses original TWP shaders, signed integer uniforms,
GPU render-to-texture, blend, retained storage and command-buffer resubmission.
Eight SDL swaps and 2,553 pixel checks pass, followed by context/native cleanup.
Reports: `artifacts/vulkan/sdlgl/`. Non-SDL runtime GLSL and float/matrix-array
regressions: `artifacts/vulkan/sdlgl-regression-*.log`.

Package: `dist/ScummVM-PS4-SDL-GL.pkg`.
SHA-256: `1b832c601c138bd982ad5a35683cb2697579ee721571d4f780ac9a1f44197e2d`.
Stable ScummVM 1.17 retains SHA-256
`c519b288b663d4f7b5db0e86b23a2a7031f1dacc97984edaf9b807f3738191e1`.
Physical PS4 execution and actual TWP gameplay through this backend remain
unverified. Next: extract the reusable Vulkan frame owner and implement the
renderer capability queries before attempting integration with ScummVM.


## Reusable frame owner and numeric queries (2026-09-26)

`compat/ps4gl/ps4gl-frame.c` now owns the frame's command pool, command buffer,
fence and lifecycle. Device, queue, two-image swapchain, compatible render pass
and framebuffer attachments are borrowed from the application bootstrap. The
caller zero-initializes the owner, calls Init once, and destroys it before its
parents. Initialization of a live owner is rejected.

An optional `PS4GLSDLOwner.begin` callback installs lazy acquisition on the GL
context. The first color clear or draw acquires an image, waits for acquisition,
starts recording and attaches the managed GL frame. Shader creation/linking and
ordinary state setters before the first draw do not start a frame. SDL swap then
ends the managed pass, transitions the image, submits, waits, presents, resets
the completed recording and only then releases its vertex/uniform/texture
snapshots. Empty swaps leave the displayed image alone and acquire nothing.
`ps4glFrameSubmit` also supports replay of a completed recording before present;
the diagnostic checks that retained snapshot contract explicitly.

Vulkan errors poison the owner rather than silently starting another frame.
Quiesce waits for the device, discards recorded references and releases retained
snapshots even when SDL has detached the GL context. It can discard an unfinished
frame during context deletion. Because that abort leaves an acquired image
unpresented, the owner stays poisoned: recreate the parent swapchain before
rendering again. A wait failure retains the owner and resources for a later
cleanup retry. This is deliberately serialized, single-threaded rendering;
multiple frames in flight, resize and swapchain recovery are not implemented.

The SDL diagnostic now renders its scene and calls SDL swap without directly
recording or submitting Vulkan work in the frame loop. Its application bootstrap
still constructs the borrowed Vulkan device/swapchain/pass/attachments; moving
that bootstrap into the actual ScummVM startup is still required.

Numeric GL queries report the actual RGBA8 color format, absent depth/stencil/
MSAA, default-vs-offscreen double buffering, texture limit 1024, eight attribute
locations and a conservative two fragment/combined texture image units. SDL's
real `SDL_GL_GetAttribute` consumes these via GL procedure lookup.
`glGetString` identifies the experimental renderer, reports no extensions and
uses **`0.0 PS4GL experimental subset`** as a deliberate non-version. SDL needs
that entrypoint even to choose how to query color sizes; reporting 2.x here would
make engine autodetection assume APIs we have not audited. GLSL is identified
as `1.10 bounded translator`. This does not yet pass ScummVM's renderer startup
version gate or claim full GL conformance.

Validation:

```sh
scripts/check-ps4-frame-owner.sh
scripts/build-vulkan-probe.sh --sdlgl
xvfb-run -a -s '-screen 0 1280x720x24' \
  python3 scripts/run-opengnm-probe.py --vulkan --sdlgl
```

Host ASan/UBSan tests verify acquisition, replay, reset-before-release ordering,
empty swaps, aborted recordings, poisoned-owner rejection, simulated GPU timeout
retention and cleanup retry. Native shadPS4 tests retain the 2,553 pixel oracle,
eight frame presents, replay and cleanup; they now additionally check numeric
SDL attributes, an empty swap and deletion with an unfinished frame. Reports
remain in `artifacts/vulkan/sdlgl/`; host results are `frame-owner-host.log` and
`frame-state-host.log` under `artifacts/vulkan/`. Non-SDL regressions are
`artifacts/vulkan/frameowner-regression-*.log`.

Frame-owner checkpoint package (superseded by the startup audit below):
`dist/ScummVM-PS4-SDL-GL.pkg`.
SHA-256: `8aa35a96d58d9a68ed270a2fdedbc1ed3dc44658201d0e5c2430a3cb4d79160b`.
The stable ScummVM package remains unchanged. The subsequent startup API audit
is in `PS4-SCUMMVM-GL-STARTUP.md`. Next: connect the Vulkan bootstrap and resolve
the identified negotiation/rendering gaps. Actual game and physical PS4
execution remain unverified.

## Constant-attribute continuation (2026-09-26)

The SDL archive and diagnostic were rebuilt with the new per-context constant
vertex attributes. This keeps the adapter's allocated context layout in sync
with the core. Current package: `dist/ScummVM-PS4-SDL-GL.pkg`.
SHA-256: `8fe91dee10347296d1274b6377a80270da0bccc986d9169458d541ca8e496e56`.
The separate `--glvbo` fixture tests constant values against independent pixel
expectations; the SDL fixture retains the existing FBO/lifecycle checks.

Startup requirements and the reproducible call inventory are documented in
[PS4-SCUMMVM-GL-STARTUP.md](PS4-SCUMMVM-GL-STARTUP.md).

## Display-owner continuation (2026-09-26)

The SDL fixture now starts and releases the native Vulkan display through
`PS4GLDisplayOwner`, including failure-safe partial initialization. All 2,553
pixel checks and existing lifecycle checks pass in shadPS4 with this bootstrap.
See [PS4-VULKAN-DISPLAY.md](PS4-VULKAN-DISPLAY.md) for ownership, tests and the
next application integration step.

Current package: `dist/ScummVM-PS4-SDL-GL.pkg`.
SHA-256: `3946eacca4a6883d4b7fb19fd1ee722893be137395dcac9eb29c7a5cc815e333`.

## Triangle-strip continuation (2026-09-26)

The diagnostic now renders four-corner surfaces through array, uint16-indexed
and uint32-indexed strips. The original 2,553-pixel oracle still passes. See
[PS4-GL-TRIANGLE-STRIPS.md](PS4-GL-TRIANGLE-STRIPS.md) for semantics and bounds.
Current package SHA-256:
`6f4552f148c20334e8764979241f79564aae7a71d18a9d84fc943073b19fd9a3`.
