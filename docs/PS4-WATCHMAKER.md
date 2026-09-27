# The Watchmaker: experimental PS4 bring-up

Checked 2026-09-27 against the current experimental Vulkan build.

## Current checkpoint

User accepted the corrected lighting and requested commit/push. The game runs
through startup and the opening dialogue with working WAV effects and speech.
Playback in shadPS4 is still slow/stuttery, explicitly reconfirmed by the user.
The apparent disappearing seated character was subsequently seen completing
his slow animation; this is not a confirmed missing-mesh defect.

The sections below record the bring-up chronologically, including superseded
hypotheses. This is not a full-game or physical PS4 compatibility claim.


## Owned game data

`artifacts/thewatchmaker/THE_WATCHMAKER.iso` extracted successfully with 7-Zip.
The source image was not modified. `artifacts/thewatchmaker/game` points to
`extracted/program files/Trecision/The Watchmaker`; its `Speech` link points
to the extracted disc's Speech directory (6,750 files).

`Data.wm` matches the English retail entry in the checked-out engine detector:

- Size: 6,017,156 bytes.
- MD5 of the first 5,000 bytes: `a1675e8fd32c2c1eaed88e292a0d153c`.

This validates the detection signature, not completeness or gameplay.

## Original blocker

`artifacts/scummvm-vulkan/configure.log` explicitly disables The Watchmaker
because classic OpenGL is missing. Its only other skipped engines are TestBed
and Playground3D. `build/scummvm-vulkan/config.h` leaves `USE_OPENGL_GAME`
undefined; `config.mk` leaves `ENABLE_WATCHMAKER` disabled.

The engine's renderer requires fixed-function calls including `glBegin` /
`glEnd`, vertex/color/texture-coordinate submission, matrix stacks and
`glAlphaFunc`. The bridge at that checkpoint did not implement these entrypoints.
Enabling the configure flag alone would not provide a working renderer.

At the initial checkpoint no Watchmaker PS4 runtime launch had been performed: the engine
is absent from the executable. The running Jönssonligan session was left intact.

Next work is a fixed-function translation layer or a shader renderer for the
engine, with matrix/geometry/texture/alpha validation before enabling the engine
and attempting its real startup. Startup, input, sound, save/load, progression
and physical PS4 validation remain open.

## Experimental translation path

The isolated Vulkan source overlay now selects Watchmaker using the shader
backend dependency. It does not enable `USE_OPENGL_GAME` or advertise general
classic OpenGL support to other engines. `ps4gl-fixed.inc` lowers the engine's
immediate triangles and quads to existing shader draws, using separate modelview
and projection stacks, current vertex colors/UVs, texture modulation and alpha
comparisons. A fixed 384-vertex batch prevents unbounded CPU accumulation;
programmable buffer, attribute and program state is restored after each batch.
Submission remains owned by presentation; this is not a general `glFlush`
implementation. Lighting, fog, texgen and enabled face culling remain unsupported.

The engine's nearest-filtered texture path uses edge clamping. Base-level
DXT1/3/5 uploads expand into the existing bounded RGBA8 texture storage. The
format is implemented from the [Khronos S3TC specification](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt).
No full S3TC extension is advertised (compressed subimages and mipmaps are not
implemented). The owned archive contains 2,793 DXT1 texture entries.

Validation before runtime:

- `scripts/check-ps4-frame-owner.sh`: ASan/UBSan state and draw tests pass,
  including matrix post-multiplication, stack limits, quad winding, incomplete
  primitives, multiple bounded batches and preservation of programmable state.
- The same suite checks BC1 transparency/opaque RGB, BC2 explicit alpha,
  both BC3 interpolation modes, small edge blocks, length/bounds rejection and
  preservation of previous texture contents on invalid input.
- `scripts/check-ps4-glslang.sh`: both actual fixed-function shader variants
  compile/link, along with the existing original Grim/TWP regression pairs.
- The changed bridge C unit cross-compiles with `-Wall -Wextra -Werror`.

These tests alone do not establish Watchmaker gameplay or physical PS4 support.

## Runtime debugging, 2026-09-27

The experimental build now launches the owned English release in shadPS4.
Mounting the GameData.wm index as a SearchMan archive makes packed T3D resources
available to the engine. The texture record pool is 1,024 entries; the aggregate
RGBA allocation limit remains 64 MiB. This is not yet a playable-game claim:
logo rendering is visibly incorrect and intro progression is under test.

