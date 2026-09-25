#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
python3 - "$root" "$sdk" <<'PY'
from pathlib import Path
import hashlib, shutil, subprocess, sys, tarfile, urllib.request
root, sdk = map(Path, sys.argv[1:])
build = root/'build/ps4-mad'; prefix = build/'install'
cache = root/'.tools/downloads'; cache.mkdir(parents=True, exist_ok=True)
archives = {
 'libmad_0.15.1b.orig.tar.gz': 'bbfac3ed6bfbc2823d3775ebb931087371e142bb0e9bb1bee51a76a6e0078690',
 'libmad_0.15.1b-11.debian.tar.xz': 'be94f0bcce7efddef02c876d9a49d47004f1f78c5428fb020c8ab491ddc881f3',
}
for name, digest in archives.items():
 p = cache/name
 if not p.exists():
  tmp = p.with_suffix('.tmp')
  urllib.request.urlretrieve('https://deb.debian.org/debian/pool/main/libm/libmad/'+name, tmp)
  tmp.rename(p)
 if hashlib.sha256(p.read_bytes()).hexdigest() != digest:
  sys.exit('Checksum mismatch: '+str(p))
flags = ['--target=x86_64-pc-freebsd12-elf', '-fPIC', '-O2', '-fwrapv', '-mno-red-zone', '-fno-stack-protector', '-D__PS4__', '-DHAVE_CONFIG_H', '-DFPM_64BIT', '-include', str(root/'compat/ps4/include/ps4-abi.h'), '-isysroot', str(sdk), '-isystem', str(sdk/'include')]
fingerprint = hashlib.sha256((root/'scripts/build-ps4-mad.sh').read_bytes() + (root/'compat/ps4/include/ps4-abi.h').read_bytes() + ' '.join(flags).encode() + subprocess.check_output(['clang','--version'])).hexdigest()
stamp = build/'.build-id'
if (prefix/'lib/libmad.a').exists() and stamp.exists() and stamp.read_text() == fingerprint:
 print('PS4 libmad is up to date'); sys.exit(0)
build.mkdir(parents=True, exist_ok=True)
source = build/'libmad-0.15.1b'
if source.exists(): shutil.rmtree(source)
with tarfile.open(cache/'libmad_0.15.1b.orig.tar.gz') as t: t.extractall(build, filter='data')
with tarfile.open(cache/'libmad_0.15.1b-11.debian.tar.xz') as t: t.extractall(build, filter='data')
patches = build/'debian/patches'
for name in (patches/'series').read_text().splitlines():
 if name.strip() and not name.startswith('#'):
  subprocess.run(['patch','-p1','--batch','-i',str(patches/name)], cwd=source, check=True)
# OpenOrbis is LP64. Do not enable libmad's fork/pipe asynchronous API.
config = '''#define SIZEOF_INT 4
#define SIZEOF_LONG 8
#define SIZEOF_LONG_LONG 8
#define HAVE_ASSERT_H 1
#define HAVE_ERRNO_H 1
#define HAVE_LIMITS_H 1
#define NDEBUG 1
'''
(source/'config.h').write_text(config)
headers = ['version.h','fixed.h','bit.h','timer.h','stream.h','frame.h','synth.h','decoder.h']
public = '/* libmad 0.15.1b, GPL-2.0-or-later; see packaged COPYRIGHT and COPYING. */\n#ifdef __cplusplus\nextern "C" {\n#endif\n#define FPM_64BIT\n'+ '\n'.join(l for l in config.splitlines() if 'SIZEOF_' in l)+'\n'
for h in headers:
 public += subprocess.check_output(['sed','-n','-f',str(source/'mad.h.sed'),str(source/h)], text=True)
public += '\n#ifdef __cplusplus\n}\n#endif\n'
(source/'mad.h').write_text(public)
objects = []
for name in ['version','fixed','bit','timer','stream','frame','synth','decoder','layer12','layer3','huffman']:
 obj = build/(name+'.o'); objects.append(str(obj))
 subprocess.run(['clang', *flags, '-I'+str(source), '-c', str(source/(name+'.c')), '-o', str(obj)], check=True)
for d in ['lib','include','share/licenses/libmad']: (prefix/d).mkdir(parents=True,exist_ok=True)
subprocess.run(['llvm-ar','rcs',str(prefix/'lib/libmad.a'),*objects],check=True)
shutil.copy2(source/'mad.h',prefix/'include/mad.h')
for name in ['COPYING','COPYRIGHT']:
 shutil.copy2(source/name,prefix/'share/licenses/libmad'/name)
stamp.write_text(fingerprint)
print('Built PS4 libmad:',prefix/'lib/libmad.a')
PY
