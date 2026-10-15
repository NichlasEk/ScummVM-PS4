#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
mode=${1:-software}
case "$mode" in
 software) folder=ps4-audit ;;
 vulkan) folder=ps4-vulkan ;;
 *) echo 'Usage: prepare.sh software|vulkan' >&2; exit 2 ;;
esac
clean="$root/.tools/upstream-2026.4.0/scummvm-2026.4.0"
source="$root/.tools/upstream-2026.4.0/$folder"
patch="$root/experiments/scummvm-2026.4.0/$mode.patch"
local_patch="$root/experiments/scummvm-2026.4.0/local-engines.patch"
if [[ ! -d "$clean" ]]; then
 echo 'Extract the verified scummvm-2026.4.0.tar.xz archive first; see README.md.' >&2
 exit 1
fi
if [[ ! -d "$source" ]]; then
 cp -a --reflink=auto "$clean" "$source"
fi
if ! patch --dry-run --batch --fuzz=0 -R -p1 -d "$source" -i "$patch" >/dev/null 2>&1; then
 patch --dry-run --batch --forward --fuzz=0 -p1 -d "$source" -i "$patch"
 patch --batch --forward --fuzz=0 -p1 -d "$source" -i "$patch"
fi
if [[ ! -d "$source/engines/pice" && ! -d "$source/engines/sotc" ]]; then
 python3 "$root/scripts/sync-local-engines.py" "$source"
fi
if ! patch --dry-run --batch --fuzz=0 -R -p1 -d "$source" -i "$local_patch" >/dev/null 2>&1; then
 patch --dry-run --batch --forward --fuzz=0 -p1 -d "$source" -i "$local_patch"
 patch --batch --forward --fuzz=0 -p1 -d "$source" -i "$local_patch"
fi
printf 'Prepared: %s\n' "$source"
