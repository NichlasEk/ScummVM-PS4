#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
export SCUMMVM_SOURCE_DIR=${SCUMMVM_SOURCE_DIR:-"$root/.tools/upstream-2026.4.0/ps4-vulkan"}
export SCUMMVM_BUILD_DIR=${SCUMMVM_BUILD_DIR:-"$root/build/scummvm-2026.4.0-vulkan"}
export SCUMMVM_EXPERIMENTAL_VULKAN=1 SCUMMVM_SDL_CONFIG=scummvm-vulkan-sdl2-config
export SCUMMVM_ENGINES=${SCUMMVM_ENGINES-scumm,pice,sotc}
native="$root/.tools/opengnm-stack"
bridge="$SCUMMVM_BUILD_DIR/bridge"
mkdir -p "$bridge"
# Reuse the installed dependency archives without rebuilding the existing port.
cp "$root/build/scummvm-vulkan/bridge/"*.o "$bridge/"
cp --reflink=auto "$root/build/scummvm-vulkan/bridge/libpsbc-scummvm.a" "$bridge/"
export SCUMMVM_VULKAN_LIBS="$bridge/ps4gl.o $bridge/ps4gl-vulkan.o $bridge/ps4gl-glsl.o $bridge/ps4gl-frame.o $bridge/ps4gl-display.o -Wl,--whole-archive $native/vulkan-ps4/libvulkan_ps4.a $native/opengnm/libopengnm.a -Wl,--no-whole-archive $bridge/libpsbc-scummvm.a $root/build/glslang-orbis/libglslang-ps4.a -lunwind -lSceGnmDriver"
python3 - "$root" <<'PYMAIN'
from pathlib import Path
import sys
root=Path(sys.argv[1]); target=root/'artifacts/upstream-2026.4.0/ps4gl-main.c'
target.parent.mkdir(parents=True,exist_ok=True)
target.write_text((root/'compat/ps4gl/ps4gl-main.c').read_text().replace('/data/scummvm-vulkan','/data/scummvm-2026-vulkan'))
PYMAIN
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
clang --target=x86_64-pc-freebsd12-elf -fPIC -O2 -D__PS4__ -D__ORBIS__ -isysroot "$sdk" -isystem "$sdk/include" -I"$root/compat/ps4gl" -I"$native/Vulkan-Headers/include" -I"$native/vulkan-ps4/include" -I"$root/.tools/glslang" -c "$root/artifacts/upstream-2026.4.0/ps4gl-main.c" -o "$bridge/ps4gl-main.o"
bash "$root/experiments/scummvm-2026.4.0/configure.sh"
printf '\nLDFLAGS += -Wl,--wrap=main -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=free -Wl,-z,max-page-size=0x4000\nLIBS += %s\n' "$bridge/ps4gl-main.o" >> "$SCUMMVM_BUILD_DIR/config.mk"
