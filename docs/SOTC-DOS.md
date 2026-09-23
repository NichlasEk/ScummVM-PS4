# Shadow of the Comet: initial DOS investigation

Inspected 2026-09-23. This is format research, not a playable engine.

## Local evidence

The user-supplied European multilingual CD is a single MODE1/2352 track,
ISO9660 volume SHADOWCD. Main-game files are in SHADOW/, with SHADOW.EXE
at the root. MUSEE/ is separate bonus content; DEMO/INDARK2/ must not be
mistaken for the main game.

The original disc is unchanged. Extracted files reside in the ignored
build/sotc-DOS/disc-files/ directory. No game resources belong in the engine
or a distributed package.

| Area | PICE DOS | Observed SOTC DOS |
| --- | --- | --- |
| Archives | KRO, Burp directory, headerless zlib path | 23 PAK archives, little-endian offset tables |
| Visual assets | PICE sprite/resource decoding | A00–A09.PAK animations, D00–D09.PAK scenery; VA2 and RAW names |
| Text and scripts | PICE resource/script machinery | R00–R09.CC4 and five additional CC4 files; semantics unverified |
| Font | PICE resource fonts | CTU.PFN inside RES.PAK |
| Sound | PICE voice/music resources | VOC names in SMP.PAK, ADD names in MUS.PAK, separate NAR banks |

All 23 main-game PAK directories have been parsed with resource bounds checked.
Example: D00.PAK contains F00.RAW with a declared unpacked size of 64000 bytes,
consistent with a 320×200 indexed background, but image and palette decoding
have not yet been verified. A00.PAK includes STUBBS0.VA2; MUS.PAK includes
SHADOW1.ADD; SMP.PAK includes C27.VOC.

PAK entry headers contain an additional-descriptor size, packed and unpacked
sizes, method byte, flags byte, metadata length, metadata and payload.
Observed methods are 0 and 1, with flags 0 or 6.
This agrees with the layout in
[FITD's PAK reader](https://github.com/jmimu/AITD_PakEdit/blob/main/fitd/pak.cpp),
which dispatches method 1 to PAK_explode. Successful SOTC decompression is
still required before considering decoder compatibility proven. ZIP implode
must not be confused with PKWARE DCL or PICE's headerless zlib path.

Reproduce the directory audit:

```sh
python3 scripts/sotc/inspect-pak.py build/sotc-DOS/disc-files/SHADOW > artifacts/sotc/pak-index.json
```

## Recommended engine boundary

Use a separate sotc engine. Reuse ScummVM graphics, mixer, input, storage and
the existing PS4 build infrastructure. Reuse specific low-level decoders only
after format verification; do not assume PICE opcodes, navigation, sprites or
save states apply. Shared archive conventions do not prove a shared script VM.

Implementation milestones:

1. Bounds-checked PAK reader and decompression, validated against original
   images, font and sound; decode palettes and VA2 animation frames.
2. Separate detector and resource-viewer engine showing one original room.
   Give it isolated build outputs, configuration and save files.
3. Map CC4 scripts, text encoding/language tables, actors and navigation;
   reproduce one interactive scene before expanding coverage.
4. Implement music, speech, menus and saves; then test progression on the
   actual PS4 build under shadPS4 and on hardware.

Not established: CC4 opcode meanings, language-file mapping, complete NAR
layout, ADD music decoding, animation semantics, or later-chapter behavior.
No runtime engine or PS4 package has been built for SOTC yet.
