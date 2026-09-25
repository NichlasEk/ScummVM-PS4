# PS4 engine support and remaining ports

Build: ScummVM 2.9.1. The 1.09 baseline below is extended by the 1.11 JPEG and 1.12 MP3 ports.

## Ogg/Vorbis port (package 1.15, 2026-09-25)

Static libogg 1.3.6 and libvorbis/libvorbisfile 1.3.7 enable The Longest Journey
and Nancy Drew (97 main engines). Target/host PCM and seeking tests pass;
see [PS4-VORBIS.md](PS4-VORBIS.md). The Longest Journey reached its menu
and 3D prologue with subtitles in shadPS4; full playthrough/save-load and
physical PS4 testing remain open.
The remaining-exclusions table below reflects the current dependencies.

## FreeType port (package 1.14, 2026-09-25)

Static FreeType 2.14.3 adds BAGEL, Buried in Time, Red Comrades and Z-Vision
(95 main engines). Host/PS4 glyph pixels and metrics matched in all 18 cases.
See [PS4-FREETYPE.md](PS4-FREETYPE.md). Buried in Time reached its menu, intro and game interface in shadPS4;
full-game and physical PS4 testing remain pending. The remaining-exclusions table below reflects subsequent ports too.

## Myst III runtime fix (package 1.13, 2026-09-25)

PS4 mouse locking now uses the existing virtual-mouse relative deltas instead
of SDL's unsupported desktop pointer-capture fallback. The visible shadPS4 run
passed the intro and reached the panorama; the user confirmed working play.
See [PS4-MYST3-RUNTIME.md](PS4-MYST3-RUNTIME.md) for evidence and limits.

## MP3 port (package 1.12, 2026-09-25)

A static OpenOrbis build of libmad now provides MP3 decoding. This enables
Adventure Game Studio and Starship Titanic, bringing the top-level engine count
to 91. MP3 audio also becomes available to previously compiled engines such as
Myst III. See [PS4-MP3.md](PS4-MP3.md) for source pins and target/host PCM tests.
The engine list below records the historical 1.09 baseline.
Myst III menu MP3 output was verified in shadPS4. With all four discs,
New Game reaches its intro movie but then hits a heap-integrity failure;
see PS4-MP3.md. AGS and Starship Titanic gameplay have not yet been tested.

## JPEG port (package 1.11, 2026-09-25)

`libjpeg-turbo` 3.2.0 is cross-compiled from its pinned official source tarball
with the OpenOrbis sysroot, statically linked, and uses the portable C decoder
(SIMD disabled). See [PS4-JPEG.md](PS4-JPEG.md) for build and test details.

This adds Myst III as a top-level engine (89 total), plus the Myst Masterpiece
and Groovie 2 subengines. Myst III uses the existing TinyGL software renderer;
no OpenGL backend is needed. These entries are no longer excluded. Other JPEG
users still require their remaining dependencies (e.g. Starship Titanic: MAD;
Wintermute: PNG). Myst III reached its original main menu in shadPS4 using the supplied CD 1.2
disc 1. New Game requires `TOHOnodes.m3a` from additional disc data; MP3 music was unavailable in 1.11; 1.12 adds MAD. Gameplay and physical hardware remain unverified.

## Baseline: package 1.09 (2026-09-20)

This records **compiled availability**, not a promise that every game works.
The build requests all upstream engines except the TestBed and Playground3D
test engines. Upstream configure excludes engines whose mandatory dependencies
are unavailable. Experimental engines are deliberately included in this broad
build. The separate native Prisoner of Ice preview is not part of this package.

## What is included

88 top-level static engines, plus their enabled subengines:

```text
SCUMM [all games]
    Access 
    ADL 
    AGI 
    AGOS [all games]
    Sanitarium 
    Lord Avalot d'Argent 
    Beavis and Butthead in Virtual Stupidity 
    Blade Runner 
    CGE 
    CGE2 
    Chamber 
    Chewy: Esc from F5 
    Cinematique evo 1 
    Magic Composer 
    Cinematique evo 2 
    Lost Eden 
    Cryo Omni3D games [all games]
    Darkseed 
    Dynamix Game Development System 
    Macromedia Director 
    Dungeon Master 
    Dragon History 
    Blazing Dragons 
    Drascula: The Vampire Strikes Back 
    Dreamweb 
    Escape From Hell 
    Freescape 
    UFOs 
    Gobli*ns 
    The Griffon Legend 
    Grim [all games]
    Groovie [7th Guest]
    Hades Challenge 
    Hyperspace Delivery Boy! 
    Hopkins FBI 
    Hugo Trilogy 
    Hypnotix Inc. 
    In Cold Blood 
    Illusions Engine 
    The Immortal 
    Kingdom: The Far Reaches 
    Kyra [all games]
    Labyrinth of Time 
    The Last Express 
    Lilliput 
    Lure of the Temptress 
    M4 
    MacVenture 
    MADE 
    MADS [all games]
    Might and Magic [all games]
    Mohawk [Living Books] [Where in Time is Carmen Sandiego?] [Myst] [Riven: The Sequel to Myst]
    Mortevielle 
    mTropolis 
    Mutation of JB 
    Neverhood 
    Nikita Game Interface 
    Parallaction 
    The Journeyman Project: Pegasus Prime 
    Pink Panther 
    Plumbers Don't Wear Ties 
    The Prince and The Coward 
    Private Eye 
    Flight of the Amazon Queen 
    SAGA [all games]
    SAGA2 
    SCI [all games]
    The Lost Files of Sherlock Holmes 
    Beneath a Steel Sky 
    Star Trek 25th Anniversary/Judgment Rites 
    Mission Supernova 
    Broken Sword 
    Broken Sword II 
    Teen Agent 
    Tinsel 
    3 Skulls of the Toltecs 
    Tony Tough and the Night of Roasted Moths 
    Toonstruck 
    Touche: The Adventures of the Fifth Musketeer 
    Trecision Adventure Module 
    TsAGE 
    Bud Tucker in Double Trouble 
    Little Big Adventure 
    Ultima [all games]
    V-Cruise 
    Voyeur 
    WAGE
```

