# Release review — 2026-09-30

Candidate: **ScummVM Vulkan Experimental 1.26**, ScummVM 2.9.1,
title ID `SCVM00010`, content ID `IV0000-SCVM00010_00-SCUMMVMPVULKAN01`.

PKG SHA-256:
`4ce002d7c5599d59a654e5b6e62db593ac22ca970dbd99f05bbafa50eee0e338`

## Completed checks

- Fresh full-engine compile/link and PKG creation succeeded. No focused
  `SCUMMVM_ENGINES` override was used.
- **109 top-level static game engines** are enabled. The only skipped engines
  are TestBed and Playground3D. Enabled subengine flags are also preserved in
  ENGINE-CONFIG.txt. PICE, SOTC and Director pass the build's required-engine gate.
- Maintained PICE and SOTC source files byte-match the prepared source tree
  compiled for this candidate. Their metaengine implementations are present in
  the linked application ELF.
- The required codec and rendering configuration passed the build gates.
  JPEG, MP3, FLAC, Vorbis, PNG, GIF, Theora, VPX, MPEG-2, A/52, FreeType,
  MT-32 emulation and the virtual keyboard are enabled.
- The application import allowlist passed: only PS4 firmware-service imports
  were found, including GnmDriver and Mouse. No host-library or Piglet import
  was introduced.
- LibOrbisPkg's PKG hash/signature validation passed.
- All **220 GP4-listed payload files** were extracted and checked. Ordinary
  payloads byte-match staging; every input SFO field is preserved in the
  packager-augmented SFO. The generated `sce_sys/keystone` is recorded separately,
  bringing the extracted file inventory to **221**.
- The payload contains the application, runtime modules, icon/theme, virtual
  keyboard, engine support data, licenses and **144 shader files**. The only
  file under `games/` is README.txt. There are no bundled commercial games,
  user saves, game configuration or MT-32 ROMs.
- Controller-only startup is enabled. Serial submission and operation-prefix
  diagnostic markers are absent; driver logs confirm both diagnostics disabled.
- The actual extracted package was run in shadPS4 with a new profile containing
  no games or saves. Bootstrap succeeded, the first presentations returned zero,
  the native color-clear path executed, and the launcher screenshot was visually
  inspected. The ScummVM theme, icons and virtual keyboard load successfully.
  A first test used the input helper's partial INI and fell back to the builtin
  theme; the final test removed that test-generated INI so the application could
  seed its real first-launch defaults.
- Every file in both compressed source archives was re-read and checked against
  the per-file SHA-256 manifests stored inside them. SHA256SUMS covers the final
  release files.

The screenshot is supplied in `verification/launcher.png`. Local detailed logs
are retained in `artifacts/public-vulkan-2026-09-30/` in the maintainer's checkout.
The emulator session was terminated after the bounded launch check; graceful
application shutdown, new gameplay/save-load and physical execution of this
exact candidate were not tested in this review.

## Public-release status

The application and source snapshots are assembled as an **experimental release
candidate**. PICE and SOTC remain under development; enabled engines do not
establish complete game compatibility. Physical TWP gameplay was reported on
1.24; the 1.25 menu fix was emulator-tested. This new 1.26 package has not yet
been tested on a physical PS4.

The subsequent online investigation recovered the official SDK artifact and
its LLVM/SDL dependency releases by exact hash comparison. Their published
source references and runtime notices are supplied with a separate source
supplement. Musl's historical checkout remains inferred from the build recipe,
commit history and 183 matching headers. An independent historical rebuild has
not been performed. See OPENORBIS-PROVENANCE.md and SOURCES.md for details.

The maintainer elected to retain this experimental package. Its PKG hash above
is unchanged; the newly recovered runtime notices accompany the package under
`licenses/`. The SDK's own license is also a PKG payload file. This is a record
of the completed checks and their limits, not a legal opinion.

No files have been uploaded, no repository visibility has been changed, and
no commit or push was made as part of this preparation.
