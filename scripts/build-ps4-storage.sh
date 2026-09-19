#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
python3 - "$root" "$sdk" <<'PY'
from pathlib import Path
import hashlib,subprocess,sys,urllib.request
root,sdk=map(Path,sys.argv[1:]);rev='835fe016ff0ae5dd89b9249f39cc0fe093fd07dd'
source=root/'.tools/ps4-libjbc';build=root/'build/ps4-storage'
source.mkdir(parents=True,exist_ok=True);build.mkdir(parents=True,exist_ok=True)
files=['README.md','defs.h','jailbreak.c','jailbreak.h','kernelrw.c','kernelrw.h','libjbc.h','utils.c','utils.h']
for name in files:
 p=source/name
 if not p.exists() or not (source/'.revision').exists() or (source/'.revision').read_text().strip()!=rev:
  p.write_bytes(urllib.request.urlopen('https://raw.githubusercontent.com/bucanero/ps4-libjbc/'+rev+'/'+name).read())
(source/'.revision').write_text(rev+'\n')
# Local adaptations are generated in the build tree; downloaded source is intact.
for name in files:
 s=(source/name).read_text()
 if name=='jailbreak.c':
  s=s.replace('restart:;', 'unsigned attempts = 0;\nrestart:;\n    if (++attempts > 8) return -1;')
  s=s.replace('for(;;)', 'for(unsigned visited = 0;; ++visited)')
  s=s.replace('        int pid;', '        if (visited >= 4096) return -1;\n        int pid;')
 if name=='kernelrw.c':
  s=s.replace('jbc_krw_kcall((uintptr_t)k_get_td)', 'jbc_krw_kcall((uintptr_t)k_get_td, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL)')
  s=s.replace('jbc_krw_kcall((uintptr_t)k_kcpy, dst, src, sz)', 'jbc_krw_kcall((uintptr_t)k_kcpy, dst, src, sz, 0ULL, 0ULL, 0ULL)')
 if name=='utils.c':
  s=s.replace('    jbc_get_cred(&cred);','    if (jbc_get_cred(&cred)) return;')
  s=s.replace('    jbc_jailbreak_cred(&root_cred);','    if (jbc_jailbreak_cred(&root_cred)) return;')
  s=s.replace('    jbc_set_cred(&root_cred);','    if (jbc_set_cred(&root_cred)) { jbc_set_cred(&cred); return; }')
  s=s.replace('nmount(data, 6, 0)', 'nmount(data, 6, 1)') # read-only game data mapping
  s=s.replace('    return;\ninvalid:', '    p->ans = 0;\n    return;\ninvalid:')
  s=s.replace('        .ans = 0,', '        .ans = -1,')
 p=build/name
 if not p.exists() or p.read_text()!=s:p.write_text(s)
flags=['--target=x86_64-pc-freebsd12-elf','-fPIC','-O2','-ffreestanding','-mno-red-zone','-fno-stack-protector','-isysroot',str(sdk),'-isystem',str(sdk/'include')]
fingerprint=hashlib.sha256((''.join((build/n).read_text() for n in files)+' '.join(flags)+(root/'compat/ps4/storage.c').read_text()).encode()).hexdigest()
stamp=build/'.build-id';lib=build/'libscummvm-jbc.a'
if lib.exists() and stamp.exists() and stamp.read_text()==fingerprint:sys.exit(0)
subprocess.run(['clang',*flags,'-D__PS4__','-include',str(root/'compat/ps4/include/ps4-abi.h'),'-I',str(build),'-c',str(root/'compat/ps4/storage.c'),'-o',str(build/'storage.o')],check=True)
for name in ['jailbreak','kernelrw','utils']:
 obj=build/(name+'.o')
 subprocess.run(['clang',*flags,'-c',str(build/(name+'.c')),'-o',str(obj)],check=True)
 # Never replace the application's libc open/close with libjbc's raw syscalls.
 subprocess.run(['llvm-objcopy','--redefine-sym','open=jbc_raw_open','--redefine-sym','close=jbc_raw_close',str(obj)],check=True)
subprocess.run(['llvm-ar','rcs',str(lib),*[str(build/(n+'.o')) for n in ['storage','jailbreak','kernelrw','utils']]],check=True)
stamp.write_text(fingerprint)
PY
