#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
root=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
export SCUMMVM_SOURCE_DIR="$root/.tools/upstream-2026.4.0/ps4-audit"
export SCUMMVM_BUILD_DIR="$root/build/scummvm-2026.4.0-audit"
export SCUMMVM_STAGE_DIR="$root/build/scummvm-2026.4.0-pkg"
export SCUMMVM_DIST_DIR="$root/dist/upstream-2026.4.0"
export SCUMMVM_TITLE_ID=SCVM00264 SCUMMVM_CONTENT_ID=IV0000-SCVM00264_00-SCUMMVM2026TEST1
export SCUMMVM_TITLE='ScummVM 2026.4.0 Test' SCUMMVM_APP_VERSION=0.01
export SCUMMVM_PACKAGE_NAME=ScummVM-2026.4.0-Software-Test.pkg
bash "$root/scripts/package-scummvm.sh"
