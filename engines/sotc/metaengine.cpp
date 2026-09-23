// SPDX-License-Identifier: GPL-3.0-or-later
#include "base/plugins.h"
#include "engines/advancedDetector.h"
#include "sotc/sotc.h"
#include "backends/keymapper/action.h"
#include "backends/keymapper/keymap.h"
namespace Sotc {
class SotcMetaEngine : public AdvancedMetaEngine<ADGameDescription> {
public:
 Common::KeymapArray initKeymaps(const char *) const override {
  Common::KeymapArray maps;
  Common::Keymap *map = new Common::Keymap(Common::Keymap::kKeymapTypeGame, "sotc", "Resource viewer");
  const char *ids[] = {"NEXT", "PREV", "BANKUP", "BANKDOWN", "PALETTE", "EXIT"};
  const char *labels[] = {"Next image", "Previous image", "Next bank", "Previous bank", "Next palette", "Exit viewer"};
  const char *keys[] = {"RIGHT", "LEFT", "UP", "DOWN", "p", "ESCAPE"};
  const char *buttons[] = {"JOY_RIGHT", "JOY_LEFT", "JOY_UP", "JOY_DOWN", "JOY_X", "JOY_BACK"};
  const Common::KeyCode codes[] = {Common::KEYCODE_RIGHT, Common::KEYCODE_LEFT, Common::KEYCODE_UP, Common::KEYCODE_DOWN, Common::KEYCODE_p, Common::KEYCODE_ESCAPE};
  for (int i = 0; i < 6; ++i) {
   Common::Action *action = new Common::Action(ids[i], Common::U32String(labels[i]));
   action->setKeyEvent(Common::KeyState(codes[i])); action->addDefaultInputMapping(keys[i]); action->addDefaultInputMapping(buttons[i]); map->addAction(action);
  }
  Common::Action *next = new Common::Action("CLICKNEXT", Common::U32String("Next image"));
  next->setLeftClickEvent(); next->addDefaultInputMapping("MOUSE_LEFT"); next->addDefaultInputMapping("JOY_A"); map->addAction(next);
  Common::Action *prev = new Common::Action("CLICKPREV", Common::U32String("Previous image"));
  prev->setRightClickEvent(); prev->addDefaultInputMapping("MOUSE_RIGHT"); prev->addDefaultInputMapping("JOY_B"); map->addAction(prev);
  maps.push_back(map); return maps;
 }
 const char *getName() const override { return "sotc"; }
 Common::Error createInstance(OSystem *system, Engine **engine, const ADGameDescription *) const override {
  *engine = new SotcEngine(system); return Common::kNoError;
 }
};
}
#if PLUGIN_ENABLED_DYNAMIC(SOTC)
REGISTER_PLUGIN_DYNAMIC(SOTC, PLUGIN_TYPE_ENGINE, Sotc::SotcMetaEngine);
#else
REGISTER_PLUGIN_STATIC(SOTC, PLUGIN_TYPE_ENGINE, Sotc::SotcMetaEngine);
#endif
