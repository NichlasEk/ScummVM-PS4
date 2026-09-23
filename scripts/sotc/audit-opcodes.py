#!/usr/bin/env python3
"""Walk decodable script branches; report remaining blockers, not gameplay coverage."""
import argparse
import collections
import ctypes as c
import json
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('directory', type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]

def entries(data, width):
    if len(data) < width:
        raise ValueError('Truncated CC4 directory')
    first = int.from_bytes(data[:width], 'little')
    if not first or first % width or first > len(data):
        raise ValueError('Invalid CC4 directory')
    offsets = [int.from_bytes(data[i:i+width], 'little') for i in range(0, first, width)] + [len(data)]
    if any(x > y or y > len(data) for x, y in zip(offsets, offsets[1:])):
        raise ValueError('Invalid CC4 offsets')
    return [data[x:y] for x, y in zip(offsets, offsets[1:])]

with tempfile.TemporaryDirectory(prefix='sotc-opcodes-') as tmp:
    tmp = Path(tmp)
    (tmp/'decode.cpp').write_text('''#include "engines/sotc/script.h"
extern "C" int decode(const uint8_t *p,unsigned n,unsigned pc,int *v) {
 Sotc::ScriptInstruction o;int status=Sotc::decodeScript(Sotc::Cc4View(p,n),pc,o);
 v[0]=o.opcode;v[1]=o.next;v[2]=o.target;v[3]=o.opcode==53 ? o.argument : 0;
 for(unsigned i=0;i<6;++i)v[4+i]=o.choiceTarget[i];return status;
}
''')
    subprocess.run(['c++','-std=c++11','-shared','-fPIC','-I',str(root),str(tmp/'decode.cpp'),'-o',str(tmp/'decode.so')],check=True)
    lib = c.CDLL(str(tmp/'decode.so'))
    lib.decode.argtypes = [c.c_char_p,c.c_uint,c.c_uint,c.POINTER(c.c_int)]
    hits = collections.Counter()
    blockers = collections.defaultdict(list)
    invalid = []
    total = 0
    for path in sorted(a.directory.glob('R[0-9][0-9].CC4')):
        for room, block in enumerate(entries(path.read_bytes(), 4)):
            for task, script in enumerate(entries(block, 2)):
                total += 1
                pending, seen = [0], set()
                while pending:
                    pc = pending.pop()
                    if pc in seen:
                        continue
                    seen.add(pc)
                    out = (c.c_int*10)()
                    status = lib.decode(script,len(script),pc,out)
                    context = dict(bank=path.stem,room=room,task=task,pc=pc)
                    if status:
                        if status == 1:
                            blockers[out[0]].append(context)
                        else:
                            invalid.append(dict(context,status=status,opcode=out[0]))
                        continue
                    hits[out[0]] += 1
                    if out[0] == 2:
                        continue
                    if out[0] == 53:
                        pending.extend(out[4:4+out[3]])
                    else:
                        if out[2] >= 0:
                            pending.append(out[2])
                        if out[0] != 3:
                            pending.append(out[1])
    print(json.dumps(dict(note='Static branch reachability only. Unknown instructions stop that path; no assets are emitted.',
        scripts=total,decoded_instructions=sum(hits.values()),opcodes=dict(sorted(hits.items())),
        unsupported={k:dict(count=len(v),examples=v[:8]) for k,v in sorted(blockers.items())},
        invalid=invalid),indent=2))
