#!/usr/bin/env bash
# Run the already-built experimental application, with separate settings/saves.
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
eboot="$root/build/scummvm-vulkan-pkg/eboot.bin"
[[ -f "$eboot" ]] || { echo 'Run scripts/build-scummvm-vulkan.sh first' >&2; exit 1; }
export SDL_VIDEODRIVER=x11
export XDG_DATA_HOME="$root/.tools/xdg-scummvm-vulkan"
export XDG_CONFIG_HOME="$root/.tools/config-scummvm-vulkan"
export XDG_CACHE_HOME="$root/.tools/cache-scummvm-vulkan"
mkdir -p "$XDG_DATA_HOME/shadPS4" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$root/artifacts/scummvm-vulkan"
# Reuse only emulator graphics options, never game settings or save files.
if [[ ! -f "$XDG_DATA_HOME/shadPS4/config.json" && -f "$root/.tools/xdg-vulkan/shadPS4/config.json" ]]; then
 cp "$root/.tools/xdg-vulkan/shadPS4/config.json" "$XDG_DATA_HOME/shadPS4/config.json"
fi
log="$root/artifacts/scummvm-vulkan/run-$(date +%Y%m%d-%H%M%S).log"
echo "Experimental startup; application log: $XDG_DATA_HOME/shadPS4/data/scummvm-vulkan"
echo "Emulator log: $log"
ulimit -c 0
timeout --kill-after=3s "${EMULATOR_TIMEOUT:-45}s" \
 "$root/.tools/shadps4/Shadps4-sdl.AppImage" --fullscreen false "$eboot" 2>&1 | tee "$log"
