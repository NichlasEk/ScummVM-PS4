#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
export SCUMMVM_SOURCE_DIR="$root/.tools/scummvm-pice-src"
export SCUMMVM_BUILD_DIR="$root/build/scummvm-pice"
export SCUMMVM_STAGE_DIR="$root/build/scummvm-pice-pkg"
export SCUMMVM_DIST_DIR="$root/dist/pice-experimental"
export SCUMMVM_ENGINES=pice
export SCUMMVM_EXTRA_CXXFLAGS=-DPICE_TESTBENCH
export SCUMMVM_TITLE_ID=SCVM00003
export SCUMMVM_CONTENT_ID=IV0000-SCVM00003_00-SCUMMVMPICE00001
export SCUMMVM_TITLE='ScummVM PICE Preview'
export SCUMMVM_PACKAGE_NAME=ScummVM-PS4-PICE-preview.pkg
mkdir -p "$root/artifacts/pice" "$SCUMMVM_BUILD_DIR"
"$root/scripts/prepare-scummvm.sh"
"$root/scripts/build-ps4-storage.sh"
"$root/scripts/build-ps4-input.sh"
mkdir -p "$SCUMMVM_SOURCE_DIR/engines/pice"
python3 - "$root/engines/pice" "$SCUMMVM_SOURCE_DIR/engines/pice" <<'COPY'
import filecmp, pathlib, shutil, sys
src, dst = map(pathlib.Path, sys.argv[1:])
for p in src.iterdir():
 if p.is_file() and (not (dst/p.name).exists() or not filecmp.cmp(p, dst/p.name, shallow=False)):
  shutil.copyfile(p, dst/p.name)
COPY
# Testbench boots the native engine directly with externally mounted game data.
# This bootstrap is confined to the separate experimental source/build tree.
python3 - "$SCUMMVM_SOURCE_DIR" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1])/'backends/platform/sdl/posix/posix-main.cpp'
s=p.read_text()
if 'PICE_TESTBENCH' not in s:
 s=s.replace('int res = scummvm_main(argc, argv);', '''#ifdef PICE_TESTBENCH
 char arg0[] = "scummvm", arg1[] = "--path=/mnt/usb0", arg2[] = "--debuglevel=3", arg3[] = "--debugflags=detection", arg4[] = "pice";
 char *testArgs[] = {arg0, arg1, arg2, arg3, arg4, nullptr};
 int res = scummvm_main(5, testArgs);
#else
 int res = scummvm_main(argc, argv);
#endif''')
 p.write_text(s)
PY
fingerprint=$( { sha256sum "$root/scripts/configure-scummvm.sh" "$root/engines/pice/configure.engine"; printf '%s\n' "$SCUMMVM_ENGINES" "$SCUMMVM_EXTRA_CXXFLAGS"; } | sha256sum | cut -d ' ' -f 1)
if [[ ! -f "$SCUMMVM_BUILD_DIR/config.h" || ! -f "$SCUMMVM_BUILD_DIR/.pice-config-id" ]] || [[ $(cat "$SCUMMVM_BUILD_DIR/.pice-config-id") != "$fingerprint" ]]; then
 "$root/scripts/configure-scummvm.sh" > "$root/artifacts/pice/configure.log" 2>&1
 touch "$SCUMMVM_BUILD_DIR/config.h"
 printf '%s\n' "$fingerprint" > "$SCUMMVM_BUILD_DIR/.pice-config-id"
fi
# Static dependency changes must relink even when no ScummVM source changed.
if [[ -f "$SCUMMVM_BUILD_DIR/scummvm" ]] && { [[ "$root/build/ps4-input/libSDL2.a" -nt "$SCUMMVM_BUILD_DIR/scummvm" ]] || [[ "$root/build/ps4-storage/libscummvm-jbc.a" -nt "$SCUMMVM_BUILD_DIR/scummvm" ]]; }; then
 rm -- "$SCUMMVM_BUILD_DIR/scummvm"
fi
make -C "$SCUMMVM_BUILD_DIR" -j"${BUILD_JOBS:-12}" > "$root/artifacts/pice/build.log" 2>&1
"$root/scripts/package-scummvm.sh" > "$root/artifacts/pice/package.log" 2>&1
printf 'Experimental package: %s/%s\n' "$SCUMMVM_DIST_DIR" "$SCUMMVM_PACKAGE_NAME"
