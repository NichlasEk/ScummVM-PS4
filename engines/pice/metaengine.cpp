// SPDX-License-Identifier: GPL-3.0-or-later
#include "base/plugins.h"
#include "engines/advancedDetector.h"
#include "pice/pice.h"
namespace Pice {
class PiceMetaEngine : public AdvancedMetaEngine<ADGameDescription> {
public:
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
