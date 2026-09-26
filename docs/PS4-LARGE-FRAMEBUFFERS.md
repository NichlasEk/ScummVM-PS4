# Screen-sized framebuffer recordings (2026-09-26)

The experimental bridge accepts textures up to 2048x2048, retaining its 64 MiB
CPU texture budget. The SDL/Vulkan framebuffer fixture now renders into a
1280x720 offscreen image instead of 64x64. Its independent expected image is
unchanged: asymmetric drawing, partial clears, blending, old storage snapshots,
feedback rejection, persistent targets, replay and destruction are exercised.

The larger case exposed two command limits. A 256 KiB recording buffer ran out
before the first present. Raising recording capacity alone then produced
3,637,456 dwords and failed GNM's 0x3ffffc-byte submission limit. The driver now
reserves a bounded 16 MiB recording allocation and partitions it into complete
PM4 packets below the per-submission limit. The exercised frame uses four
parts, submitted in order in one GNM call. The original allocation stays alive,
preserving addresses of inline data. Entire streams are validated before any
part is submitted; malformed/truncated packets or excessive chunk counts fail.
Fence/semaphore completion remains after the full recording.

This is a bounded correctness step, not an optimized upload path. The recording
allocation increased from 256 KiB to 16 MiB per command buffer; tiled uploads
still emit many small DMA packets. Larger or more complex frames can still
exhaust that bound. A future GPU tiling/copy path should reduce command volume.
No physical PS4 result or general OpenGL conformance is claimed.

## Validation

- `scripts/check-ps4-frame-owner.sh`: ASan/UBSan state tests, including 2048x2048
  allocations to the 64 MiB budget, rejection without mutation, replacement
  with 1280x720 storage, and last-pixel subimage addressing.
- `scripts/check-ps4-pm4-chunks.sh`: ASan/UBSan packet-boundary, payload/header,
  malformed/truncated stream, maximum chunk count and 16 MiB coverage tests.
- `scripts/build-vulkan-probe.sh --sdlgl`, then
  `xvfb-run -a -s '-screen 0 1280x720x24' python3 scripts/run-opengnm-probe.py --vulkan --sdlgl`:
  **2,553 pixel checks passed**, followed by Vulkan lifecycle cleanup.

The SDL fixture now waits for the final draw loop and captures the settled
X window via ImageMagick. F12 during long recordings captured an intermediate
black frame with only the partial clears; F12 after the final frame waits for a
new host present that the idle fixture never produces. The expected pixels were
not relaxed. This capture requires a dedicated 1280x720 Xvfb display.
Evidence is in `artifacts/vulkan/sdlgl/`, with a copied driver log showing chunk
counts and the host results in `artifacts/vulkan/`.

## Remaining Thimbleweed Park gate

The user's Start attempt reaches the game's explicit shader-feature check and
returns its unsupported-shaders error. Shader support is deliberately still
not advertised. Besides the remaining GL operation/lifetime work, SDL's
`detectOpenGLFeaturesSupport()` attempts a separate hidden 32x32 window; our
fixed single-window adapter rejects it. Capability discovery must use the real
validated context, without a second window, before enabling engine shaders.

Rebuilt experimental application package SHA-256:
`1b53e94e01ce9c97d8925c5f608db7583c4d3d5015ebec87fffcbf44938f73a8`.
Validated SDL fixture package SHA-256:
`48425645a04bbbe1adddddd7d86068e5058b1b76d6d569b0d8a3c25745d1d5ca`.
The stable 1.17 package remains unchanged:
`c519b288b663d4f7b5db0e86b23a2a7031f1dacc97984edaf9b807f3738191e1`.
