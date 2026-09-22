# Windows 95 edition (experimental)

The locally supplied `cthulhu_prisoner_of_ice.iso` is an ISO9660 disc named
`IceW95`, containing `WICE.EXE`, `SMACKW32.DLL`, `KSOUNDW.KRO` and `VIDEO/*.SMK`.
It includes English, French, German, Dutch and Swedish text banks. Text remains
DOS-850 encoded, including Swedish å/ä/ö. Swedish is selected by default when
installed; Options can select another installed language after engine restart.

Use extracted original files at the game-directory root, keeping `VIDEO`
below it. The engine runs resources directly; it does not execute WICE.EXE.
The Windows directory layout is discovered by the same USB bootstrap as DOS.
Detection matches graphics and KSOUNDW fingerprints/sizes. Saves use a separate
`-win-<language>-pice-...` namespace, leaving DOS and Mac saves unchanged.
Commercial files and local extracted data under `build/pice-win` are ignored
by Git and are not part of the package.

## Playback

* Windows MUX script names resolve to Smacker `VIDEO/S*.SMK`; ending names
  resolve to the selected language, e.g. `VFINAL3.MUX` -> `SWFINAL3.SMK`.
* ScummVM's existing Smacker decoder supplies video, palette, audio and timing.
  Startup/extras may be skipped; scripted movie playback waits for completion.
  Music and room audio pause during movies and resume afterward.
* `KSOUNDW.KRO` has 308 resources: 45 Standard MIDI Files followed by 263
  sound resources. It has no DOS/Mac PCM music resources 308–352. Windows
  music commands use MIDI resources 0–44 instead. Playback currently uses
  ScummVM's software AdLib driver; its timbre is not a claim of identical
  output to every original Windows General MIDI device.
* MIDI handles stop, looping, script and settings volume, pause/resume, and
  save restoration. Windows music position is stored as MIDI ticks with the
  remaining repeat count, in the isolated Windows save namespace.

## Evidence and limits

2026-09-22, actual PS4 eboot in visible shadPS4:

* Edition/language detection and engine parser tests pass.
* New game reaches the opening text, full SIN1 submarine film (181 frames),
  radio closeup and playable bridge (bank 1146) with Swedish text.
* `pice_test_windows_assets=true` decodes all 39 installed SMK files completely
  using the engine's Smacker decoder, and parses all 45 MIDI files using its
  SMF parser. This is a decoder audit, not 39 real-time playback checks.
* Local archive audit decompresses every entry in all seven KRO archives.
  1331 of 1338 graphics/script/font entries are identical to the tested DOS
  set; the differing IDs are 1139, 1197, 1242, 1294, 1295, 1317 and 1318.
* Saving with active MIDI and restoring its tick position passed; engine
  snapshot roundtrip passed. Saving from the paused menu also succeeds, and reloading it retains active
  music (tick 9230 in the fixture).
* No full Windows playthrough or physical PS4 test has been completed.
  Successful decompression/decoding does not prove all gameplay interactions.

Local evidence: `artifacts/pice/windows-visible.log`, `windows-audit.log`,
`windows-build.log` and the separate `xdg-pice-win`/`xdg-pice-win-audit` profiles.

Save regression logs: `windows-save-test.log`, `windows-restore-test.log`
and `windows-menu-save-restore.log`. The music checkpoint fixture waits for
a snapshot-safe moment rather than attempting a save during opening dialogue.
