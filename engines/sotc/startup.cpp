// SPDX-License-Identifier: GPL-3.0-or-later
#include "sotc/startup.h"
#include "sotc/startstate.h"
#include "sotc/sequence.h"
#include "sotc/script.h"
#include "sotc/pak.h"
#include "common/file.h"
#include "common/debug.h"
#include "common/textconsole.h"
namespace Sotc {
static bool readFile(const char *name, Common::Array<byte> &data) {
 Common::File file;
 if (!file.open(Common::Path(name)) || file.size() < 1 || file.size() > 4 * 1024 * 1024) return false;
 data.resize(file.size()); return file.read(data.data(), data.size()) == data.size();
}
class SnapshotRenderer {
 Common::Array<byte> _pixels, _mask;
 Common::Array<byte> &_screen;
public:
 SnapshotRenderer(Common::Array<byte> &screen) : _screen(screen) { _pixels.resize(640 * 256); _mask.resize(640 * 256); }
 bool pose(const Common::Array<byte> &data, unsigned pose, int x, int y, unsigned clipX1 = 0, unsigned clipY1 = 0, unsigned clipX2 = 319, unsigned clipY2 = 199, int onlyLayer = -1) {
  if (data.empty()) return false;
  const uint8_t *record; unsigned layers;
  if (!va2Record(data.data(), data.size(), 0, pose, record, layers)) return false;
  if (onlyLayer >= 0 && unsigned(onlyLayer) >= layers) return false;
  unsigned begin = onlyLayer < 0 ? 0 : unsigned(onlyLayer), end = onlyLayer < 0 ? layers : begin + 1;
  // Two passes prevent partial drawing when a later layer is unsupported.
  for (unsigned pass = 0; pass < 2; ++pass) for (unsigned i = begin; i < end; ++i) {
   PoseLayer layer; unsigned width, height;
   if (!va2Layer(data.data(), data.size(), pose, i, layer) ||
       !va2Frame(data.data(), data.size(), layer.frame, _pixels.data(), _mask.data(), _pixels.size(), width, height)) return false;
   if (!pass) continue;
   for (unsigned py = 0; py < height; ++py) for (unsigned px = 0; px < width; ++px) {
    int dx = x + layer.x + int(px), dy = y + layer.y - int(height) + 1 + int(py);
    unsigned source = py * width + (layer.mirror ? width - 1 - px : px);
    if (dx >= 0 && dx < 320 && dy >= 0 && dy < 200 && unsigned(dx) >= clipX1 && unsigned(dx) <= clipX2 &&
        unsigned(dy) >= clipY1 && unsigned(dy) <= clipY2 && _mask[source]) _screen[dy * 320 + dx] = _pixels[source];
   }
  }
  return true;
 }
};
bool loadStartupScene(Common::Array<byte> &pixels, Common::String &status) {
 pixels.clear(); status = "INIT.SAV unavailable";
 Common::Array<byte> save, scripts;
 StartState state;
 if (!readFile("INIT.SAV", save) || !state.load(Cc4View(save.data(), save.size()))) return false;
 unsigned bank = state.word(0x2df2), blockIndex = state.word(0x2df8), background = state.word(0x2e14);
 Common::String roomName = Common::String::format("R%02u.CC4", bank);
 Cc4View block;
 if (!readFile(roomName.c_str(), scripts) || !cc4Entry(Cc4View(scripts.data(), scripts.size()), 4, blockIndex, block) ||
     cc4Count(block, 2) != state.word(0x2e02)) return false;
 // Verify every rebased PC before rendering any of this snapshot.
 for (unsigned i = 0; i < state.word(0x2e02); ++i) {
  uint32_t pc;
  if (!state.taskPC(block, i, pc)) return false;
  debug("[sotc-init-task] task=%u pc=%u actor=%u flags=%02x", i, pc, state.byte(0x3118 + i * 22), state.byte(0x3119 + i * 22));
 }
 Pak rooms, actors, resources;
 Common::Array<byte> scenery, actorData[20];
 if (!rooms.open(Common::String::format("D%02u.PAK", bank)) || !actors.open(Common::String::format("A%02u.PAK", bank)) ||
     !resources.open("RES.PAK") || !rooms.load(background, pixels) || pixels.size() != 64000 || !rooms.load(background + 1, scenery)) {
  pixels.clear(); return false;
 }
 for (unsigned i = 0; i < 20; ++i) {
  unsigned slot = 0x3286 + i * 6, type = state.byte(slot);
  // Resource ID/type metadata is portable; the serialized far pointer is not.
  bool occupied = state.word(slot + 2) || state.word(slot + 4);
  if (!occupied) continue;
  if (type == 0) actors.load(state.byte(slot + 1), actorData[i]);
  else if (type == 1) resources.load(2, actorData[i]); // 0x6cb5 -> 0x609e, initialized at 0xc96.
  else if (type == 2) actorData[i] = scenery;
 }
 SnapshotRenderer renderer(pixels);
 unsigned rendered = 0, unsupported = 0, sceneryLayer = 0;
 const uint8_t *sceneryLayers; unsigned sceneryCount;
 bool haveSceneryPose = va2Record(scenery.data(), scenery.size(), 0, 0, sceneryLayers, sceneryCount);
 for (unsigned i = 0; i < state.byte(0x34de); ++i) {
  unsigned id = state.byte(0x34e1 + i * 2);
  if ((id & 0xf0) == 0x10) {
   // Scene pose 0 is interleaved with actors by the serialized draw list.
   bool ok = false;
   if (haveSceneryPose && sceneryLayer < sceneryCount) {
    const uint8_t *layer = sceneryLayers + sceneryLayer * 8;
    if (layer[0] == 0 && layer[1] == 1) // Original 0xc806: nested pose.
     ok = renderer.pose(scenery, va2LE16(layer + 2), va2Signed16(layer + 4), va2Signed16(layer + 6));
    else if (layer[0] == 1 && layer[1] == 1)
     ok = renderer.pose(scenery, 0, 0, 0, 0, 0, 319, 199, sceneryLayer);
   }
   ++sceneryLayer;
   if (ok) ++rendered; else ++unsupported;
   continue;
  }
  if (id >= 11) { ++unsupported; continue; }
  unsigned a = 0x2e54 + id * 52, slot = state.byte(a + 11);
  if (slot >= 20 || actorData[slot].empty()) { ++unsupported; continue; }
  const Common::Array<byte> &data = actorData[slot];
  unsigned sequence = state.word(a + 12), stepIndex = state.word(a + 14);
  SequenceStep step;
  int x = state.signedWord(a), y = state.signedWord(a + 2), mx = 0, my = 0;
  bool ok = va2Step(data.data(), data.size(), sequence, stepIndex, step);
  if (ok && state.word(a + 6) == 2) {
   ok = (step.control >> 14) == 0 && va2Position(data.data(), data.size(), sequence, stepIndex, mx, my);
   x += mx; y += my;
  }
  ok = ok && renderer.pose(data, step.pose, x, y, state.word(a + 44), state.byte(a + 48), state.word(a + 46), state.byte(a + 49));
  debug("[sotc-init-actor] id=%u slot=%u seq=%u step=%u x=%d y=%d rendered=%d", id, slot, sequence, stepIndex, x, y, ok);
  if (ok) ++rendered; else ++unsupported;
 }
 status = Common::String::format("INIT scene: %u drawn, %u unsupported", rendered, unsupported);
 debug("[sotc-init] bank=%u block=%u background=%u %s", bank, blockIndex, background, status.c_str());
 return true;
}
}
