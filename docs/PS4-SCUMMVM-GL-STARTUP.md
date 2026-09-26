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
2. Create an isolated experimental ScummVM build/configuration. Stable
   `configure-scummvm.sh` still explicitly disables OpenGL. Its SDL config must
   select the experimental archive and native graphics/compiler dependencies.
3. Resolve SDL context negotiation: `openglsdl-graphics.cpp` requests 16 depth
   bits and desktop GL 1.4 by default. The diagnostic adapter accepts only its
   bounded 2.0/2.1 request with zero depth/stencil/MSAA, one fixed-size window.
   A deliberate color-only shader path or real depth support is required;
   silently accepting an unsupported depth buffer is not a solution.
4. Exercise real GLAD/context initialization. Current `GL_VERSION=0.0` is an
   intentional gate, not a usable engine version. Desktop engine shaders need
   GLSL 1.20; FBO/NPOT/multitexture flags also depend on capability detection.
   Do not raise version strings or claim full extensions just to bypass this.
5. Implement the actual renderer's draw/state requirements: launcher shader
   pipeline uses `GL_TRIANGLE_STRIP`; TWP also uses line strips/loops. Only
   triangles are currently supported. Launcher setup disables cull, depth and
   dither, which the current state API rejects. Distinguish disable semantics
   from claiming working depth/cull support.
6. Validate real texture sizes/formats and object lifetime. The current 1024
   texture limit is below TWP's 1280x720 screen width. RGBA8-only uploads cannot
   substitute for all launcher's packed/RGB/indexed formats. TWP deletes attached
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
