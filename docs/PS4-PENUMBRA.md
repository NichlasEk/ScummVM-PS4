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

The loading crash was narrowed down to a null texture returned to
MaterialManager (the earlier suspected animated-texture branch was not the
failing path). Direct start-map loading reproduces it without replaying the
story intro: boat_barrel.mat / boat_barrel.jpg, a present 512x512 JPEG.
The bridge had 66,545,960 resident CPU texture bytes and rejected its 1,048,576
byte upload at the 67,108,864-byte (64 MiB) limit, returning GL_OUT_OF_MEMORY.
Evidence: /tmp/hpl-texture-runtime.log. The bounded CPU texture budget is now
256 MiB; upload failure diagnostics identify the texture, dimensions and GL
error. The budget test fills the configured cap, checks atomic rejection,
shrinks storage and verifies all bytes are released. The first room now loads with 86,469,952 resident CPU texture bytes; GPU
cache memory is separately accounted.

Follow-up: the 256 MiB candidate passed the barrel texture load and reached
RenderWorld, then stopped at an unresolved glTexEnvfv. Added bounded
texture-environment constant color, RGB/alpha source and operand state, and
replace/modulate/add/interpolate lowering for the existing unit-zero fixed
shader. Host state and production shader compilation checks pass.

The next run reached shadow rendering and found unresolved
 glStencilFuncSeparate in SetStencilTwoSide. Separate front/back stencil
function, reference, masks and operations now flow through the bridge to
VkPipelineDepthStencilStateCreateInfo. The native driver already programs
separate back-face state. Host tests cover independent faces, invalid-call
atomicity and legacy both-face setters. The subset-loader audit now recognizes
explicit aliases and passes all 128 required entries.

Candidate eboot SHA-256:
9479fb7c7b6125d8e76ec08df19ea8c04f1ec51e41d5ba6655f4751b4b75fff7
The /tmp/hpl-stencil-runtime.log run still used direct-map loading: the
runtime had moved show_menu=false within the INI section, so the initial
exact-string restoration missed it. It reached DrawQuadMultiTex and stopped
at unresolved glMultiTexCoord3fARB. Full post-intro gameplay is not yet
validated. These follow-up fixes are not included in checkpoint 92a64d7.


Added immediate multitexture coordinates and the ARB alias, with independent
unit-zero/unit-one coordinates fed to the existing legacy shader attributes.
The host test verifies the second coordinate set survives actual vertex
packing and programmable-state restoration.

That candidate reached GPU submission but shadPS4 stopped in
PM4CmdEventWriteEop::SignalFence: occlusion completion had INT_SEL=3 from the
generic OpenGNM helper. Completion now emits the same polled INT_SEL=0
signal used for queue fences. The native packet test checks event index,
data selection, interrupt selection, address, completion value and buffer
bounds. Driver patch reverse-apply validation and native rebuild pass.
Latest runtime: /tmp/hpl-eop-runtime.log.


### Accepted first-room checkpoint (2026-09-27)

The user confirmed the rendered boat cabin looks correct after disabling
postprocessing. The accepted local target configuration is:

```ini
[penumbraoverture]
post_effects=false
show_pre_menu=false
show_menu=false
```

These settings are in the untracked runtime scummvm.ini under
.tools/xdg-scummvm-vulkan/shadPS4/data/scummvm-vulkan/. Apply post_effects=false
when reproducing this checkpoint; it is not yet the engine default. The two
show_* settings bypass the menu for direct first-room debugging. Set both to
true for normal startup. The complete intro-to-game transition has not been
revalidated with this final candidate.

The additional missing geometry came from exhausting the 512 GL buffer-object
slots. The bounded limit is now 4096. Client shader packing resolves attribute
buffer objects once per draw instead of searching the buffer table for each
vertex. The first room now renders its desk, map, bottle, walls and lighting.
Evidence: /tmp/hpl-nopost-runtime.log, /tmp/hpl-nopost.png and the user's final
cabin screenshot. A 120-frame interval took 10.783 seconds (about 11 fps);
performance and mouse control still need work. Postprocessing enabled still
produces black output, so bloom/depth-of-field are not accepted as working.
The diagnostic upload/reuse deltas in that log cross per-frame counter resets
and must not be interpreted as cumulative upload statistics.

Looping world ambience affected by environmental audio now uses 30% output
volume on PS4, preserving the logical volume used for fades. Non-looping speech
is unchanged. The user confirmed the quieter background sounds better.

Host validation passed for frame ownership, GL state/vertex packing, shader
compilation, all 130 subset-loader entries and occlusion completion packets.
The shared Watchmaker path has not had a new visual regression run. Real halo
occlusion remains subject to the emulator limitation described above.


### Mouse input follow-up

HPL previously overwrote relative movement with each mouse event, losing all
but the last report between game updates. Button/wheel events could also erase
pending motion. Mouse::processEvent now accumulates motion reports only;
GetRelPosition consumes and clears the accumulated delta as before.

On PS4 the existing Options -> Controls -> Mouse Sensitivity control defaults
to 2.0 (range 0.2–5.0). Left click increases and right click decreases by 0.2.
Changes persist immediately in the target's hpl_mouse_sensitivity INI key,
stored as hundredths. This replaces the otherwise transient HPL XML setting
for this control. Current local launch restores show_menu/show_pre_menu=true;
post_effects=false remains. Build/package passed. The user accepted Overture as finished after this
control update (2026-09-27). Runtime: /tmp/hpl-input-runtime.log. The known
postprocessing fallback and hardware-validation limits above remain recorded.


