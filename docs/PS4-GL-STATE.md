# Experimental OpenGL state bridge

The next small integration slice is in `compat/ps4gl/`. It is a reusable,
single-threaded C frontend for clear color, scissor, viewport state, selected
queries, error handling and procedure lookup. It forwards color clears to the
existing Vulkan attachment-clear implementation. It does not yet own an SDL
context or compile/link GLSL. Bounded RGBA8 textures, float vertex/index-buffer
uploads, and named float/vector/mat4 uniforms and arrays are supported by the snapshot paths described below. The imported-program
draw subset added below is separate from runtime GLSL compilation.
No OpenGL version or extension strings are advertised. Unknown procedure names
return NULL, so the partial frontend cannot silently stand in for a complete GL.
A full Mesa/Zink integration remains a separate, much broader compatibility gate;
this slice neither supplies nor establishes those Vulkan requirements.

The owner initializes a `PS4GLContext` with fixed positive framebuffer dimensions,
makes it current and attaches a command buffer inside a render pass covering the
entire color-only RGBA8 UNORM attachment. After the pass it detaches the command
buffer, and before destroying the context it makes NULL current. The owner is
responsible for recording/submission and synchronization. Context dimensions
cannot be changed during a pass. Viewport is tracked for queries and future draws;
clears are independent of it. No draw pipeline state is changed by the frontend.

The supported scissor semantics follow the Khronos references for
[glScissor](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glScissor.xhtml)
and [glClear](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glClear.xhtml):
lower-left coordinates, clipping to the target, empty rectangles and ignoring
viewport/blending during color clears. Rectangle arithmetic is widened to 64 bits
before addition. Negative extents preserve previous state and set an error.
The first pending error is retained until glGetError consumes it. Missing depth
and stencil buffers are ignored by glClear; invalid mask bits are rejected.
Only all-channel color writes are supported; color masks, dithering control,
depth/stencil attachments and general glGet type conversions are not implemented.
This is not a conformance claim.

## Verification

Host checks exercise independent context state, clipping, coordinate conversion,
integer extremes, error retention and unknown procedure lookup under ASan/UBSan:

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Icompat/ps4gl -I.tools/opengnm-stack/Vulkan-Headers/include \
  tests/ps4/check-ps4gl.c compat/ps4gl/ps4gl.c -o build/check-ps4gl
build/check-ps4gl
```

The GPU test resolves glClear through the procedure lookup and issues three
scissored GL clears with overlapping rectangles. A deliberately tiny GL viewport
must not restrict them. Existing Vulkan draws run before and afterward, testing
that the bridge preserves their pipeline and scissor state. The independent
readback oracle checks every output pixel, crop and guard byte, followed by the
screenshot oracle. GL rectangles use inverted Y coordinates relative to the
Vulkan oracle, so an omitted or doubled origin conversion fails visibly.

```sh
scripts/build-vulkan-probe.sh --glstate
xvfb-run -a -s '-screen 0 1280x720x24' \
  python3 scripts/run-opengnm-probe.py --vulkan --glstate
