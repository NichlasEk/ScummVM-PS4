/* Compare glyph pixels and metrics on host and PS4 using the same font. */
#include <stdio.h>
#include <stdint.h>
#include <ft2build.h>
#include FT_FREETYPE_H
int main(int argc, char **argv) {
#ifdef __PS4__
 const char *font="/app0/test.ttf", *output="/data/freetype-smoke.txt";
#else
 if(argc!=3) return 2;
 const char *font=argv[1], *output=argv[2];
#endif
 FILE *out=fopen(output,"w"); if(!out)return 2;
 FT_Library library; FT_Face face;
 if(FT_Init_FreeType(&library) || FT_New_Face(library,font,0,&face)) {
  fprintf(out,"FAIL opening font\n");fclose(out);return 1;
 }
 const unsigned glyphs[]={65,103,48,197,228,246};
 const unsigned sizes[]={9,16,32};
 int result=0;
 for(unsigned s=0;s<3;s++) {
  if(FT_Set_Pixel_Sizes(face,0,sizes[s])){result=1;break;}
  for(unsigned i=0;i<6;i++) {
   if(!FT_Get_Char_Index(face,glyphs[i]) || FT_Load_Char(face,glyphs[i],FT_LOAD_RENDER)){result=1;break;}
   FT_GlyphSlot g=face->glyph; FT_Bitmap *b=&g->bitmap;
   if(b->pixel_mode!=FT_PIXEL_MODE_GRAY || b->pitch<0){result=1;break;}
   uint32_t hash=2166136261u;
   for(unsigned y=0;y<b->rows;y++)for(unsigned x=0;x<b->width;x++)hash=(hash^b->buffer[y*b->pitch+x])*16777619u;
   fprintf(out,"size=%u U+%04X %ux%u left=%d top=%d advance=%ld hash=%08x\n",sizes[s],glyphs[i],b->width,b->rows,g->bitmap_left,g->bitmap_top,(long)g->advance.x,hash);
  }
 }
 FT_Done_Face(face);FT_Done_FreeType(library);
 fprintf(out,"%s\n",result?"FAIL":"PASS");fclose(out);return result;
}
