# PS4 MP3 port

Package 1.12 adds static libmad for MP3 decoding. It enables Adventure Game
Studio and Starship Titanic as well as MP3 streams in already compiled engines,
including Myst III. This is compiled availability, not broad gameplay validation.

## Source and build

The source is [libmad 0.15.1b](https://www.underbit.com/products/mad/) plus the
[Debian 0.15.1b-11 patch series](https://packages.debian.org/trixie/source/libmad),
including the frame/main-data length checks. Both source archives are fetched
from Debian and pinned by SHA-256 in `scripts/build-ps4-mad.sh`:

- Original: `bbfac3ed6bfbc2823d3775ebb931087371e142bb0e9bb1bee51a76a6e0078690`
- Debian patches: `be94f0bcce7efddef02c876d9a49d47004f1f78c5428fb020c8ab491ddc881f3`

The script builds a PIC static archive using OpenOrbis, the shared PS4 ABI
header, LP64 sizes and libmad's portable `FPM_64BIT` arithmetic. Asynchronous
fork/pipe decoding and architecture-specific assembly are disabled. `-fwrapv`
preserves signed fixed-point wraparound. Source, objects and the isolated
installation live under `build/ps4-mad`; no host libraries or SDK files change.

The same integer sizes/FPM selection are exported in the generated `mad.h`.
Build scripts track the dependency fingerprint and archive timestamps for
reconfiguration/relinking. Packaging includes libmad COPYRIGHT and GPL license.

## PCM comparison

Run `scripts/test-ps4-mad-build.sh INPUT.mp3` with a locally available track.
The helper builds host and PS4 versions of the same patched decoder and test.
Boot `build/ps4-mad-smoke/stage/eboot.bin` in an isolated shadPS4 profile and
compare that profile's `data/mad-smoke.txt` to
`build/ps4-mad-smoke/expected.txt` using `cmp`.

Myst III CD 1.2's `M3Data/TOHO/TOHO_MUSIC2.mp3` passed this comparison in
shadPS4 0.18.0. Both implementations decoded the full file into identical
fixed-point PCM:

```text
frames=6534 samples=15054336 nonzero=15054247 recoverable=1 rate=44100 channels=2 PCM-fnv1a=6a12c171
PASS
```

Evidence: `artifacts/emulator/mad-smoke.log` and `mad-smoke-result.txt`.
Input hash is recorded in `build/ps4-mad-smoke/input.sha256`. No proprietary
MP3 fixture is tracked or included in the general ScummVM package.

This decoder test does not itself prove the game's audio mixer or physical
PS4 audio output. Test those separately, along with gameplay and saves.

## Actual ScummVM test (package 1.12)

The PS4 executable reached Myst III's main menu with MP3 music enabled and no
missing-MP3 warning. Only the shadPS4 sink input was routed to the temporary
`scummvm_mp3_test` null sink. A 12.003-second stereo recording of that sink at
48 kHz contains nonzero audio (16-bit peak 1533, RMS 315.194), verifying output
through the game mixer and emulator audio path. This was not microphone or
whole-desktop capture. Evidence: `artifacts/emulator/myst3-menu-mp3.wav`,
`myst3-menu-mp3.json`, `myst3-mp3-menu.png`, `myst3-mp3-launch.log` and
`myst3-mp3-guest.log`.

Package SHA-256:
`2df86310fd79ea534ff18522f7c409dbb7f4fbeaf51a4f1a8bb0cf3d35bb881a`.
PS4 import allowlist passed; all 91 main engines linked, including AGS/Titanic.

All four user-supplied CD 1.2 images have now been extracted locally. The
self-contained game directory is `artifacts/myst3/extracted`, containing
`M3Data` and `Data` (27 node archives). The emulator staging tree links it at
`/app0/games/myst3`; none of those files are packaged or tracked in Git.

Selecting New Game now passes the missing-archive failure and displays the
opening linking-book movie. It subsequently crashes before a playable panorama
was verified. shadPS4 reports PC `0x4091c4`; subtracting the executable load
base `0x400000` gives `0x91c4`, an `hlt` in musl `__bin_chunk`, after a failed
chunk-size consistency check. This indicates heap corruption detected while
freeing memory, not a proven null-pointer bug. The corrupting code and whether
it is related to video, rendering, audio or another path are not yet known.
Do not treat this run as successful gameplay validation.

Next investigation: reproduce New Game with heap diagnostics/call stack and
identify the write that damages the chunk metadata. Screenshot:
`artifacts/emulator/myst3-intro-before-crash.png`. Preserve the package/logs
above as the 1.12 baseline. Physical PS4 audio and gameplay remain untested.
