#!/usr/bin/env python3
"""Exercise the production API matrix reader with float-only alignment."""
import pathlib, subprocess, sys, tempfile
source = pathlib.Path(sys.argv[1]) / 'engines/hpl1/engine/libraries/newton/Newton.cpp'
s = source.read_text()
a = s.index('static dgMatrix readApiMatrix(')
b = s.index('\n}\n', a) + 3
stub = '''#include <cassert>
using dFloat = float;
struct alignas(16) dgMatrix {
 float m[4][4]; float *operator[](int row) {return m[row];}
};
'''
main = '''
int main() {
 alignas(16) float storage[20];
 for(int offset=0;offset<4;offset++) {
  float *src=storage+offset;
  for(int i=0;i<16;i++) src[i]=i*0.25f-2;
  auto m=readApiMatrix(src);
  for(int i=0;i<16;i++) assert(m[i/4][i%4]==src[i]);
 }
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp); (p/'test.cpp').write_text(stub+s[a:b]+main)
 subprocess.run(['c++','-O2','-g','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: production Newton matrix reader at 0/4/8/12-byte alignment')