```

Separate diagnostic: `dist/ScummVM-PS4-GL-State.pkg`, title SCVM00009.
Reports: `artifacts/vulkan/glstate/`. Stable ScummVM is not replaced. GLSL compilation/linking, uniforms, textures/buffers, complete draw state and SDL
lifecycle remain before
this can run TWP. Physical PS4 validation is also still pending.

Validated on 2026-09-26: sanitizer host checks, eight GL recordings without
errors, nine full 64-KiB readback/guard comparisons (including resubmission),
1,230 screenshot samples and complete Vulkan cleanup pass in shadPS4.
Package SHA-256:
`da4efa684f40275d3b1c4ad88b4c769f791f802fc5920f04b1b46fb30767585a`.
The existing pure-Vulkan composition and rectangle-clear regressions also pass;
logs: `artifacts/vulkan/glstate-regression-*.log`. Their runner restores the GL
state diagnostic executable afterward. Stable ScummVM's package hash is unchanged.

## Imported programs and GL draws

`ps4glCreateProgramSPIRV` imports a caller-validated, compatible Vulkan SPIR-V
vertex/fragment pair, creates shader modules and a real graphics pipeline on the
PS4, and returns a context-local GL program name. The native driver performs
SPIR-V-to-GCN compilation. This is an explicit bootstrap API, not an implementation
of glShaderSource, glCompileShader, glAttachShader or glLinkProgram. It does not
recognize shader text or game-specific names. The input must already have Vulkan
clip conventions: inverted Y and depth in [0,1]. Stage-interface matching remains
the caller's responsibility. Malformed instruction spans, descriptor resources
and unsupported vertex types/locations are rejected before pipeline creation.

Implemented program entrypoints are glUseProgram, glIsProgram, glDeleteProgram,
glGetProgramiv (LINK_STATUS/DELETE_STATUS), and CURRENT_PROGRAM via glGetIntegerv.
An active program marked for deletion remains usable until it is unbound, as
specified by [glDeleteProgram](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glDeleteProgram.xhtml).
Native pipelines remain allocated even after GL names are retired because recorded
command buffers can still reference them. The owner must finish all such GPU work
and discard those commands before ps4glDestroyPrograms. There is a deliberate
16-program-per-context lifetime limit; retired slots are not recycled until full
teardown. All imports in a context use one device/render-pass pair. No context
sharing, program relinking or automatic resource reclamation is implemented.

`glDrawArrays` supports TRIANGLES, first=0, and shaders generating their own
vertices from gl_VertexIndex or float buffer attributes (see below). It binds the selected program, applies viewport and
scissor state, converts GL window Y coordinates to Vulkan framebuffer coordinates,
and records vkCmdDraw. Empty viewport/scissor/count draws are no-ops. Unsupported
first values, viewports outside the target, missing program/pass and unsupported
modes produce explicit GL errors. Indexed GL draws, uniforms,
textures, blending and depth are not part of this draw subset. The native draw
probe uses an sRGB target; the earlier clear subset still requires UNORM.

Reproduce with `scripts/build-vulkan-probe.sh --gldraw`, followed by
`xvfb-run -a -s '-screen 0 1280x720x24' python3 scripts/run-opengnm-probe.py --vulkan --gldraw`.
The separate package is `dist/ScummVM-PS4-GL-Draw.pkg`, with reports under
`artifacts/vulkan/gldraw/`. The test draws a full-screen background, switches
programs, uses a smaller viewport and an asymmetric GL scissor, then draws an
interpolated triangle through the resolved glDrawArrays entrypoint. In the last
frame the current program is marked for deletion before drawing. Teardown checks
both names are retired before releasing their pipelines after device idle.
The independent RGB oracle checks the expected triangle and clipped/background
regions across the full framebuffer. This is still a diagnostic, not TWP startup
or a complete GL implementation. Vertex buffers were added in the next
checkpoint below; resource bindings remain, followed by a real runtime GLSL compilation/linking path and SDL integration.

Draw checkpoint validation (2026-09-26): 216 interpolated triangle samples and
3,741 background/clipped samples pass, all eight frames report GL_NO_ERROR,
three program-interface rejection checks pass, and Vulkan cleanup completes.
Host ASan/UBSan checks also cover viewport/scissor application, invalid draws,
program switching, delete-pending state and retired-name rejection.
Package SHA-256:
`e6f5e93f0f36552731dd250e06b2cb8085aa4d7533a9b2865f0c4711921e80ba`.
Stable ScummVM's package SHA-256 remains unchanged.
The basic native Vulkan triangle and prior GL clear/readback diagnostic also
pass after these changes (`artifacts/vulkan/gldraw-regression-*.log`). The runner
restores the GL draw executable after those regressions.

## Vertex buffers and immutable draw data

The frontend now implements glGenBuffers, glBindBuffer, glIsBuffer,
glDeleteBuffers, glBufferData, glBufferSubData and glGetBufferParameteriv for
ARRAY_BUFFER. glVertexAttribPointer captures the buffer bound when it is called;
a subsequent glBindBuffer does not redirect existing attribute pointers. This
follows the [Khronos attribute definition](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glVertexAttribPointer.xhtml).
Float attributes of 1..4 components, arbitrary byte offsets, interleaved or
separate strides, and zero stride (tightly packed) are supported at locations
0..7. Disabled inputs use the default (0,0,0,1), and missing components are
filled with those defaults. Normalization does not change float input values.
There is one implicit vertex array; VAO APIs, client arrays, non-float formats,
attribute divisors and generic constant setters remain unimplemented.

SPIR-V import reflects plain 32-bit float/scalar-vector input declarations and
creates a canonical packed vertex layout for the native pipeline. On every draw,
the frontend validates all active source ranges before packing. It then copies
the draw's selected vertex values into a separate host-coherent Vulkan buffer.
The CPU shadows backing the GL buffer names may subsequently be updated,
reallocated or deleted without changing previously recorded draws. Bounds checks
use subtraction to avoid offset/stride arithmetic overflow. This is deliberately
a correctness-first staging strategy, not a zero-copy or benchmarked streaming
implementation. No game-specific vertex formats or shader names are recognized.

The owner calls ps4glConfigureVertexDevice with the physical device matching the
program device. After fences complete AND all referencing command buffers have
been reset/discarded, it calls ps4glReleaseVertexSnapshots. Finishing one submit
alone is insufficient when the command buffer can be submitted again. The probe
releases after reset on each frame. CPU shadows are released independently by
glDeleteBuffers or ps4glDestroyBuffers. Program teardown also releases any remaining
snapshots, under its existing requirement that no commands can still use them.

Current bounds: 32 GL buffer names at once; 16 MiB per CPU buffer and per packed
draw; 64 retained GPU snapshots and 64 MiB of retained payload bytes per context
(allocator alignment is additional). Exhaustion produces GL_OUT_OF_MEMORY.
Source buffer overruns fail with GL_INVALID_OPERATION before recording a draw.
Only TRIANGLES with first=0 are supported by glDrawArrays. Indexed drawing is
added in the next checkpoint below; uniforms, textures and runtime GLSL
compilation remain future work.

Reproduce:

```sh
scripts/build-vulkan-probe.sh --glvbo
xvfb-run -a -s '-screen 0 1280x720x24' \
  python3 scripts/run-opengnm-probe.py --vulkan --glvbo
