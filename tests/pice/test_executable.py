#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
import struct
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location('le_inspector', Path(__file__).resolve().parents[2] / 'scripts/pice/inspect-executable.py')
inspector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inspector)


class ExecutableTests(unittest.TestCase):
    def fixture(self):
        # Original synthetic embedded executable with reversed physical pages.
        data = bytearray(1024)
        mz, le = 64, 128
        data[mz:mz + 2] = b'MZ'
        struct.pack_into('<I', data, mz + 60, le - mz)
        data[le:le + 4] = b'LE\0\0'
        fields = {20: 2, 24: 1, 28: 4, 40: 256, 44: 256,
                  64: 196, 68: 1, 72: 220, 128: 448}
        for offset, value in fields.items():
            struct.pack_into('<I', data, le + offset, value)
        struct.pack_into('<6I', data, le + 196, 512, 0x10000, 0x2005, 1, 2, 0)
        data[le + 220:le + 228] = b'\0\0\2\0\0\0\1\0'
        return data

    def test_embedded_noncontiguous_mapping(self):
        result = inspector.inspect(self.fixture())
        self.assertEqual(result['embedded_mz_offset'], 64)
        self.assertEqual(result['entry']['file_offset'], 772)
        self.assertEqual(result['objects'][0]['pages'][1]['file_offset'], 512)
        self.assertFalse(result['fixups_applied'])

    def test_every_truncated_prefix(self):
        data = self.fixture()
        for length in range(len(data)):
            with self.subTest(length=length), self.assertRaises(ValueError):
                inspector.inspect(data[:length])

    def test_invalid_tables_and_entry(self):
        for offset, value in [(20, 0), (24, 0), (24, 2), (28, 512), (40, 0),
                              (44, 257), (64, 1024), (68, 65537), (72, 1024), (128, 1024)]:
            data = self.fixture()
            struct.pack_into('<I', data, 128 + offset, value)
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                inspector.inspect(data)

    def test_unsupported_page_encoding(self):
        data = self.fixture()
        data[128 + 223] = 1
        with self.assertRaises(ValueError):
            inspector.inspect(data)


if __name__ == '__main__':
    unittest.main()
