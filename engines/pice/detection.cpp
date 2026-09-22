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
 {"pice", "Macintosh multilingual CD",
  AD_ENTRY2s("KSVGA.KRO", "a28f988a822ecca247a59c46dcc99b73", 45719760,
             "E_KLANG.KRO", "cef7f74a762becd4c6c813ab5cf4597f", 32611722),
  Common::EN_ANY, Common::kPlatformMacintosh, ADGF_UNSTABLE, GUIO2(GUIO_NOMIDI, GUIO_NOLAUNCHLOAD)},
 {"pice", "Macintosh English/Swedish CD",
  AD_ENTRY2s("KSVGA.KRO", "a28f988a822ecca247a59c46dcc99b73", 45719789,
             "W_KLANG.KRO", "bf5b0447a3530f478b0a100f94066b94", 32919521),
  Common::SE_SWE, Common::kPlatformMacintosh, ADGF_UNSTABLE, GUIO2(GUIO_NOMIDI, GUIO_NOLAUNCHLOAD)},
 {"pice", "Windows 95 multilingual CD",
  AD_ENTRY2s("KSVGA.KRO", "f933283265f8e8816acac67c9d72da85", 45874865,
             "KSOUNDW.KRO", "bfbfd6fe1f15d4eaa8b7bf71cdd1cf4d", 7160115),
  Common::EN_ANY, Common::kPlatformWindows, ADGF_UNSTABLE, GUIO2(GUIO_NOMIDI, GUIO_NOLAUNCHLOAD)},
 {"pice", "English SVGA demo",
  AD_ENTRY2s("KSVGA.KRO", "cabd2c120e30ddd608d25b9c1ea816a9", 10916737,
             "E_KLANG.KRO", "3b1ba68af306305a26fe2f02a2e3fa17", 12707023),
  Common::EN_ANY, Common::kPlatformDOS, ADGF_UNSTABLE | ADGF_DEMO, GUIO2(GUIO_NOMIDI, GUIO_NOLAUNCHLOAD)},
 {"pice", "Saturn Japanese - movies and artwork preview (no gameplay)",
  AD_ENTRY2s("KVGA.KRO", "4141f95d6edfceeae776f804e030678a", 15468702,
             "J_KLANG.KRO", "7fa70934c284fef4e0308006d70d8316", 32921880),
  Common::JA_JPN, Common::kPlatformSaturn, ADGF_UNSTABLE, GUIO2(GUIO_NOMIDI, GUIO_NOLAUNCHLOAD)},
 AD_TABLE_END_MARKER
};
class PiceDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
public:
 PiceDetection() : AdvancedMetaEngineDetection(descriptions, games) { static const char *const dirs[]={"MAC_KRO",nullptr}; _directoryGlobs=dirs; _maxScanDepth=2; }
 const char *getName() const override { return "pice"; }
 const char *getEngineName() const override { return "Prisoner of Ice (experimental)"; }
 const char *getOriginalCopyright() const override { return "Prisoner of Ice (C) 1995 Infogrames"; }
};
}
REGISTER_PLUGIN_STATIC(PICE_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, Pice::PiceDetection);
