#!/usr/bin/env python3
"""Archive the accepted RC2 build inputs without rebuilding its executable.

Run from the reviewed publication checkout. --workspace points to the private
build workspace; only explicit source groups and manifests are copied from it.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile


def digest(p):
    with p.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def files(p):
    return sorted(x for x in p.rglob('*') if x.is_file()
                  and not {'.git', '__pycache__'} & set(x.relative_to(p).parts)
                  and x.suffix not in ('.orig', '.rej', '.pyc'))


def tracked(p):
    return [p / os.fsdecode(n) for n in subprocess.check_output(
        ['git', '-C', str(p), 'ls-files', '-z']).split(b'\0') if n]


def archive(dest, groups, identity, replace_paths=()):
    records = {}
    with tarfile.open(dest, 'w:gz', compresslevel=6) as tar:
        for origin, prefix, paths in groups:
            for path in sorted(set(paths)):
                name = str(Path(prefix) / path.relative_to(origin))
                assert name not in records, name
                data = path.read_bytes()
                if b'\0' not in data:
                    for old, new in replace_paths:
                        data = data.replace(old, new)
                records[name] = hashlib.sha256(data).hexdigest()
                info = tarfile.TarInfo(name)
                info.size = len(data)
                info.mode = 0o755 if path.stat().st_mode & 0o111 else 0o644
                info.mtime = 0
                tar.addfile(info, io.BytesIO(data))
        data = (json.dumps({'identity': identity, 'files': records}, indent=2)+'\n').encode()
        info = tarfile.TarInfo('SOURCE-MANIFEST.json'); info.size = len(data)
        tar.addfile(info, io.BytesIO(data))
    with tarfile.open(dest) as tar:
        for item in tar:
            if item.name in records:
                assert hashlib.file_digest(tar.extractfile(item), 'sha256').hexdigest() == records[item.name]
    print(dest.name, len(records), 'verified files', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--stage', type=Path, required=True)
    args = parser.parse_args()
    work = args.workspace.resolve(); out = args.output.resolve()
    port = Path(__file__).resolve().parents[1]
    sdk = Path(os.environ.get('OO_PS4_TOOLCHAIN', '/opt/openorbis/OpenOrbis/PS4Toolchain'))
    out.mkdir(parents=True, exist_ok=True)
    package = out/'ScummVM-PS4-2026.4.0-RC2.pkg'
    assert package.is_file()
    assert not subprocess.check_output(['git', 'status', '--porcelain'], cwd=port).strip(), 'Commit reviewed source first'
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=port, text=True).strip()
    source = args.source.resolve()
    build = args.build.resolve()
    assert digest(args.stage/'eboot.bin') == 'cfb0008acb6c6f83e6f9dfa6341806853ec425a23b0d7613932c4c615d443f8b'
    identity = {'release': '2026.4.0 RC2', 'port_commit': revision,
                'title_id': 'SCVM00265', 'app_version': '0.43', 'title': 'ScummVM PS4',
                'package_sha256': digest(package), 'package_bytes': package.stat().st_size,
                'eboot_sha256': digest(args.stage/'eboot.bin'),
                'changes': 'TLJ mirrored-repeat textures and committed Mjolner improvements through 0.42',
                'physical_ps4': 'Not tested; TLJ cabin verified and accepted in shadPS4',
                'build_elf_sha256': digest(build/'scummvm')}
    (out/'BUILD-IDENTITY.json').write_text(json.dumps(identity, indent=2)+'\n')
    archive(out/'ScummVM-PS4-2026.4.0-RC2-source.tar.gz', [
        (port, 'port', tracked(port)), (source, 'scummvm-src', files(source)),
        (build, 'build-config', [build/'config.mk', build/'config.h'])], identity,
        [(str(port).encode(), b'/path/to/port'),
         (str(work).encode(), b'/path/to/port'),
         (str(Path.home() / work.name).encode(), b'/path/to/port'),
         (str(Path.home()).encode(), b'/home/developer')])
    groups = []; revisions = {}
    for relative in ('opengnm-stack/opengnm', 'opengnm-stack/opengnm-psbc',
                     'opengnm-stack/vulkan-ps4', 'opengnm-stack/Vulkan-Headers',
                     'opengnm-stack/SPIRV-Headers', 'glslang'):
        path = work/'.tools'/relative
        revisions[relative] = subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()
        groups.append((work/'.tools', 'dependency-cache', tracked(path)))
    for relative in ('sdl-input-src', 'ps4-libjbc', 'downloads'):
        path = work/'.tools'/relative
        groups.append((work/'.tools', 'dependency-cache', files(path)))
    storage = work/'build/ps4-storage'
    groups.append((storage, 'adapted-libjbc', [p for p in files(storage) if p.suffix in ('.c', '.h', '.md')]))
    groups.append((work/'build/ps4-sdlgl', 'adapted-sdl', [work/'build/ps4-sdlgl/SDL_ps4audio.c']))
    sdk_sources = [p for p in files(sdk/'src') if p.suffix in ('.c', '.h', '.S', '.md', '.sh', '.bat', '.png') or p.name == 'Makefile']
    groups.append((sdk, 'openorbis-available-source', sdk_sources + files(sdk/'include') +
                   [sdk/'LICENSE', sdk/'README.md', sdk/'link.x', sdk/'build/build.sh', sdk/'build/Dockerfile']))
    archive(out/'ScummVM-PS4-2026.4.0-RC2-dependency-sources.tar.gz', groups,
            {'git_revisions': revisions, 'sdl_revision': (work/'.tools/sdl-input-src/.source-revision').read_text().strip()},
            [(str(port).encode(), b'/path/to/port'),
         (str(work).encode(), b'/path/to/port'),
         (str(Path.home() / work.name).encode(), b'/path/to/port'),
         (str(Path.home()).encode(), b'/home/developer')])
    previous = work/'dist/public-vulkan-1.26-2026-09-30'
    sdk_inputs = json.loads((previous/'SDK-INPUTS.json').read_text())
    for name, record in sdk_inputs.items():
        assert digest(sdk/'lib'/name) == record['sha256'], name
    for name in ('OpenOrbis-runtime-source-supplement.tar.gz', 'SDK-INPUTS.json',
                 'OPENORBIS-PROVENANCE.md', 'OPENORBIS-PROVENANCE.json'):
        shutil.copyfile(previous/name, out/name)
    notice_files = files(previous/'licenses')
    packaged = args.stage/'data'
    packaged_notices = [p for p in packaged.iterdir() if p.is_file() and any(
        s in p.name.upper() for s in ('LICENSE','COPYING','COPYRIGHT','PATENTS','README.IJG','FTL.TXT','GPLV2.TXT','NOTICE'))]
    archive(out/'ScummVM-PS4-2026.4.0-RC2-notices.tar.gz', [
        (previous/'licenses', 'runtime-notices', notice_files),
        (packaged, 'packaged-notices', packaged_notices)], identity)
    for name in ('RELEASE-NOTES.md', 'SOURCES.md'):
        shutil.copyfile(port/'docs/releases/rc2'/name, out/name)
    config = (build/'config.mk').read_text()
    (out/'ENGINE-CONFIG.txt').write_text('\n'.join(re.findall(r'^ENABLE_\w+ = .*$',config,re.M))+'\n')
    sums = out/'SHA256SUMS'
    sums.write_text(''.join(f'{digest(p)}  {p.name}\n' for p in sorted(out.iterdir()) if p.is_file() and p != sums))


if __name__ == '__main__':
    main()
