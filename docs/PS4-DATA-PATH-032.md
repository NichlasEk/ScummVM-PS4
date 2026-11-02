# PS4 packaged data fallback — 0.32

## Problem and change

A tester reported having to extract engine support files from the PKG and put
them into a custom Extra Path. The Vulkan build compiled SDL's system data
fallback as `/usr/local/share/scummvm`, while the package installs resources at
`/app0/data`. The initial configuration hid this mismatch by setting Extra Path
to `/app0/data`.

The PS4 branch of `OSystem_SDL::addSysArchivesToSearchSet` now registers
`/app0/data` independently of the configuration. SearchManager restores this
archive after every clear, at priority -1; game and user Extra Path directories
remain at priority 0. Non-PS4 DATA_PATH behavior is unchanged. Existing settings,
saves and game folders are preserved. The change is carried by
`experiments/scummvm-2026.4.0/vulkan.patch`; the older port and software
experiment were not changed.

## Validation

- Incremental PS4 cross-build passed.
- Patch replay from clean upstream matched all 156 modified files.
- PKG digest/signature validation passed; all 241 extracted payload files
  matched staging, including SFO. Extracted SFO identifies SCVM00265 / 0.32.
- Isolated shadPS4 test: Lure of the Temptress with a custom empty Extra Path
  and no lure.dat in its game directory. The unchanged 0.31 displays the
  missing engine data error; 0.32 reaches the Revolution intro using the
  packaged lure.dat.
- With a deliberately invalid lure.dat in the custom Extra Path, 0.32 displays
  the corrupt engine data error. This verifies that explicit user data still
  takes precedence over the packaged fallback.

Evidence and the repeatable local test script are in
`artifacts/data-path-032/`: baseline.png, fixed.png, override.png,
run-regression.py, per-run profiles/logs, replay.log, validate.log,
payload.json, sfo.txt and build-032.json. Initial X11 capture failed, and a
subsequent baseline image was black; the completed comparison explicitly
focuses the emulator and moves its pointer before capture.

The packaging tool initially failed because its default .NET bundle extraction
was incomplete. A fresh task-local DOTNET_BUNDLE_EXTRACT_BASE_DIR resolved it;
no SDK files were changed.

These are emulator startup/resource-resolution checks, not full gameplay or
physical PS4 proof. No FTP transfer, console installation or console launch was
performed in this task. Next physical test: install 0.32, retain a custom Extra
Path without copied engine support files, and launch a game needing packaged
motordata. Confirm it works without extracting the PKG.

## Artifacts and rollback

- PKG: `dist/ps4-data-path-032/ScummVM-2026.4.0-Vulkan-Test.pkg`
- Title ID: `SCVM00265`; APP_VER: `0.32`
- Content ID: `IV0000-SCVM00265_00-SCUMMVM2026VK001`
- PKG SHA-256: `9097e7e16cf9b3eea1d57b2ec7e0a3f3c6485dd73b7c37ace3d47fd868c9734e`
- ELF: `artifacts/data-path-032/candidate-032.elf`
- ELF SHA-256: `c9c9649b80e78a30e939de469f10d12e5ed3d44117fb3a11d5daff745cd1d843`
- Eboot: `build/scummvm-data-path-032-pkg/eboot.bin`
- Eboot SHA-256: `03942cc9b742f52fcd11bb479ecc5affbf906702274be0a6a33468e5c67e9a34`
- Rollback PKG: `dist/ps4-jonsson-031/ScummVM-2026.4.0-Vulkan-Test.pkg`
- Rollback SHA-256: `09e56870480e611ac30eb9b50f2baa7ec5a0145f2a947e87032aa4fbbe35c339`
- Preserved rollback ELF: `artifacts/data-path-032/baseline-031.elf`

The repository already had substantial tracked and untracked work. This task
adds the fallback hunk and this report, preserves the existing changes, and
makes no commit or push. Full build identity and artifact hashes are recorded
in build-032.json.
