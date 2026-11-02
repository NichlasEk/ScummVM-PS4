#!/usr/bin/env python3
"""Verify downloaded OpenOrbis evidence and bundle recovered runtime sources.

Read-only with respect to the installed SDK and application package. Inputs are
the reference checkouts and downloads described in OPENORBIS-PROVENANCE.md.
"""
import hashlib
import json
from pathlib import Path
import runpy
import shutil
import subprocess
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
helpers = runpy.run_path(str(ROOT / 'scripts/prepare-vulkan-release.py'))
digest, tree, tracked, archive = (helpers[n] for n in ('digest', 'tree', 'tracked', 'archive'))
OUT = ROOT / 'dist/public-vulkan-1.26-2026-09-30'
EVIDENCE = ROOT / 'artifacts/openorbis-online-audit'
SDK = Path('/opt/openorbis/OpenOrbis/PS4Toolchain')
PINS = {
    'toolchain': '6929bafac0b565692ed61e66032a79185ee9cf65',
    'llvm': '83e2569f0c062d980ae142d8dc583f6b8733be35',
    'sdl': '7774cf3e7d04448c44d569c50d1f9a4532a2efe7',
    'musl_inferred': '779f95174b44d39e6a8a788b36289cf4768944a9',
}


def main():
    toolchain = ROOT / '.tools/openorbis-v053-reference'
    llvm = ROOT / '.tools/openorbis-llvm-v12-reference'
    sdl = ROOT / '.tools/SDL-PS4'
    musl = ROOT / '.tools/openorbis-musl-2023-reference'
    for name, path in [('toolchain', toolchain), ('llvm', llvm), ('sdl', sdl)]:
        assert subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip() == PINS[name]
    before_pkg = digest(OUT / 'ScummVM-PS4-Vulkan-Experimental-1.26.pkg')
    local_tar = Path('/opt/openorbis/toolchain-llvm-18.tar.gz')
    with zipfile.ZipFile(EVIDENCE / 'toolchain-v0.5.3-llvm-18.2.zip') as z:
        with z.open('toolchain-llvm-18.tar.gz') as stream:
            official_hash = hashlib.file_digest(stream, 'sha256').hexdigest()
    assert official_hash == digest(local_tar) == '562e0bf5088d11041534059ab8963029a355786736d6b28fa62250fe5d02cb2d'
    report = {'toolchain_release': 'v0.5.3', 'llvm_asset': 'toolchain-llvm-18.2.zip',
              'official_zip_sha256': digest(EVIDENCE / 'toolchain-v0.5.3-llvm-18.2.zip'),
              'inner_tar_sha256': official_hash, 'source_references': PINS,
              'installed_file_matches': {}, 'dependency_asset_matches': {},
              'musl_reference_confidence': 'inferred; not independently rebuilt',
              'application_pkg_unchanged_sha256': before_pkg}
    names = ['lib/crt1.o', 'lib/libc.a', 'lib/libc++.a', 'lib/libc++abi.a', 'lib/libunwind.a',
             'lib/libSDL2.a', 'lib/libclang_rt.builtins-x86_64.a',
             'samples/SDL2/sce_module/libc.prx', 'samples/SDL2/sce_module/libSceFios2.prx',
             'samples/piglet/sce_sys/about/right.sprx']
    with tarfile.open(local_tar) as tar:
        for name in names:
            with tar.extractfile('OpenOrbis/PS4Toolchain/' + name) as stream:
                member_hash = hashlib.file_digest(stream, 'sha256').hexdigest()
            assert member_hash == digest(SDK / name), name
            report['installed_file_matches'][name] = member_hash
    for asset in ('libcxx-full-v1.2.tar.gz', 'SDL2-v1.0.tar.gz'):
        with tarfile.open(EVIDENCE / asset) as tar:
            for member in tar.getmembers():
                if member.isfile() and Path(member.name).name in ('libc++.a', 'libc++abi.a', 'libunwind.a', 'libSDL2.a', 'libclang_rt.builtins-x86_64.a'):
                    with tar.extractfile(member) as stream:
                        member_hash = hashlib.file_digest(stream, 'sha256').hexdigest()
                    assert member_hash == digest(SDK / 'lib' / Path(member.name).name)
                    report['dependency_asset_matches'][member.name] = {'asset': asset, 'sha256': member_hash}
    header_matches = []
    for path in tree(musl / 'include'):
        relative = path.relative_to(musl / 'include')
        target = SDK / 'include' / relative
        if target.is_file():
            assert digest(path) == digest(target), str(relative)
            header_matches.append(str(relative))
    assert len(header_matches) == 183
    report['musl_header_matches'] = header_matches
    (OUT / 'OPENORBIS-PROVENANCE.json').write_text(json.dumps(report, indent=2) + '\n')
    shutil.copyfile(ROOT / 'docs/releases/vulkan-1.26/OPENORBIS-PROVENANCE.md', OUT / 'OPENORBIS-PROVENANCE.md')
    pristine_sdl = ROOT / '.tools/openorbis-sdl-v10-pristine'
    pristine_sdl.mkdir(exist_ok=True)
    export = subprocess.Popen(['git', '-C', str(sdl), 'archive', PINS['sdl']], stdout=subprocess.PIPE)
    subprocess.run(['tar', '-xf', '-', '-C', str(pristine_sdl)], stdin=export.stdout, check=True)
    export.stdout.close()
    assert export.wait() == 0
    # The toolchain repository also tracks an old installer PKG and sample
    # movies/music. They are not runtime sources and do not belong in this bundle.
    toolchain_files = [p for p in tracked(toolchain)
                       if p.relative_to(toolchain).parts[0] in ('src', 'include', 'build', '.github', 'scripts')
                       or p.name in ('LICENSE', 'README.md', 'CHANGELOG.md', 'link.x', 'Dockerfile')]
    toolchain_files = [p for p in toolchain_files
                       if p.suffix.lower() not in ('.pkg', '.prx', '.sprx', '.elf', '.oelf', '.self', '.o', '.a', '.so', '.exe', '.dll', '.pdb')]
    groups = [(toolchain, 'openorbis-v0.5.3', toolchain_files),
              (llvm, 'llvm-runtime-v1.2', tracked(llvm)),
              (musl, 'musl-inferred-779f951', tree(musl)),
              (pristine_sdl, 'sdl-v1.0', tree(pristine_sdl)),
              (ROOT, 'port-audit', [ROOT / 'scripts/recover-openorbis-provenance.py'])]
    archive(OUT / 'OpenOrbis-runtime-source-supplement.tar.gz', groups,
            {'source_references': PINS, 'musl_confidence': report['musl_reference_confidence'],
             'source_scope': 'LLVM runtime projects, workflow and CMake support; not full compiler monorepo'})
    for component in ('libcxx', 'libcxxabi', 'libunwind', 'compiler-rt'):
        shutil.copyfile(llvm / component / 'LICENSE.TXT', OUT / 'licenses' / (component + '-LICENSE.TXT'))
    shutil.copyfile(musl / 'COPYRIGHT', OUT / 'licenses/musl-COPYRIGHT')
    assert digest(OUT / 'ScummVM-PS4-Vulkan-Experimental-1.26.pkg') == before_pkg
    sums = OUT / 'SHA256SUMS'
    sums.write_text(''.join(f'{digest(path)}  {path.relative_to(OUT)}\n'
                            for path in tree(OUT) if path != sums))
    print('Official artifact identity and dependency archive matches verified; PKG unchanged.', flush=True)


if __name__ == '__main__':
    main()
