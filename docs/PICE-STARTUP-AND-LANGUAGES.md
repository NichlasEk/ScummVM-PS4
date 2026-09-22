# Original startup, menu and installed languages

Investigation and implementation, 2026-09-22.

## Installed language evidence

The mounted cd-eur directory and local P-ICE.iso contain E_KLANG.BIN and
E_KLANG.KRO. __ICE__.PAR specifies LANGUAGE_LETTER=E. Decoding the BIN
index yields E_M voice groups, E_T text groups, E_TCETXT and E_X groups;
it does not expose another language set. The local distribution ZIP also
contains cd-us/a_klang.bin and a_klang.kro (American English). No other
KLANG language set was found in these sources. This does not establish
which languages were sold in other releases or imply those releases lack
multilingual support.

The engine now detects installed KLANG banks and selects their matching text,
voice and resource-name indices. Mac edition details and validation limits
are documented in [PICE-MAC.md](PICE-MAC.md).
Original executable data includes LANGUAGE_LETTER= and F_KLANG.KRO.

## Startup and native menu

Original startup code at 0x18df0, 0x18e0a and 0x18e24 invokes vintro.mux,
vcredits.mux and vtitre.mux respectively. All these clips, including their
S variants, exist locally. Header inspection confirms SINTRO/SCREDITS/
STITRE are 640x480, VINTRO is 320x200: the S/V prefix is a resolution
variant, not a subtitle-language selector.

Normal startup now plays SINTRO, SCREDITS and STITRE in that order, then
opens the main menu. Escape, Space or left click skips the current startup
or extras clip. Script-triggered gameplay movies retain their existing
completion semantics. Movie palette restoration also works before any room
has been initialized.

The main menu uses the original KSVGA resource 1321 wooden crate. These
static panels have no sequence table; SpriteResource accepts this format
only when explicitly requested, with synthetic parser/rendering coverage.
Menu text now uses the original font resource; see PICE-NATIVE-TEXT.md. No commercial assets are included in the source or package.

The six main actions are Return to game, New game, Load a game, Save a game,
Options and Exit game. Escape opens the same menu during gameplay and
returns from pause. A dialogue-choice menu consumes the first Escape to
close its choices. Both death and completion menus offer Back to main menu.
New game and Exit ask for confirmation; load exposes latest/manual/autosave.
Save a game writes the existing manual Q slot when a valid snapshot can be
made. Opening a menu never replaces a save. Starting a new game can later
replace the autosave through normal gameplay.

Menus suspend room ticks and pause active sound handles, restoring speech
caption timing on resume. Options persist speech/music/effects volumes and
subtitle visibility. Left/right or clicking the corresponding half of a
volume row adjusts it. Text-only dialogue remains visible with subtitles
off. Movie audio is a single mixed effects track, not separate speech/music.
English is displayed as information; other text packs are not yet supported.
Films and extras provides startup replay, credits and outtakes.

## Verification and local continuation

- `frontend-check.log`: full original startup clips completed with 709,
  973 and 603 frames respectively; main/options PPMs visually inspected.
- `frontend-load-check.log`: ordinary injected menu keys loaded bank 1215,
  opened pause and resumed without replacing the room.
- `frontend-newgame-check.log`: saved the isolated test slot, then started a
  new game from that chapter-3 save; bank 1140 and Vin12 movie reached, with
  the first playable submarine scene visually inspected.
- `frontend-settings-check.log`: speech=166 and subtitles=false persisted in
  scummvm.ini, including the automatically created temporary game target.
- `frontend-extras-check.log`: restarted with persisted settings; credits
  and outtakes selected through menu input and skipped through Escape.
- Built PS4 package; guest parser self-tests pass, including static panels.
  Logs are local under artifacts/pice, captures under the isolated profile.

Personal ending saves were copied intact to `.tools/xdg-pice-main-menu`;
original ending/stone-circle profiles remain untouched. This clean profile
has no automatic restore or input-injection flags. Restart it with:

```sh
PICE_PROFILE=xdg-pice-main-menu scripts/pice/run-emulator.sh
```

The launcher builds before running. Q/U shortcuts remain available in game;
use Load a game to return to the copied saves instead of New game.

## Correction: full New game route

The initial menu implementation inherited the preview's hardcoded room 0
entry. That is the monster/crate scene, not the first playable scene.
Original ICE640 code at 0x18d10 and 0x18d19 initializes chapter 2 and room
39. Bootstrap 1139 at byte 1144 reads context slots 5 and 4 and requests
that room through opcode 34.

New game now seeds that original context, requires the bootstrap to stop
normally with a valid destination, and follows the requested room. It no
longer ignores a failed bootstrap or silently falls back to room 0.

`artifacts/pice/opening-route-check.log` verifies the actual PS4 build:
bootstrap stopped normally at PC 1154, room 39 (1179), INTRO_A's four
text-only pages, all 181 Vin1.mux frames without skipping/error, room 22
(1162), then room 6 (1146). The bridge scene was visually inspected and its
opening dialogue finished with locks=0, action=-1, exclusive=-1 and
dialogue=-1. This restores the playable bridge section before the crates;
it is not a claim to have replayed the entire game from the new start.
