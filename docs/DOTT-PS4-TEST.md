# DOTT hardware-test package — 2026-09-19

Artifact: `/home/developer/ScummVM-PS4/dist/ScummVM-PS4-2.9.1-DOTT.pkg`

- Size: 293,666,816 bytes (about 294 MB).
- SHA-256: `abcaa508acd00ae57ad7727825adc0811363e8584d3e8ae6a2788a88b68564c1`
- Title: **ScummVM PS4 + DOTT**; title ID: `SCVM00002`.
- Uses the same title ID as the plain port; this is a bundled variant of it.
- Contains the user's full English CD game: `tentacle.000`, `tentacle.001`,
  `monster.sou`, normalized under `/app0/games/dott`.
- Game data and generated packages remain ignored by Git.

Install the PKG using the homebrew-enabled PS4's Package Installer. Launch
**ScummVM PS4 + DOTT**, then select **Start**. The left stick controls the
pointer; Cross clicks; Circle right-clicks; Options provides the ScummVM menu
mapping. The user subsequently confirmed this package running on real PS4 hardware.

On a fresh installation, DOTT is preselected automatically. Existing writable
ScummVM settings are preserved; if DOTT is not listed, use **Add Game** and
select `/app0/games/dott`. Configuration and saves live in `/data/scummvm/`.

## Verification

- User supplied a screenshot showing the DOTT intro running in the emulator.
- PKG validation: all reported digest and signature checks passed.
- Extracted the built PKG and compared all three game data files, eboot,
  runtime modules, and default config against their inputs: SHA-256 matched.
- Booted the executable extracted from the package in a separate emulator
  profile. First-launch config seeding succeeded; DOTT appeared preselected.
- Extracted SFO identifies SCVM00002 / ScummVM PS4 + DOTT correctly.
- Shell syntax, source-patch reverse check, and Git whitespace check passed.

Evidence: `artifacts/scummvm/dott-validation.log`, `dott-content-check.json`,
`dott-extracted-boot.log`, and `dott-package-launcher.png`.

The user subsequently confirmed console installation and DOTT running. Sound,
save/load and exit still need explicit hardware validation. Capture
`/data/scummvm/scummvm.log` if something fails.

This records the original 1.00 bundle. The current 1.01 standalone package
is documented in [STANDALONE-PS4.md](STANDALONE-PS4.md); rebuilding this optional
bundle now produces a newer binary and different hashes.

## Rebuild

```sh
./scripts/build-scummvm.sh
./scripts/package-scummvm.sh --with-dott build/scummvm-pkg/games/dott/dott
```

The bundle uses a separate `build/scummvm-dott-pkg` staging directory. The
three full-game files are required; DOS launchers and unrelated demos are
not packaged. `config/scummvm-dott.ini` provides first-launch settings through
`/app0/scummvm-default.ini`; it never overwrites an existing config.
