# Penumbra PS4 startup tests

Tested 2026-09-27 with the experimental Vulkan package after the accepted
Watchmaker fixes. Eboot SHA-256: `55ad12975e5b3e2df242b4ce9ee2b5acb99a18a66c6fbb70fa37c95fb1446591`.

## Owned data and detection

Both extracted games are under `artifacts/Penumbra/extracted/app/`, with their
actual game root in the `redist` subdirectory. Local game-profile symlinks and
ScummVM targets are configured for `penumbraoverture` and
`penumbrablackplague`; game assets are not committed.

| Title | Penumbra.exe bytes | First-5000-byte MD5 | Detector match |
| --- | ---: | --- | --- |
| Overture | 2736128 | 202990aa064ae67a675e2b187384036a | The Penumbra Collection |
| Black Plague | 2985984 | a066f7284e063ac3e5d7409102b7a497 | GOG v1.0 |

## Initial shadPS4 result

Both are identified correctly, enter HPL1, and crash during
`hpl::cLowLevelGraphicsSDL::SetupGL()`, before gameplay. GDB catches a null
function call at RIP 0, with return address 0x166afe3. Subtracting the guest
load base 0x400000 gives ELF address 0x126afe3. Disassembly shows the preceding
indirect call through `glad_glHint`, for GL_PERSPECTIVE_CORRECTION_HINT /
GL_NICEST (LowLevelGraphicsSDL.cpp:201).

Evidence: `artifacts/scummvm-vulkan/penumbra-overture-gdb.log` and
`penumbra-blackplague-gdb.log`. The normal Overture startup attempt is also
in `penumbra-overture-first.log`.

The bridge neither exposes nor explicitly loads glHint. Subsequent SetupGL
calls include glEnableClientState/glDisableClientState, also outside the
current supported subset. Thus adding one hint function does not establish
HPL1 rendering support. A follow-up needs a supported HPL1 array/shader path,
then real menu/gameplay, sound and controls checks. Neither title is currently
playable in this tested PS4 build. No rendering changes were made by these
startup tests. Watchmaker's accepted renderer remains unchanged.

## Earlier HPL1 bridge checkpoint

The candidate eboot SHA-256 is
`917512df5943dcba48439c9244239beb5a96a56520b9b3ecf26f2c4a3d9d0b4f`.
Both Overture and Black Plague now pass SetupGL and compile/import the gamma
program in shadPS4. Both stop later on `hpl1_Fog_Solid.fragment`, whose
`sampler1D` is outside the bridge subset. Neither reaches the menu yet.
Evidence: `penumbra-overture-hpl-progress.log` and
`penumbra-blackplague-hpl-progress.log` in `artifacts/scummvm-vulkan/`.

Implemented so far:

- Validated GL hints, client-array declarations, client texture selection and
  orthographic matrices, with explicit context entrypoint loading.
- Float client arrays backed by CPU pointers or VBOs, CPU or element-buffer
  indices, bounded prevalidation and immutable draw snapshots. Both fixed
  rendering and shader inputs preserve caller state. Shader inputs cover
  position, color, normal and the first two texture coordinates.
- Legacy GLSL vertex builtins lowered to explicit vertex inputs, including
  the global attribute initializers used by HPL1.
- Independent rectangle texture bindings, target immutability, level-zero
  storage and restricted filtering/wrapping. Rectangle samplers normalize
  pixel coordinates using reflected dimension uniforms supplied at draw time.
  An earlier `textureSize` implementation compiled on the host but failed
  native program import; the dimension-uniform version passes that gate.
- HPL1 rectangle shader sources explicitly request their required desktop
  GLSL extension through `hpl1-experimental.patch`.

Validation: ASan/UBSan frame/state/texture-cache tests, shader compilation,
linking and reflection tests, and display-owner tests pass. Added checks cover
CPU-index order, captured VBO bindings, atomic rejection of invalid ranges,
programmable client inputs, state restoration, rectangle target isolation,
texture restrictions and gamma uniform reflection. Existing Grim, TWP and
fixed-function shader checks also pass. This is not full renderer validation.

