# TWP menu, texture lifetime and copy-volume fixes (2026-09-26)

The experimental SCVM00010 application now displays Thimbleweed Park's actual
Casual/Hard selection screen in shadPS4. The user confirmed the image, pointer
movement and starting the game. Sustained gameplay and physical PS4 remain
unverified; this is not a full compatibility claim.

## Why the first real frames failed

The first startup exhausted the 16 MiB command recording. Uploading a full
1280x720 linear CPU image into tiled render-target storage emits many small GPU
copy commands. TWP constructs three temporary render textures each frame.
The bridge imported old data even when the next operation cleared the entire
attachment, or the texture had been allocated with null (undefined) data.

A complete clear now skips the initial import. Null-data render textures skip
it as well. A subsequent CPU subimage update marks their shadow as defined and
conservatively restores the complete upload. Supplied pixel data and partial
clears keep the preservation path. Separately, the driver fills a complete
RGBA8 surface with one packed-color operation, retaining scratch/copy and cache
ordering. Partial clears retain the coordinate-aware preservation path.
Command capacity remains 16 MiB; no image-resolution or quality reduction.

After the first fix, TWP exposed a lifetime leak: its temporary destructor
unbound the FBO, deleted the texture, then deleted the FBO. The old subset
rejected the texture deletion, leaving storage behind each frame. Deleted
texture names now disappear immediately while unbound FBOs retain the original
object by slot until detached/deleted. Reusing the numerical name cannot
redirect an older attachment. The bound FBO detaches on texture deletion, and
recorded GPU storage survives until frame retirement. This follows the object
lifetime rules in the [OpenGL specification](https://registry.khronos.org/OpenGL/specs/gl/glspec30.pdf).

## Snapshot reuse and input

Within a frame, unchanged textures reuse an existing GPU snapshot. Keys include
CPU content revision, texture identity, render-target storage and sampler state.
Mutations get distinct snapshots; borrowers never destroy shared resources.
Snapshots still retire at the frame fence, so cross-frame reuse is future work.
Borrowed render targets are not charged a second time to the upload budget.

The isolated TWP keymap now leaves left-stick axes to ScummVM's analog virtual
mouse. Its old digital arrow bindings consumed the axes before that mapper,
which made shadPS4 mouse-to-stick movement fall below the digital threshold.
Keyboard arrows, engine button bindings and physical stick bindings remain.
The user confirmed the crosshair moves and clicks can start the game. At low
frame rates short clicks can still be missed because pad polling is infrequent.

## Measured result and validation

On the same difficulty menu, the last 40 logged present intervals averaged
414 ms (2.4 fps) before undefined-initialization removal and 90 ms (11.1 fps)
after it. The latter was a separate Xvfb/profile run while the user's older
instance was open; this is not a controlled hardware benchmark. Typical menu
recordings fell from roughly 3.2 million dwords to a few thousand. Pointer
telemetry in the newer visible instance reports roughly 10–12 updates/second.
The new package is running visibly after the user's requested restart.

- Host ASan/UBSan frame/state tests pass, including deletion through bound and
  unbound FBOs, retained objects, name reuse and final CPU memory release.
- Final SDL fixture: **2,553 pixel checks and lifecycle cleanup pass**. Alternating
  frames cover supplied-content preservation versus full-clear discard; null
  initialization skips copying; recorded images survive reversed deletion order.
- Native texture fixture: **3,933 pixel checks and cleanup pass**. Repeated draws
  reuse both snapshots; pixel/filter changes get new snapshots. Mutation,
  reallocation and deletion before submission preserve earlier results.
- Checked driver patch reverse-application and bridge loader (78 entries).

Evidence: `artifacts/scummvm-vulkan/twp-fast-clear.png`, `twp-analog-pointer.png`,
`twp-menu-driver.log`, `twp-perf-driver.log`, `twp-perf-bootstrap.log`, plus native
fixture reports in `artifacts/vulkan/sdlgl/` and `artifacts/vulkan/gltexture/`.
The headless performance profile is `.tools/xdg-twp-perf`, with separate saves.

Current application SHA-256:
`195df3ad85406a65bd0bb81656b7632b1c8bad2ae5d4bb20aba19aad1e1f0e27`.
Final SDL fixture:
`b5f4d8fd46888c3d39028a0ccf076a34110529da958ee17610050d5402c917c9`.
Texture-cache fixture:
`bf29a9586f247404ceddf28725e83098b27c133db84b9c82e47a197da011667e`.

Next: reuse unchanged textures across retired frames, profile remaining GPU
allocation/fence costs, then test scene transitions, capture/save, and longer
play. glReadPixels, line primitives and finite lifetime shader slots remain
known gaps. Normal package 1.17 is unchanged.
