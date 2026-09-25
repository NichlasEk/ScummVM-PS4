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
# Remove our generated entry point before refreshing the shared PS4 patch.
# Only remove the exact maintained block; do not overwrite unrelated edits.
python3 - "$SCUMMVM_SOURCE_DIR" "$root/scripts/pice/bootstrap.inc" <<'PYUNBOOT'
from pathlib import Path
import sys
p=Path(sys.argv[1])/'backends/platform/sdl/posix/posix-main.cpp'
if p.exists():
 s=p.read_text(); block=Path(sys.argv[2]).read_text().strip()
 if block in s:
  s=s.replace(block,'int res = scummvm_main(argc, argv);')
  s=s.replace('extern "C" void ps4_refresh_usb_mounts(void);\n','')
  p.write_text(s)
PYUNBOOT
"$root/scripts/prepare-scummvm.sh"
"$root/scripts/build-ps4-storage.sh"
"$root/scripts/build-ps4-input.sh"
"$root/scripts/build-ps4-jpeg.sh"
mkdir -p "$SCUMMVM_SOURCE_DIR/engines/pice"
python3 - "$root/engines/pice" "$SCUMMVM_SOURCE_DIR/engines/pice" <<'COPY'
import filecmp, pathlib, shutil, sys
src, dst = map(pathlib.Path, sys.argv[1:])
for p in src.iterdir():
 if p.is_file() and (not (dst/p.name).exists() or not filecmp.cmp(p, dst/p.name, shallow=False)):
  shutil.copyfile(p, dst/p.name)
COPY
# Keep the hardware USB bootstrap and emulator entry point in one source.
python3 - "$SCUMMVM_SOURCE_DIR" "$root/scripts/pice/bootstrap.inc" <<'PYBOOT'
from pathlib import Path
import sys
p=Path(sys.argv[1])/'backends/platform/sdl/posix/posix-main.cpp'
s=p.read_text()
declaration='extern "C" void ps4_refresh_usb_mounts(void);'
if declaration not in s:
 s=s.replace('extern "C" void ps4_release_usb_mounts(void);', 'extern "C" void ps4_release_usb_mounts(void);\n'+declaration)
block=Path(sys.argv[2]).read_text().strip()
if '#ifdef PICE_TESTBENCH' in s:
 start=s.index('#ifdef PICE_TESTBENCH'); end=s.index('#endif',start)+len('#endif')
 s=s[:start]+block+s[end:]
else:
 s=s.replace('int res = scummvm_main(argc, argv);',block)
if p.read_text()!=s: p.write_text(s)
PYBOOT
fingerprint=$( { sha256sum "$root/scripts/configure-scummvm.sh" "$root/engines/pice/configure.engine" "$root/build/ps4-jpeg/.build-id"; printf '%s\n' "$SCUMMVM_ENGINES" "$SCUMMVM_EXTRA_CXXFLAGS"; } | sha256sum | cut -d ' ' -f 1)
if [[ ! -f "$SCUMMVM_BUILD_DIR/config.h" || ! -f "$SCUMMVM_BUILD_DIR/.pice-config-id" ]] || [[ $(cat "$SCUMMVM_BUILD_DIR/.pice-config-id") != "$fingerprint" ]]; then
 "$root/scripts/configure-scummvm.sh" > "$root/artifacts/pice/configure.log" 2>&1
 touch "$SCUMMVM_BUILD_DIR/config.h"
 printf '%s\n' "$fingerprint" > "$SCUMMVM_BUILD_DIR/.pice-config-id"
fi
# Static dependency changes must relink even when no ScummVM source changed.
if [[ -f "$SCUMMVM_BUILD_DIR/scummvm" ]] && { [[ "$root/build/ps4-input/libSDL2.a" -nt "$SCUMMVM_BUILD_DIR/scummvm" ]] || [[ "$root/build/ps4-storage/libscummvm-jbc.a" -nt "$SCUMMVM_BUILD_DIR/scummvm" ]] || [[ "$root/build/ps4-jpeg/install/lib/libjpeg.a" -nt "$SCUMMVM_BUILD_DIR/scummvm" ]]; }; then
 rm -- "$SCUMMVM_BUILD_DIR/scummvm"
fi
make -C "$SCUMMVM_BUILD_DIR" -j"${BUILD_JOBS:-12}" > "$root/artifacts/pice/build.log" 2>&1
"$root/scripts/package-scummvm.sh" > "$root/artifacts/pice/package.log" 2>&1
printf 'Experimental package: %s/%s\n' "$SCUMMVM_DIST_DIR" "$SCUMMVM_PACKAGE_NAME"