```

The diagnostic uses interleaved position/color/UV data (stride 32) after a 16-byte
prefix. It points attributes at buffer A, binds a deliberately wrong buffer B,
and draws the left triangle. It then updates A, draws the right triangle, replaces
A's storage and deletes both buffers before GPU submission. Both earlier draws
must retain their own colors and geometry. A new draw after deletion must fail;
the final recorded command buffer is submitted twice without re-recording.

Validation: all 3,933 RGB samples pass (246 left, 248 right, 3,439 background),
eight frames report no unexpected GL errors, eight deleted-buffer rejection
checks pass, final command-buffer resubmission completes and Vulkan cleanup
passes. ASan/UBSan host checks cover captured buffer identity, default components,
updates, deletion, oversized offsets/strides, underruns and resource caps.
Package: `dist/ScummVM-PS4-GL-Vertex.pkg`; reports: `artifacts/vulkan/glvbo/`.
SHA-256: `23affdce34a22e59e6e26398c486680e41fb072f29d5c996da829d19215e4aaf`.
This remains a separate diagnostic. TWP startup and physical PS4 execution are
not yet validated.
The earlier GL draw/program and GL clear/readback diagnostics also pass with the
new frontend (`artifacts/vulkan/glvbo-regression-*.log`); the vertex diagnostic is
restored afterward. Stable ScummVM's package hash is unchanged.


## Indexed GL draw checkpoint (2026-09-26)

`GL_ELEMENT_ARRAY_BUFFER` shares buffer names and storage operations with
`GL_ARRAY_BUFFER`, with a separate binding and binding query. Deletion clears
both bindings when they refer to the same object. `glDrawElements` supports
TRIANGLES, UNSIGNED_SHORT and UNSIGNED_INT, with a byte offset into the bound
index buffer. Client index pointers, byte indices, other topologies, instancing,
base-vertex draws, primitive restart and VAOs are not implemented.

Each indexed draw validates the index range, scans the maximum index, then packs
the vertex prefix through that index. The original indices follow the vertices
in one immutable Vulkan buffer; the driver binds both ranges and records a real
`vkCmdDrawIndexed`. Indices are not expanded into a non-indexed draw, preserving
vertex-index identity. Combined payload is bounded to 16 MiB per draw; the existing
64-snapshot/64-MiB retained-payload cap and fence-plus-command-reset lifetime
contract still apply. Sparse large indices can hit the dense-prefix limit.

Reproduce:

```sh
scripts/build-vulkan-probe.sh --glindex
xvfb-run -a -s '-screen 0 1280x720x24' \
  python3 scripts/run-opengnm-probe.py --vulkan --glindex
