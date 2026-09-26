# Triangle strips for launcher surfaces (2026-09-26)

The GL frontend accepts `GL_TRIANGLE_STRIP` through both `glDrawArrays` and
`glDrawElements`. This supplies the primitive used by ScummVM's launcher shader
pipeline (`backends/graphics/opengl/pipelines/shader.cpp`). It does not yet make
the real launcher's context/capability initialization succeed.

## Implementation

Strips are expanded into a captured uint32 triangle-list index stream, reusing
the existing native pipeline rather than adding new Vulkan topology variants.
For input vertices 0,1,2,3,4, triangles become 0,1,2 / 2,1,3 / 2,3,4. Alternating
winding and the default last provoking vertex are preserved. Degenerate
triangles remain in the stream. Vertex IDs are unchanged, including for
attribute-free shaders using `gl_VertexIndex`.

Indexed input accepts the existing uint16/uint32 types and byte offsets into
an element buffer. Array draws generate indices without touching the bound
element buffer. Packed vertex data and expanded indices share one immutable
upload snapshot; later source mutations/deletion cannot rewrite recorded draws.

Expansion is checked before multiplying/allocating and combined with the
existing 16 MiB per-draw vertex/index budget. Range failures do not upload or
emit a draw. Zero-, one- and two-vertex strips emit no primitives. Existing lazy
frame acquisition/target preparation semantics remain, even for empty draws.
Nonzero `first` remains unsupported, as do primitive restart, line strips,
line loops, fans, base-vertex and integer/normalized attributes. No GL version
or extension claims were raised.

## Validation

```
scripts/check-ps4-frame-owner.sh
scripts/build-vulkan-probe.sh --sdlgl
xvfb-run -a -s '-screen 0 1280x720x24' \
  python3 scripts/run-opengnm-probe.py --vulkan --sdlgl
```

Host ASan/UBSan tests verify alternating winding over three triangles,
16-/32-bit shuffled and degenerate indices with poisoned prefixes, index/vertex
snapshots, attribute-free vertex IDs, unchanged element bindings, short strips,
negative/huge counts and out-of-bounds offsets.

The native SDL fixture now renders actual four-corner quads instead of oversized
triangles, cycling between array, uint16-indexed and uint32-indexed strips.
Index stores are overwritten/deleted before GPU submission. The independently
specified framebuffer oracle is unchanged: all 2,553 pixel checks passed in
shadPS4, including orientation, partial preservation, blending and retained
storage. Eight frames, replay and cleanup passed. The runner requires the new
strip marker, so an older diagnostic cannot silently satisfy this checkpoint.
Reports: `artifacts/vulkan/sdlgl/` and `frame-state-host.log`.

Diagnostic package: `dist/ScummVM-PS4-SDL-GL.pkg`, SHA-256
`6f4552f148c20334e8764979241f79564aae7a71d18a9d84fc943073b19fd9a3`.
The experimental full ScummVM package was not rebuilt for this primitive-only
change; its previous observed GLAD gate remains. Real launcher/game and physical
PS4 validation are still outstanding.

The existing non-SDL `--glindex` triangle path also passed its 3,933 sampled
pixels and cleanup with the updated core. Evidence:
`artifacts/vulkan/strip-regression-glindex.log`. The final native eboot was
restored to the new SDL strip diagnostic after this regression.
