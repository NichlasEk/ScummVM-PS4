# SOTC Preview Android 0.1.0

Android companion to the PS4 preview, using the same `engines/sotc` source.
Upstream ScummVM revision: `9d9b9e93108a276c551aeffa390169ccc5148e15`.
Engine baseline: `db6473d`. This is an experimental fork, not an official ScummVM release.

## Install and play

Install `SOTC-Preview-0.1.0.apk` from EutherOxide Apps. Android 8+ and ARM64
are supported; the APK also includes x86_64 for emulator testing.
Copy your own European CD version's SHADOW directory to a folder on the phone.
Game data is not included in either download.
Choose **Add Game → Add a new folder**, open that directory in Android's picker,
choose **Use this folder**, grant access, then **Choose → OK → Start**.
The experimental-engine warning is expected.

The top-right pointer icon switches ScummVM touch modes. Move the pointer to the
picture's top edge for the game's original action toolbar. Landscape is recommended.
The **SOTC** button offers save (Q), load latest (U), game menu (Escape), help,
and export. Use save/load while the game is running. Export writes a ZIP containing
both alternating quick saves from the default internal save directory; custom save
paths are not exported. Send that ZIP with a screenshot when reporting a blocker.
Uninstalling deletes internal saves, so export first. Updating with the same signing
key preserves them. Save compatibility can change as the experimental engine evolves.

## Build

Run `scripts/sotc/build-android.sh` from this repository. Requirements used:
Linux, Java 17, Android SDK platform 36, NDK 30.0.16248370, make, Python 3,
and upstream's Gradle 8.13 wrapper/AGP 8.9.1. Gradle downloads dependencies.
`ANDROID_SDK_ROOT`, `ANDROID_NDK_ROOT`, `JAVA_HOME`, `BUILD_JOBS` can be overridden.
The pinned upstream archive is `.tools/scummvm-<revision>.tar`; if missing,
`SCUMMVM_REFERENCE` must point to an upstream git checkout containing that revision.
The build uses a separate Android source tree, without PS4 backend patches.
Output: `dist/sotc-android/SOTC-Preview-0.1.0.apk`.
Package: `se.apothic.sotc.debug`; versionCode 1. Preserve the local Android debug
keystore for updates. Signing keys are not part of the source download.

The corresponding source download includes the complete patched upstream tree,
licenses, engine, and a standalone `build-sotc-preview.sh` to build both ABIs.

## Validation

2026-09-24: both native ABIs compiled; universal APK signature verified. Installed
on the x86_64 Android emulator; SAF folder grant and game detection passed, opening
sequence and gameplay rendered, mobile save created `sotc-sotc.quick1`, and export
created a valid ZIP with the same save bytes. Load resumed gameplay. No physical
ARM64 phone has been tested yet. Existing engine gameplay limitations still apply.
