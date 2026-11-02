# ScummVM for PS4 — Vulkan Experimental 1.26

An unofficial PS4 homebrew port of **ScummVM 2.9.1**, with native Vulkan
rendering and the complete game-engine build, including the experimental
**Prisoner of Ice** and **Shadow of the Comet** engines.

**Bring your own game data.** This download contains the application, engine
support files and shaders. Commercial games and MT-32 ROMs are not included.

## Install and play

1. Use a PS4 homebrew environment that can install and run homebrew PKGs.
2. Install `ScummVM-PS4-Vulkan-Experimental-1.26.pkg` with your package installer.
3. Copy your original game data to a separate folder on a USB drive. Use the
   data files needed by ScummVM, rather than an ISO or the original installer.
4. Start **ScummVM Vulkan Experimental 1.26**. Select **Add Game**, browse to
   **USB 0** (or the appropriate USB device), open the game's folder and select
   **Choose**. Confirm the detected game, then select **Start**.

The title ID is `SCVM00010`. This updates an existing ScummVM Vulkan installation
with that ID. The ordinary software-output port uses a different title ID.
Settings and saves live under `/data/scummvm-vulkan`; keep a backup of existing
saves before upgrading. No universal minimum firmware has been established.

## Controller

| Button | Default action |
| --- | --- |
| Left stick | Move the pointer |
| Cross | Select / left click |
| Circle | Right click in games using the default mapping |
| Triangle | Close a dialog / Escape |
| Options | ScummVM menu; save/load where the engine supports it |
| R1, held | Move the pointer more precisely |
| Share | Virtual keyboard |

Individual games can override these mappings. Use ScummVM's keymapper where
needed. This build starts in controller-only mode; USB mouse initialization is
disabled. The on-screen keyboard is included for text entry.

## What is included

The full build requests all upstream engines, including unstable engines, and
adds PICE and SOTC. Only **TestBed** and **Playground3D**, internal test engines,
are excluded. The exact enabled engines and subengines appear in
[ENGINE-LIST.txt](ENGINE-LIST.txt), with the compiled IDs in
[ENGINE-CONFIG.txt](ENGINE-CONFIG.txt).

- Prisoner of Ice: native experimental engine, included in the common launcher.
  Point Add Game at the original data directory containing the KRO archives.
- Shadow of the Comet: native experimental engine, still under development.
  Point Add Game at the main `SHADOW` data directory. Known incomplete behavior
  and progression issues remain; do not assume a complete playthrough.
- Director, including the experimental Director 7 work for Jönssonligan;
  Thimbleweed Park, Grim, The Longest Journey, HPL1, The Watchmaker, Wintermute,
  and the other engines listed in the inventory.
- JPEG, PNG, Ogg/Vorbis, MP3, FLAC, Theora, VP8/VP9, MPEG-2, AC-3, GIF,
  FreeType and MT-32 emulation. MT-32 requires separately supplied ROMs.
- GUI theme, icons, virtual keyboard, engine support data and shader resources.

Jönssonligan: Jakten på Mjölner needs the original directory containing
`toInstall`, plus these game-specific settings in its ScummVM target section:

```ini
engineid=director
gameid=jonssonligan1
platform=windows
language=sv
ps4_director7_experimental=true
start_movie=toInstall/Game.exe
```

## Experimental status

An enabled engine means it is compiled into this application. It does not mean
that every game, edition, renderer or save format has been tested on PS4.
PICE, SOTC, Director 7 and several upstream engines remain experimental.

The maintainer reported physical PS4 gameplay in Thimbleweed Park with the
earlier **Clear 1.24** build. This candidate retains the native color-clear and
direct GPU command-memory fixes and adds the shared 3D-menu cursor fix from
1.25. Physical verification of this exact 1.26 package is pending. See
[RELEASE-AUDIT.md](RELEASE-AUDIT.md) for the checks performed on this candidate.

The Vulkan backend remains incomplete. Some 3D games have rendering or runtime
problems. This is a community experimental release, not an official ScummVM
release or a promise of full library compatibility.

For a useful bug report, include the package version, PS4 firmware/homebrew
environment, exact game edition/language, reproduction steps and a screenshot.
The port writes `bootstrap.log` and `driver.log` under `/data/scummvm-vulkan`.

## Source, checksums and credits

Compressed source snapshots sit alongside the PKG: the actual patched
ScummVM/port sources, dependency sources, and the recovered OpenOrbis runtime
sources. Build instructions are in [SOURCES.md](SOURCES.md). The installed SDK
and its LLVM/SDL dependency releases have been identified by binary hashes;
the musl source revision is a documented historical inference. See
[OpenOrbis provenance](OPENORBIS-PROVENANCE.md) for evidence and limitations.

On Linux, verify the files from this directory with `sha256sum -c SHA256SUMS`.

ScummVM and its artwork belong to their upstream contributors. This unofficial
port uses OpenOrbis, OpenGNM, Vulkan-PS4, PSBC, glslang, SDL and the codec/font
libraries named in `licenses/`. ScummVM is distributed under the GPL; component
licenses and notices are preserved in the source archives and `licenses/`.

Upstream: [ScummVM](https://www.scummvm.org/) ·
[OpenOrbis](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain) ·
[PS4 OpenGNM](https://github.com/PS4-OpenGNM)
