# RGB texture uploads (2026-09-26)

The local ScummVM OpenGL backend selects RGB byte and RGB565 uploads as well as
RGBA byte textures. The bridge now accepts these source layouts in both
`glTexImage2D` and `glTexSubImage2D`:

| Format | Type | Client bytes per pixel |
| --- | --- | --- |
| RGB | UNSIGNED_BYTE | 3 |
| RGB | UNSIGNED_SHORT_5_6_5 | 2 |
| RGBA | UNSIGNED_BYTE | 4 |

Packed words use host byte order and unaligned reads via memcpy. Each 5/6-bit
channel is normalized to the nearest 8-bit value. All uploads become RGBA8 CPU
storage before the existing immutable GPU snapshot path. Unpack alignment
1/2/4/8 applies to source rows, including subimages; destination rows use the
texture's full width. The existing RGBA byte row-copy fast path is retained.

Internal RGB/RGB8 storage forces alpha to 255, including later RGBA subimages.
Internal RGBA/RGBA8 storage preserves source alpha or supplies 255 for RGB input.
Null image allocation remains deterministically zeroed (opaque for RGB).
Allocation accounting still uses four bytes per pixel, with the existing
1024-dimension and 64 MiB limits. Invalid calls preserve previous storage.

RGB internal formats are **upload/sample-only** for now: attaching one to an
FBO returns FRAMEBUFFER_UNSUPPORTED. Rendering into the RGBA backing image
would otherwise expose writable alpha on a texture that must sample opaque.
RGBA render targets continue to work. Packed RGBA, reverse-packed RGB565,
BGRA, palette/luminance formats, pixel row-length/skip/swap state and mipmap
uploads are not implemented by this change.

## Validation

`scripts/check-ps4-frame-owner.sh` runs ASan/UBSan state tests. Coverage includes
odd-width two-row images with all four alignments, unaligned RGB565 sources,
primary and intermediate packed colors, partial updates with padded source rows,
opaque RGB alpha, unchanged neighboring texels, invalid-call atomicity,
zero-sized subimages, accounting and FBO format redefinition.

The native `--gltexture` fixture now uploads one RGB565 and one padded RGB8
texture alongside an RGBA texture. It changes an RGB subimage after the first
draw, then reallocates/deletes textures before submission. The unchanged pixel
oracle passes 3,933 sampled pixels (246 left, 248 right, 3,439 background), with
eight rendered frames and native cleanup. The runner requires the new upload
marker on every frame. Evidence: `artifacts/vulkan/rgb-regression-gltexture.log`
and `artifacts/vulkan/gltexture/triangle-result.txt`.

Reproduce with `scripts/build-vulkan-probe.sh --gltexture`, followed by
`xvfb-run -a -s '-screen 0 1280x720x24' python3 scripts/run-opengnm-probe.py --vulkan --gltexture`.
The experimental SDL package is rebuilt separately because the context layout
changed. These diagnostics do not demonstrate actual ScummVM launcher/game
startup or physical PS4 execution. GL_VERSION stays at the bounded subset's
0.0; the real application package has not been rebuilt past its known GLAD gate.
