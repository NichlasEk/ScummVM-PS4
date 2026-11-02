#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
"$root/scripts/build-ps4-input.sh"
source="$root/.tools/sdl-input-src"
[[ $(cat "$source/.source-revision") == 7774cf3e7d04448c44d569c50d1f9a4532a2efe7 ]]
build="$root/build/ps4-sdlgl"
mkdir -p "$build"
# Keep the SDK archive and the normal input-patched SDL completely unchanged.
clang --target=x86_64-pc-freebsd12-elf -fPIC -O2 -D__PS4__ -D__ORBIS__ \
 -isysroot "$sdk" -isystem "$sdk/include" -I"$source/include" -I"$source/src" \
 -I"$root/compat/ps4gl" -I"$root/.tools/opengnm-stack/Vulkan-Headers/include" \
 -Wall -Wextra -Werror -c "$root/compat/ps4gl/ps4gl-sdl.c" -o "$build/SDL_ps4video.c.o"
# SDK SDL disables GL attribute setters at compile time. Enable the generic
# GL API in this archive only; all actual GL calls still use procedure lookup.
clang --target=x86_64-pc-freebsd12-elf -fPIC -O2 -D__PS4__ -D__ORBIS__ -DSDL_VIDEO_OPENGL=1 \
 -isysroot "$sdk" -isystem "$sdk/include" -I"$source/include" -I"$source/src" \
 -c "$source/src/video/SDL_video.c" -o "$build/SDL_video.c.o"
cp "$root/build/ps4-input/libSDL2.a" "$build/libSDL2.a"
llvm-ar r "$build/libSDL2.a" "$build/SDL_ps4video.c.o" "$build/SDL_video.c.o"
# Keep audio queued while the next callback renders. Draining with a NULL
# output after every buffer serializes playback and synthesis (MT-32 stutters).
python3 - "$source" "$build" <<'PY'
from pathlib import Path
import sys
source, build = map(Path, sys.argv[1:])
s = (source/'src/audio/ps4/SDL_ps4audio.c').read_text()
old = 'sceAudioOutOutput(this->hidden->h_aout, NULL);'
assert s.count(old) == 1, 'Unexpected SDL audio wait implementation'
s = s.replace(old, '(void)this; /* ScummVM PS4: PlayDevice already paces queued output. */')
s = s.replace('// Call with NULL to wait until play of current buffer is complete', '')
s = s.replace('"../../SDL_internal.h"', '"SDL_internal.h"')
s = s.replace('<audioout.h>', '<orbis/AudioOut.h>').replace('<user_service.h>', '<orbis/UserService.h>')
s = s.replace('SCE_AUDIO_OUT_', 'ORBIS_AUDIO_OUT_').replace('SCE_USER_SERVICE_', 'ORBIS_USER_SERVICE_')
# The pinned source always sets attributes to zero; the SDK omits this macro.
s = s.replace('p_fmt = 0, p_attr = 0', 'p_fmt = 0')
s = s.replace('((p_fmt&255) | ((p_attr&15) << ORBIS_AUDIO_OUT_PARAM_ATTR_SHIFT))', 'p_fmt')
(build/'SDL_ps4audio.c').write_text(s)
PY
clang --target=x86_64-pc-freebsd12-elf -fPIC -O2 -D__PS4__ -D__ORBIS__ \
 -isysroot "$sdk" -isystem "$sdk/include" -I"$source/include" -I"$source/src" \
 -I"$source/src/audio/ps4" -c "$build/SDL_ps4audio.c" -o "$build/SDL_ps4audio.c.o"
llvm-ar r "$build/libSDL2.a" "$build/SDL_ps4audio.c.o"