The initial apparent hang was traced to omitted Win32 record padding in Data.wm
readers. Correctly sized records recover intro dialog 99's RTV item 339 instead
of zero. Text loading now reads the remaining file bytes, bounded by the text
buffer capacity, rather than requiring the full 512,000-byte capacity on disk.
Dynamic credit arrays transfer ownership instead of shallow copying temporaries.

`scripts/check-watchmaker-data.py` compiles the production readers under
ASan/UBSan and checks disk record lengths, padding sentinels and ownership. Its
optional owned-English-data check confirms all table offsets through the intro
command sequence. These tests pass; no commercial game data is embedded.

The first advancing intro reached a Common::Array assertion. A debugger stopped
at guest __assert_fail, with return address resolving to CameraMan::ProcessCamera.
Forced cameras must bypass the player-grid lookup; nearby-camera search also
had OR instead of AND in all four bounds checks, in two callers. Both are fixed.
Runtime confirmation of this next step is recorded separately below.

Reference logs: artifacts/scummvm-vulkan/watchmaker-intro-progress.log,
watchmaker-bounds.log, and watchmaker-camera-grid.log. Physical PS4 validation,
correct rendering, gameplay/input and save/load remain open.

### Confirmed checkpoint: main menu reached

`watchmaker-movie-frame.log` and `watchmaker-movie-frame.png` record the actual
PS4 executable in shadPS4 reaching the game's main menu. The user independently
confirmed the transition. The intro still renders incorrectly; this proves
startup progression, not gameplay or correct rendering.

`rSetMovieFrame` and `rGetMovieFrame` now use the existing material movie player
instead of stubs; seeking updates its timing origin. The prior camera-grid
assertion and fatal movie-frame stub no longer block the menu in this run.
`scripts/check-watchmaker-camera.py` passes ASan/UBSan and checked-container
execution for forced off-grid cameras and all four nearby-camera search edges.
The consolidated engine patch passes exact forward/reverse byte comparison.

Checkpoint eboot SHA-256:
`a0fc2474e41693585e6ad3d8fcce776f3beefaccfcf0d65fd958d8e21d366d34`.
Package SHA-256:
`4334d64401c33441eb4e6a5d598db2e91ea8fc155ec873126999fd324294f4b8`.

Next: correct intro camera/projection and screen-space colored/alpha triangles;
then validate Play the game, input, scene progression and save/load. In
particular drawPrimitives currently submits screen-space coordinates through
3D matrices and ignores vertex alpha/color. Do not claim a finished renderer.

### Follow-up: Play the game and intro layers

The main-menu checkpoint subsequently failed after Play the game. First the
viseme/lip-sync code hit fatal Randomness stubs; these now use a named ScummVM
RandomSource, preserving the inclusive random ranges. The next run reached a
null read in Fonts::getTextDim (guest 0x364d2b4 in the screen-overlay build).
Renderer::_fonts was never initialized; its constructor now points to the owning
WGame's Fonts instance. Runtime verification continues in watchmaker-font-owner.log.

Screen-space triangles now use identity matrices and viewport-normalized
coordinates, preserving their packed color and alpha via glColor4f. The bridge
regression checks per-vertex alpha survives quad expansion and glColor3f resets
alpha to one. ASan/UBSan bridge tests pass. The user reports the intro looks
better; geometry/material problems remain. The prior note about drawPrimitives
ignoring color/alpha describes the older checkpoint and is now superseded.

The engine still contains significant upstream stubs, notably bitmap surface
creation/naming/release. Do not treat reaching the menu as proven gameplay.

### Persistent menu and first scene (follow-up)

The font-owner build continued processing room 91 behind an apparently frozen
menu. Debugger sample: Game:Main in ps4glFrameSubmit/vkWaitForFences while the GPU
thread was active; repeated movie updates continued. The OpenGL back-buffer
implementation retained old blit references while Regen assumed persistent
pixels. Regen now rebuilds the list from current PaintRect every frame. The user
and a capture confirmed the bedroom scene becomes visible after Play.

The engine's per-frame color/depth clear was commented out. It is now restored,
with black clear color and depth writes enabled during the clear. Missing
combined lightmap materials previously caused entire faces to be discarded;
they now retain base-material geometry, with an explicit one-time warning.
Combined lightmap lighting remains unsupported.