```

The diagnostic shuffles vertex storage and puts a poison vertex at index zero.
It draws the left triangle with 16-bit indices at byte offset 4, replaces the
index store, updates vertex colors and draws the right triangle with 32-bit
indices at offset 8. It then overwrites/deletes the buffers before submission.
This checks both index addressing and retained vertex/index data; the last frame
is submitted a second time from the same recorded command buffer.

Actual shadPS4 framebuffer validation passes all 3,933 RGB samples (246 left,
248 right, 3,439 background), all eight frames, buffer-deletion rejection,
command-buffer resubmission and Vulkan cleanup. ASan/UBSan host tests cover
independent bindings, both widths, offsets, captured payloads, malformed ranges,
negative counts, unsupported types, missing/deleted buffers, oversized indices
and the dense-prefix cap. This is still a diagnostic, not TWP startup or a
physical PS4 validation. The next frontend slice is uniforms/texture binding;
runtime GLSL and SDL context integration remain separate gates.

Package: `dist/ScummVM-PS4-GL-Index.pkg`.
Reports: `artifacts/vulkan/glindex/`.
SHA-256: `a69dd5cf35cfa932e8ff74be9c84f52d6c8c5074d5e6027c834474077517fca1`.
The prior GL vertex, procedural draw/program and clear/readback probes also pass
with this frontend (`artifacts/vulkan/glindex-regression-*.log`). The indexed
diagnostic binary is restored after regression testing. Stable ScummVM 1.17
retains SHA-256 `c519b288b663d4f7b5db0e86b23a2a7031f1dacc97984edaf9b807f3738191e1`.


## Named uniform checkpoint (2026-09-26)

`glGetUniformLocation`, `glUniform1f/2f/3f/4f`, their `fv` variants and
`glUniformMatrix4fv` now update per-program CPU state. The SPIR-V import reflects
member names, types and offsets from at most one default-uniform bridge block
per stage: set 0 / binding 0 for vertex, binding 1 for fragment. This explicit
import contract maps translated default uniforms to Vulkan UBOs; it is not the
OpenGL uniform-block API or a GLSL linker. Member names must be retained in
SPIR-V. Matching names in both stages share a location and receive both updates;
conflicting types reject import. Initial values are zero, missing names return
-1, updates to location -1 are ignored, and errors preserve prior values.

Accepted members are float32, vec2/3/4 and column-major mat4 (matrix stride 16).
Matrix setters also transpose CPU input when requested. There are at most 16
unique names (63 bytes each), 256 bytes per stage, and no arrays, integers,
booleans, nested structs, row-major shader matrices, samplers or push constants.
Vector/scalar offsets must meet their base alignment; overlapping and
out-of-bounds members reject import. Setters support count 0/1, with larger
counts rejected because arrays are absent. The parser is a bounded interface
guard; complete SPIR-V validation remains the importing caller's responsibility.

At draw time the bridge snapshots both 256-byte blocks into coherent GPU memory,
allocates a retained descriptor set and binds it before the native draw. The
native device reports 256-byte uniform-buffer offset alignment. Both descriptor
pool and buffer survive edits, program deletion and command resubmission until
the owner completes fences and discards all referencing command buffers. Uniform
and vertex/index snapshots share the existing 64-snapshot/64-MiB payload cap;
a draw using both consumes two slots. This allocation path prioritizes correct
lifetime over batching/allocator performance.

Reproduce:

```sh
scripts/build-vulkan-probe.sh --gluniform
xvfb-run -a -s '-screen 0 1280x720x24' \
  python3 scripts/run-opengnm-probe.py --vulkan --gluniform
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Icompat/ps4gl -I.tools/opengnm-stack/Vulkan-Headers/include \
  tests/ps4/check-ps4gl-uniforms.c -o build/check-ps4gl-uniforms
build/check-ps4gl-uniforms build/vulkan-probe/triangle_vert.spv \
  build/vulkan-probe/triangle_frag.spv
