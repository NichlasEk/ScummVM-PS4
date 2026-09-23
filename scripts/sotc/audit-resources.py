#!/usr/bin/env python3
"""Compare the engine's implode decoder with Info-ZIP for every supplied PAK.

Requires a C++ compiler and unzip. No original resources are copied into source.
"""
import argparse
import ctypes
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('pak', Path(__file__).with_name('inspect-pak.py'))
pak = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pak)
archives = sorted(args.directory.glob('*.PAK'))
if not archives:
    parser.error('No PAK archives')
results = []
with tempfile.TemporaryDirectory(prefix='sotc-audit-') as temp:
    temp = Path(temp)
    source = temp / 'wrapper.cpp'
    source.write_text('#include "engines/sotc/explode.h"\n'
                      'extern "C" bool decode(const uint8_t*s,uint32_t n,uint8_t*d,uint32_t m,unsigned f)'
                      '{return Sotc::explode(s,n,d,m,f);}\n')
    subprocess.run(['c++', '-shared', '-fPIC', '-I', str(root), str(source), '-o', str(temp / 'decoder.so')], check=True)
    decode = ctypes.CDLL(str(temp / 'decoder.so')).decode
    decode.restype = ctypes.c_bool
    decode.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint]
    for archive in archives:
        data = archive.read_bytes()
        for entry in pak.inspect(archive)['entries']:
            payload = data[entry['payload']:entry['payload'] + entry['packed']]
            if entry['method'] == 0:
                output = payload
            else:
                assert entry['method'] == 1, entry
                dst = ctypes.create_string_buffer(entry['unpacked'])
                assert decode(payload, len(payload), dst, len(dst), entry['flags']), entry
                output = dst.raw
                # Exercise bounded failure paths on real truncated streams.
                for length in (0, 1, len(payload) // 2):
                    assert not decode(payload, length, dst, len(dst), entry['flags']), entry
                assert not decode(payload, len(payload), dst, len(dst), 8), entry
                # Wrap the original compressed payload as ZIP method 6. CRC is
                # only transport bookkeeping; byte comparison is independent.
                crc = zlib.crc32(output)
                name = b'resource'
                local = struct.pack('<IHHHHHIIIHH', 0x04034b50, 10, entry['flags'], 6, 0, 0,
                                    crc, len(payload), len(output), len(name), 0) + name + payload
                central = struct.pack('<IHHHHHHIIIHHHHHII', 0x02014b50, 10, 10, entry['flags'], 6, 0, 0,
                                      crc, len(payload), len(output), len(name), 0, 0, 0, 0, 0, 0) + name
                end = struct.pack('<IHHHHIIH', 0x06054b50, 0, 0, 1, 1, len(central), len(local), 0)
                testzip = temp / 'reference.zip'
                testzip.write_bytes(local + central + end)
                check = subprocess.run(['unzip', '-p', str(testzip)], capture_output=True, check=True)
                assert check.stdout == output, entry
            results.append(dict(archive=archive.name, index=entry['index'], name=entry['name'],
                                size=len(output), sha256=hashlib.sha256(output).hexdigest()))
print(json.dumps(dict(archives=len(archives), entries=len(results), resources=results), indent=2))