SCUMM v7/v8, Humongous Entertainment and SCI32 are enabled. Lua is provided
by ScummVM's bundled implementation; Hyperspace Delivery Boy and Ultima VI do
not need a separate Lua library port now. Original Myst and Riven are included;
Myst Masterpiece Edition and Myst III are separate dependency cases below.

The package copies upstream's configured engine-data manifests, including
required `.dat`/table files, script patches, core support data and the virtual
keyboard pack into `/app0/data`. It does not package the user's game directories.

## Remaining exclusions: package 1.15

This table reflects current configure results, not the historical 1.09 list.
TestBed and Playground3D are deliberately disabled test engines and omitted.

| Engine / game family | Mandatory dependencies still missing |
| --- | --- |
| CRAB | PNG |
| Glk Interactive Fiction games | PNG |
| Hpl1 | GIF, PNG, OpenGL with shaders |
| QD Engine | MPEG-2 |
| Sludge | PNG |
| Broken Sword 2.5 | PNG, Theora decoder |
| Tetraedge | PNG, Theora decoder |
| Thimbleweed Park | PNG, OpenGL with shaders |
| The Watchmaker | Classic OpenGL |
| Wintermute | PNG |

## Work still needed

1. **Images:** port zlib/libpng, then GIF for HPL1. PNG alone removes the
   remaining configure blockers for CRAB, Glk, Sludge and Wintermute.
2. **Video:** port libtheora's decoder and libmpeg2. Verify cutscenes and
   audio/video timing; compilation is insufficient.
3. **Graphics backend:** HPL1 and Thimbleweed Park require OpenGL shaders;
   The Watchmaker requires classic OpenGL. The SDL software-output backend
   provides neither. These need a PS4 graphics backend or an engine-supported
   alternative rather than another configure flag.
4. **Optional audio:** FLAC and other disabled codecs may be required by
   particular compressed editions, even when their engine is available.
5. **Gameplay verification:** test startup, input, rendering, audio, save/load
   and progression using owned game data in shadPS4 and on physical PS4.

JPEG (1.11), MAD (1.12), FreeType (1.14), and Ogg/Vorbis (1.15) are now ported.
No host libraries were substituted. Newly enabled games still need testing.
`SCUMMVM_ENGINES=scumm,scumm_7_8,he scripts/build-scummvm.sh` can select a narrower
build. Unsetting `SCUMMVM_ENGINES` restores the broad default. The configuration
fingerprint includes that override to avoid silently reusing the wrong selection.

## Upstream experimental engines / subengines

The configured warning list is reproduced below. It is upstream's warning
output, not a hardware compatibility report (some entries can also be excluded
by dependency checks; the compiled list above is authoritative).

```text
Lord Avalot d'Argent 
    Chamber 
    Lost Eden 
    Darkseed 
    Dungeon Master 
    Grim [Escape from Monkey Island]
    In Cold Blood 
    The Immortal 
    The Last Express 
    Lilliput 
    MacVenture 
    MADS [MADS V2]
    Mohawk [Where in Time is Carmen Sandiego?]
    Mutation of JB 
    Star Trek 25th Anniversary/Judgment Rites 
    Ultima [Ultima I - The First Age of Darkness]
    WAGE 
    Wintermute
```

## Validation and known limits

- DOTT has previously been played by the user on physical PS4, using an earlier
  build. The user also verified USB browsing after the storage/input fixes.
- The preceding 1.08 build passed The Dig's former v7/v8 startup rejection and
  opened its intro movie in shadPS4. This is not a full-game playthrough.
- Package 1.09 built and linked successfully with 88 static main engines.
  The PS4 import allowlist check passed. Its 41-file manifest contains 34
  support/license files and no bundled game files.
- The actual PS4 executable booted in shadPS4, with the launcher visually
  confirmed. During a 75-second smoke run The Dig opened its data, music and
  intro movie (`sq1.san`) without the former missing-engine rejection. The run
  ended at the requested timeout (exit 124), not a claimed clean game exit.
  Evidence: `artifacts/scummvm/all-engines-emulator.log` and
  `artifacts/scummvm/all-engines-launcher.png`.
- Built package SHA-256: `bb8a7f1a1dc59e39fd999042449faf6f4ce51c997b66f7de48c7f4014b5cbb3f`.
- No broad physical-PS4 engine compatibility matrix exists yet. Treat all newly
  enabled engines as unverified on hardware, even if upstream calls them stable.
- Separate PICE work remains incomplete and is documented in `PICE-SAVE-RUNTIME.md`.

Local build evidence: `artifacts/scummvm/configure.log`, `build.log`, `package.log`.
