#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
src="$root/.tools/scummvm-sotc-android-src"
[[ -f "$src/configure" ]] || { echo 'Run build-android.sh first' >&2; exit 1; }
cp "$root/scripts/sotc/android/build-source-release.sh" "$src/build-sotc-preview.sh"
cp "$root/docs/SOTC-ANDROID.md" "$src/SOTC-ANDROID.md"
mkdir -p "$root/dist/sotc-android"
XZ_OPT='-T2 -1' tar --exclude='build-arm64-v8a' --exclude='build-x86_64' --exclude='.git' -cJf "$root/dist/sotc-android/SOTC-Preview-0.1.1-source.tar.xz" -C "$root/.tools" scummvm-sotc-android-src
