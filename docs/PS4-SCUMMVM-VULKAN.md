# Experimental ScummVM application integration (2026-09-26)

This build links the real ScummVM SDL backend and GLAD against our native Vulkan
bridge. It is a startup diagnostic, not yet a playable launcher or game build.
The GL bridge still reports experimental version 0.0; raising the version to
bypass missing features is intentionally not part of this integration.

## Isolation and reproduction

```
scripts/build-scummvm-vulkan.sh
xvfb-run -a -s '-screen 0 1280x720x24' scripts/run-scummvm-vulkan.sh
```

The native dependency archives must already exist from the Vulkan probe build.
This entrypoint uses separate paths:

- Source: `.tools/scummvm-vulkan-src`, pinned revision plus normal PS4 patches
  and a reversible, checked `.vulkan-overlay.patch`.
- Build/stage: `build/scummvm-vulkan` and `build/scummvm-vulkan-pkg`.
- App: `SCVM00010`, package `dist/ScummVM-PS4-Vulkan-Experimental.pkg`.
- Guest settings, saves, logs: `/data/scummvm-vulkan/`.
- Emulator profile: `.tools/xdg-scummvm-vulkan`; the runner copies only emulator
  graphics options on first use, not another profile's game settings or saves.
- Reports: `artifacts/scummvm-vulkan/`.

The runner uses X11, so `xvfb-run` actually isolates its window even in a Wayland
session. Default runtime limit is 45 seconds (`EMULATOR_TIMEOUT` overrides it).
The application always requests the OpenGL launcher with debug level 5, no game.
The ordinary build keeps OpenGL disabled and its own SDL archive, title,
settings and package. Extra native dependencies are allowed only for this
experimental package. Third-party licenses are included.

## Application wiring

`ps4gl-main.c` wraps `main` at link time. It creates the display/frame owners and
installs the SDL owner before entering ScummVM's real main. An exit callback
releases parents only after SDL has released its borrowers. If a context/window
or GPU operation remains outstanding, cleanup reports the retained resources
instead of destroying parents underneath them.

ScummVM's launcher requests a resizable desktop window and GL 1.4/depth16 by
default. The isolated overlay instead requests fixed 1280x720, GL 2.0, no depth
and FIFO synchronization. This is a diagnostic color-only request, not a claim
of complete GL 2.0 support. The overlay also preserves `SDL_WINDOW_OPENGL` in the existing PS4 fixed-window
branch, which previously dropped all flags for its software renderer. The
regular source tree is not modified.

The experiment enables `USE_OPENGL`, `USE_GLAD`, `USE_OPENGL_SHADERS` and the TWP
engine; classic OpenGL game rendering remains disabled. Compiler configuration
probes run without the `main` wrapper; it is added only for the final app link.
The SDL config supplies the full native dependency chain so its own configure
link test exercises the real SDL archive.

## PSBC / libc collision

Real ScummVM linking exposed duplicate `call_once` and `thrd_create` definitions
between PSBC's bundled C11 shim and the OpenOrbis libc. Changing library order
was insufficient. The build copies the PSBC archive and uses `llvm-objcopy` to
prefix all 25 exported shim functions and their internal references with
`ps4gl_psbc_`. This preserves PSBC's own thread implementation while avoiding
cross-library ABI mixing. The original PSBC archive and SDK remain untouched.
The symbol names are discovered from its `threads_posix.orbis.o` and restricted
to the known C11 families before rewriting. No duplicate-symbol suppression is
used. `artifacts/scummvm-vulkan/psbc-symbols.txt` records the audited copy.

## Observed startup

The first real application run reached ScummVM's main, initialized the PS4 SDL
video, joystick and audio backends, then rejected the desktop window request.
`bootstrap.log` recorded both application Vulkan bootstrap and cleanup. This
motivated the fixed-window/color-only overlay above.

ScummVM may call `exit` rather than return from main. The pinned shadPS4 build
reports guest exit through its `process.cpp` trap, so a host exit status of 133
alone is not a GPU crash diagnosis. Use the guest logs, the actual ScummVM error
and the bootstrap cleanup marker together. A blank window is not evidence of a
working renderer.

