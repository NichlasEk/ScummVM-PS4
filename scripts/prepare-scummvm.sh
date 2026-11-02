#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
revision=9d9b9e93108a276c551aeffa390169ccc5148e15
source_dir=${SCUMMVM_SOURCE_DIR:-"$root/.tools/scummvm-src"}
patch_file="$root/patches/scummvm/ps4.patch"
patch_id=$(sha256sum "$patch_file" | cut -d ' ' -f 1)
mkdir -p "$root/.tools"
if [[ ! -f "$source_dir/configure" ]]; then
    archive="$root/.tools/scummvm-$revision.tar"
    reference=${SCUMMVM_REFERENCE:-$root/../ps4-scummvm/scummvm}
    if [[ -d "$reference/.git" ]] && git -C "$reference" cat-file -e "$revision^{commit}"; then
        git -C "$reference" archive --format=tar -o "$archive" "$revision"
        mkdir -p "$source_dir"
        tar -xf "$archive" -C "$source_dir"
    else
        git init "$source_dir"
        git -C "$source_dir" remote add origin https://github.com/scummvm/scummvm.git
        git -C "$source_dir" fetch --depth 1 origin "$revision"
        git -C "$source_dir" checkout --detach "$revision"
    fi
fi
if [[ -f "$source_dir/.ps4-patch-id" ]] && [[ $(cat "$source_dir/.ps4-patch-id") == "$patch_id" ]]; then
    exit 0
fi
# Refuse to overwrite unrelated source edits. Also accept a patch already applied.
if git -C "$source_dir" apply --reverse --check "$patch_file" 2>/dev/null; then
    :
else
    if [[ -f "$source_dir/.ps4-applied.patch" ]]; then
        git -C "$source_dir" apply --reverse --check "$source_dir/.ps4-applied.patch"
        git -C "$source_dir" apply --reverse "$source_dir/.ps4-applied.patch"
    fi
    git -C "$source_dir" apply --check "$patch_file"
    git -C "$source_dir" apply "$patch_file"
fi
cp "$patch_file" "$source_dir/.ps4-applied.patch"
printf '%s\n' "$patch_id" > "$source_dir/.ps4-patch-id"
printf '%s\n' "$revision" > "$source_dir/.ps4-revision"