Remaining work is substantial: sampler1D and cubemap storage/sampling,
more than two shader samplers, framebuffer-to-texture copies, programmable
immediate-mode drawing, additional fixed-function texture state and the
remaining HPL1 entrypoints. Do not substitute empty stubs or advertise these
capabilities just to advance startup. The shader inventory at
`artifacts/glslang/hpl-shader-inventory.log` has 40 standalone stages passing
and 36 rejected, including malformed upstream sources in some fallback
variants. These counts do not prove complete program-pair compatibility.

Continue with the fog shader and its one-dimensional falloff texture,
then test actual menu/gameplay, controls, audio and timing. Black Plague's
successful detection still does not establish gameplay-engine support.
The unrelated Director/Jonssonligan work in the checkout must remain separate.

A 40-second Watchmaker smoke run with the same candidate displayed the colored
Trecision intro logo and advanced into the opening animation. The log contains
no `PS4GL error` entries. This checks startup only, not the earlier door/lampshade
acceptance scenes or full-game behavior. Evidence:
`artifacts/scummvm-vulkan/penumbra-bridge-watchmaker-smoke.log`.


## Current bring-up: visible startup, later texture-combiner crash

Overture now passes fog, refraction, cubemap lighting and ambient shader
creation. It remained alive with audible sound but a black image in the first
base-level texture run. Skipping the identity gamma pass (gamma exactly 1)
then produced visible startup according to the user, before a later null
glTexEnvi call in SetTextureEnv. This establishes startup progress only.

Implemented since the earlier checkpoint:

- One-dimensional textures as single-row images and cube maps as six-face
  vertical atlases, with independent bindings and explicit shader lowering.
  Cubes currently support only square base-level faces, no seamless edge
  filtering or mipmaps.
- Eight shader samplers and immutable per-draw resource snapshots.
- GPU framebuffer copies through detile/readback and upload commands,
  including default-framebuffer row reversal. Host transfer tests pass;
  gamma-adjusted output still needs real GPU validation.
- Programmable immediate-mode quads, including three-component texture
  coordinates, plus RGB/BGR and RED/RG upload conversion.
- Removed an unused mismatched ambient-shader input.
- PS4 HPL textures explicitly use base-level filtering without mip generation.
  Distant texture shimmer is a known limitation, not full mipmap support.
- HPL uses the provided core active-texture API regardless of the ARB flag.
  Fixed 2D enable state is independent per texture unit.
- Identity gamma correction avoids the redundant copy and fullscreen pass.
- Bounded fixed texture combiners support modulate, replace and add using
  default texture/previous sources, plus the second RGB operand selection.
  Unsupported combiner settings remain explicit errors.

Host state/array/cache/copy tests and production shader compilation pass.
The new combiner candidate passes the prior crash and displays the opening
letter text in shadPS4. The process is left running for user testing.
Candidate eboot SHA-256:
`484fd45a14534b19f5bb37cd3854f7fe152264521c53cc9826d1fc8832201e49`.
Logs: `artifacts/scummvm-vulkan/penumbra-combiner-runtime.log`
and the adjacent baselevel/identity-gamma runtime logs. Neither game is yet
accepted as playable. Black Plague has not been retested past the earlier fog
checkpoint. Watchmaker must receive another runtime regression smoke after
these shared bridge changes; the earlier smoke predates them.


## Follow-up: shader program capacity

The combiner candidate later stopped when loading
hpl1_Fog_Trans/hpl1_Fog_Trans_Alpha. The first error is GL_OUT_OF_MEMORY
from glCreateProgram, followed by invalid program-handle errors. The bridge
had only 16 program slots; this is an internal capacity failure, not evidence
of exhausted console RAM.

Raised the bounded program table to 128. Retired names still remain reserved
until context teardown, protecting queued GPU commands; long sessions and
game restarts still need validation against this lifetime limit. Updated host
tests fill all slots, verify retired handles cannot be used, and check clean
exhaustion. Larger test contexts use static storage instead of the host stack.
Frame/state/cache/copy tests and git diff --check pass.

Rebuilt both the SDL context allocator and bridge/package. New eboot SHA-256:
`b692c9bc8b0fca886a8e0b3a66dc27b5fc847585f1171f873d208f332ade1472`.
Overture restarted under GDB, log /tmp/hpl-capacity-runtime.log.
Passing the previous failing scene is pending user/runtime verification.


## Menu visibility and first map loading

