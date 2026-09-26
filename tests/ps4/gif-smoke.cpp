#define FORBIDDEN_SYMBOL_ALLOW_ALL
#include "common/memstream.h"
#include "graphics/surface.h"
#include "image/gif.h"
#include <stdio.h>
#include <stdlib.h>

static int check(const char *name, bool bad, FILE *out) {
 char path[256]; snprintf(path,sizeof(path),"/app0/%s.gif",name);
 FILE *in=fopen(path,"rb"); if(!in) return 1;
 fseek(in,0,SEEK_END); long size=ftell(in); rewind(in);
 if(size<=0 || size>1024*1024) { fclose(in); return 1; }
 byte *data=(byte *)malloc(size);
 if(!data) { fclose(in); return 1; }
 bool read=fread(data,1,size,in)==(size_t)size; fclose(in);
 Common::MemoryReadStream stream(data,size);
 Image::GIFDecoder decoder;
 bool loaded=read && decoder.loadStream(stream);
 int failed=0;
 if(bad) {
  failed=loaded;
  fprintf(out,"%s %s\n",name,loaded ? "UNEXPECTED SUCCESS" : "rejected");
 } else if(!loaded) {
  fprintf(out,"%s FAIL load\n",name); failed=1;
 } else {
  snprintf(path,sizeof(path),"/app0/%s.rgba",name);
  FILE *ref=fopen(path,"rb");
  const Graphics::Surface *surface=decoder.getSurface();
  uint32 hash=2166136261u; unsigned samples=0;
  if(!ref || !surface || surface->w!=32 || surface->h!=24) failed=1;
  if(!failed) for(int y=0;y<surface->h;++y) for(int x=0;x<surface->w;++x) {
   unsigned index=*(const byte *)surface->getBasePtr(x,y);
   if(index>=decoder.getPaletteColorCount()) { failed=1; continue; }
   const byte *color=decoder.getPalette()+index*3;
   for(unsigned c=0;c<4;++c) {
    byte value=c<3 ? color[c] : (decoder.hasTransparentColor() && index==decoder.getTransparentColor() ? 0 : 255);
    if(fgetc(ref)!=value) failed=1;
    hash=(hash^value)*16777619u; ++samples;
   }
  }
  if(ref) { if(fgetc(ref)!=EOF || ferror(ref)) failed=1; fclose(ref); }
  fprintf(out,"%s rgba-fnv1a=%08x samples=%u %s\n",name,hash,samples,failed ? "FAIL" : "PASS");
 }
 decoder.destroy();
 if(decoder.hasTransparentColor() || decoder.getPaletteColorCount() || decoder.getSurface()) failed=1;
 free(data); return failed;
}
extern "C" int __wrap_main(int, char **) {
 FILE *out=fopen("/data/gif-smoke.txt","w"); if(!out) return 2;
 int result=0;
 const char *names[]={"palette","interlaced","local","transparent","offset","truncated","invalid"};
 for(unsigned i=0;i<7;++i) result |= check(names[i],i>=5,out);
 fprintf(out,"%s\n",result ? "FAIL" : "PASS"); fclose(out); return result;
}