```

The native probe transforms half-size vertex positions with a mat4, sets a
shared scalar in both shader stages, changes fragment tint between two indexed
draws, then overwrites all parameter values before submission. The captured
framebuffer passes 3,933 RGB samples, eight frames, program/buffer deletion
checks, command resubmission and lifecycle cleanup. Host ASan/UBSan tests cover
scalar/vector/matrix writes, transpose, program isolation, shared-stage writes,
ignored/missing locations, wrong setter types/counts, and reflection rejection
for wrong binding, matrix stride, scalar type, overlapping offsets and malformed
instructions. Original game shaders with uniform arrays are still unsupported.

Package: `dist/ScummVM-PS4-GL-Uniform.pkg`.
Reports: `artifacts/vulkan/gluniform/`.
SHA-256: `8a8d3cc8426d40eb328169a35c84c085f5dadac73104521252b40fdf078b7761`.
This verifies the diagnostic in shadPS4; game startup, SDL GL integration and
physical PS4 remain unverified. Next: uniform arrays and GL texture/sampler
bindings, then translating GLSL at runtime and connecting the ScummVM backend.
The previous index, vertex, procedural draw/program and clear/readback probes
all pass (`artifacts/vulkan/gluniform-regression-*.log`). The uniform diagnostic
is restored afterward; the stable ScummVM package remains unchanged.


## Uniform array checkpoint (2026-09-26)

The imported default-uniform blocks now accept one-dimensional fixed arrays of
float32, vec2/3/4 and mat4. SPIR-V array length must be a positive 32-bit integer
constant, with at most 16 elements and a declared array stride that is a multiple
of 16 and covers the element. The entire array, including trailing padding, must
fit the existing 256-byte stage block and not overlap other members. Matrix
arrays retain column-major / matrix-stride-16 requirements. Nested, runtime and
specialization-sized arrays remain unsupported. Shared names across stages must
have matching element type and length; each stage keeps its own offset/stride.

`glGetUniformLocation` accepts the bare array name, `[0]` and individual `[i]`
elements. Locations are opaque: query each element instead of doing arithmetic.
Malformed, overflowing and out-of-range names return -1. Vector/matrix `fv`
setters accept multiple elements starting at the requested element, unpacking
tightly packed caller data into the shader's declared stride. Excess elements
past the end are ignored before accessing input data; a count greater than one
for a non-array still raises INVALID_OPERATION. This follows the Uniform rules
in the [Khronos OpenGL specification](https://registry.khronos.org/OpenGL/specs/gl/glspec44.compatibility.pdf).
Matrix transpose is applied independently to each element. No element update
changes neighbouring members or padding. The existing immutable draw snapshots
also retain all array values through CPU edits and command resubmission.

Reproduce:

```sh
scripts/build-vulkan-probe.sh --glarray
xvfb-run -a -s '-screen 0 1280x720x24' \
  python3 scripts/run-opengnm-probe.py --vulkan --glarray
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Icompat/ps4gl -I.tools/opengnm-stack/Vulkan-Headers/include \
  tests/ps4/check-ps4gl-arrays.c -o build/check-ps4gl-arrays
build/check-ps4gl-arrays build/vulkan-probe/triangle_vert.spv \
  build/vulkan-probe/triangle_frag.spv
