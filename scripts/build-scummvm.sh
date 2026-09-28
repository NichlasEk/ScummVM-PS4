#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [[ ${1:-} == --vulkan && $# == 1 ]]; then
 exec "$root/scripts/build-scummvm-vulkan.sh"
elif [[ $# != 0 ]]; then
 echo 'Usage: scripts/build-scummvm.sh [--vulkan]' >&2
 exit 2
fi
"$root/scripts/prepare-scummvm.sh"
python3 "$root/scripts/sync-local-engines.py" "$root/.tools/scummvm-src"
"$root/scripts/build-ps4-storage.sh"
"$root/scripts/build-ps4-input.sh"
"$root/scripts/build-ps4-jpeg.sh"
"$root/scripts/build-ps4-mad.sh"
"$root/scripts/build-ps4-freetype.sh"
"$root/scripts/build-ps4-vorbis.sh"
"$root/scripts/build-ps4-png.sh"
"$root/scripts/build-ps4-theora.sh"
"$root/scripts/build-ps4-mpeg2.sh"
"$root/scripts/build-ps4-gif.sh"
"$root/scripts/build-ps4-flac.sh"
"$root/scripts/build-ps4-vpx.sh"
"$root/scripts/build-ps4-a52.sh"
mkdir -p "$root/artifacts/scummvm" "$root/build/scummvm"
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
fingerprint=$( { sha256sum "$root/scripts/configure-scummvm.sh" "$root/scripts/sync-local-engines.py" "$root/engines/pice/configure.engine" "$root/engines/sotc/configure.engine" "$root/scripts/scummvm-sdl2-config" "$root/build/ps4-jpeg/.build-id" "$root/build/ps4-mad/.build-id" "$root/build/ps4-freetype/.build-id" "$root/build/ps4-vorbis/.build-id" "$root/build/ps4-png/.build-id" "$root/build/ps4-theora/.build-id" "$root/build/ps4-mpeg2/.build-id" "$root/build/ps4-gif/.build-id" "$root/build/ps4-flac/.build-id" "$root/build/ps4-vpx/.build-id" "$root/build/ps4-a52/.build-id"; printf '%s\n' "$sdk" "${SCUMMVM_ENGINES:-all}" "${SCUMMVM_EXTRA_CXXFLAGS:-}"; } | sha256sum | cut -d ' ' -f 1)
stamp="$root/build/scummvm/.configuration-id"
if [[ ! -f "$root/build/scummvm/config.mk" || ! -f "$stamp" ]] || [[ $(cat "$stamp") != "$fingerprint" ]]; then
    "$root/scripts/configure-scummvm.sh" 2>&1 | tee "$root/artifacts/scummvm/configure.log"
    touch "$root/build/scummvm/config.h"
    printf '%s\n' "$fingerprint" > "$stamp"
fi
# Static dependency changes must relink even when no ScummVM source changed.
if [[ -f "$root/build/scummvm/scummvm" ]] && { [[ "$root/build/ps4-input/libSDL2.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-storage/libscummvm-jbc.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-jpeg/install/lib/libjpeg.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-mad/install/lib/libmad.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-freetype/install/lib/libfreetype.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-vorbis/install/lib/libogg.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-vorbis/install/lib/libvorbis.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-vorbis/install/lib/libvorbisfile.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-png/install/lib/libpng16.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-png/install/lib/libz.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-theora/install/lib/libtheoradec.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-mpeg2/install/lib/libmpeg2.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-gif/install/lib/libgif.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-flac/install/lib/libFLAC.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-vpx/install/lib/libvpx.a" -nt "$root/build/scummvm/scummvm" ]] || [[ "$root/build/ps4-a52/install/lib/liba52.a" -nt "$root/build/scummvm/scummvm" ]]; }; then
 rm -- "$root/build/scummvm/scummvm"
fi
make -C "$root/build/scummvm" -j"${BUILD_JOBS:-12}" 2>&1 | tee "$root/artifacts/scummvm/build.log"
"$root/scripts/package-scummvm.sh" 2>&1 | tee "$root/artifacts/scummvm/package.log"
