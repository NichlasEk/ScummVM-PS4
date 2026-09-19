#!/usr/bin/env bash
set -euo pipefail
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
emulator="$project_dir/.tools/shadps4/Shadps4-sdl.AppImage"
if [[ ! -x "$emulator" ]]; then
    "$project_dir/scripts/setup-emulator.sh"
fi
target=scummvm
if [[ ${1:-} == --diagnostic ]]; then
    target=diagnostic
    shift
fi
if [[ $# != 0 ]]; then
    echo "Usage: $0 [--diagnostic]" >&2
    exit 2
fi
if [[ "$target" == diagnostic ]]; then
    "$project_dir/scripts/build-diagnostic.sh"
    eboot="$project_dir/diagnostic/build-openorbis/pkgroot/eboot.bin"
else
    "$project_dir/scripts/build-scummvm.sh"
    eboot="$project_dir/build/scummvm-pkg/eboot.bin"
fi
export XDG_DATA_HOME="$project_dir/.tools/xdg"
export XDG_CONFIG_HOME="$project_dir/.tools/config"
export XDG_CACHE_HOME="$project_dir/.tools/cache"
mkdir -p "$XDG_DATA_HOME" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$project_dir/artifacts/emulator"
"$project_dir/scripts/setup-scummvm-input.py"
log="$project_dir/artifacts/emulator/run-$(date +%Y%m%d-%H%M%S).log"
echo "Launching $target. Log: $log"
echo "F7 captures/releases PC mouse; left/right mouse buttons click."
echo "WASD = left stick; N = Cross; B = Circle; Return = Options; F12 = screenshot."
echo "Time limit: ${EMULATOR_TIMEOUT:-0}s (EMULATOR_TIMEOUT=0 disables it)."
ulimit -c 0
timeout --kill-after=3s "${EMULATOR_TIMEOUT:-0}s" "$emulator" --fullscreen false \
    "$eboot" 2>&1 | tee "$log"
