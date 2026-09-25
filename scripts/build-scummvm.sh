#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
"$root/scripts/prepare-scummvm.sh"
"$root/scripts/build-ps4-storage.sh"
"$root/scripts/build-ps4-input.sh"
"$root/scripts/build-ps4-jpeg.sh"
"$root/scripts/build-ps4-mad.sh"
"$root/scripts/build-ps4-freetype.sh"
mkdir -p "$root/artifacts/scummvm" "$root/build/scummvm"
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
fingerprint=$( { sha256sum "$root/scripts/configure-scummvm.sh" "$root/scripts/scummvm-sdl2-config" "$root/build/ps4-jpeg/.build-id" "$root/build/ps4-mad/.build-id" "$root/build/ps4-freetype/.build-id"; printf '%s\n' "$sdk" "${SCUMMVM_ENGINES:-all}" "${SCUMMVM_EXTRA_CXXFLAGS:-}"; } | sha256sum | cut -d ' ' -f 1)
stamp="$root/build/scummvm/.configuration-id"
if [[ ! -f "$root/build/scummvm/config.mk" || ! -f "$stamp" ]] || [[ $(cat "$stamp") != "$fingerprint" ]]; then
    "$root/scripts/configure-scummvm.sh" 2>&1 | tee "$root/artifacts/scummvm/configure.log"
    touch "$root/build/scummvm/config.h"
    printf '%s\n' "$fingerprint" > "$stamp"
fi
# Static dependency changes must relink even when no ScummVM source changed.
if [[ -f "$root/build/scummvm/scummvm" ]] && { [[ "$root/build/ps4-input/libSDL2.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-storage/libscummvm-jbc.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-jpeg/install/lib/libjpeg.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-mad/install/lib/libmad.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-freetype/install/lib/libfreetype.a" -nt "$root/build/scummvm/scummvm" ]]; }; then
 rm -- "$root/build/scummvm/scummvm"
fi
make -C "$root/build/scummvm" -j"${BUILD_JOBS:-12}" 2>&1 | tee "$root/artifacts/scummvm/build.log"
"$root/scripts/package-scummvm.sh" 2>&1 | tee "$root/artifacts/scummvm/package.log"
