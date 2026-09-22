# Original startup, menu and installed languages

Read-only investigation, 2026-09-22. No startup/menu behavior changed.

## Installed language evidence

The mounted cd-eur directory and local P-ICE.iso contain E_KLANG.BIN and
E_KLANG.KRO. __ICE__.PAR specifies LANGUAGE_LETTER=E. Decoding the BIN
index yields E_M voice groups, E_T text groups, E_TCETXT and E_X groups;
it does not expose another language set. The local distribution ZIP also
contains cd-us/a_klang.bin and a_klang.kro (American English). No other
KLANG language set was found in these sources. This does not establish
which languages were sold in other releases or imply those releases lack
multilingual support.

The current engine hardcodes E_KLANG and E_T resource resolution. A future
language selector needs both available-data detection and runtime resource
selection; changing a settings label alone will not translate subtitles.
Original executable data includes LANGUAGE_LETTER= and F_KLANG.KRO.

## Startup and native menu

Original startup code at 0x18df0, 0x18e0a and 0x18e24 invokes vintro.mux,
vcredits.mux and vtitre.mux respectively. All these clips, including their
S variants, exist locally. Header inspection confirms SINTRO/SCREDITS/
STITRE are 640x480, VINTRO is 320x200: the S/V prefix is a resolution
variant, not a subtitle-language selector. Exact visual contents of each
startup clip have not yet been reviewed during this investigation.

Our run path restores a checkpoint or enters showScriptScene directly,
bypassing this original startup sequence. Restore the startup presentation
without deleting or overriding existing manual or automatic saves.

User supplied the original wooden-crate Main menu reference, containing:
Return to game, New game, Load a game, Save a game, Options, Exit game.
Use this visual/layout reference instead of the earlier proposed generic
pixel-text title menu. Executable data exposes MAIN_PANEL, OPTIONS_PANEL,
DIVOPT_PANEL, VOLOPT_PANEL, LOAD_PANEL and SAVE_PANEL identifiers. Locate
and decode the panel assets before claiming the native menu is restored.

Requested future scope: a main-menu return after completion and from pause,
original logo/film startup, persistent speech/music/effects volumes, subtitle
visibility and a language selector limited to actual supported data.