Menu batches were submitted with valid positions, RGBA atlases, depth testing
disabled and a symmetric orthographic projection. Setting only batched Z to
zero made the menu visible (user confirmed); that diagnostic was removed.
Changing the fixed vertex shader from matrix-matrix-vector multiplication to
explicit projection * (modelview * vertex) preserves the intended transform
and makes the real-depth menu visible in shadPS4 (user confirmed and selected
New Game). This points to the native matrix-matrix shader path; no general
compiler fix is claimed. Bounded diagnostic prints were removed as well.
Screen still occupies the requested 800x600 viewport inside the larger
display target; presentation scaling remains open.

Loading subsequently crashed at NewtonCreateBox, instruction movaps reading
an external float matrix cast to aligned dgMatrix. Primitive offset matrices
now copy with memcpy; rebuilt disassembly uses movups. The next real run
passed that point and reached a null glGenQueries call while creating halo
billboards. PS4 temporarily hides halo sprites and avoids requesting queries.
This is an explicit missing visual feature, not simulated query results.

Host frame/state/cache/copy tests and production shader compilation pass.
Current eboot SHA-256:
`45e45fc8cc02e1a23f0dddefc327b46469175df3b45b12de4aea5ee9eb8c8418`.
Restarted Overture under GDB; /tmp/hpl-menu-final-runtime.log captures the
current attempt. Passing map loading is pending; gameplay remains unverified.
Diagnostic game profile has show_pre_menu=false to reach the menu faster;
the new-game story intro is still enabled. Original profile snapshot:
 /tmp/hpl-menu-original.ini. Earlier crash/comparison logs are saved as
penumbra-menu-runtime.log, penumbra-clip-runtime.log and
penumbra-matrix-runtime.log in artifacts/scummvm-vulkan/.


## Occlusion query implementation and full drawable sizing

Replaced the temporary PS4 halo suppression with GL_SAMPLES_PASSED support.
The bridge owns bounded query names and Vulkan query pools, resets pools
outside render passes, and records precise begin/end queries around geometry.
It reads results only after the frame fence. Deleted pools remain alive until
the recorded command buffer is reset, including when a frame is replayed.
The HPL fetch loop polls once after swap and preserves the previous result
instead of busy-waiting indefinitely.

The native occlusion implementation previously wrote a literal zero using
EVENT_WRITE_EOP and treated a nonzero counter as availability. It now records
GFX7 ZPASS_DONE start/end counter snapshots, sums eight PS4 base-mode render
backend deltas, checks the counter-valid bits, and uses a separate EOP marker.
Completed zero samples are distinct from pending results. Sixteen counter
pairs are reserved to avoid an overrun; decoding currently targets base mode.
Blocking pre-submit result reads and GPU query-result copies are outside the
bridge subset. Timestamp queries are not used by HPL.

Packet layout reference:
[Mesa radeonsi query implementation](https://cgit.freedesktop.org/mesa/mesa/tree/src/gallium/drivers/radeonsi/si_query.c?id=1f31a216640f294ce310898773d9b42bda5d1d47).
The local shadPS4 source snapshot handles PixelPipeStatDump with synthetic
counters and does not implement the GFX7 ZPASS_DONE samples. Consequently
real halo occlusion cannot yet be accepted from that emulator path; unavailable
results are not replaced with invented visibility. Hardware visual testing or
a correctly implemented emulator counter path remains necessary.

Validation: scripts/check-ps4-occlusion.sh checks pending/completed-zero,
counter sums, invalid samples, output bounds, packet words, GL lifecycle,
repeated use, render-pass ordering and deletion after command-buffer retirement.
Frame tests verify harvest happens after successful fences and never after a
timeout. Existing state/cache/copy checks also pass.

HPL now requests 1280x720 on PS4, matching the existing native SDL display,
instead of leaving an 800x600 viewport within it. Menu virtual coordinates
remain 800x600 and stretch to that full drawable. This also keeps the backend
input resolution and screen-copy textures consistent. Runtime capture confirms the menu fills the 1280x720 game window, with
readable UI text (/tmp/hpl-stretch-runtime.log). Input selection and gameplay
remain separate runtime checks.

A separate loading crash occurred after CreateAnim2D returned null and
MaterialManager dereferenced it. The loader now diagnoses the material and
texture name before access; finding the underlying missing/failed texture is
still in progress.
