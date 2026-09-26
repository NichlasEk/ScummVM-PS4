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


## Legacy GLSL and attribute-binding checkpoint (2026-09-26)

The separate `--gllegacy` diagnostic now renders with the original TWP vertex
and default fragment sources, including the original ScummVM compatibility
preambles. Sources are extracted from `.tools/scummvm-src` by
`prepare-legacy-glsl-probe.py`; their hashes go to `legacy-sources.json`. This
is runtime source compilation through GL shader objects, not an offline rewrite
of those bodies. The package includes the embedded sources and ScummVM notices
in `scummvm-shaders.txt`, alongside the existing glslang license.

`ps4glCompileLegacyGLSL` and `ps4glLinkLegacyGLSL` first ask glslang to preprocess
and validate the original GLSL. A bounded tokenizer then rewrites global
attribute/varying/uniform declarations into Vulkan GLSL. Attribute locations
honor pre-link `glBindAttribLocation`; remaining locations use free slots.
Varyings are matched by name across stages, including differently ordered
source declarations. Shared uniform types and array sizes must agree.
Non-sampler uniforms become per-stage std140 blocks at bindings 0/1; fragment
sampler2D declarations use bindings 2/3. A wrapper converts vertex clip Y and
Z to the existing Vulkan/GL frontend convention. `texture2D` and `gl_FragColor`
are translated too. Original macros are expanded before any rewriting.

`glGetAttribLocation` reads the linked SPIR-V names/locations. A binding changed
after linking leaves the current executable and query results intact; successful
relinking remains unsupported. The link callback now takes the requested
bindings; explicit Vulkan GLSL uses `ps4glLinkBoundVulkanGLSL`, whose explicit
source locations take precedence over API bindings.

The deliberately limited language surface is:

- One named declaration per statement, at most 64 declarations, names under
  64 bytes. Attributes/varyings are float or vec2/3/4, with at most 8 attribute
  and 16 varying locations. Aliased attribute locations are rejected.
- Uniform float/int/bool/vec2/vec3/vec4/mat4, optionally literal arrays of 1–16;
  at most two fragment sampler2D declarations, without sampler arrays. Native
  reflection remains a second gate: integer/bool uniform uploads are **not yet
  supported**, even when source compilation succeeds.
- Source up to 256 KiB, expanded/transformed text below 512 KiB. Includes,
  unhandled directives/extensions, complex declarations and bridge-reserved
  identifiers fail with diagnostics. User-visible builtins other than
  `gl_Position` and `gl_FragColor` are rejected; fragment-coordinate/depth and
  point semantics need their own implementation.
- Existing single-thread, object-count, per-frame resource retention and
  successful-relink restrictions remain. No complete GL version is advertised.

Reproduce:

```sh
scripts/check-ps4-glslang.sh
scripts/build-vulkan-probe.sh --gllegacy
xvfb-run -a -s '-screen 0 1280x720x24' \
  python3 scripts/run-opengnm-probe.py --vulkan --gllegacy
```

Host ASan/UBSan checks compile/link all five original TWP fragment shaders with
the original vertex shader for both GLSL 110 and ES 100 preambles. They also
cover declaration/builtin rejection, missing/reordered varyings, two samplers,
aliased attributes and GL binding/query state. This does **not** mean every
post-effect runs through the native GL frontend yet.

In shadPS4, the default TWP pair plus a small uniform-color fragment probe pass
2,553 framebuffer pixel checks, eight frames, command-buffer resubmission,
shader lifetime checks and native cleanup. Nonsequential attribute locations
4/1/6 and unchanged queries after a post-link rebind are checked on target.
Reports are in `artifacts/vulkan/gllegacy/`. Explicit Vulkan GLSL, precompiled
framebuffer and CPU-texture regressions are in
`artifacts/vulkan/gllegacy-regression-*.log`.

Package: `dist/ScummVM-PS4-GL-Legacy.pkg`.
SHA-256: `6a8edd7c4f0aacc4bf835e81c9adf8e3d3a2d0abd274a222102eb63fe6d13618`.
The stable ScummVM 1.17 package remains unchanged at
`c519b288b663d4f7b5db0e86b23a2a7031f1dacc97984edaf9b807f3738191e1`.

Next: close the real engine's remaining uniform/state requirements and connect
SDL context/procedure/frame ownership, then attempt an actual TWP boot. This
checkpoint validates the shader bridge in an isolated diagnostic; physical PS4
and real-game execution through this frontend remain unverified.


## Signed integer uniform checkpoint (2026-09-26)

The GL frontend now accepts signed 32-bit scalar uniforms and arrays through
`glUniform1i`/`glUniform1iv`. Reflection retains their type separately from float
members and samplers, including type agreement for shared stage members.
Uploads copy integer bits directly into the existing per-stage std140 storage;
there is no float conversion. Array locations, stage-specific strides, clipped
writes and immutable per-draw snapshots use the existing uniform machinery.
Invalid counts, pointers and float/int setter mismatches leave values intact.
Sampler setters still validate texture-unit numbers separately.

The `--gllegacy` diagnostic now uses the **original TWP fade fragment shader**
with its original vertex shader. It sets `u_fadeToSep=0`, `u_fade=1` and both
samplers to the same texture, so the established framebuffer RGB oracle remains
applicable. This validates the normal fade branch; it does not establish a
pixel oracle for that shader's sepia branch. The small color program now uses
one signed scalar and an int[2] array to gate its output. Each draw supplies
negative values and a partial array update, then mutates CPU values immediately
after recording. Correct pixels therefore also verify retained integer snapshots.

Host ASan/UBSan checks cover INT32_MIN/MAX, shared stages with different offsets
and array strides, clipped writes, atomic errors and original fade-shader
reflection. Existing float/matrix/array reflection tests pass. The native run
passes 2,553 pixel checks, eight frames, resubmission and lifecycle cleanup.
Regressions for explicit Vulkan GLSL, float/matrix arrays and sampler textures
are recorded in `artifacts/vulkan/glinteger-regression-*.log`; integer state
checks are in `artifacts/glslang/integer-state-host.log`.

Unsigned integers, integer vectors and native bool uniform storage remain
unsupported and fail import. The native rejection fixture now uses `uint`
instead of the newly supported `int`. This supersedes the earlier blanket
integer rejection, without advertising a complete GL implementation.

Current package: `dist/ScummVM-PS4-GL-Legacy.pkg`.
SHA-256: `cc0c63da5909f73b60ba893b6ea53232d738a019f64f525591904fab3ca15edf`.
The stable 1.17 package is unchanged. Next: SDL context/procedure/frame ownership
and the remaining engine state gaps, followed by a real TWP startup attempt.
Physical console and real-game execution through this frontend remain unverified.


The next [SDL adapter checkpoint](PS4-SDL-GL.md) now validates real SDL context,
procedure lookup and swap calls in the native diagnostic. Its Vulkan frame
owner is still supplied by the probe; complete engine integration remains pending.
