# Full PS4 build and local engines

Updated 2026-09-27. The complete graphics-capable build is the **experimental
Vulkan package**, title ID `SCVM00010`. It combines the enabled upstream
ScummVM engines with the maintained PICE and SOTC engines and the experimental
Director 7 changes. It is separate from the software-output package `SCVM00002`.

```sh
./scripts/build-scummvm-vulkan.sh
./scripts/run-scummvm-vulkan.sh
```

Output: `dist/ScummVM-PS4-Vulkan-Experimental.pkg` and
`build/scummvm-vulkan-pkg/eboot.bin`. No commercial game data or MT-32 ROMs are
included. Engine activation is checked in `build/scummvm-vulkan/config.mk`;
codec activation is checked in `config.h`.

| Part | Full-build integration | Status / detailed notes |
| --- | --- | --- |
| Upstream ScummVM | All engines requested except TestBed and Playground3D; compiled static | [Engine inventory](ENGINE-SUPPORT.md); availability is not full-game validation |
| Prisoner of Ice | `engines/pice` copied into generated sources and enabled as `ENABLE_PICE` | Native experimental implementation; [playthrough ledger](PICE-PLAYTHROUGH.md), [engine development](PICE-ENGINE.md) |
| Shadow of the Comet | `engines/sotc` copied into generated sources and enabled as `ENABLE_SOTC` | Native experimental implementation; [progress and regression notes](SOTC-DOS.md) |
| Jönssonligan: Jakten på Mjölner | Director enabled; D7 runtime patch included in Vulkan overlay | Title-specific opt-in; [setup and verified interaction](PS4-JONSSONLIGAN.md) |
| The Watchmaker | Experimental rendering and engine patches included | [Runtime work](PS4-WATCHMAKER.md) |
| Penumbra | HPL1 patches included | [Tested states and outstanding limitations](PS4-PENUMBRA.md) |
| FLAC, VP8/VP9, AC-3, MT-32 | Required shared build features | [Codec tests and MT-32 setup](PS4-OPTIONAL-CODECS.md) |

## Adding the three local targets

Use **Add Game** in the common launcher. Point PICE at the original game-data
folder containing its KRO archives; point SOTC at the main `SHADOW` folder,
not the museum or another game's demo. The engines retain their unstable
classification. Their dedicated testbench executables are still available,
but the common package does not force test scenes or auto-start them.

For the tested Jönssonligan release, use the root containing `toInstall` and
retain these game-specific settings in its ScummVM target section:

```ini
engineid=director
gameid=jonssonligan1
platform=windows
language=sv
ps4_director7_experimental=true
start_movie=toInstall/Game.exe
```

The existing local Vulkan emulator profile already contains these options.
The extension does not turn on unsupported Director 7 behavior for every title.

## Build wiring and limits

`scripts/sync-local-engines.py` copies the maintained PICE/SOTC files before
configuration, preserving timestamps of unchanged files. The default build
requests all engines and fails if PICE, SOTC or Director is absent. An explicit
`SCUMMVM_ENGINES` selection still permits focused engine builds. The isolated
PICE/SOTC scripts retain their testbench entry points and profiles.

The ordinary `scripts/build-scummvm.sh` also includes PICE and SOTC, plus the
new codecs. It does **not** include the experimental Vulkan/Director overlay;
use the Vulkan command above for the combined feature set described here.

Inclusion, successful compilation, emulator startup, game progression and
physical PS4 testing are separate checks. The engine-specific Markdown files
record the tested scenes and outstanding work. Neither PICE nor SOTC is
claimed to support a verified complete playthrough, and Jönssonligan still
has incomplete Director features.

## Combined-build verification (2026-09-27)

The complete Vulkan build and PKG packaging passed, including the PS4 import
allowlist. Running this exact PS4 executable with `--list-engines` in shadPS4
listed PICE, SOTC and Director and reported guest exit status 0. Evidence:
`artifacts/codecs/full-engine-list.log`. Codec decoding evidence is linked above.

The local Vulkan profile also now contains PICE and SOTC entries pointing to
the existing extracted game directories. Its previous INI was backed up under
`artifacts/codecs/scummvm-before-local-engines-*.ini`. Dedicated testbench
profiles and their saved progress were not migrated or overwritten.

Package SHA-256 at this checkpoint:
`42c1cdd41b633f83dae5d9068e8b41393982741519cb8d8f8aa10de6b7f98d7a`.

Guest executable SHA-256:
`90ecaf0ead439d6f16ae2d88b2a31e5818de3ffa4414c1e5cbc8de6c8de9274b`.

The launcher was also visually checked with all three target entries visible.
In this shadPS4 session the initial idle window appeared black until mouse
motion triggered another redraw. No bridge change was made for this observation;
it remains an emulator/presentation behavior to investigate if it recurs.
Temporary shader diagnostics were removed, and the final executable matches
the engine-list checkpoint above byte for byte.
