# Vulkan 3D menu cursor — 1.25 (2026-09-29)

## Cause and fix

Thimbleweed Park draws its own game pointer and hides the backend cursor.
Opening the ScummVM menu requests a visible backend pointer again. The SDL
3D graphics manager delegates that pointer to `SDL_ShowCursor`, unlike the
2D manager which draws a cursor. The native PS4 VideoOut path has no visible
SDL system cursor, leaving the menu clickable but its pointer invisible.

The isolated Vulkan overlay now draws a fixed white arrow with a black outline
and transparent background after the GUI overlay. This replaces the SDL 3D
manager's system arrow; that manager already ignores game/theme cursor bitmaps.
It uses the existing window-space cursor coordinates (tip hotspot), scales for
the output height, and respects cursor visibility and GUI-overlay state.
`showMouse` now maintains the backend visibility state and returns its previous
value. The texture is released before the graphics context closes.
Game-owned pointers and the existing 2D launcher cursor are unaffected.

Durable sources: `compat/ps4gl/scummvm-menu-cursor.inc` and the checked source
edits in `scripts/scummvm-vulkan-runtime.py`. The generated source checkout
remains ignored. This applies to the shared SDL 3D menu path, not a TWP-only
workaround.

## Emulator regression

Used an isolated shadPS4 profile under `build/cursor-test/profile`, with owned
TWP data linked read-only by convention and saves copied into the test profile.
The user's normal configuration and saves were not edited.

- Before: opened the actual ScummVM menu from TWP; menu visible, pointer absent.
- After: the extracted 1.25 package renders the pointer; stick movement changes
  its position and highlights the button under its tip.
- Cross clicks Options; the nested options dialog retains the pointer.
- Triangle closes the nested dialog and menu; the fixed arrow disappears on
  return to the engine screen.
- Loaded copied `twp.000` at the Trestle Trail gate: the in-game menu pointer
  is visible, and closing the menu restores the engine crosshair without an
  extra arrow.

Captures are in `artifacts/cursor-test/`: `before-menu.png`,
`menu-position.png`, `menu-hover.png`, `options-after.png`, `resumed.png`,
`saved-game.png`, `saved-menu.png`, `saved-resumed.png`. These were visually inspected, not inferred
from successful GPU submissions. Emulator sessions are terminated after the
bounded checks; graceful application shutdown is not a result of this test.
Physical PS4 confirmation remains pending.

## Build and hardware test

```sh
SCUMMVM_CONTROLLER_ONLY=1 SCUMMVM_APP_VERSION=1.25 \
 SCUMMVM_TITLE='ScummVM Vulkan Cursor 1.25' scripts/build-scummvm-vulkan.sh
```

Package: `dist/ScummVM-PS4-Vulkan-Experimental.pkg`, title `SCVM00010`, version
1.25. It preserves controller-only startup and the compiled color-clear fix.
No serial/operation submission diagnostic markers are enabled.

For the console: load a TWP save, open the ScummVM menu with Options, move the
left stick, click a menu button with Cross, then close with Triangle. Repeat
menu entry and verify the game's own cursor returns without an extra arrow.

Package validation and all 219 extracted manifest-file checks passed.
SHA-256: `87a1b574782a49bf3b17a0866b0c9dcd65a6595fe2a07fe14fc6272662da773f`.
Audit: `artifacts/scummvm-vulkan/cursor-125-package-audit.json`.