Next fatal stub reached was rSetBitmapName during dialogue text construction.
The bitmap allocator/name/release functions now use the active renderer's bitmap
pool. Virtual surfaces reserve dimensions without allocating GPU textures;
compositing translates/clips/scales nested glyph rectangles. The renderer owns
and clears the compatibility pointer over its lifetime and releases bitmap
textures on destruction. Pool exhaustion is checked before indexing.

Tests passed under ASan/UBSan:
- scripts/check-watchmaker-redraw.py: 2,000 frames remain bounded, removed menus
  disappear, replacement overlays contain no old entries.
- scripts/check-watchmaker-composite.py: virtual allocation marker, nested glyph
  placement, clipping, scaling and cleared blits.

Current live run: artifacts/scummvm-vulkan/watchmaker-scene-text.log.
Current eboot SHA-256: c21a07fd7e780cc2bc1d439edde2d4630f5192abd9e08d5e1bea84b4a7726e4d.
Current package SHA-256: 6a3e0b38f44276bd24c0e9f3c39e23249604dbeafce419cd69c302107b6132d9.
Correct scene rendering and progression remain acceptance gates.

### Trail isolation and remaining materials (2026-09-27)

The repeated clears reached PS4GL, but the previous DMA color clear did not
remove cached display pixels in shadPS4. An actual textured fullscreen draw
removed old menu/Exit pixels. Its initial diagnostic runs were invalid:
`g_renderer` was null, so guarded diagnostic code never executed. Initialize
it to a real stateless renderer. The earlier inference that depth clearing
caused color clearing to fail was not established.

The workaround now lives in the native Vulkan full-attachment color clear:
- Upload the embedded clear shader into aligned GPU memory, retaining its
  trailing metadata. The code offset inside the binary is not GPU-aligned.
- Set full viewport/scissors, disable depth/blending, clear the target and
  restore the application's pipeline, viewport and scissors.
- The embedded VS emits four corners of a rectangle. Three vertices in a
  triangle list only cover half the target. Both color and depth clear draws
  now use four vertices in a triangle strip.
- Partial color clears retain the existing tiled scratch path.

`watchmaker-native-clear-aligned.log` exposed a diagonal half-clear. The
subsequent `watchmaker-full-clear-strip.log` run and user screenshots show
that diagonal and old menu pixels removed. Missing room surfaces remain;
this is not a claim of complete rendering or physical PS4 validation.

Validation: build-vulkan-probe --rectclear and run-opengnm-probe --vulkan
--rectclear passed 1,230 actual pixel checks, all four quadrants, readback
resubmission and Vulkan lifecycle cleanup. Reports: artifacts/vulkan/rectclear.
The native changes are recorded in patches/opengnm/vulkan-runtime.patch;
reverse-apply validation passes. The Watchmaker overlay passed an exact
forward/reverse roundtrip (17 files), redraw and bitmap composition tests.

Next material correction: drawIndexedPrimitivesVBO unconditionally enabled
alpha blending and replaced the blend factors, overriding the opaque pass.
It now respects the pass's blend state. Visible test is running in
artifacts/scummvm-vulkan/watchmaker-material-blend.log. Rendering correctness
is still pending. Eboot SHA-256:
73f986920c1081510dffa65a016253119e681d4659a0028f2d3bb5022c57031c.

### Vertex-lighting corrections (2026-09-27)

The original reference screenshots show a dark bedroom, a sleeping character,
a complete bedside table/clock and shaded bed. Missing faces/character and the
pulsating logo remain explicit unresolved acceptance failures.

Three concrete upstream port bugs corrected in the overlay:
- Packed RGB extraction masked bits but never shifted them down. Lighting
  arithmetic and the 254/254/254 lightmap marker therefore received incorrect
  values. Packing now uses unsigned masked bytes, avoiding signed-shift UB.
- Indexed drawing replaced every computed diffuse color with white. It now
  sends the actual four vertex channels to the fixed-function adapter.
- Four loops in LoadLightmaps repeatedly processed FList[0] instead of FList[j].
  They now update each face's lightmap/material and vertex-light data.

scripts/check-watchmaker-lighting.py passes ASan/UBSan using production macros:
all byte values, half-intensity lighting, preserved alpha and lightmap marker.
Experimental build succeeded. Visible verification is pending in
artifacts/scummvm-vulkan/watchmaker-vertex-light.log, eboot SHA-256
58a24f9d84520c389aac33cba956a206e4b7365b0299ed464ff783f1a286c46b.
Combined lightmap rendering is still incomplete; these fixes do not establish
original lighting or a playable Watchmaker.

