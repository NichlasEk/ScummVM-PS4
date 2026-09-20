#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export OO_PS4_TOOLCHAIN=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
sdk=$OO_PS4_TOOLCHAIN
build_dir=${SCUMMVM_BUILD_DIR:-"$root/build/scummvm"}
source_dir=${SCUMMVM_SOURCE_DIR:-"$root/.tools/scummvm-src"}
"$root/scripts/build-ps4-storage.sh"
"$root/scripts/build-ps4-input.sh"
mkdir -p "$build_dir"
cd "$build_dir"
export CXX=clang++ CC=clang AR=llvm-ar RANLIB=llvm-ranlib
export SDL_CONFIG=scummvm-sdl2-config
export PATH="$root/scripts:$PATH"
export PKG_CONFIG_LIBDIR="$sdk/lib/pkgconfig"
export CXXFLAGS="--target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -O2 -D__PS4__ -include $root/compat/ps4/include/ps4-abi.h -D_BSD_SOURCE -D_LIBCPP_HAS_NO_LOCALE -nostdinc++ -isysroot $sdk -isystem $sdk/include -isystem $sdk/include/c++/v1 ${SCUMMVM_EXTRA_CXXFLAGS:-}"
export LDFLAGS="--target=x86_64-pc-freebsd12-elf -fuse-ld=lld -nostdlib -Wl,-pie -Wl,--script=$sdk/link.x -Wl,--eh-frame-hdr -L$sdk/lib $sdk/lib/crt1.o -lc -lkernel"
export LIBS="$root/build/ps4-storage/libscummvm-jbc.a -lc++ -lc++abi -lc -lkernel"
engine_options=(--enable-all-engines --disable-engine=testbed,playground3d)
if [[ -n ${SCUMMVM_ENGINES:-} ]]; then
 engine_options=(--disable-all-engines --enable-engine="$SCUMMVM_ENGINES")
fi
"$source_dir/configure" --host=x86_64-pc-freebsd --backend=sdl \
 "${engine_options[@]}" --disable-detection-full \
 --enable-release --enable-optimizations --enable-text-console --enable-vkeybd \
 --opengl-mode=none --disable-opengl-game --disable-opengl-game-classic --disable-opengl-game-shaders \
 --disable-timidity --disable-alsa --disable-seq-midi --disable-mt32emu --disable-cloud --disable-sdlnet --disable-enet \
 --disable-libcurl --disable-discord --disable-taskbar --disable-system-dialogs --disable-libunity --disable-gtk \
 --disable-tts --disable-sparkle --disable-ogg --disable-vorbis --disable-tremor --disable-mad --disable-flac \
 --disable-fribidi --disable-zlib --disable-jpeg --disable-png --disable-gif --disable-theoradec --disable-vpx \
 --disable-faad --disable-fluidsynth --disable-fluidlite --disable-sonivox --disable-freetype2 \
 --disable-mikmod --disable-mpeg2 --disable-a52 --disable-openmpt --disable-mpcdec --disable-nasm --disable-readline --disable-sndio --enable-lua --disable-translation
