#!/usr/bin/env python3
"""Sanitizer checks of the real ring and extracted VM boundary methods.

The error stub throws only in this host harness to inspect pre-fatal state;
production uses ScummVM's non-returning error(), with no exception recovery.
Full interpreter semantics are tested separately with directortest Lingo files.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=Path)
parser.add_argument('--cxx', default='c++')
args = parser.parse_args()
root = args.source.resolve()


def method(filename, signature):
    text = (root / 'engines/director/lingo' / filename).read_text()
    start = text.index(signature)
    begin = text.index('{', start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


code = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#include "director/lingo/eutherlingo-guard.h"
using namespace Director;
struct Fatal {};
[[noreturn]] void error(const char *, ...) { throw Fatal(); }
struct Datum { int value; };
struct State {
 std::vector<Datum> stack;
 std::vector<int> *script = nullptr;
 unsigned pc = 0;
};
struct Lingo {
 State state;
 State *_state = &state;
 unsigned reports = 0;
 void guardReport(const char *, bool fatal) { assert(fatal); ++reports; }
 Datum pop(); Datum peek(unsigned);
 void guardCheckPC(); void guardCheckArguments(int);
};
'''
for file, sig in [('lingo-code.cpp', 'Datum Lingo::pop()'),
                  ('lingo-code.cpp', 'Datum Lingo::peek(uint offset)'),
                  ('lingo.cpp', 'void Lingo::guardCheckPC()'),
                  ('lingo.cpp', 'void Lingo::guardCheckArguments(int nargs)')]:
    code += method(file, sig) + '\n'
code += r'''
template<class F> void fatal(F f) {
 bool caught = false;
 try { f(); } catch (const Fatal &) { caught = true; }
 assert(caught);
}
int main() {
 EutherLingoGuard ring;
 assert(ring.size() == 0);
 char name[100]; memset(name, 'h', sizeof(name)); name[99] = 0;
 int object;
 for (unsigned i = 0; i < 300; ++i)
  ring.record("call", name, nullptr, nullptr, &object, i, i + 1, i + 2);
 memset(name, 'z', sizeof(name)); // Ring owns the copied name; never retains it.
 assert(ring.size() == 128);
 for (unsigned i = 0; i < ring.size(); ++i) {
  const auto &e = ring.at(i);
  assert(e.pc == i + 172 && e.stack == i + 173 && e.depth == i + 174);
  assert(e.object == &object && strlen(e.handler) == 63 && e.handler[0] == 'h');
 }
 ring.record("return", nullptr, nullptr, nullptr, nullptr, 0, 0, 0);
 assert(ring.at(127).handler[0] == 0);
 Lingo vm;
 fatal([&] { vm.pop(); });
 fatal([&] { vm.peek(0); });
 fatal([&] { vm.guardCheckArguments(-1); });
 fatal([&] { vm.guardCheckArguments(1); });
 assert(vm.state.stack.empty());
 vm.guardCheckArguments(0);
 vm.state.stack = {{11}, {22}};
 vm.guardCheckArguments(2);
 assert(vm.peek(0).value == 22 && vm.peek(1).value == 11);
 fatal([&] { vm.peek(2); });
 fatal([&] { vm.peek(~0u); });
 fatal([&] { vm.guardCheckArguments(3); });
 assert(vm.state.stack.size() == 2 && vm.state.stack[0].value == 11);
 assert(vm.pop().value == 22 && vm.pop().value == 11);
 fatal([&] { vm.guardCheckPC(); });
 std::vector<int> script;
 vm.state.script = &script;
 fatal([&] { vm.guardCheckPC(); });
 script = {1, 0};
 vm.guardCheckPC();
 vm.state.pc = 1; vm.guardCheckPC(); // STOP is a valid slot.
 vm.state.pc = 2; fatal([&] { vm.guardCheckPC(); });
 vm.state.pc = ~0u; fatal([&] { vm.guardCheckPC(); });
 assert(vm.reports == 11);
}
'''
with tempfile.TemporaryDirectory(prefix='eutherlingo-') as temp:
    temp = Path(temp)
    (temp / 'test.cpp').write_text(code)
    (temp / 'config.h').write_text('#define SCUMM_LITTLE_ENDIAN\n')
    subprocess.run([args.cxx, '-std=c++11', '-g', '-O1', '-DPOSIX',
                    '-DHAVE_CONFIG_H', '-I' + str(temp), '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', '-I' + str(root),
                    '-I' + str(root / 'engines'), str(temp / 'test.cpp'),
                    '-o', str(temp / 'test')], check=True)
    subprocess.run([str(temp / 'test')], check=True)
print('PASS: bounded history, copied names, stack/argument/PC boundaries; ASan/UBSan')
