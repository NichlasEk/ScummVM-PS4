# Launcher 2D startup state (2026-09-26)

The prepared ScummVM backend's `OpenGLGraphicsManager::notifyContextCreate`
(in `backends/graphics/opengl/opengl-graphics.cpp`) disables CULL_FACE,
DEPTH_TEST and DITHER, then sets PACK_ALIGNMENT to 4. These calls previously
raised INVALID_ENUM even though the native pipeline already draws without
culling, depth or dithering.

The bridge now accepts disabling those three capabilities, idempotently.
`glIsEnabled` and `glGetIntegerv` report false, matching actual rendering.
`glEnable` explicitly returns INVALID_OPERATION for those capabilities:
there is still no general implementation of their enabled behavior. Unknown
capabilities continue to return INVALID_ENUM; existing blend/scissor state and
first-error retention are unaffected. No state call acquires a frame.

This is a bounded-backend policy, not full desktop GL conformance. In particular,
desktop GL normally initializes DITHER enabled, whereas this backend keeps it
fixed off. GL_VERSION remains 0.0; no new version or extension is advertised.

PACK_ALIGNMENT now has independent per-context storage, defaults to 4, accepts
1/2/4/8 and supports integer queries. Invalid values leave it unchanged. It does
not affect UNPACK_ALIGNMENT or texture uploads. Pixel readback remains absent;
storing this setting alone does not implement screenshots or glReadPixels.

## Verification

`scripts/check-ps4-frame-owner.sh` runs the state suite with ASan/UBSan. New
checks cover idempotent disables, enabled-state rejection, isolation from
blend/scissor, sticky errors, alignment values/defaults/invalid inputs and
context isolation. Existing RGB conversion and renderer state tests also pass.

The SDL native fixture replays the launcher's actual 2D startup sequence before
frame acquisition, checks independent pack/unpack settings, restores alignment
and verifies unsupported enable rejection. The runner requires its startup
marker before accepting a successful diagnostic run. The existing independent
pixel oracle then checks that rendered results and lifecycle remain correct.

Reproduce:

```
scripts/check-ps4-frame-owner.sh
scripts/build-vulkan-probe.sh --sdlgl
xvfb-run -a -s '-screen 0 1280x720x24' python3 scripts/run-opengnm-probe.py --vulkan --sdlgl
```

The actual ScummVM application remains at its known GLAD/version gate. This
fixture executes the same state calls; it does not prove the launcher reaches
those calls, displays a menu or starts a game. Stable packaging is unchanged.

Validated in shadPS4: the required startup marker, all 2,553 framebuffer pixel
checks and native lifecycle cleanup pass. Evidence:
`artifacts/vulkan/sdlgl/triangle-result.txt` and
`artifacts/vulkan/sdlgl/pixel-test.log`.

Next integration work should target the actual loader/capability boundary in
`graphics/opengl/context.cpp`, using an explicit isolated subset path rather
than advertising desktop GL 2.x. Its current GLAD return of zero prevents these
otherwise validated startup calls from being reached by the real application.
