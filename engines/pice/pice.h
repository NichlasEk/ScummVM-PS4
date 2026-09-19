// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_ENGINE_H
#define PICE_ENGINE_H
#include "engines/engine.h"
#include "audio/mixer.h"
#include "pice/kro.h"
#include "pice/sprite.h"
namespace Pice {
class PiceEngine : public Engine {
public:
 explicit PiceEngine(OSystem *system) : Engine(system), _picture(0), _speech(0), _textColor(255) {}
 Common::Error run() override;
private:
 bool showPicture(uint position);
 void playSpeech();
 void drawLabels();
 bool showAnimation(uint resource);
 void advanceAnimation();
 SpriteResource _animation;
 Common::Array<SpriteImage> _sprites;
 Common::Array<byte> _canvas;
 uint _animationId = 0, _sequence = 0, _step = 0;
 uint32 _nextFrame = 0;
 int _originX = 0, _originY = 0;
 bool _spriteMode = false;
 byte _backgroundColor = 0;
 KroArchive _graphics, _language;
 Common::Array<uint> _pictures;
 uint _picture, _speech;
 byte _textColor;
 Audio::SoundHandle _speechHandle;
};
}
#endif
