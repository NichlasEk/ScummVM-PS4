# Experimental OpenGL state bridge

The next small integration slice is in `compat/ps4gl/`. It is a reusable,
single-threaded C frontend for clear color, scissor, viewport state, selected
queries, error handling and procedure lookup. It forwards color clears to the
existing Vulkan attachment-clear implementation. It does not yet own an SDL
context, compile/link GLSL, or upload GL textures. Float vertex-buffer uploads
are now supported by the snapshot path described below. The imported-program
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
Only TRIANGLES with first=0 are still supported. GL index buffers/draws, uniforms,
textures and runtime GLSL compilation remain future work.

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
