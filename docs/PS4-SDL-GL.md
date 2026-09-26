# Experimental SDL GL adapter checkpoint (2026-09-26)

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
