#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
revision=9d9b9e93108a276c551aeffa390169ccc5148e15
src="$root/.tools/scummvm-sotc-android-src"
export JAVA_HOME=${JAVA_HOME:-/usr/lib/jvm/java-17-openjdk}
export ANDROID_SDK_ROOT=${ANDROID_SDK_ROOT:-/opt/android-sdk}
export ANDROID_NDK_ROOT=${ANDROID_NDK_ROOT:-$ANDROID_SDK_ROOT/ndk/30.0.16248370}
ndk=$(sed -n 's/^Pkg.Revision *= *//p' "$ANDROID_NDK_ROOT/source.properties")
mkdir -p "$src" "$root/artifacts/sotc/android" "$root/dist/sotc-android"
if [[ ! -f "$src/configure" ]]; then
 archive="$root/.tools/scummvm-$revision.tar"
 if [[ ! -f "$archive" ]]; then
  git -C "${SCUMMVM_REFERENCE:-/home/developer/ps4-scummvm/scummvm}" archive --format=tar -o "$archive" "$revision"
 fi
 tar -xf "$archive" -C "$src"
fi
mkdir -p "$src/engines/sotc"
cp "$root"/engines/sotc/* "$src/engines/sotc/"
python3 "$root/scripts/sotc/android/prepare.py" "$src" "$ndk"
for abi in arm64-v8a x86_64; do
 build="$root/build/scummvm-sotc-android-$abi"
 [[ "$abi" != arm64-v8a ]] || build="$root/build/scummvm-sotc-android"
 mkdir -p "$build"
 (cd "$build"
  "$src/configure" --host="android-$abi" --disable-all-engines --enable-engine=sotc --disable-debug
  make -j"${BUILD_JOBS:-12}" libscummvm.so
 ) > "$root/artifacts/sotc/android/build-$abi.log" 2>&1
 if [[ "$abi" == arm64-v8a ]]; then
  make -C "$build" -j"${BUILD_JOBS:-12}" > "$root/artifacts/sotc/android/apk-build.log" 2>&1
 fi
done
project="$root/build/scummvm-sotc-android/android_project"
mkdir -p "$project/lib/x86_64"
cp "$root/build/scummvm-sotc-android-x86_64/libscummvm.so" "$project/lib/x86_64/"
"$project/gradlew" -p "$project" assembleDebug > "$root/artifacts/sotc/android/universal-build.log" 2>&1
cp "$project/build/outputs/apk/debug/ScummVM-debug.apk" "$root/dist/sotc-android/SOTC-Preview-0.1.0.apk"
sha256sum "$root/dist/sotc-android/SOTC-Preview-0.1.0.apk"
