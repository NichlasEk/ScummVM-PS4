# OpenGL next milestone: backend and shader capability

Source audit and isolated capability probe after package 1.17 (2026-09-25).
The probe is implemented; a working PS4 graphics port is not yet validated.

**Active direction:** the user selected the open GPU stack instead of Softpipe
or Piglet. The native GNM baseline is in [PS4-OPENGNM.md](PS4-OPENGNM.md); validated Vulkan
texture uploads, offscreen composition, indexed vertices, readback and partial
clears are now in [PS4-VULKAN.md](PS4-VULKAN.md). TWP shader translation and
reflection pass offline, and all five original fragment effects now render in
the native Vulkan diagnostic with bounded RGB checks. A first bounded
[GL state/draw frontend](PS4-GL-STATE.md) now exists, with SPIR-V program import
and streamed float vertex attributes plus 16/32-bit indexed draws. Named float/vector/mat4 uniforms now reach native vertex and fragment
shader blocks with immutable per-draw snapshots. Bounded uniform arrays (including TWP's float[5]) and RGBA8 textures with named
fragment sampler2D parameters now work as well. GL blending now supports separate RGB/alpha factors and equations with
retained pipeline variants. Managed color-only GL framebuffers now support GPU
render-to-texture sampling, target switches and retained storage across frames.
An explicit [runtime Vulkan GLSL compiler](PS4-GLSL.md) now works in the native
diagnostic. GL shader objects and cross-stage linking now drive that diagnostic. Legacy
source translation/attribute bindings, remaining GL state and SDL context
integration are still missing. The alternatives below preserve the earlier
investigation; they are not the current implementation priority.

## Capability probe checkpoint

Run `scripts/build-ps4-opengl-probe.sh` to build the standalone
`dist/ScummVM-PS4-OpenGL-Probe.pkg` (SCVM00005). It imports only libkernel;
Piglet is resolved dynamically so a missing runtime can be reported instead
of preventing boot. The target writes `/data/scummvm-opengl-probe.log`:
exit 0 means all tests passed, exit 2 means not ready. It has no software
fallback and does not modify the main ScummVM package.

The host EGL oracle passed source shader compilation/linking, textured VBO
rendering, FBO render-to-texture sampling, alpha blending, depth rejection,
pixel comparisons, and rejection of invalid GLSL. The actual host driver was
NVIDIA, not a PS4 driver; these results only validate the test itself.

The target SELF booted in an isolated shadPS4 profile and reported Piglet
absent at all four checked filesystem paths. **No target GL context or shader
test ran.** This preflight uses `stat` before module loading; it does not prove
that every firmware-specific loader route is unavailable. The guest exited 2;
the emulator wrapper needed the timeout to close. Evidence is in
`artifacts/opengl/host-oracle.log`, `ps4-probe-result.txt`, and
`ps4-probe-emulator.log`. Physical console execution remains untested.

Next gate: use an owned decrypted Piglet dump with its firmware provenance,
verify actual module loading/exports, then context and GLSL availability.
There is no established firmware requirement for this probe yet. Do not
assume a normal retail dump includes `libSceShaccVSH.sprx`: the
[ioQuake3 PS4 port](https://github.com/Mayo1970/Ioquake3-PS4) describes it as
devkit-only and uses precompiled shaders instead. We have not implemented
that shader route for ScummVM. SDL integration remains pending this gate.

Owned Thimbleweed Park installer data was unpacked without execution into
`artifacts/thimbelweedpark/extracted`. `ThimbleweedPark.ggpack1` matches the
local detector's GOG 1.0.958 entry: size 502661439 and first-5000-byte MD5
`6180145221d18e9e9caac6459e840579`. This confirms detection compatibility,
not game startup. The optional DLC installer has not been applied.

### Avoiding Shacc at runtime

The ioQuake3 reference was inspected at commit
`d4c7912af73c3b0eaa57195391ad76f21e79a910`. Its
`code/ps4/ps4_shaderbin.c` loads per-stage Piglet blobs with `glShaderBinary`;
its debug capture path calls `glPigletGetShaderBinarySCE` **after a successful
source compilation**. It therefore removes the compiler dependency for players,
but does not supply an independent offline compiler for new ScummVM shaders.
Its Quake shader blobs cannot substitute for TWP's shaders. No reference blobs
or runtime modules have been included in our package.

`python3 scripts/audit-twp-gles-shaders.py` exports the eight TWP shader pairs
(default, monochrome, ghost, sepia, fade, HUD, lighting and debug lighting)
with the actual local ScummVM GLES2 compatibility preambles. All eight pass
glslangValidator's GLSL ES 1.00 compilation/link validation. Sources, SHA-256
identities, validator version and results are recorded in
`artifacts/opengl/twp-shaders/manifest.json`. This is source-language validation,
not GPU resource-limit validation, a Piglet binary build, or rendering proof.
The launcher/common backend shaders are outside this inventory.

To ship without Shacc, the remaining work is to produce matching native shader
blobs (one-time compiler/capture access, or a separately verified open compiler
and Piglet-container route), load and link them on Piglet, and test pixels and
effects. Cache identity must cover stage and exact source/preamble, and missing
or rejected binaries must fail explicitly. Host GL program binaries and SPIR-V
must not be relabeled as Piglet shader binaries. A dump of Piglet is still
needed for emulator validation of this native path.

## Current boundary

### Alternatives that avoid Piglet as well (2026-09-25)

Recommended first experiment: port Mesa's **Softpipe** software GL path to
OpenOrbis, initially without LLVM/JIT, rendering into a CPU buffer presented
by the existing VideoOut/SDL route. Unlike TinyGL, this is a shader-capable
implementation. A headless EGL or an offscreen frontend must be selected for
the pinned Mesa version; do not assume an old OSMesa build recipe still works.
Target work includes the C/C++ runtime, threading/TLS, allocation and platform
glue, context/proc lookup and buffer presentation. Rendering cost, especially
TWP lighting/full-screen effects, is an explicit acceptance gate. No claim of
playable PS4 performance follows from a Linux pass.

A first **host-only** Softpipe test passed the existing probe's shaders,
VBO/textures, FBO sampling, alpha, depth and exact/toleranced pixel comparisons.
The report identifies `GL_RENDERER=softpipe`, Mesa 26.2.3-arch1.1, GLES 3.1.
Reproduce after building the host probe:

```sh
env __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
    EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=softpipe \
    build/ps4-opengl-probe/host-probe > artifacts/opengl/host-softpipe.log 2>&1
```

This explicitly selects Mesa: `LIBGL_ALWAYS_SOFTWARE` alone previously let
GLVND choose NVIDIA. LLVMpipe is a possible later performance route, but adds
LLVM and runtime code generation to the port. Sources:
[Mesa software drivers](https://docs.mesa3d.org/systems.html),
[LLVMpipe](https://docs.mesa3d.org/drivers/llvmpipe.html).

Longer-term GPU route: OpenGL -> Mesa Zink (or a separately audited GLES
translator) -> vulkan-ps4 -> OpenGNM, using opengnm-psbc for shaders. This
avoids Piglet/Shacc but still uses normal PS4 GNM/VideoOut firmware services.
The [OpenGNM architecture](https://github.com/PS4-OpenGNM/opengnm-stack/blob/main/docs/architecture.md)
describes the layers and compiler. It is not already an OpenGL implementation.
Local source inspection of vulkan-ps4 commit
`f4d940b723771b3fe637e8ac0704bb5d6ab19622` confirms a static OpenOrbis build
path, but also unsupported entrypoints and a nonfunctional-on-GPU shader stub
when libpsbc is missing. Do not use the README's broad API claims as proof of
Zink compatibility. [Current Zink requirements](https://docs.mesa3d.org/drivers/zink.html)
include many extensions beyond a nominal Vulkan 1.0 implementation; check the
exact selected Zink release's profile against the driver before integration.
Local OpenGNM/Vulkan builds and emulator pixel tests are now recorded in
[PS4-VULKAN.md](PS4-VULKAN.md); physical PS4 tests remain pending.

A dedicated TWP software renderer is another option, but rewrites its effects
and benefits one engine. For the user's broader ScummVM goal, a reusable Mesa
backend is the stronger initial experiment. First target milestone should be
the existing pixel probe in a separate PS4 package without Piglet imports,
then TWP rendering/performance, and only then standard-package integration.

- ScummVM deliberately configures `--opengl-mode=none` and disables both game
  OpenGL variants. Its PS4 output is SDL's software framebuffer. TinyGL already
  supplies software 3D for engines that explicitly support it (e.g. Myst III,
  Stark); it does not provide a generic GLSL implementation.
- `scripts/build-ps4-input.sh` starts from the SDK SDL archive and replaces only
  the joystick object. `llvm-nm build/ps4-input/libSDL2.a` contains no
  `PS4_GL_*` implementations. In `.tools/sdl-input-src/src/video/ps4/SDL_ps4video.c`,
  the GL callbacks are behind `PS4_VIDEO_GL`; setting ScummVM flags alone cannot
  create a working context.
- The installed OpenOrbis `samples/piglet` documents **OpenGL ES 2.0 / EGL 1.4**,
  not desktop OpenGL. Its `PigletApplication.cpp` dynamically loads
  `libScePigletv2VSH.sprx` and `libScePrecompiledShaders.sprx` from the console's
  common library directory. The SDK `.so` import libraries are not those runtime
  modules. The audited SDK/repo trees contained import libraries and headers,
  but not those runtime `.sprx` files.
- The sample's `createShaders()` uses `glShaderBinary` with precompiled WebKit
  PSSL shaders. That sample does **not** prove arbitrary ScummVM GLSL can be
  compiled. Runtime shader compiler availability must be measured, or an offline
  compilation/translation route must be supplied.

## First game target

**Thimbleweed Park** is the first candidate: a shader-based 2D renderer using
textures, vertex buffers, uniforms, framebuffer objects and effects. Its
`engines/twp/shaders.cpp` contains actual game shaders, including animated
effects; replacing them with a stock textured quad would not be sufficient.
ScummVM's common shader layer has GLES2 compatibility handling, but the full
engine call/shader set still needs an ES2 audit.

Penumbra/HPL1 also has immediate-mode calls (`glBegin`, etc.) in its OpenGL path
as well as shader dependencies, and currently lacks GIF. There is separate
TinyGL implementation code in the source; its completeness and integration
would need a distinct investigation before weakening the configure dependency.
The Watchmaker explicitly needs classic OpenGL, with immediate-mode rendering.
Neither should be declared enabled merely because an ES2 context works.

## Concrete next implementation

1. Build an isolated capability probe with no changes to the working launcher.
   Log module loading, EGL errors, context version, renderer/extensions and
   `GL_SHADER_COMPILER`. Compile a small vertex/fragment pair from source and
   record the compile/link logs. Test a textured quad, alpha blending, FBO
   rendering and pixel readback; compare actual pixels, not just return codes.
   If source compilation is unavailable, establish an offline shader path before
   proceeding. This gate must also run on physical PS4; emulator support can
   differ from the console runtime.
2. Implement SDL's PS4 GL context/proc lookup/swap lifecycle using the proven
   route, including software-to-GL transitions, resizing and cleanup. Preserve
   the working software renderer for existing games and launcher fallback.
3. Enable a separate GLES2 build and audit Thimbleweed Park's calls/shaders.
   Verify launcher/game transitions and then startup, effects, input, audio and
   save/load with owned game files before including it in the standard package.
4. Evaluate classic OpenGL compatibility separately for The Watchmaker and HPL1.
   A software Mesa-style route is another architectural option if native shader
   support is blocked, with its own porting and performance cost. It is not an
   existing library we can enable here today.

No GL flags, runtime modules or graphics changes were added to package 1.17.
MPEG-2 for QDEngine remains an independent, smaller codec port.
