#!/usr/bin/env python3
"""Check the Mjolner AIFF workaround and exact decoded PCM samples."""
from pathlib import Path
import subprocess, sys, tempfile, struct, signal
source, build = (Path(p).resolve() for p in sys.argv[1:3])
files = sys.argv[3:]
if files[:1] == ['--game-data']:
    root = Path(files[1])
    files = []
    for path in sorted(root.rglob('*')):
        if not path.is_file():
            continue
        with path.open('rb') as stream:
            header = stream.read(12)
            if header[:4] != b'FORM' or header[8:] not in (b'AIFF', b'AIFC'):
                continue
            data = header + stream.read()
        pos, unaligned = 12, False
        while pos + 8 <= len(data):
            length = int.from_bytes(data[pos+4:pos+8], 'big')
            if data[pos:pos+4] == b'SSND':
                unaligned = bool((length - 8) & 1)
                break
            pos += 8 + length + (length & 1)
        files += ['padded' if unaligned else 'normal', str(path)]
with tempfile.TemporaryDirectory(prefix='aiff-padding-') as d:
    binary = Path(d) / 'test'
    subprocess.run(['c++', '-std=c++17', '-g', '-O1', '-fsanitize=address,undefined',
                    '-DHAVE_CONFIG_H', '-I'+str(build), '-I'+str(source),
                    '-I'+str(source/'engines'), str(Path(__file__).with_suffix('.cpp')),
                    '-Wl,--start-group', str(build/'audio/libaudio.a'), str(build/'common/libcommon.a'),
                    str(build/'common/formats/libformats.a'), str(build/'common/compression/libcompression.a'),
                    str(build/'math/libmath.a'), '-Wl,--end-group',
                    '-lFLAC', '-lvorbisfile', '-lvorbis', '-logg', '-lmad', '-lfaad', '-lz',
                    '-o', str(binary)], check=True)
    def fixture(extra=b'\0', channels=1, bits=16, frames=3, offset=0,
                rate=bytes.fromhex('400dac44000000000000'), duplicate=False):
        def chunk(tag, data):
            return tag + struct.pack('>I', len(data)) + data + (b'X' if len(data) & 1 else b'')
        comm = chunk(b'COMM', struct.pack('>HIH', channels, frames, bits) + rate)
        body = b'AIFF' + comm + (comm if duplicate else b'') + chunk(
            b'SSND', struct.pack('>II', offset, 0) + b'\x12\x34\xff\xfe\x80\x00' + extra)
        return b'FORM' + struct.pack('>I', len(body)) + body
    cases = [('padded', fixture()), ('normal', fixture(extra=b'')),
             ('reject', fixture(extra=b'Q')), ('reject', fixture(extra=b'\0\0\0')),
             ('reject', fixture(channels=2)), ('reject', fixture(bits=8)),
             ('reject', fixture(frames=4)), ('reject', fixture(frames=0xffffffff)),
             ('reject', fixture(offset=1)), ('reject', fixture(duplicate=True)),
             ('reject', fixture(rate=b'\0' * 10))]
    padded = fixture()
    cases += [('reject', padded[:i]) for i in range(len(padded))]
    for i, (mode, data) in enumerate(cases):
        path = Path(d) / ('fixture-' + str(i))
        path.write_bytes(data)
        subprocess.run([str(binary), mode, str(path)], check=True)
    assert len(files) % 2 == 0
    for mode, filename in zip(files[::2], files[1::2]):
        result = subprocess.run([str(binary), mode, filename])
        if mode == 'baseline':
            assert result.returncode == -signal.SIGABRT, 'expected original decoder assertion'
        else:
            result.check_returncode()
print(f'PASS: {len(cases)} synthetic cases; {len(files)//2} supplied files; '
      'AIFF padding recognition, source position, exact PCM samples and rewind')
