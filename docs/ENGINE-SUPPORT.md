# PS4 engine support and remaining ports

Build: ScummVM 2.9.1. The 1.09 baseline below is extended by the 1.11 JPEG and 1.12 MP3 ports.

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
The original table below still records 1.09, before JPEG and MAD were available.
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

## Excluded: exact configure dependencies

The following table records the 1.09 exclusions. Apply the 1.11 and 1.12 changes above.
It lists missing
mandatory dependencies from `artifacts/scummvm/configure.log`, not a complete
list of optional codecs a particular edition might use.

| Engine / game family | Missing dependencies to port and enable |
| --- | --- |
| Adventure Game Studio | MAD |
| BAGEL | FreeType2 |
| The Journeyman Project 2: Buried in Time | FreeType2 |
| CRAB | PNG FreeType2 Vorbis file support |
| Glk Interactive Fiction games | FreeType2 JPEG PNG |
| Groovie 2 games | JPEG |
| Hpl1 | JPEG GIF PNG OpenGL with shaders |
| Myst ME | JPEG |
| Myst 3 | JPEG |
| Nancy Drew | Vorbis file support |
| Red Comrades | FreeType2 |
| QD Engine | Vorbis file support mpeg2 |
| Sludge | PNG |
| The Longest Journey | FreeType2 Vorbis file support |
| Broken Sword 2.5 | PNG libtheoradec |
| Tetraedge | FreeType2 Vorbis file support PNG JPEG libtheoradec |
| Starship Titanic | JPEG MAD |
| Thimbleweed Park | Vorbis file support PNG OpenGL with shaders |
| The Watchmaker | OpenGL (classic) |
| Wintermute | JPEG PNG |
| Z-Vision | FreeType2 |

## Work needed to enable the excluded engines

1. **Image and font libraries:** cross-compile zlib/libpng, a compatible JPEG
   library and FreeType2 for OpenOrbis. Add GIF support for HPL1. Integrate their
   target include/library paths in `scripts/configure-scummvm.sh`, remove the
   corresponding disable flags, and ensure no Linux host libraries are linked.
2. **Audio:** cross-compile libmad (MAD) and libogg/libvorbis, including
   vorbisfile. This opens AGS, Nancy Drew and several other engines. MAD and
   JPEG together are required for Starship Titanic. Optional FLAC and other
   audio decoders are also currently disabled; compressed game editions may
   therefore fail even when their engine is compiled.
3. **Video:** port libtheora's decoder and libmpeg2 for the engines listed
   above. Test cutscenes and audio/video timing on PS4, not just configuration.
4. **Graphics backend:** HPL1 and Thimbleweed Park need an OpenGL shader path;
   The Watchmaker needs classic OpenGL. Our current SDL software-output backend
   exposes neither. These require a working PS4 rendering backend/GL integration
   or an engine-supported alternative, not merely another configure flag.
5. **Verification:** rebuild, check PS4 imports, package any newly selected
   upstream engine data, then exercise representative legally owned game data
   in shadPS4 and on physical PS4. Configure success does not prove gameplay,
   saving/loading, input, rendering, audio or end-to-end completion.

Except for JPEG in 1.11 and MAD in 1.12, these dependency ports are still pending; no host libraries were substituted.
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
