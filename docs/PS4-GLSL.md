# Native GLSL compiler checkpoint

The standalone `--glsl` probe compiles shader **text on the PS4 target**, then
passes the generated SPIR-V through the existing native Vulkan/GCN compiler.
The resulting programs run the GL framebuffer diagnostic in shadPS4. There is
no host `glslc` invocation in this runtime path and no Piglet/Shacc dependency.
This does not yet accept ScummVM's legacy shader sources or establish game
startup or physical-console support.

## Reproduce

```sh
scripts/build-vulkan-probe.sh --glsl
xvfb-run -a -s '-screen 0 1280x720x24' \
  python3 scripts/run-opengnm-probe.py --vulkan --glsl
scripts/check-ps4-glslang.sh
```

`build-ps4-glslang.sh` pins Khronos glslang 14.3.0 at
`fa9c3deb49e035a8abcabe366f26aac010f6cbfb`. It builds static PS4 libraries with
OpenOrbis C/C++ headers and merges them into
`build/glslang-orbis/libglslang-ps4.a`. HLSL, SPIR-V optimization, RTTI,
exceptions, standalone tools and external dependencies are disabled. The
aggregate license and the C interface's separate attribution are packaged as
`glslang-LICENSE.txt`. Build evidence is in `artifacts/glslang/`.

OpenOrbis lacks `__cxa_thread_atexit_impl`. The small maintained
`patches/opengnm/glslang-orbis.patch` makes the PS4 compiler allocator explicitly
single-threaded and releases its default pool after glslang finalization. Host
builds retain upstream thread-local behavior. The patch also supplies the
missing `<cstdint>` include needed by modern host compilers. This port must not
be used concurrently from multiple compiler threads.

## Explicit API and boundaries

`compat/ps4gl/ps4gl-glsl.h` exposes initialize, compile and finalize operations.
The owner initializes once, compiles vertex/fragment Vulkan GLSL with explicit
locations and descriptor bindings, and finalizes after compilation is finished.
The wrapper returns caller-owned SPIR-V allocation/size and bounded diagnostic
text; failure clears both output fields. Source length is capped at 256 KiB,
output at 1 MiB. These are interface limits, not a compiler heap/time budget.
File includes are explicitly rejected by callbacks, including existing files.
The target is Vulkan 1.0 / SPIR-V 1.0; generated code still passes the existing
bounded native program-interface guard. Arbitrary GLSL support in glslang does
not imply arbitrary native shader support.

The diagnostic embeds the GLSL text for a procedural vertex shader, uniform
color fragment shader and sampled-texture fragment shader. Two programs are
created from runtime output, then all eight frames use them for framebuffer
switches, UNORM/SRGB rendering, blending, persistent textures and command-buffer
resubmission. The existing independent oracle checks 2,553 pixels. The runner
also requires three source-compilation rejections, nonzero imported programs,
error-free frames, storage collection and compiler/native lifecycle cleanup.
Precompiled SPIR-V remains in the shared diagnostic source only for the existing
malformed-interface rejection checks; it is not used for these rendered programs.

Host tests cover successful output (also checked with `spirv-val`), invalid
syntax, undefined names, wrong stage, argument/source bounds, truncated logs,
include denial and initialize/finalize/reinitialize. ASan/UBSan instrument the
wrapper and host test; the upstream compiler itself is a Release build.

## Next integration gate

The current ScummVM source under `.tools/scummvm-src` uses a GL-facing
shader-object and linked-program layer (implemented in the checkpoint below). `graphics/opengl/shader.cpp` supplies
source as multiple strings, checks compile/link status and logs, binds attribute
locations before linking, and detaches/deletes shaders after linking. Its
compatibility path prepends GLSL 110/120 or ES 100 macros. These inputs cannot
be passed unchanged into this Vulkan GLSL compiler wrapper.

The next step is source assembly plus a bounded legacy-interface translation:
attributes/varyings need shared locations, default uniforms need the existing
stage-specific blocks, samplers need bindings, and vertex position needs the
established clip-Y conversion. The existing offline TWP translator is useful
reference evidence, not a runtime implementation. Shared varying/type checks
must happen across linked stages before native program creation.

After that, wire GL procedure lookup and SDL context/frame ownership. The game
API audit still includes glReadPixels, non-sampler integer uniforms, constant
vertex attributes and draw-mode/texture-lifetime gaps beyond this milestone.
Do not advertise a complete GL version based on the diagnostic subset.

