# Frame upload memory and warning audit (2026-09-26)

The old bridge exhausted its 64 snapshot records after about 32 draws, even
with only 528,608 payload bytes. An 80-repeat native fixture reproduced
GL_OUT_OF_MEMORY at repeat 31. This was a bookkeeping limit, not evidence of
system RAM exhaustion.

Uploads now share mapped 1 MiB pages with aligned vertex/index/uniform slices.
Uniforms copy only reflected stage ranges instead of a fixed 16 KiB per draw.
Records grow on demand to a bounded 4096; both frame payload and reserved upload
pages have separate 64 MiB limits. These are bridge budgets, not a process-wide
memory limit. Descriptors, textures, records and pages retire after the frame
fence. Failure logging identifies the allocation category and current usage.

## Evidence

- Native texture fixture: 83 draws, 166 records, 32,446 payload bytes, one
  1 MiB upload page; 3,933 pixel checks and lifecycle cleanup pass.
- Native SDL/FBO fixture: 2,553 pixel checks and cleanup pass, including CPU
  image import at a nonzero shared-buffer offset and retained texture lifetime.
- Host ASan/UBSan frame/state tests, 78-entry loader and GLSL reflection tests pass.
- Separate TWP profile `.tools/xdg-twp-memory` reached the difficulty menu,
  tutorial pages and opening titles. No GL_OUT_OF_MEMORY in this run. A later
  scene reached 131 records but exposed GL_INVALID_OPERATION on texture upload
  and drawing; that is an outstanding error, not a successful scene test.
- `artifacts/vulkan/memory-before-probe.log`, native fixture report directories,
  `artifacts/scummvm-vulkan/twp-memory-{bootstrap,scummvm}.log` preserve evidence.

Experimental package SHA-256 at this checkpoint:
`52b614e5f67fc6eebb8e763bd3b1a4428a7a3d5f455cec6c7d0d3bcbf5c851bc`.

## Remaining warnings under investigation

Startup includes filtering on an unavailable texture, an unsupported optional
Libretro pipeline, missing GUI icons, audio rate negotiation and successful
filesystem listings incorrectly logged as warnings. TWP also reports unfinished
controller/UI/music/statistics, actor slot and object effect functions.
The user requested a warning-free result: fix behavior, keep actual failures
visible, and move only normal diagnostic events to debug logging.

Texture reuse currently lasts one frame. Cross-frame caching, sustained scene
transitions and physical PS4 validation remain separate follow-up checks.
