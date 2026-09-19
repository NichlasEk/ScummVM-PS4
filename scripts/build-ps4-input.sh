#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
source="$root/.tools/sdl-input-src"
revision=7774cf3e7d04448c44d569c50d1f9a4532a2efe7
if [[ ! -f "$source/.source-revision" ]]; then
 mkdir -p "$source"
 if git -C "$root/.tools/SDL-PS4" cat-file -e "$revision" 2>/dev/null; then
  git -C "$root/.tools/SDL-PS4" archive "$revision" | tar -x -C "$source"
 else
  curl -fL "https://github.com/OpenOrbis/SDL-PS4/archive/$revision.tar.gz" | tar -xz --strip-components=1 -C "$source"
 fi
 printf '%s\n' "$revision" > "$source/.source-revision"
fi
python3 - "$root" "$sdk" "$source" <<'PY'
from pathlib import Path
import hashlib,subprocess,sys,shutil
root,sdk,source=map(Path,sys.argv[1:]);build=root/'build/ps4-input';build.mkdir(parents=True,exist_ok=True)
p=source/'src/joystick/ps4/SDL_sysjoystick.c'
s=p.read_text()
s=s.replace('#include <pad.h>', '#include <orbis/Pad.h>\n#include <orbis/UserService.h>')
s=s.replace('#include <kernel.h>', '#include <orbis/libkernel.h>')
s=s.replace('ScePadData','OrbisPadData').replace('SceUserService','OrbisUserService').replace('SCE_PAD_', 'ORBIS_PAD_').replace('SCE_USER_SERVICE_', 'ORBIS_USER_SERVICE_')
s=s.replace('#define setA(n,axe) { Sint16 tmp = aS16(axe); if (tmp < -1000 || tmp > 1000) { SDL_PrivateJoystickAxis(joystick, (n), tmp); } }', '''// ScummVM PS4 modification: always report neutral; filtering it leaves stale velocity.
#define setA(n,axe) { Sint16 tmp = aS16(axe); if (tmp >= -1000 && tmp <= 1000) tmp = 0; SDL_PrivateJoystickAxis(joystick, (n), tmp); }''')
if s!=p.read_text():p.write_text(s)
fingerprint=hashlib.sha256((s+(source/'.source-revision').read_text()).encode()+ (sdk/'lib/libSDL2.a').read_bytes()).hexdigest()
stamp=build/'.build-id';lib=build/'libSDL2.a'
if lib.exists() and stamp.exists() and stamp.read_text()==fingerprint:sys.exit(0)
subprocess.run(['clang','--target=x86_64-pc-freebsd12-elf','-fPIC','-O2','-D__PS4__','-D__ORBIS__','-isysroot',str(sdk),'-isystem',str(sdk/'include'),'-I',str(source/'include'),'-c',str(p),'-o',str(build/'SDL_sysjoystick.c.o')],check=True)
shutil.copyfile(sdk/'lib/libSDL2.a',lib)
subprocess.run(['llvm-ar','r',str(lib),str(build/'SDL_sysjoystick.c.o')],check=True)
stamp.write_text(fingerprint)
PY
