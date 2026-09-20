// SPDX-License-Identifier: GPL-3.0-or-later
#include "base/plugins.h"
#include "engines/advancedDetector.h"
#include "pice/pice.h"
#include "backends/keymapper/action.h"
#include "backends/keymapper/keymap.h"
namespace Pice {
class PiceMetaEngine : public AdvancedMetaEngine<ADGameDescription> {
public:
 Common::KeymapArray initKeymaps(const char *target) const override {
  Common::KeymapArray maps=MetaEngine::initKeymaps(target);
  // Default map already sends F5 for L1 (PC Q in our emulator profile).
  Common::Action *load=new Common::Action("PICE_LOAD", Common::U32String("Load checkpoint"));
  load->setKeyEvent(Common::KeyState(Common::KEYCODE_F7, Common::ASCII_F7));
  load->addDefaultInputMapping("JOY_RIGHT_SHOULDER");
  maps[0]->addAction(load);
  return maps;
 }
 const char *getName() const override { return "pice"; }
 Common::Error createInstance(OSystem *s, Engine **engine, const ADGameDescription *gd) const override {
  *engine = new PiceEngine(s); return Common::kNoError;
 }
};
}
#if PLUGIN_ENABLED_DYNAMIC(PICE)
REGISTER_PLUGIN_DYNAMIC(PICE, PLUGIN_TYPE_ENGINE, Pice::PiceMetaEngine);
#else
REGISTER_PLUGIN_STATIC(PICE, PLUGIN_TYPE_ENGINE, Pice::PiceMetaEngine);
#endif