The first vertex-light build hit checkVertices: its assertion always used the
base material's VertsList, although finalization assigns lightmapped indices
from the matching additional material. Validation now selects that same
material, preserving bounds checks. Recorded in watchmaker-vertex-light.log.

The second pass is now enabled: lightmaps use u2/v2, white modulation (so vertex
lighting is not applied twice), ZERO/SRC_COLOR blending and disabled depth
writes. ZWRITE_ENABLE now maps to glDepthMask instead of being ignored. Normal
geometry returns to u1/v1 and vertex colors. Build and color tests passed;
22-file overlay roundtrip passed. Actual scene acceptance is still pending in
watchmaker-lightmaps.log. Eboot SHA-256:
518e9ed94a405c66b940ffbb9be0e74cd6c544ccaf812809bd2b7c22c90a815f.

Follow-up: watchmaker-lightmaps.log reached an assertion for a missing combined
material. rMergeMaterial used the empty per-frame FacesList count during loading
and therefore dropped lightmap associations. It now preserves each association
before vertex finalization. An ASan/UBSan harness exercising the production merge
with an empty draw list passes. Overlay roundtrip passes (23 files). Rebuilt and
launched visibly: watchmaker-lightmap-merge.log. Logo animation still reportedly
loops/pulsates; not fixed. Latest scene rendering still requires verification.

### 3D depth state after overlays

The user correctly raised draw ordering as a possible source of missing faces.
renderTexture disables depth testing and sets GL_ALWAYS, while RenderSky only
restores depth if a sky exists. RenderGeometry now explicitly establishes
ZENABLE/LEQUAL and depth writes before the sky/base pass on every frame. This
removes dependence on prior menu/subtitle state. This is a concrete state leak;
its visual impact is still to be verified against the bedroom reference.

The user confirmed the depth-state build restores the sleeping character.
Transparent bedding, incorrect character materials, missing room surfaces and
logo animation remain. Next correction: ALPHAREF now uses a normalized byte
cutoff with GL_GREATER instead of GL_ALWAYS and no longer changes blend enable.
This prevents invisible texels from occluding later geometry. Removed the
unconditional return in the glass-material branch, which aborted the pass.
Production render-state harness passes ASan/UBSan for alpha normalization,
blend independence and depth-write state. Visible run: watchmaker-alpha-depth.log.
Eboot c3697a1af53f97eb1be57b30bed1704cfacdd670902a339b680da1d5c1ba5149.
Visual acceptance is pending; do not equate build/test success with game correctness.

### Shared normal corruption

User reports the head may momentarily appear inverted; this is not yet a
confirmed animation bug. Investigation found a separate concrete ownership
bug: mesh loading assigns Mesh.NList = body.NList, then initNormals appended
local normals without clearing the shared entries. Its indexed copies therefore
overwrote shared face planes, corrupting both culling and lighting across meshes.
Clear the mesh list before allocating local per-vertex normals.

scripts/check-watchmaker-normals.py exercises the production initNormals under
ASan/UBSan with shared face planes and two meshes using remapped normal indices.
It verifies preserved global values, correct local copies and independent mesh
mutation. Passed. Overlay exact roundtrip passed. Visible run:
watchmaker-normal-ownership.log. Eboot SHA-256:
1260a2e3c392cfaa82b23db72ab03a0c662b23402f7d8b61c04d877cae8a3268.
Visual acceptance, animation, texture mapping and complete room remain pending.

### Script-controlled material movies

After normal ownership was fixed the user confirmed walls, floor and furniture
returned. Head/face orientation and perceived backwards animation remain open.
No global reversal or UV flip has been applied without evidence.

rSetMovieFrame now holds an explicit script-selected frame instead of letting
updateMovie's clock immediately overwrite it. 0xFFFF resumes from that frame;
this sentinel is used by the existing gear-animation operate code. Timed movies
retain their existing looping behavior. This does not yet prove the logo loop
is fixed. setFrame also reconstructs a cold non-keyframe seek from the previous
keyframe instead of treating delta data as a complete texture. Invalid frame
indices and zero keyframe intervals are rejected.

scripts/check-watchmaker-movies.py passes with ASan/UBSan, exercising production
seek/control functions: initial delta seek, backwards seek, keyframe crossing,
manual hold, resume and invalid input. Visible acceptance still pending.
Visible run: watchmaker-movie-control.log. Eboot SHA-256:
2beb2f5b8ba7a2d80a6c174d9243f1c0f5f8fa46996b93d715549e5a9546705b.
Earlier log inspection confirms gear movies advancing 0..29 and wrapping to 0,
not generally decrementing every frame. Whether scripts should hold their final
image remains a visual/script acceptance check, not a blanket reverse fix.