### Black Plague: first-room compatibility blocker (2026-09-27)

The user confirmed the menu works. New Game then ran Overture's hard-coded
IntroStory and requested absent intro_02_*.ogg etc., finally loading the absent
Overture map level00_01_boat_cabin.dae. The caller continued without a world,
crashing in iCharacterBody::SetYaw (guest 0x16ff7a4).
Evidence: /tmp/hpl-blackplague-runtime.log.

Black Plague's own config/default_settings.cfg specifies level01_cells.dae,
link01. The engine now chooses that default for penumbrablackplague and does
not run the Overture-only IntroStory for it. This is a temporary missing-intro
limitation, not an implementation of Black Plague's intro.

A direct load of the correct cells map exposed a deeper unsupported feature:
cEntityLoader_GameEnemy::AfterLoad supports only Dog, Spider and Worm. The
map requests Infected; pEnemy remains null and the following msSubType string
assignment crashes at guest 0x3fb154e. Captured stack includes AfterLoad at
0x17822f8 and the string Infected. Evidence:
/tmp/hpl-blackplague-cells-trace.log. A guard now reports the unsupported enemy
instead of dereferencing null. It intentionally does not silently remove the
enemy or replace it with a different creature. Black Plague is NOT playable;
its missing gameplay/AI support needs implementation and script validation.

Local diagnostic profile uses show_menu/show_pre_menu/show_intro=false and
starting_map=level01_cells.dae. Overture's accepted target is unchanged.

Guard validation: rebuilt/package passed, loader audit passed all 130 entries.
/tmp/hpl-blackplague-guard.log reports the unsupported Infected entity and exits
with status 1 instead of the null-pointer access violation.

### Black Plague gameplay implementation candidate

Implemented a distinct Infected humanoid class using HPL's character bodies,
perception and navigation. It implements idle, patrol/wait, investigate, move,
pursuit, melee, flinch and death states, reads the shipped movement/damage/FOV
properties and uses the humanoid animation names. Flashlights attach to the
LeftHand bone and switch movement animations. Flashlight presence is saved.
This is an initial implementation, not a claim of exact original AI parity:
weapons, throat grabs, door breaking and later scripted enemy callbacks remain
unfinished. Other unsupported enemy subtypes retain an explicit error.

The first cells map and global script now compile. Added wheel entity loading,
accumulated hinge angles, limit callbacks and saved angle/callback state;
locked link/door feedback and persistence; radio stop/fade and positioned
messages; area teleport; low-battery callbacks; and overloads for Black
Plague's body flags, loading-screen images and links. Sector sound environment
selection is tracked, but the existing audio backend advertises no environment
support, so reverb is unavailable (reported instead of calling fatal stubs).
Script runtime exceptions now propagate to diagnostics instead of returning
success unconditionally. Existing Overture signatures are retained.

Runtime /tmp/hpl-bp-save-runtime.log reaches the actual cell room, invokes its
scripts without reported compilation/execution errors or the prior null
crash. Capture /tmp/hpl-bp-first-script.png shows the cell; the later scripted
camera capture is /tmp/hpl-bp-running.png. The new run is deliberately a direct
map test, with the standalone Overture intro suppressed. Cell-room geometry
uses ~107 MB texture storage and 336 draw snapshots. A measured 120-frame
interval took 32.767 seconds (~3.7 fps): performance remains a substantial
limitation. Black Plague mouse sensitivity is locally 400 (4.0), independently
of Overture's default 2.0, following user feedback that motion is smooth but
turns too little. User interaction and actual save/reload behavior still need
validation; compilation/serialization declarations alone do not prove them.

Validation: native build/package pass, loader audit pass, and an isolated
baseline->patch application exactly reproduces all 43 HPL source files and
reverse-checks successfully. The patch was consolidated so newly added files
can be reversed/reapplied reliably by the overlay builder. Watchmaker/Overture
have not been rerun after these shared HPL changes.

A static call-name audit finds no remaining missing registered function names
in level02_vents, but does not verify signatures or behavior. Later maps still
request unsupported APIs including OpenScreenGui, lever callbacks/stuck state,
MoveEntityToArea, dream filters, Infected weapons, notebook callbacks, gas-mask
queries, stick-area attachment, wheel connections, security cameras and ending
sequences. These are outstanding implementation work; Black Plague must not
be described as complete or fully playable from this first-room checkpoint.

### Requiem initial runtime test

Requiem is bundled inside Black Plague/redist, with Requiem.exe and
expansion01/. Added a separate experimental detector/target penumbrarequiem
for the owned executable (2998272 bytes, first-5000-byte MD5
8810d6b5d6790595dab4aae580d6a57c). Its startup selects requiem_resources.cfg,
requiem_materials.cfg, expansion01/config/requiem_game.cfg, English_exp.lang,
requiem_global_script.hps and level00.dae/link01. It does not run Overture's
standalone intro. Local target uses direct map loading, post_effects=false,
and hpl_mouse_sensitivity=400. Shared game assets remain unchanged.

Build/package and subset audit pass. /tmp/hpl-requiem-runtime.log confirms
correct target selection and first-map rendering (~66.9 MB textures, 248 draw
snapshots; a 120-frame interval took 10.150 seconds). Neither global nor first
map scripts compile yet: TurnOffGlowStick, DropFlare, StartDreamFilter,
StopDreamFilter, StartImageFlash and StartEnding1 are unimplemented. This is a
rendering/input trial only, not playable Requiem progression. The test window
is left open for visual feedback.