```

The shader fixture uses TWP's `RandomValue[5]` layout: offset 16, stride 16,
followed by `TimeLapse` at offset 96. It also uses the second of two mat4 values
in the vertex stage while the first deliberately moves geometry out of view.
The fragment stage sums array values in a loop and uses the fifth value to
select the right-hand draw's tint. The fifth element is changed through its
indexed location, and both arrays are overwritten before GPU submission.
This is a targeted fixture, not the full original TWP shader or game.

Host ASan/UBSan checks cover reflection, rejected lengths/strides, indexed names,
partial writes, distinct stage strides, padded vec3 arrays, transposed matrix
arrays, wrong types, invalid locations and excess-count truncation without
reading past the final input value. Existing scalar uniform reflection, uniform
GPU and indexed-draw GPU regressions also pass; see
`artifacts/vulkan/glarray-regression-*.log`. Texture/sampler binding and runtime
GLSL/SDL integration remain the next gates before game startup validation.

Package: `dist/ScummVM-PS4-GL-Array.pkg`.
Reports: `artifacts/vulkan/glarray/`.
SHA-256: `3280b0ec384df4b6c0b2b0240538273e08c43f99dd6c9272340258ad2299008e`.
The final loop-based shader passes all 3,933 RGB samples, eight frames, deletion
checks, recorded-command resubmission and Vulkan cleanup in shadPS4. SPIR-V
contains OpLoopMerge and an OpAccessChain with a variable array index. This does
not validate all TWP effect branches or physical PS4 execution.


## GL texture and sampler checkpoint (2026-09-26)

The frontend now supports `glGenTextures`, `glBindTexture`, `glDeleteTextures`,
`glIsTexture`, `glActiveTexture`, `glTexImage2D`, `glTexSubImage2D`,
`glTexParameteri` and `glPixelStorei(UNPACK_ALIGNMENT)`. Texture names and bindings
are separate from buffers. There are 32 texture names, four texture units,
1024x1024 maximum dimensions and a 64 MiB CPU texture payload budget. Deletion
unbinds the name from every unit; recorded draws retain independent GPU images.
Binding queries for ACTIVE_TEXTURE, TEXTURE_BINDING_2D and UNPACK_ALIGNMENT are
available. Uploads honour alignments 1/2/4/8, preserving RGBA row order.

This is a bounded 2D, level-zero, RGBA/RGBA8 + UNSIGNED_BYTE path. Border must be
zero; mipmaps, compressed formats, RGB conversion, pixel-buffer objects, unpack
row-length/skips, texture arrays and the default texture object's storage are
not implemented. NULL TexImage data is initialized to zero. Supported filters
are NEAREST and LINEAR, with REPEAT or CLAMP_TO_EDGE addressing. New textures
retain GL's mipmapped default min filter; select a supported non-mip filter
before drawing. Missing/empty textures or unsupported mip completeness cause an
explicit INVALID_OPERATION in this prototype, not a silent black fallback.

The SPIR-V importer accepts at most two non-array floating-point fragment
`sampler2D` resources, named via OpName, at set 0 bindings 2/3. Uniform blocks
remain at bindings 0/1. `glGetUniformLocation` and `glUniform1i/1iv` assign the
sampler to one of the four texture units; default assignment is unit zero.
Wrong setter types and out-of-range units are rejected without changing state.
Vertex samplers, separate GLSL texture/sampler resources, integer/depth/cube
samplers and sampler arrays reject import. There are no GL sampler objects;
texture-object sampling parameters create the native Vulkan sampler snapshots.

Each textured draw snapshots the required CPU images and sampler parameters into
coherent linear Vulkan images and retained image views/samplers. Memory layout
uses vkGetImageSubresourceLayout, including row pitch. PREINITIALIZED-to-GENERAL
transitions and HOST_WRITE-to-SHADER_READ visibility use separate, synchronous
upload command buffers on queue 0, outside the owner's recording render pass.
The frontend and owning renderer must serialize queue use on one thread. Each
upload fence completes before its transient upload pool is destroyed. GPU image
resources and descriptors remain alive through the owning draw's fence AND
command reset/discard, including resubmission. CPU edits/deletion cannot alter
earlier recorded draws. Snapshot payload shares the existing 64 MiB budget;
physical allocation alignment adds overhead. This correctness-first path copies
images per draw and is not yet a batched/cached texture uploader.

Reproduce:

```sh
scripts/build-vulkan-probe.sh --gltexture
xvfb-run -a -s '-screen 0 1280x720x24' \
  python3 scripts/run-opengnm-probe.py --vulkan --gltexture
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Icompat/ps4gl -I.tools/opengnm-stack/Vulkan-Headers/include \
  tests/ps4/check-ps4gl-textures.c -o build/check-ps4gl-textures
build/check-ps4gl-textures build/vulkan-probe/triangle_frag.spv
```

The actual shadPS4 framebuffer passes 3,933 RGB checks. Left and right indexed
triangles sample two 2x2 textures through different units. Between draws, one
texel is replaced, one sampler changes units, filtering changes from nearest to
linear and wrapping changes from clamp to repeat. The right red value requires
linear averaging of 0/128; the left red value requires retaining nearest
filtering at UV x=0.6. The blue value requires wrapping UV (1.25,1.75)
to the correct texel in the newly selected unit. Thus wrong filtering, row order,
wrapping, units or mutable prior resources produce a pixel mismatch. Texture
storage is then overwritten and all texture names deleted before submission;
the final command is submitted twice. All eight frames, deletion checks and
Vulkan cleanup pass. Alpha sampling/blending and large-texture performance are
not established by this RGB fixture.

Host ASan/UBSan checks cover row alignment, dimensions/offset overflow, partial
updates, preservation after invalid calls, texture units, sampler setters,
zero initialization, deletion/unbinding and reflection rejection of unsupported
bindings/types/stages. This remains a diagnostic rather than a game boot or
physical PS4 validation. Next: GL blending/offscreen framebuffer state and
runtime shader/context integration using the now-connected resource paths.

Package: `dist/ScummVM-PS4-GL-Texture.pkg`.
Reports: `artifacts/vulkan/gltexture/`.
SHA-256: `4f3b0766098297c8357476faf6d85387db93a438e907cd3c4a4879a37d9c7f0f`.
The previous array, uniform and indexed-draw GPU probes all pass with the new
frontend (`artifacts/vulkan/gltexture-regression-*.log`), as do both earlier
uniform/array reflection tests. The final texture diagnostic remains installed
in the isolated probe build; stable ScummVM 1.17 is unchanged.


## GL blending checkpoint (2026-09-26)

`glEnable/Disable/IsEnabled(GL_BLEND)`, `glBlendFunc`, `glBlendFuncSeparate`,
`glBlendEquation` and `glBlendEquationSeparate` now drive native Vulkan pipeline
state. Queries cover BLEND, BLEND_SRC/DST_RGB/ALPHA and BLEND_EQUATION_RGB/ALPHA.
Default factors are ONE/ZERO and equations ADD, with blending disabled. Invalid
setters preserve the entire previous state. Clears remain independent of blend
state. Reference semantics are the Khronos
[blend factors](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBlendFuncSeparate.xhtml)
and [blend equations](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBlendEquationSeparate.xhtml).

The supported factor subset is ZERO, ONE, SRC_COLOR, DST_COLOR, SRC_ALPHA,
DST_ALPHA and their ONE_MINUS forms. Equations are ADD, SUBTRACT,
REVERSE_SUBTRACT, MIN and MAX, separately for RGB and alpha. Constant-color,
saturate and dual-source factors, indexed per-attachment blend state,
glBlendColor, color masks and logic operations remain unsupported. The current
render-pass attachment format determines native sRGB handling; there is no GL
FRAMEBUFFER_SRGB toggle yet, and no complete GL version is advertised.

Shader modules now remain owned by the imported program. Each new enabled blend
state creates a Vulkan pipeline from the same shader/interface definition;
identical states reuse their cached variant. The disabled path uses the original
pipeline. The cap is 16 enabled variants per program; exhausted capacity returns
OUT_OF_MEMORY before recording a draw. Variants are not evicted because earlier
recorded commands may still reference them. They survive glDeleteProgram until
the existing owner-controlled teardown after completed fences and discarded
commands, and are destroyed along with the retained shader modules and layouts.
This avoids mutating the pipeline of a previously recorded draw.

Reproduce:

```sh
scripts/build-vulkan-probe.sh --glblend
xvfb-run -a -s '-screen 0 1280x720x24' \
  python3 scripts/run-opengnm-probe.py --vulkan --glblend
