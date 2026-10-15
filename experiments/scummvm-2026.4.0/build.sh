#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
mode=${1:-software}
engines=${2:-scumm,pice,sotc}
[[ "$engines" != all ]] || engines=
export SCUMMVM_ENGINES="$engines"
bash "$root/experiments/scummvm-2026.4.0/prepare.sh" "$mode"
case "$mode" in
 software)
  bash "$root/experiments/scummvm-2026.4.0/configure.sh"
  build="$root/build/scummvm-2026.4.0-audit" ;;
 vulkan)
  bash "$root/experiments/scummvm-2026.4.0/configure-vulkan.sh"
  build="$root/build/scummvm-2026.4.0-vulkan" ;;
esac
make -C "$build" -j"${BUILD_JOBS:-8}"
# Packaging is separate so a broad engine experiment cannot overwrite a known test PKG.
