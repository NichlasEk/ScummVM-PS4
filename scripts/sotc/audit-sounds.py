#!/usr/bin/env python3
"""Inspect original unpacked SMP.PAK VOCs; no game data is copied into source."""
import argparse
import json
from pathlib import Path
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=Path, help='Unpacked SMP.PAK directory from audit-resources.py')
args = parser.parse_args()
result = []
for path in sorted(args.directory.glob('*.VOC')):
    data = path.read_bytes()
    assert data[:20] == b'Creative Voice File\x1a', path
    offset, version, check = struct.unpack_from('<HHH', data, 20)
    assert check == (~version + 0x1234) & 65535 and 26 <= offset < len(data), path
    samples, rates = 0, []
    while True:
        assert offset < len(data), (path, 'missing terminator')
        kind = data[offset]
        offset += 1
        if kind == 0:
            break
        assert offset + 3 <= len(data), path
        size = int.from_bytes(data[offset:offset + 3], 'little')
        offset += 3
        assert offset + size <= len(data), (path, 'truncated block')
        # Every supplied sample is a type-1 block of uncompressed unsigned PCM.
        # Expose other encodings rather than pretending to validate them.
        assert kind == 1 and size >= 2 and data[offset + 1] == 0, path
        rates.append(1000000 / (256 - data[offset]))
        samples += size - 2
        offset += size
    assert offset == len(data) and samples, path
    result.append(dict(name=path.name, samples=samples, encoded_rates=rates))
assert result, 'No VOCs found'
print(json.dumps(dict(count=len(result), files=result), indent=2))
