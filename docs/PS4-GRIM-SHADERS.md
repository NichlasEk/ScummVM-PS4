# Grim shader bridge checkpoint — 2026-09-26

The experimental Vulkan build now translates all nine original Grim Fandango shader pairs. This is a shader milestone, not a claim that the complete game renders correctly or runs on physical PS4.

## Implementation

- Package the upstream conditional `DIST_FILES_SHADERS` manifest under `data/shaders` (144 files in the combined build).
- Flatten simple uniform structs into checked std140 leaf fields. Preserve public names such as `shadow._active` and `shadow._color`, reconstruct GLSL values for reads, and keep bool storage as normalized signed integers.
- Use named stage-specific uniform-block instances so common uniforms link across vertex and fragment stages.
- Support positive integer literal constants as bounded uniform array sizes, including Grim's `maxLights`.
- Translate fragment window-coordinate Y using a private per-draw transform: swapchain rows use `height - y`, offscreen rows already follow the GL texture convention. Preserve X, Z and W. Snapshot the transform with the draw's other uniforms.
- Increase the bounded reflected-uniform table from 16 to 64 entries; retain the existing stage byte budgets.

Limits remain explicit: eight struct definitions, sixteen scalar/vector/mat4 fields per struct, no nested structs or struct arrays, no field arrays/samplers, 63-character public leaf names, sixteen named literal array-size constants. This is not a general OpenGL implementation.

## Verification

`scripts/check-ps4-glslang.sh` passes ASan/UBSan compilation, linking and reflection tests, including every original Grim shader pair, existing TWP pairs, shared bool/vector fields, and rejection of incompatible/nested structs.

The native SDL/GL probe sets bool, vec3 and vec2 struct fields via dotted GL uniform names. Its fragment shader verifies window-coordinate bounds on asymmetric offscreen and swapchain viewports. Values are changed after draws to check snapshot retention. shadPS4 passed 2,553 pixel checks, framebuffer readback and lifecycle checks. Evidence: `artifacts/vulkan/sdlgl/pixel-test.log` and `triangle-result.txt`.

Reproduce native checks with:

```sh
scripts/build-vulkan-probe.sh --sdlgl
xvfb-run -a -s '-screen 0 1280x720x24' python3 scripts/run-opengnm-probe.py --vulkan --sdlgl
```

After changing `ps4gl.h`, rebuild SDL as well as bridge objects; the shared context layout is an ABI between them. The normal `scripts/build-scummvm-vulkan.sh` does this. Relinking only bridge objects can combine incompatible layouts and produce misleading allocation errors.

## Game files

Both original MODE1/2352 BIN/CUE discs are retained under `artifacts/Grimm`; GRIMDATA is merged into `artifacts/Grimm/game` with duplicate hashes checked. The official `gfupd101.exe` patch matches the engine's expected MD5 `bc57c4db2ab814f3bb115ec5702b62f4`. The original emulator profile and mouse bindings are retained.

## Visible runtime result

The final package passes the original shader startup and visibly plays `intro.snm` with subtitles across the 1280x720 target. Screenshot: `artifacts/scummvm-vulkan/grim-actual-size-intro.png`. Experimental Grim renderer setup now uses the actual backend dimensions after `initGraphics3d`, rather than continuing to render into its original 640x480 request. The initial attempt to change only backend local dimensions did not fix this and was removed.

Model loading also exceeded the old 32 live-buffer records; the bounded table now holds 512. Buffer deletion already frees/reuses records. Host GL state/lifetime tests pass with ASan/UBSan at the new capacity.

## Intro-to-room crash repair

The captured guest backtrace (`artifacts/scummvm-vulkan/grim-crash-backtrace.log`)
identified a null `glColorMask` call in `Grim::GfxOpenGLS::drawShadowPlanes`.
The experimental context now loads color-mask, depth and stencil entrypoints.
Raster pipeline variants retain color masks, depth comparison/write state and
stencil comparison/operations. The default display owns a D32/S8 attachment per
swapchain image; offscreen framebuffers remain color-only.

Native driver fixes were required as well: depth tile mode, combined depth/stencil
allocation size, read/write plane addresses, and depth/stencil attachment clears.
Stencil-only clears preserve depth writes. The GPU oracle now checks masked
stencil geometry, stencil-only clear preservation, and nearer geometry hiding
farther geometry, in addition to the existing framebuffer checks. All 2,553 pixel
checks and Vulkan lifecycle cleanup pass. Host tests cover eighteen display
startup failure points, resource cleanup, retry, and frame lifecycle with
ASan/UBSan.

Grim also uploads its background occlusion texture as `GL_LUMINANCE_ALPHA`.
Missing support made Manny invisible even after the crash was fixed. Legacy
luminance/alpha uploads now expand into RGBA snapshots, preserving luminance in
RGB and alpha independently. Host checks exercise padded rows and subimages.
The latest visible run shows Manny with model textures in the first room:
`artifacts/scummvm-vulkan/grim-luma-room.png`. Runtime continues beyond the old
crash; full gameplay and physical PS4 remain unverified.

Current package SHA-256:
`a986e38ec670f3f04ecb9806902031b9e495c9d767d312f60f8653ed4e2fd305`.

Use PC arrow keys for the emulated D-pad, N for use/talk, V examine, B pick up/put
away, C inventory, and U plus movement to run. WASD remains the existing virtual
mouse mapping. No input-profile changes were needed. The PS4 SDL event backend now maps
OpenOrbis buttons 13-16 through the normal keymapper and ignores duplicate hat
events. The old hat path pushed synthetic events, bypassing keymapping and
preventing Grim from receiving its mapped arrow keys. A held Up arrow now walks
Manny out of the office into the corridor; evidence is
`artifacts/scummvm-vulkan/grim-input-before.png` and `grim-input-after.png`.
The visible session is left running in that corridor.

Remaining limits include masked partial-channel clears, color-only offscreen
framebuffers, unavailable front-face/culling APIs for EMI, and later capture
APIs such as `glCopyTexImage2D`. Upstream SetGamma/MakeSectorActive and other
runtime warnings still exist; this is not a warning-free or full-library claim.
