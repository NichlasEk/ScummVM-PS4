// SPDX-License-Identifier: GPL-3.0-or-later
#include "base/plugins.h"
#include "engines/advancedDetector.h"
namespace Sotc {
static const PlainGameDescriptor games[] = {
 {"sotc", "Shadow of the Comet (resource preview)"}, {nullptr, nullptr}
};
static const ADGameDescription descriptions[] = {
 {"sotc", "European CD - resource preview",
  AD_ENTRY2s("RES.PAK", "12d4a0aed1370e67f89e26753272e58a", 40224,
             "D00.PAK", "bcbe9f6e79ebe85cd2cc01d0011d3b29", 815772),
  Common::UNK_LANG, Common::kPlatformDOS, ADGF_UNSTABLE, GUIO2(GUIO_NOMIDI, GUIO_NOLAUNCHLOAD)},
 AD_TABLE_END_MARKER
};
class SotcDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
public:
 SotcDetection() : AdvancedMetaEngineDetection(descriptions, games) {}
 const char *getName() const override { return "sotc"; }
 const char *getEngineName() const override { return "Shadow of the Comet (experimental)"; }
 const char *getOriginalCopyright() const override { return "Shadow of the Comet (C) 1993 Infogrames"; }
};
}
REGISTER_PLUGIN_STATIC(SOTC_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, Sotc::SotcDetection);
