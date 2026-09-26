# External PS4 runtime modules

Future work: [source-built runtime replacement roadmap](../../docs/PS4-OPEN-RUNTIME-ROADMAP.md).

This directory documents the three prebuilt modules currently copied into
ScummVM PS4 packages. Git contains text placeholders only, not the modules.
Their source provenance and redistribution terms have not been verified here;
the repository's GPL license does not grant rights to these external binaries.

| Module | Current source relative to `OO_PS4_TOOLCHAIN` | Package destination |
| --- | --- | --- |
| `libc.prx` | `samples/SDL2/sce_module/libc.prx` | `sce_module/libc.prx` |
| `libSceFios2.prx` | `samples/SDL2/sce_module/libSceFios2.prx` | `sce_module/libSceFios2.prx` |
| `right.sprx` | `samples/piglet/sce_sys/about/right.sprx` | `sce_sys/about/right.sprx` |

## Local setup

Use locally obtained modules you are entitled to use, in the SDK locations
listed above. Set `OO_PS4_TOOLCHAIN` to your SDK directory if necessary; the
default is `/opt/openorbis/OpenOrbis/PS4Toolchain`.

`scripts/package-scummvm.sh` reads those SDK paths directly. This placeholder
directory is documentation, not an alternate module search path. Do not rename
the text placeholders to `.prx` or `.sprx`: they are not executable modules.

The root `.gitignore` excludes `*.prx` and `*.sprx` anywhere in the checkout.
Build outputs and generated packages are also ignored. Generated packages
still contain the real local modules; these exclusions apply to Git, not to
package contents. Check the modules' terms before distributing a package.

The experimental Vulkan/OpenGNM graphics path does not require
`libScePigletv2VSH.sprx` or `libSceShaccVSH.sprx`. The SDK's `piglet` directory
name above is simply the current source location of `right.sprx`.