Follow-up requirements remain in [PS4-SCUMMVM-GL-STARTUP.md](PS4-SCUMMVM-GL-STARTUP.md):
capability negotiation, primitive/state/texture support, context recreation and
input, then real launcher/game/save-load validation. Physical PS4 unverified.

### Earlier GLAD checkpoint

After preserving the GL window flag, the real application creates its GL context
and reaches ScummVM's GLAD initialization. It stops with `Couldn't initialize
OpenGL!`, consistent with the retained `0.0 PS4GL experimental subset` version.
The window-mode rejection is gone, and `PASS application Vulkan cleanup` is
present after this fatal exit. The launcher has **not rendered**.

`USE_IMGUI` is also auto-enabled by this ScummVM configuration. Its additional
loader probes missing modern entrypoints (VAOs, sampler objects, base-vertex,
readback and related queries) before ScummVM's own GLAD gate. Those probe names
are not automatically requirements for the 2D launcher; decide whether to
exclude the optional ImGui debugger before expanding scope to that renderer.

Evidence: `artifacts/scummvm-vulkan/startup-evidence.json`, `startup-bootstrap.log`,
`startup-scummvm.log`, `startup-driver.log` and timestamped emulator run logs.
Final package SHA-256:
`7c6ddab42cba3b646aee78ff29fa4f27f874d3bff9a5040a09bda0d3ce61c771`.
The stable 1.17 package retains SHA-256
`c519b288b663d4f7b5db0e86b23a2a7031f1dacc97984edaf9b807f3738191e1`.

The private namespaced PSBC archive was also substituted into the native SDL/GL
pixel diagnostic. Runtime shader compilation, all 2,553 framebuffer pixel
checks, frame replay and cleanup passed. Evidence:
`artifacts/scummvm-vulkan/private-psbc-render.log`. The diagnostic driver's
previous ELF/SELF/eboot files were restored afterward. Packaging rejection of a
stable title/output under the experimental flag was checked before staging.

## Explicit launcher subset (2026-09-26)

The isolated source overlay injects `compat/ps4gl/scummvm-context.inc` into
`Context::initialize`, after its normal reset. It assigns 76 explicitly required
entrypoints to the real GLAD typed function pointers. Every lookup is checked;
wrong context type, renderer identity, missing function or invalid texture limit
is fatal. It neither changes GL_VERSION nor enables GLAD version/extension flags.

Only shader-based launcher drawing, NPOT sampling and edge clamping are enabled.
Engine shaders, libretro, GPU palette conversion, FBO capability, packed-pixel
capability, extended unpack state and multisampling remain disabled. Available
bridge functions are not equivalent to unrestricted engine capabilities. The
experimental configure path now disables the optional ImGui debugger, whose
separate modern loader is outside this launcher subset. Stable builds retain
their previous configuration.

`scripts/check-scummvm-subset.sh` checks the manifest against actual resolver
exports and compiles the injected code against ScummVM's actual GLAD header.
ASan/UBSan tests cover successful loading and rejection of missing functions,
wrong renderer/context and zero texture limit, plus absent version and engine
promotion. The real PS4 application builds and runs with this loader.

### Earlier result and clear blocker

Real ScummVM now passes the previous GLAD fatal error, compiles its launcher
shaders into native pipelines, loads fonts/UI data, chooses a 1024x576 overlay
within the advertised texture limit, and issues launcher clear/draw calls.
However, **zero frames have been presented**. No visible or interactive launcher
success is claimed.

The first present fails while ending the command buffer with
`VK_ERROR_FEATURE_NOT_PRESENT` (-8). Temporary driver breadcrumbs traced it to
`vk_ps4_CmdClearAttachments`'s unsupported branch. Inspection shows why:

- The meta-clear requires `img->memory` and RGBA8_UNORM storage.
- Swapchain images deliberately have `memory == NULL`; VideoOut owns their
  storage. The display owner requests RGBA8_SRGB.
- Offscreen FBO pixel tests do not exercise this default-framebuffer clear.

The poisoned frame owner then rejects subsequent clear/draw attempts. Its new
application bootstrap log records begin/present failures, frame count and quiesce
state, distinguishing this from GLAD or shader compilation failure. Runs were
terminated by the diagnostic timeout; graceful application cleanup was not
verified at this checkpoint. Two earlier texture-filter calls also report
INVALID_OPERATION and should be revisited after first-frame presentation works.

Next: implement default/swapchain attachment clears with correct borrowed
VideoOut storage bounds and SRGB conversion, and add native clear/preservation
pixel tests. Do not bypass the clear or suppress the poisoned command-buffer
error. Then rerun the actual launcher and add real input/clean exit validation.

Evidence under `artifacts/scummvm-vulkan/subset-loader/`: host test log, real
ScummVM/bootstrap logs, the temporary driver trace and machine-readable result.
Temporary driver instrumentation was removed and the driver rebuilt before
producing the final experimental package. This milestone changes no native
driver implementation and makes no physical PS4 or game-start claim.

Current experimental package SHA-256:
`2a994e0dfc15aa1f0f4b43c2e081ab96be2b5e1c7d28684cc694aa9bd1c8c059`.
Stable package SHA-256 remains
`c519b288b663d4f7b5db0e86b23a2a7031f1dacc97984edaf9b807f3738191e1`.

## First visible Vulkan launcher (2026-09-26)

The VideoOut attachment-clear fix now allows the real application to present
its startup frames and display the actual ScummVM launcher. The settled X-window
capture has been visually inspected. See [PS4-VIDEOOUT-CLEAR.md](PS4-VIDEOOUT-CLEAR.md)
for the storage/sRGB/cache fix, pixel tests, screenshot procedure and limitations.
Input, game startup and graceful application exit are not yet verified.
Current experimental package SHA-256:
`4d9c265a1a3edc70aa9b687bd97a0ce724d4a6a95144500b4fe6a8defbe22f67`.

## Emulator mouse input (2026-09-26)

The user confirmed desktop mouse control in the visible experimental launcher.
The runner now seeds title SCVM00010 with the existing stable title's bindings
and disables unified emulator bindings in the isolated experimental profile.
F7 toggles mouse capture; mouse movement maps to the left stick, with left/right
buttons mapped to Cross/Circle. WASD and physical stick bindings remain intact.
No guest input backend or physical PS4 behavior was changed or newly validated.
Existing per-title custom bindings are retained.

Thimbleweed Park GOG/1.0.958 data matched the detection table (ggpack1 size
502661439, first-5000-byte MD5 6180145221d18e9e9caac6459e840579) and was copied
into the local experimental data directory under games/thimbleweedpark. A twp
entry was added to the experimental launcher only. Game startup remains
unverified; the explicit GL subset still does not advertise engine shaders.

### Mouse tuning and first game-start attempt

The initial mouse mapping could produce barely visible movement through the
PS4 cubic stick response. A live trial at `0.65, 6.0, 0.125` was too sensitive;
the current experimental default is `0.40, 3.0, 0.05`. It was reloaded with F8
without restarting. Existing custom profiles are not overwritten. Physical
stick mappings and guest sensitivity remain unchanged. Final feel still needs
user confirmation; this is not a proven fix for intermittent capture behavior.

Coordinate diagnostics observed movement across the 1024x576 virtual overlay,
and the user reached Start. Thimbleweed Park then reported that OpenGL with
shaders is unsupported. The next gate is the engine shader capability and its
actual required GL operations, not game detection. Temporary coordinate logging
was removed from source and the package rebuilt; the live diagnostic instance
retains it until the next restart.

## Screen-sized framebuffer checkpoint

See [PS4-LARGE-FRAMEBUFFERS.md](PS4-LARGE-FRAMEBUFFERS.md) for the 1280x720
offscreen test, command capacity/submission fix, measured memory cost and
remaining engine shader discovery gate. The visible test window is not restarted
by this build. Shader capabilities remain disabled for game startup.
