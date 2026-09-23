#!/usr/bin/env python3
"""Validate and list SOTC PAK directories; does not decompress resources."""
import argparse
import json
from pathlib import Path
import struct


def inspect(path):
    data = path.read_bytes()

    def read(fmt, offset):
        if offset < 0 or offset + struct.calcsize(fmt) > len(data):
            raise ValueError(f"{path}: truncated header at {offset}")
        return struct.unpack_from(fmt, data, offset)

    first, = read('<I', 4)
    if first < 8 or first % 4 or first > len(data):
        raise ValueError(f"{path}: invalid directory extent")
    offsets = [read('<I', pos)[0] for pos in range(4, first, 4)]
    if offsets != sorted(set(offsets)):
        raise ValueError(f"{path}: unordered or duplicate offsets")
    entries = []
    for index, offset in enumerate(offsets):
        end = offsets[index + 1] if index + 1 < len(offsets) else len(data)
        descriptor, = read('<I', offset)
        header = offset + (descriptor if descriptor else 4)
        if descriptor and descriptor < 4:
            raise ValueError(f"{path}: invalid additional descriptor")
        packed, unpacked, method, flags, metadata_size = read('<IIBBH', header)
        payload = header + 12 + metadata_size
        if payload + packed > end:
            raise ValueError(f"{path}: entry {index} exceeds its boundary")
        metadata = data[header + 12:payload]
        name = metadata[2:].split(b'\0', 1)[0].decode('latin1')
        entries.append(dict(index=index, name=name, offset=offset,
                            payload=payload, packed=packed, unpacked=unpacked,
                            method=method, flags=flags))
    return dict(archive=path.name, entries=entries)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path, help='Extracted SHADOW directory')
    args = parser.parse_args()
    paths = sorted(args.directory.glob('*.PAK'))
    if not paths:
        parser.error('No uppercase *.PAK files found')
    print(json.dumps([inspect(path) for path in paths], indent=2))
