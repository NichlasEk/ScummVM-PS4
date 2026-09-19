# ScummVM PS4 without bundled games

This is an unofficial PS4 port of ScummVM 2.9.1, currently built with the
SCUMM engine only. The user has confirmed that the previous DOTT package
starts and plays on a real PS4. The changes in package version 1.01 require
another hardware test.

## Install

Copy `dist/ScummVM-PS4-2.9.1.pkg` to a USB drive and install it with Package
Installer. It uses the same title ID (SCVM00002) as the DOTT test package.
Settings and saves remain under `/data/scummvm`; the package contains no
commercial game files. A DOTT entry from the bundled version may still point
to `/app0/games/dott`: use Edit Game to change its path to the external game
folder, or remove the launcher entry and add the game again.

## Games on USB

Put extracted game data in separate folders, for example:

```
USB/
  ScummVM-PS4-2.9.1.pkg
  scummvm/
    games/
      dott/
        tentacle.000
        tentacle.001
        monster.sou
```

Choose **Add Game**, then **Go up** until reaching `/`. Choose **USB 0**
(or the USB entry matching the connected drive), open `scummvm/games/dott`,
and choose that directory. The browser checks `/mnt/usb0` through
`/mnt/usb7` and shows only existing directories. Leave the USB connected
while playing. USB access on physical hardware still needs verification.
Do not select an ISO: this SCUMM build uses the extracted game data.

## Games on internal storage

Copy each game directory with your PS4 file manager or FTP service to:

```
/data/scummvm/games/dott/
```

Choose **Add Game → Go up → Internal storage - games** and select the game
folder. The port creates this writable directory automatically. ScummVM
does not yet contain a USB-to-HDD copy button. Saves always go to
`/data/scummvm/saves`, separately from game data.

## Pointer and icon

PS4 virtual pointer speed is multiplied by eight, including existing installs.
The ScummVM pointer-speed setting still adjusts it further. Small stick
movements retain analog control, and releasing the stick still stops motion.
This also speeds up the emulator's mouse-to-stick mapping.
The PS4 home-screen icon is the upstream ScummVM green S.

## Distribution

Distribute the standalone PKG, not the optional `-DOTT.pkg`. Share the port's
source and build instructions alongside the binary, retaining upstream license
notices and supplying corresponding source as required by those licenses.
Do not include your game data, local configuration, saves or bundled test PKG.
The repository's default packaging file list explicitly excludes game data,
even if local emulator games exist under the staging directory.

## Validation of version 1.01

The extracted standalone package boots in shadPS4. DOTT was detected and
registered from a separately mounted `/mnt/usb0` directory. Package signatures
and digests pass validation; extracted contents contain no game data or
bundled-game configuration, and the packaged icon matches `assets/icon0.png`.
The final package is 12,845,056 bytes with SHA-256:

```
2699e40e22e9a11346ed420d601c22ddd4cbe3689995725de8079c1702122020
```

Physical USB browsing and the new 8× sensitivity still need user validation.
