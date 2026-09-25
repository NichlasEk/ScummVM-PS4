# PS4 Ogg/Vorbis port

Package 1.15 adds static libogg, libvorbis and libvorbisfile. The Longest
Journey (Stark) and Nancy Drew now pass their mandatory configure dependencies,
bringing the build to 97 top-level engines. Game-specific validation is still
required; enabling an engine does not prove a playable game.

## Source and build

`scripts/build-ps4-vorbis.sh` builds the official Xiph releases from
https://xiph.org/downloads/ with pinned SHA-256 archives:

- libogg 1.3.6: `5c8253428e181840cd20d41f3ca16557a9cc04bad4a3d04cce84808677fa1061`
- libvorbis 1.3.7: `b33cc4934322bcbf6efcbacf49e3ca01aadbea4114ec9589d1b1e9d20f72954b`

The OpenOrbis build is static PIC, uses the common PS4 ABI header, disables
floating-point contraction, and explicitly supplies target Ogg headers and
archive to Vorbis's CMake configuration. Output lives under
`build/ps4-vorbis/install`; no host library or SDK changes are needed.
ScummVM links the decoding libraries; libvorbisenc is built by upstream but
is not linked into ScummVM. Both BSD license notices are included in the PKG.
Main, SOTC and PICE build fingerprints and relink checks track the new libraries.

## Decoder and seeking test

Run `scripts/test-ps4-vorbis-build.sh`. FFmpeg generates two synthetic test
streams (no proprietary audio): 2 seconds mono/22050 Hz and 3 seconds
stereo/44100 Hz. Input hashes are retained in
`build/ps4-vorbis-smoke/input.sha256`.

The host and PS4 harness uses vorbisfile read/seek/tell/close callbacks,
checks complete decoding, sample count, nonzero audio, rewind to byte-identical
initial PCM, exact midpoint seek/tell, EOF and rejection of an out-of-range
seek. Run its staged eboot in an isolated emulator profile and compare
`data/vorbis-smoke.txt` with `build/ps4-vorbis-smoke/expected.txt`.

Verified in shadPS4: guest exit 0, byte-identical output versus host:

```
mono.ogg
rate=22050 channels=1 frames=44100 bytes=88200 hash=77e78e43
mid=22050 bytes=700 hash=36105be0
stereo.ogg
rate=44100 channels=2 frames=132300 bytes=529200 hash=5cbddc2e
mid=66150 bytes=3944 hash=6a577ace
PASS
```

Evidence: `artifacts/emulator/vorbis-smoke.log` and `vorbis-smoke-result.txt`.
This proves the target decoder/callback path, not the game's mixer or physical
PS4 audio output.

## The Longest Journey handoff

All four user-supplied US CD ZIPs under `artifacts/TheLongestJourney/isos`
were unpacked to BIN/CUE, converted from MODE1/2352 to ISO data sectors, and
extracted. `artifacts/TheLongestJourney/extracted` is the self-contained game
folder (9677 files); duplicate paths were byte-compared with no conflicts.
The original files are unchanged. The two detection hashes match the English
GOG.com entry even though these are CD files. Only a local staging symlink is
used; game assets are not packaged or tracked.

A PS4-specific default disables `enable_assets_mod`: the TinyGL renderer
cannot load replacement assets, and upstream's enabled default produced an
unnecessary warning on an unmodified game. Other platforms are unchanged.
Stark already has Bink and TinyGL available in this build; Vorbis was the last
mandatory missing dependency. The engine chooses its TinyGL software renderer
without requiring a desktop OpenGL backend. Broader frame-rate, video/audio, input and save/load testing remains necessary.
Physical PS4 testing and Nancy Drew gameplay also remain open.


## Package verification

The PS4 executable starts in shadPS4 and reaches The Longest Journey's original
logo animation and main menu with the normal game data and TinyGL. New Game
accepted the controller-generated mouse click and reached the prologue
3D scene, with visible character models and rendered dialogue subtitles. The user
confirmed audible output. The software-renderer assets warning is absent with
the corrected PS4 default. This is not a full-game or physical-hardware test.
Evidence: `artifacts/emulator/tlj-auto.log`, `tlj-auto-start.png` and
`tlj-auto-later.png`, `tlj-newgame.png` and `tlj-prologue.png`.

PS4 imports passed the allowlist. Package 1.15 SHA-256:
`13bc894cc3f893bc67d591fcd23caa99f18b58d36a40560aa8d9b9c071c6600c`.
Eboot SHA-256:
`d4a1188a01779a1ef6bef398a1a1bcd40586a011264b3599d49c9af0f86003f7`.