```

The independent screenshot oracle checks eight columns: disabled replacement,
standard source-alpha blending, separate alpha factors, additive blending,
subtraction, reverse subtraction, MIN and MAX. The final column uses MAX for
RGB and MIN for alpha to exercise distinct equations. Each column first writes
an RGBA base and then an overlay. In the lower half, a white draw using
DST_ALPHA as its RGB factor exposes the stored alpha as visible grayscale;
this validates alpha computation rather than ignoring the screenshot's alpha.
MIN/MAX use ZERO factors to ensure factors are correctly ignored for those
equations. The target is SRGB, and the oracle computes linear blending followed
by SRGB conversion with tolerance for 8-bit storage quantization.

The probe checks 2,624 RGB/grayscale pixels across all eight columns, eight
error-free frames, eight cached variants without growth, deletion of the bound
program on the last frame, recorded-command resubmission and native lifecycle
cleanup. Host ASan/UBSan checks cover defaults, queries, combined/separate
setters, atomic rejection and clear independence. This remains a shadPS4
diagnostic; neither full game boot nor physical PS4 is validated. Next is GL
framebuffer/render-to-texture support, followed by runtime GLSL and SDL context
integration with the existing resource and blending paths.

Package: `dist/ScummVM-PS4-GL-Blend.pkg`.
Reports: `artifacts/vulkan/glblend/`.
SHA-256: `1d64b3eb0bf332dd8c090efb0de5191507aa045dc179c2449e0f889476e9d714`.
The texture, array, indexed-draw and clear/readback GPU probes also pass with
the refactored pipeline creation/lifetime (`artifacts/vulkan/glblend-regression-*.log`).
The final blend diagnostic remains in the isolated probe build; stable ScummVM
1.17 retains its prior package hash.

## Managed framebuffer checkpoint (2026-09-26)

The GL frontend now renders into a level-zero RGBA8 texture and samples that
GPU image in later draws. `--glfbo` exercises this through native Vulkan/OpenGNM
in shadPS4. This is still a diagnostic, not a completed ScummVM GL backend or a
physical-console result.

The bounded API includes glGen/Bind/Delete/IsFramebuffer,
glFramebufferTexture2D, glCheckFramebufferStatus and FRAMEBUFFER_BINDING.
There is one COLOR_ATTACHMENT0, with 16 framebuffer names and 32 retained
native target allocations. Texture dimensions retain the existing 1024 limit;
native target logical payload has a separate 64 MiB budget. Memory alignment,
command storage and other native allocations are additional costs. No depth,
stencil, renderbuffers, multiple attachments, multisampling, mip attachments,
read/draw framebuffer split or blit API is claimed.

The owner begins Vulkan command recording outside any render pass, calls
`ps4glBeginFrame(command, pass, framebuffer, width, height)`, records GL work,
and finishes with `ps4glEndFrame()`. The supplied default pass must be the
imported programs' pass, LOAD/STORE, with GENERAL initial/final layouts. The
owner transitions the default image before/after the managed frame and owns
submission and fences. The frontend starts passes lazily and ends them on target
changes. Offscreen passes LOAD/STORE; attachment writes and sampling have
explicit barriers. The older borrowed `ps4glBeginPass` interface remains valid
for default-target drawing, but rejects drawing to a nonzero GL framebuffer.
Viewport/scissor values remain GL state across bindings; the current attachment
supplies the bounds. A viewport extending outside those bounds is still rejected.

Texture row zero must mean GL v=0 for both CPU uploads and GPU rendering.
Offscreen draws therefore use a negative viewport height and direct GL scissor
rows; the default framebuffer keeps the existing top-left conversion. This
requires the native backend's signed-height viewport support, already present
in OpenGNM. Imported vertex shaders still use the existing GL-to-Vulkan clip-Y
conversion. The asymmetric screenshot oracle checks this combination, including
scissored clears; no shader-specific UV workaround or CPU readback is used.

Initial CPU pixels are staged into the target before its first render pass.
The driver upload path now accepts color targets using their actual GNM surface
layout and checks both logical image bounds and bound-memory ranges. Subsequent
sampling borrows the native image view and snapshots sampler/descriptor state.
Pipeline variants are keyed by render pass as well as blend state, so an UNORM
offscreen attachment does not reuse an incompatible SRGB default pipeline.
The 16-variant cap now covers these combined keys; the disabled default-target
pipeline remains the uncached base pipeline.

Full glTexImage2D redefinition creates a new storage generation on demand;
previously recorded references keep the old image alive. Partial CPU updates
of a GPU-rendered texture return INVALID_OPERATION because its CPU shadow is
stale. Sampling the currently attached output is rejected as feedback. Texture
deletion while attached to any framebuffer is also explicitly rejected in this
subset: detach it or delete the framebuffer first. This is a temporary API
restriction, not full GL deletion semantics. Incomplete targets reject clear
and draw recording with INVALID_FRAMEBUFFER_OPERATION.

The owner may release snapshots only after **all referencing commands have
completed and been reset/discarded**, just as for vertices and uniforms. That
release now collects only target storage no longer owned by a live texture;
live GPU contents survive across frames. Program teardown destroys remaining
native targets and the offscreen pass. Final context teardown should destroy
framebuffer names and textures too. Single-threaded, ordered queue use remains
part of the frontend contract.

Reproduce:

```sh
scripts/build-vulkan-probe.sh --glfbo
xvfb-run -a -s '-screen 0 1280x720x24' \
  python3 scripts/run-opengnm-probe.py --vulkan --glfbo
