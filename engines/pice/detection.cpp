// SPDX-License-Identifier: GPL-3.0-or-later
#include "base/plugins.h"
#include "engines/advancedDetector.h"
namespace Pice {
static const PlainGameDescriptor games[] = {
 {"pice", "Prisoner of Ice (experimental resource preview)"}, {nullptr, nullptr}
};
static const ADGameDescription descriptions[] = {
 {"pice", "English European CD - resource preview",
  AD_ENTRY2s("KSVGA.KRO", "f933283265f8e8816acac67c9d72da85", 45874014,
             "E_KLANG.KRO", "a8e66acb3ce6a8fc126b948d4417e345", 32611279),
  Common::EN_GRB, Common::kPlatformDOS, ADGF_UNSTABLE, GUIO2(GUIO_NOMIDI, GUIO_NOLAUNCHLOAD)},
 AD_TABLE_END_MARKER
};
class PiceDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
public:
 PiceDetection() : AdvancedMetaEngineDetection(descriptions, games) {}
 const char *getName() const override { return "pice"; }
 const char *getEngineName() const override { return "Prisoner of Ice (experimental)"; }
 const char *getOriginalCopyright() const override { return "Prisoner of Ice (C) 1995 Infogrames"; }
};
}
REGISTER_PLUGIN_STATIC(PICE_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, Pice::PiceDetection);
