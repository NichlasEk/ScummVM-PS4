# ScummVM GL startup audit (2026-09-26)

The bridge now exports the three constant vertex-attribute calls used by
`Graphics::OpenGL` shader helpers (`graphics/opengl/shader.cpp`, `Shader::use`):
`glVertexAttrib2fv`, `glVertexAttrib3fv`, and `glVertexAttrib4fv`. Values belong to
the context, default to (0,0,0,1), and apply only when the corresponding array is
disabled. Enabled arrays retain their own missing-component defaults. The CPU
vertex packer captures constants at draw time, so later changes cannot rewrite
pending GPU work. Invalid indices/null inputs leave the previous value intact.

This closes the missing **call names** in the shared shader helper. It does not
remove the bridge's shader, texture, primitive or lifecycle restrictions.

## Reproduce the inventory

```
python3 scripts/audit-scummvm-gl.py > artifacts/vulkan/scummvm-gl-audit.md
scripts/check-ps4-frame-owner.sh
```

The inventory scans selected startup, shader-renderer and TWP files in the local
prepared ScummVM checkout. It ignores comments/strings but includes conditional
branches. It records source hashes, missing calls and their first location.
This is an intentionally conservative static inventory, not executed GLAD
loading, reachable-call analysis or GL conformance. Fixed-function, libretro,
other engines and indirect loader references are outside its scope.

Current result: 76 resolver exports. Shared shader helpers use 26 names, all
exported. Other scanned files still reference `glReadPixels`, `glBlendColor`,
and the GLES-only `glGetShaderPrecisionFormat`. `glBlendColor` already has an
explicit null-procedure fallback in ScummVM's framebuffer code; it is not by
itself a startup blocker.

## Remaining integration gates, in dependency order

1. **Bootstrap extraction implemented:** `PS4GLDisplayOwner` now owns the native
   instance/device/swapchain/render-pass/image-view/framebuffer startup and
   teardown, and the SDL fixture uses it. See [PS4-VULKAN-DISPLAY.md](PS4-VULKAN-DISPLAY.md).
   Application wiring must install this owner before SDL video initialization
   and keep the legacy software VideoOut bootstrap out of this path.
2. **Isolated application build implemented:** `build-scummvm-vulkan.sh` builds
   real ScummVM with GLAD and the experimental SDL archive under title SCVM00010.
   Stable configuration still defaults to OpenGL disabled. See
   [PS4-SCUMMVM-VULKAN.md](PS4-SCUMMVM-VULKAN.md) for the native entrypoint,
   dependency-link fix, separate settings and observed startup gates.
3. **Diagnostic context request adapted:** only the isolated overlay requests a
   fixed 1280x720 GL 2.0 color-only window and preserves its GL flag through the
   old PS4 software-window branch. Depth/stencil, resize, context recreation and
   general game graphics negotiation remain unimplemented.
4. **Explicit launcher loader implemented:** the isolated application loads 76
   checked entrypoints directly into GLAD's typed pointers, retaining version
   0.0 and a deliberately narrow capability set. It reaches real shader
   compilation and launcher draw calls. The swapchain attachment-clear blocker
   is now fixed: the real launcher displays its menu and two frames present
   successfully in shadPS4. Input and game execution remain unverified. See
   [PS4-VIDEOOUT-CLEAR.md](PS4-VIDEOOUT-CLEAR.md) for evidence and remaining gates.
5. **Triangle strips implemented:** launcher-style four-corner surfaces now pass
   the native pixel oracle through array and uint16/uint32 index paths. See
   [PS4-GL-TRIANGLE-STRIPS.md](PS4-GL-TRIANGLE-STRIPS.md). TWP line strips/loops
   remain unsupported. Launcher setup now accepts disabling cull, depth and
   dither under the existing fixed-disabled Vulkan policy. Enabling them is
   still rejected; this does not add depth/cull support. Pack alignment is
   separately tracked. See [PS4-GL-LAUNCHER-STATE.md](PS4-GL-LAUNCHER-STATE.md).
6. Screen-sized framebuffer recordings now pass the native pixel oracle,
   with a 2048 texture limit and packet-aligned command submissions. See
   [PS4-LARGE-FRAMEBUFFERS.md](PS4-LARGE-FRAMEBUFFERS.md). RGB8 and RGB565 uploads
   now work alongside RGBA8, but other packed/BGRA/indexed formats remain. See
   [PS4-GL-RGB-TEXTURES.md](PS4-GL-RGB-TEXTURES.md). TWP deletes attached
   textures before deleting their FBO, currently rejected by our subset. Shader
   linking during command recording and lifetime program-slot exhaustion also
   need explicit treatment.
7. Add synchronous readback with correct GL row orientation for screenshots and
   TWP capture; integrate SDL input and context/window recreation. Then test the
   real launcher, game startup, transitions, save/load, and physical PS4.

Texture investigation correction: this checkout's `GLTexture::updateArea`
updates entire rows rather than using `GL_UNPACK_ROW_LENGTH`. Adding row-length
and skip-pixel state is not an immediate startup prerequisite. Existing
alignment handling stays unchanged.

## Validation for constant attributes

Host ASan/UBSan tests cover default values, 2/3/4-component completion, enabled
array precedence, invalid-call atomicity, context isolation and resolver lookup.
The native `--glvbo` test now supplies UV as a disabled-array constant, changes
it between two draws, then poisons it before submission. Existing independent
pixel expectations remain unchanged. In shadPS4, all 3,933 sampled pixels passed
(left/right/background counts 246/248/3439), over eight rendered frames, and
native cleanup passed. Evidence: `artifacts/vulkan/attrib-regression-glvbo.log`
and `artifacts/vulkan/glvbo/triangle-result.txt`.

The experimental SDL package is rebuilt separately for the changed context
layout. See `PS4-SDL-GL.md` for its lifecycle and framebuffer checks. No actual
ScummVM GL game boot or physical PS4 result is claimed by these diagnostics.
