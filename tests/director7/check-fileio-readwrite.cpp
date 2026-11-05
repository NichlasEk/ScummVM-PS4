#include "director/lingo/xlibs/f/fileio-read-stream.h"
#include <cassert>
#include <cstring>
#include <climits>

int main() {
 Common::MemoryWriteStreamDynamic buffer(DisposeAfterUse::YES);
 Director::FileIOReadStream input(&buffer);
 char data[16] = {};
 assert(input.read(data, 1) == 0 && input.eos());
 assert(input.seek(0) && !input.eos());
 const char save[] = "[25, 27]\rGHV";
 buffer.write(save, sizeof(save) - 1);
 assert(input.pos() == sizeof(save) - 1);
 assert(input.seek(0));
 assert(input.read(data, 8) == 8 && !memcmp(data, "[25, 27]", 8));
 assert(buffer.pos() == 8);
 buffer.write("\rAFK", 4); // write shares the position reached by reading
 assert(input.seek(-3, SEEK_END));
 assert(input.read(data, 3) == 3 && !memcmp(data, "AFK", 3));
 assert(!input.eos());
 assert(input.read(data, 1) == 0 && input.eos());
 assert(!input.seek(-1) && !input.seek(100));
 assert(input.seek(0) && !input.eos());
 assert(input.read(data, 16) == 12 && input.eos());
 assert(input.seek(0));
 for (int i=0; i<4096; ++i) buffer.write("abcd", 4); // force reallocation
 assert(input.size() == 16384 && input.pos() == 16384);
 assert(input.seek(-4, SEEK_END));
 assert(input.read(data, 4) == 4 && !memcmp(data, "abcd", 4));
 assert(input.seek(-4, SEEK_CUR));
 assert(input.read(data, 4) == 4);
 assert(!input.seek(0, 999));
 assert(!input.seek(INT64_MAX, SEEK_CUR));
 assert(!input.seek(INT64_MIN, SEEK_END));
}
