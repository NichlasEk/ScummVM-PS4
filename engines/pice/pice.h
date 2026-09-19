// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_ENGINE_H
#define PICE_ENGINE_H
#include "engines/engine.h"
#include "audio/mixer.h"
#include "pice/kro.h"
namespace Pice {
class PiceEngine : public Engine {
public:
 explicit PiceEngine(OSystem *system) : Engine(system), _picture(0), _speech(0), _textColor(255) {}
 Common::Error run() override;
private:
 bool showPicture(uint position);
 void playSpeech();
 void drawLabels();
 KroArchive _graphics, _language;
 Common::Array<uint> _pictures;
 uint _picture, _speech;
 byte _textColor;
 Audio::SoundHandle _speechHandle;
};
}
#endif
