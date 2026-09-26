# Experimental OpenGL state bridge

The next small integration slice is in `compat/ps4gl/`. It is a reusable,
single-threaded C frontend for clear color, scissor, viewport state, selected
queries, error handling and procedure lookup. It forwards color clears to the
existing Vulkan attachment-clear implementation. It does not yet own an SDL
context, compile/link shaders, upload GL buffers/textures, or issue GL draws.
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
Reports: `artifacts/vulkan/glstate/`. Stable ScummVM is not replaced. Shader
programs, uniforms, textures/buffers, draw state and SDL lifecycle remain before
this can run TWP. Physical PS4 validation is also still pending.

Validated on 2026-09-26: sanitizer host checks, eight GL recordings without
errors, nine full 64-KiB readback/guard comparisons (including resubmission),
1,230 screenshot samples and complete Vulkan cleanup pass in shadPS4.
Package SHA-256:
`da4efa684f40275d3b1c4ad88b4c769f791f802fc5920f04b1b46fb30767585a`.
The existing pure-Vulkan composition and rectangle-clear regressions also pass;
logs: `artifacts/vulkan/glstate-regression-*.log`. Their runner restores the GL
state diagnostic executable afterward. Stable ScummVM's package hash is unchanged.