```

The probe renders and samples three versions of a texture, preserves pixels
when returning to an attachment, blends into an UNORM target, rejects feedback
and incomplete attachments, and deletes/redefines objects before submission.
A small magenta image stays GPU-resident across frames and snapshot collection.
The final command buffer is resubmitted before releasing its storage. The runner
requires eight error-free frames, eight storage-collection checks, resubmission,
pixel validation and lifecycle cleanup. Host ASan/UBSan tests also cover FBO
names, completeness, attachment validation, deletion restrictions and exhaustion.
Regression evidence for blend, CPU textures, clear/readback and staging uploads
is in `artifacts/vulkan/glfbo-regression-*.log`.

Package: `dist/ScummVM-PS4-GL-Framebuffer.pkg`.
Reports: `artifacts/vulkan/glfbo/`.
Next integration gate: runtime GLSL and SDL context/procedure wiring, with the
remaining GL state required by the real engine audited against this subset.

Final package SHA-256:
`b410e137f2593543240953139c550c9ab7a86647bf73b158dacc02677f8d10d3`.
The final oracle passes 2,553 pixel checks including the persistent texture.
Stable ScummVM 1.17 remains unchanged at
`c519b288b663d4f7b5db0e86b23a2a7031f1dacc97984edaf9b807f3738191e1`.


## Runtime compiler continuation

The next standalone milestone is documented in [PS4-GLSL.md](PS4-GLSL.md):
explicit Vulkan GLSL text now compiles on target and drives this framebuffer
probe. Legacy GL shader objects/linking and SDL integration are still pending.