### Rejected pause experiment; material wrapping

User confirmed the explicit-frame pause regressed the first logo (lost color
and animation). Reverted automatic pausing on seek; seek now anchors the timer
and keeps playback running. Cold delta reconstruction and resume handling remain.
Updated production movie-control tests pass. Do not claim the logo loop fixed.

Base material binding now uses REPEAT; the separate lightmap pass uses CLAMP.
Previously every loaded texture was clamped, and setTextureWrapMode(1) also
mutated the currently bound stage-0 texture. The unsupported second-stage state
request no longer mutates stage 0. One-time per-texture UV range diagnostics
were added to distinguish out-of-range tiling from flipped animation/UVs.
No blanket V flip or animation reversal was applied.

Visible test: watchmaker-material-wrap.log. Production movie/render-state tests
and 24-file overlay roundtrip pass. Eboot:
456fe9f63f9d44dd0ddc9dd4f09c1104c9934d1f964bb76876da236cb0a99926.
Texture/head orientation, logo behavior and runtime visual acceptance pending.

### Bone indexing audit

CalcBones read SavedVertexBuffer[modVertex-cv] and accumulated influence counts
at Average[modVertex-cv], but both buffers cover the full mesh and final averaging
uses absolute indices. saveVertexBuffer copies all NumVerts. Changed both accesses
to absolute modVertex and retained a vertex bounds assertion. No blanket rotation,
UV flip or frame-order reversal was added.

scripts/check-watchmaker-bones.py executes production CalcBones with a nonzero
first animated vertex and two bones sharing one vertex. Old implementation fails;
corrected implementation passes ASan/UBSan, including repeated-frame stability.
This proves the index bug, not yet the cause of the user's inverted head.

Visible run: watchmaker-bone-indices.log. Eboot:
d7e7d4643c3ce3c6bbaaeb237a18fc57e0df89c08d3ab72002bcf6ab62a7b964.
Overlay roundtrip passes. Head appearance still needs visual acceptance.

### TGA orientation and hair at neck

The user clarified the face looks plausible but hair is at the neck. The bone
index fix did not resolve it. Extracted DarrellHead.tga, DarrellALetto.tga and
Darrell.tga from the owned archive for inspection (all 256x256, descriptor 0,
bottom origin). The decoded head atlas has hair at its top. TGADecoder normalizes
to top-first rows; ReadTgaImage ignored its flag parameter. Fonts/menu loaders
explicitly request rSURFACEFLIP, while model TGA loading passes zero. Restored
that distinction: normalized rows remain for rSURFACEFLIP; default 3D TGA rows
are flipped vertically. DDS remains on its own path; no bone rotation changes.

Production loader policy test scripts/check-watchmaker-tga.py passes ASan/UBSan
with a normalized two-row decoder fixture, checking model vs UI/font orientation.
25-file overlay roundtrip passed. Visual confirmation remains necessary.
Visible run watchmaker-tga-orientation.log. Eboot SHA-256:
2da60a8a91f3457002d313e8c2cf404dee7218c8599f53749d9a318bff994174.

### Menu orientation scope and movie stop commands

User visually confirmed that the TGA flip corrected Darrell's head/hair and
blue clothing, but menu lettering became vertically inverted. The flag-based
split above was insufficient: bitmap loading passes cached loader flags rather
than consistently forwarding the caller's flags. ReadTgaImage now returns the
normalized image for all callers, and only the TGA branch of the 3D texture
loader flips rows. DDS is unchanged. Updated production TGA fixture checks
both flagged and unflagged UI callers plus the actual model loading fragment.

User also reports startup logos repeating instead of stopping. Found two
concrete engine bugs: ChangeMeshMaterialFlag rejected meshes *with* valid
materials, and rAddToMaterialList ignored T3D_MATERIAL_MOVIEPAUSED. Corrected
the guard and honored the pause flag during movie updates. An initially paused
movie uploads frame zero; resuming reanchors the clock to the held frame.
ATFM_START_MOVIE still seeks as scripted. Ordinary unpaused movies retain their
looping behavior; this does not impose a global one-shot policy.

