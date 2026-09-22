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
 bool _mac=false;
 byte _shadow=241;
 int _height=0,_ascent=0,_maxWidth=0;
};
// External Unicode bitmap font for CJK; no glyph data is linked into the engine.
class UnicodeBitmapFont : public Graphics::Font {
public:
 bool load(const Common::Array<byte> &bytes);
 int getFontHeight() const override { return 18; }
 int getMaxCharWidth() const override { return 18; }
 int getCharWidth(uint32 chr) const override;
 void drawChar(Graphics::Surface *dst,uint32 chr,int x,int y,uint32 color) const override;
private:
 int find(uint32 chr) const;
 Common::Array<byte> _data;
 uint32 _count=0;
};
bool runUnicodeFontTests();
bool runFontTests();
}
#endif