Package: `dist/ScummVM-PS4-GLSL.pkg`.
SHA-256: `8a53f001e2c236557818658783f25153d75c7af866dc19d1d7e42bd0c01ec849`.
Native reports: `artifacts/vulkan/glsl/`; host tests:
`artifacts/glslang/host-test.log`. The previous precompiled-SPIR-V framebuffer
path also passes (`artifacts/vulkan/glsl-regression-glfbo.log`). The final runtime
GLSL diagnostic remains in the isolated probe build after regression testing.
The stable ScummVM 1.17 package retains SHA-256
`c519b288b663d4f7b5db0e86b23a2a7031f1dacc97984edaf9b807f3738191e1`.

## Shader-object and linked-program checkpoint (2026-09-26)

The native `--glsl` diagnostic now creates its programs through glCreateShader,
glShaderSource, glCompileShader, glCreateProgram, glAttachShader and glLinkProgram.
Status, source/log queries, detach and deferred deletion are implemented too.
`ps4glConfigureShaderCompiler` connects the explicit compiler/linker callbacks to
an initialized native device and compatible render pass. This replaces direct
SPIR-V import in the rendering test, while preserving that older import API.

Source fragments are copied and concatenated with optional explicit lengths;
negative lengths use NUL-terminated strings. Caller mutation does not alter the
stored source. Replacement source does not change the last compilation result
until glCompileShader is called. Successful compilation retains a separate
source snapshot; linking parses those compiled snapshots together with glslang
and emits both stages from the linked program. It does not link a newer source
that the application has not compiled. The second parse at link time is
intentional; retaining compiler ASTs/optimized shader caches is a later concern.

The post-link interface check requires every fragment user input to have a
matching vertex output at the same location and float/vector width. Missing
producers are explicitly rejected: Vulkan glslang linking alone allowed that
case. The current check accepts float scalars/vec2/vec3/vec4 at locations 0–15,
skips builtins and rejects unsupported user interface shapes. Unused vertex
outputs remain legal. This is an interface guard over generated SPIR-V, not a
general SPIR-V validator. Native descriptor/uniform/input limits remain a second
check; their rejection becomes a failed GL link with a program info log.

Shader objects have a separate name range and reference counts. A deleted
shader attached to one or more programs stays queryable until the last reference
is detached or its program is destroyed. Linked GPU executables remain valid
after shader source replacement, detach and deletion. A deleted current program
is retained until unbound, and native executable resources continue to follow
the established fence/command-discard teardown contract. Existing sticky GL
errors survive successful linking. Compile/link diagnostics themselves are
reported through status and logs instead of setting a generic GL error.

Bounds and remaining restrictions:

- 32 live shader objects, 256 KiB source per object plus its compiled snapshot;
  names increase monotonically. Program slots retain the existing 16-name
  lifetime limit, including reserved/deleted unlinked programs, until teardown.
- One vertex and one fragment shader per program. Multiple compilation units of
  the same stage are rejected. Embedded NUL in an explicitly sized source
  fragment is rejected atomically; this is a text-only subset.
- Failed initial links can be retried. Relinking a successfully linked native
  executable is explicitly rejected with INVALID_OPERATION and preserves the
  executable/status. Supporting GL relink semantics requires retained executable
  generations and uniform-location handling. Linking during command recording
  is also rejected.
- Sources are still **explicit Vulkan GLSL**, with locations/bindings supplied.
  glBindAttribLocation/glGetAttribLocation and the legacy source translation are
  not implemented by this milestone. No complete GL version is advertised.

The native probe checks split source strings, compiled-vs-replacement source,
shader deletion before link and final release after detach, varying mismatch,
and native integer-uniform rejection after successful GLSL compilation. Both
rendered programs then pass the same 2,553-pixel framebuffer oracle, eight
frames, resubmission and lifecycle cleanup. Host ASan/UBSan tests cover copied
source, explicit lengths, bounded queries, retry/error status, sticky errors,
shared/deferred deletion and shader exhaustion. The compiler host suite also
checks missing producers and incompatible varyings with real glslang.

Next: bounded legacy GLSL source translation and attribute bindings, then SDL
context/frame ownership. The earlier engine API audit still applies; this
checkpoint is not a real-game boot result.

Current shader-object package: `dist/ScummVM-PS4-GLSL.pkg`, SHA-256
`614a4e46fcca4c00dff172dccaee9bc31a5f4f3d02488296c0f7125691a08300`.
Reports remain in `artifacts/vulkan/glsl/`, with host object-state checks in
`artifacts/glslang/shader-object-host.log`. Framebuffer, texture, blend and
clear/readback regressions are in `artifacts/vulkan/globjects-regression-*.log`.
