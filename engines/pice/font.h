// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_FONT_H
#define PICE_FONT_H
#include "graphics/font.h"
#include "common/array.h"
namespace Pice {
// Runtime decoder only: glyph artwork stays in the user's S_FONT resource.
class OriginalFont : public Graphics::Font {
public:
 bool load(const Common::Array<byte> &bytes);
 void setShadow(byte color) { _shadow=color; }
 bool ready() const { return !_data.empty(); }
 int getFontHeight() const override { return _height; }
 int getMaxCharWidth() const override { return _maxWidth; }
 int getCharWidth(uint32 chr) const override;
 void drawChar(Graphics::Surface *dst,uint32 chr,int x,int y,uint32 color) const override;
private:
 int glyph(uint32 chr) const;
 Common::Array<byte> _data;
 Common::U32String _characters;
 byte _shadow=241;
 int _height=0,_ascent=0,_maxWidth=0;
};
bool runFontTests();
}
#endif