Production movie-stop fixture covers invalid/valid meshes, initial paused
image, stop across a 30-second clock advance and continuous resume. ASan/UBSan
passes, alongside the movie seek and revised TGA tests. Overlay exact forward/
reverse roundtrip passes for 27 files. Visual logo and menu acceptance is still
pending; these tests do not establish that the entire intro is now correct.

Packaged and launched visibly via watchmaker-movie-stop-menu.log.
Eboot SHA-256: 8b40f3c4cdc858e9813e34660065d595513bf3b293bf214f7a26e654a4e32279.


### Audio, lightmap alpha and display gamma (accepted visual checkpoint)

Replaced low-level sound stubs with ScummVM mixer streams: WAV effects and
speech, per-index stop/query, bounded channels, looping, distance attenuation,
stereo balance, separate percent volumes and teardown. Speech files were
already present via the existing ISO-extraction symlink; the engine's empty
speech-directory default was corrected. No new game assets are committed.
Music can decode the edition's MP3 files via legacy MIDI-name mapping, but the
room-change call was initially disabled upstream. It is now enabled, and the
main menu starts/stops Music/menu.mp3. Scripted submusic selection uses the
existing scene tables. Crossfades are not yet implemented.

The live log confirms WAV/brrrrruing.wav, WAV/tictac.wav, character sounds and
Speech/s4298.wav through s4301.wav. A 12-second PipeWire capture targeted only
the shadPS4 output node: mean -23.2 dB, peak -5.6 dB. Decoder completion closes
the speech streams and permits dialogue to advance. Production sound-control
fixture passes ASan/UBSan: start/restart/loop, path mapping, position, volume,
end-of-speech queries, stream ownership and cleanup.

The room's DXT1 lightmaps include black texels with zero alpha; reject-by-alpha
must be disabled for the RGB multiplication pass. That correction alone did
not fix the washed-out image. Live instrumentation proved both room lightmaps
were drawn with UV2 coordinates in their expected atlas ranges.

The decisive issue was implicit sRGB encoding on the display attachment:
legacy GL writes already display-encoded values without FRAMEBUFFER_SRGB.
The shared SDL display owner now uses RGBA8 UNORM consistently for swapchain,
render pass and views, avoiding the extra brightening. User explicitly accepted
the resulting lighting. This changes the experimental bridge's default display
encoding; the other engines need visual regression checks. Host display-owner
failure/cleanup/format tests pass. The SDL native pixel oracle now expects UNORM
values, but that native probe has not been rerun for this checkpoint.

Moved repetitive movie/event traces to debug level 5, bounded the unimplemented
halo warning to once, removed the per-call logical screen-size warning, and
passed draw index arrays by const reference. These changes do not establish a
performance fix: user reports playback is still slow. No animation speed or
gameplay timing was artificially changed.

Visible run: artifacts/scummvm-vulkan/watchmaker-display-gamma.log.
Eboot SHA-256: 0f58213f4cc734f985eccb438f6f6f2e4b9cbede5816c4133ceb6a5f1b0cc554.


### Quiet diagnostics follow-up

The user requested removal of most diagnostics after accepting the lighting.
One live run logged 523,302 Kernel.Vmm informational lines. The isolated Vulkan
profile now defaults to `*:Warning Tty:Info` with asynchronous logging; game
warnings and emulator errors remain visible. Set
`SCUMMVM_VULKAN_DIAGNOSTICS=1 scripts/run-scummvm-vulkan.sh` to opt back into
full synchronous emulator traces. Existing nonempty custom filters are retained.

Removed temporary GL-clear/UV traces and stale per-call bitmap/file-date
messages. Missing shadow/halo features warn once rather than every frame.
Movie and event traces remain available at ScummVM debug level 5. These are
logging reductions, not proof that the emulator performance issue is resolved.


### Music and reduced-volume checkpoint

The quiet live run (`watchmaker-quiet-music.log`) confirms playback of
`Music/menu.mp3` and `Music/Intro.mp3`. The log had 755 lines at inspection,
versus the earlier very verbose run; durations differ, so this is not a
performance comparison. User still reports excessive slowness and an arm
protruding through the wall beside the office door. Whether this is animation
placement or occlusion remains unresolved; do not classify it as fixed.

Default game-option volumes are now 50 percent for effects, speech and music,
as requested. Existing loaded options retain their own values. The production
audio fixture passes after this change and the experimental package builds.
Final eboot SHA-256:
`85a2ba5cbef17d55fa98f7da7784c82abeaa7f4b8780d69050ac97a72d1c627e`.
