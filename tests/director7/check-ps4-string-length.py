#!/usr/bin/env python3
"""Exercise the actual string scan against a simulated 16-bit PS4 wcslen."""
import pathlib
import subprocess
import sys
import tempfile

source = (pathlib.Path(sys.argv[1]) / 'common/str-base.cpp').read_text()
start = source.index('TEMPLATE size_t BASESTRING::cStrLen(const value_type *str) {')
body = source[start:source.index('\n}', start) + 2]
body = body.replace('TEMPLATE size_t BASESTRING::cStrLen', 'template<class value_type> size_t scan')
program = r'''
#include <cstddef>
#include <cstdint>
// The target library counts 16-bit units even when Clang folds a UTF-32 loop.
extern "C" size_t wcslen(const wchar_t *p) noexcept {
    const volatile uint16_t *q = reinterpret_cast<const volatile uint16_t *>(p);
    size_t n = 0;
    while (q[n]) ++n;
    return n;
}
''' + body + r'''
int main() {
    return !(scan(U"") == 0 && scan(U"_1") == 2 &&
             scan(U"sickan_walk_1") == 13 && scan(U"åäö😀") == 4 &&
             scan(u"_1") == 2 && scan("_1") == 2);
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp = pathlib.Path(tmp) / 'scan.cpp'
    binary = pathlib.Path(tmp) / 'scan'
    cpp.write_text(program)
    subprocess.run(['clang++', '-std=c++11', '-O2', '-D__PS4__', str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('PS4 string lengths: empty, ASCII, UTF-16 and UTF-32 passed with 16-bit wcslen')
