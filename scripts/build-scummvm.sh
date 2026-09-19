#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
"$root/scripts/prepare-scummvm.sh"
mkdir -p "$root/artifacts/scummvm" "$root/build/scummvm"
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
fingerprint=$( { sha256sum "$root/scripts/configure-scummvm.sh" "$root/scripts/scummvm-sdl2-config"; printf '%s\n' "$sdk"; } | sha256sum | cut -d ' ' -f 1)
stamp="$root/build/scummvm/.configuration-id"
if [[ ! -f "$root/build/scummvm/config.mk" || ! -f "$stamp" ]] || [[ $(cat "$stamp") != "$fingerprint" ]]; then
    "$root/scripts/configure-scummvm.sh" 2>&1 | tee "$root/artifacts/scummvm/configure.log"
    touch "$root/build/scummvm/config.h"
    printf '%s\n' "$fingerprint" > "$stamp"
fi
make -C "$root/build/scummvm" -j"${BUILD_JOBS:-12}" 2>&1 | tee "$root/artifacts/scummvm/build.log"
"$root/scripts/package-scummvm.sh" 2>&1 | tee "$root/artifacts/scummvm/package.log"
