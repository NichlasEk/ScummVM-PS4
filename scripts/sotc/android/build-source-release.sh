#!/usr/bin/env bash
set -euo pipefail
src=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export ANDROID_SDK_ROOT=${ANDROID_SDK_ROOT:-/opt/android-sdk}
export ANDROID_NDK_ROOT=${ANDROID_NDK_ROOT:-$ANDROID_SDK_ROOT/ndk/30.0.16248370}
export JAVA_HOME=${JAVA_HOME:-/usr/lib/jvm/java-17-openjdk}
for abi in arm64-v8a x86_64; do
 mkdir -p "$src/build-$abi"
 (cd "$src/build-$abi"; ../configure --host="android-$abi" --disable-all-engines --enable-engine=sotc --disable-debug; make -j"${BUILD_JOBS:-12}" libscummvm.so)
done
make -C "$src/build-arm64-v8a" -j"${BUILD_JOBS:-12}"
project="$src/build-arm64-v8a/android_project"
mkdir -p "$project/lib/x86_64"
cp "$src/build-x86_64/libscummvm.so" "$project/lib/x86_64/"
"$project/gradlew" -p "$project" assembleDebug
cp "$project/build/outputs/apk/debug/ScummVM-debug.apk" "$src/SOTC-Preview-0.1.1.apk"
