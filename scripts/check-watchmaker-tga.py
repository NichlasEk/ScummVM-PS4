#!/usr/bin/env python3
"""Exercise production TGA orientation policy after decoder normalization."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'.tools/scummvm-vulkan-src/engines/watchmaker/tga_util.cpp').read_text();a=s.index('Graphics::Surface *ReadTgaImage(');b=s.index('\n}',a)+2
model=(root/'.tools/scummvm-vulkan-src/engines/watchmaker/3d/render/opengl_3d.cpp').read_text()
start=model.index('\t\tauto image = ReadTgaImage(')
end=model.index('\t\tSurfaceBackedTextureData texData(image);',start)
model=model[start:end]
code=r'''
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
namespace Common {struct SeekableReadStream {};struct Rect {Rect(int,int){}};}
namespace Graphics {struct PixelFormat {PixelFormat()=default;PixelFormat(int,int,int,int,int,int,int,int,int){}};struct Surface {int w=1,h=2;int pixels[2]={11,22};
Surface* convertTo(PixelFormat)const{return new Surface(*this);}void flipVertical(Common::Rect){std::swap(pixels[0],pixels[1]);}};}
namespace Image {struct TGADecoder {Graphics::Surface surface;bool loadStream(Common::SeekableReadStream&){return true;}const Graphics::Surface*getSurface(){return &surface;}};}
void error(const char*,const char*){std::abort();}
constexpr unsigned rSURFACEFLIP=1<<4;
'''+s[a:b]+'''
Graphics::Surface *loadModel(Common::SeekableReadStream *stream) {
const char *TextName="model";
'''+model+'''return image;}
'''+r'''
int main(){Common::SeekableReadStream stream;Graphics::PixelFormat format;
 auto model=loadModel(&stream);
 auto bitmap=ReadTgaImage("bitmap",stream,format,0);
 auto ui=ReadTgaImage("font",stream,format,rSURFACEFLIP);
 assert(model->pixels[0]==22 && model->pixels[1]==11);
 assert(ui->pixels[0]==11 && ui->pixels[1]==22);
 assert(bitmap->pixels[0]==11 && bitmap->pixels[1]==22);
 delete model;delete ui;delete bitmap;
 puts("PASS production TGA loading: model rows bottom-first; UI/font rows top-first");}
'''
work=root/'build/watchmaker-data-test';work.mkdir(parents=True,exist_ok=True);p=work/'tga.cpp';p.write_text(code)
subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined',str(p),'-o',str(work/'tga')],check=True)
subprocess.run([str(work/'tga')],check=True)
