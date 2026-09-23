// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SOTC_TOOLBAR_H
#define SOTC_TOOLBAR_H
#include "sotc/poserenderer.h"
namespace Sotc {
// Original ICONE.VA2 poses 0..7: neutral bar and seven hover highlights.
// Coordinates are in the 320x200 game surface. Unsupported verbs stay inert.
class Toolbar {
 Common::Array<byte> _data,_pixels;
 bool _visible=false;
 int _hover=-1;
public:
 bool load(const Common::Array<byte> &data) {
  _data=data; _pixels.resize(64000);
  return render();
 }
 bool visible() const { return _visible; }
 int hover() const { return _hover; }
 void hide() { _visible=false; _hover=-1; }
 void move(int x,int y) {
  if (x<0 || x>=320 || y<0 || y>=40) { hide(); return; }
  if (y<3) _visible=true;
  _hover=_visible ? (x/45>6 ? 6 : x/45) : -1;
 }
 bool render() {
  for (unsigned i=0;i<_pixels.size();++i) _pixels[i]=0;
  PoseRenderer renderer(_pixels);
  return renderer.pose(_data,unsigned(_hover+1),0,0,0,0,319,199);
 }
 const Common::Array<byte> &pixels() const { return _pixels; }
 bool menuAt(int x,int y) const { return _visible && x>=270 && x<320 && y>=0 && y<40; }
};
}
#endif
