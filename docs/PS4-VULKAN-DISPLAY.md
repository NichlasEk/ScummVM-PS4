# Reusable native Vulkan display (2026-09-26)

`compat/ps4gl/ps4gl-display.c` now owns the Vulkan bootstrap previously embedded
in the SDL diagnostic fixture. The `--sdlgl` probe actually uses this owner for
its normal rendering and cleanup; other probe variants retain their original
bootstrap, including their different render-pass layout requirements.

The owner creates instance, physical-device selection, logical device, queue,
two-image swapchain, LOAD/STORE render pass, image views and framebuffers. It is
specific to our OpenGNM Vulkan implementation: no platform surface, queue family
zero, fixed 1280x720 RGBA8-sRGB images and FIFO presentation. It does not claim
portable Vulkan WSI negotiation, resize, depth/stencil, HDR or multisampling.

## Lifetime and error contract

Zero-initialize a `PS4GLDisplayOwner`, then call `ps4glDisplayInit`. Return zero
means success; otherwise `last_result` contains the Vulkan error (or
`VK_ERROR_INITIALIZATION_FAILED` for an invalid returned device/image list).
An already initialized owner is rejected without mutation. No rendering is
submitted during initialization. Every partial initialization failure unwinds
created resources in reverse order, keeping the error and permitting retry.

The owner must outlive every borrower. Application startup sequence:

1. Initialize display owner.
2. Initialize `PS4GLFrameOwner` using display device/queue/swap/pass/images/targets.
3. Install `PS4GLSDLOwner` with those same handles and frame callbacks before
   `SDL_Init(SDL_INIT_VIDEO)`.
4. Create the SDL GL window/context and render using the existing lazy frame path.

Teardown reverses the dependency order: successfully quiesce/delete the SDL GL
context, destroy the frame owner, quit SDL and clear its owner, release any
additional device resources, then destroy the display. Failed context or frame
cleanup must be retried before touching parents. The display module cannot
detect external borrowers; this is an explicit caller obligation.

`ps4glDisplayDestroy` waits for device idle. If that fails, **all handles remain
owned**, `ready` becomes false and `last_result` records the failure. Retry
destruction explicitly; do not render or reinitialize the retained object.
Successful destruction clears it. Repeated destruction of an empty owner is
safe. Swapchain images, queue and physical device are borrowed child handles,
not separately destroyed.

## Validation

```
scripts/check-ps4-display-owner.sh
scripts/build-vulkan-probe.sh --sdlgl
xvfb-run -a -s '-screen 0 1280x720x24' \
  python3 scripts/run-opengnm-probe.py --vulkan --sdlgl
```

Host ASan/UBSan checks inject failure at each of ten fallible bootstrap calls,
then successfully initialize and destroy again. Mocks verify reverse dependency
order, no leaked/double-freed handles, exact display/render-pass parameters,
invalid device/image results, repeated init/destroy, and device-idle timeout
retention followed by successful retry. Evidence:
`artifacts/vulkan/display-owner-host.log`.

The native runner now requires both `PASS reusable display owner startup` and
`PASS reusable display owner destruction`, in addition to the existing 2,553
pixel checks, eight frame presents, command replay, and failed-quiesce recovery.
The shadPS4 run passed all of these checks. Reports remain in
`artifacts/vulkan/sdlgl/`. Current diagnostic package:
`dist/ScummVM-PS4-SDL-GL.pkg`, SHA-256
`3946eacca4a6883d4b7fb19fd1ee722893be137395dcac9eb29c7a5cc815e333`.

This is the reusable bootstrap needed for a future experimental ScummVM build.
The fixture still creates extra test shaders and an unused legacy command pool;
these do not belong to the display owner. Actual ScummVM GL startup, context
negotiation, input and game execution remain separate integration gates. See
[PS4-SCUMMVM-GL-STARTUP.md](PS4-SCUMMVM-GL-STARTUP.md).
