#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
root=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
export SCUMMVM_SOURCE_DIR="$root/.tools/upstream-2026.4.0/ps4-vulkan"
export SCUMMVM_BUILD_DIR="$root/build/scummvm-2026.4.0-vulkan"
export SCUMMVM_STAGE_DIR=${SCUMMVM_STAGE_DIR:-"$root/build/scummvm-2026.4.0-vulkan-pkg"}
export SCUMMVM_DIST_DIR=${SCUMMVM_DIST_DIR:-"$root/dist/upstream-2026.4.0"}
export SCUMMVM_TITLE_ID=SCVM00265 SCUMMVM_CONTENT_ID=IV0000-SCVM00265_00-SCUMMVM2026VK001
export SCUMMVM_TITLE=${SCUMMVM_TITLE:-'ScummVM PS4'} SCUMMVM_APP_VERSION=${SCUMMVM_APP_VERSION:-0.32}
export SCUMMVM_EXPERIMENTAL_VULKAN=1
# Native mouse reports and the controller can coexist. Retain an opt-out for probes.
export SCUMMVM_CONTROLLER_ONLY=${SCUMMVM_CONTROLLER_ONLY:-0}
export SCUMMVM_PACKAGE_NAME=ScummVM-2026.4.0-Vulkan-Test.pkg
bash "$root/experiments/scummvm-2026.4.0/package-vulkan-local.sh"
