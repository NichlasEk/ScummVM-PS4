# OpenGL next milestone: backend and shader capability

Source audit after package 1.17 (2026-09-25). This is a plan grounded in the
current local SDK/source, not an implemented or validated graphics port.

## Current boundary

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
