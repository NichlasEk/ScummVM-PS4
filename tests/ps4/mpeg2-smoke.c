#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <mpeg2dec/mpeg2.h>

/* Synthetic, finite elementary streams with a known sequence end and 50 frames.
 * The rejection policy below belongs to this fixture, not to libmpeg2's API. */
static int decode(const char *dir, const char *name, unsigned chunk, int bad, FILE *out) {
 char path[1024];
 snprintf(path, sizeof(path), "%s/%s.m2v", dir, name);
 FILE *in=fopen(path,"rb"), *ref=NULL;
 if (!in) return 1;
 if (!bad) {
  snprintf(path,sizeof(path),"%s/%s.yuv",dir,name);
  ref=fopen(path,"rb");
  if (!ref) { fclose(in); return 1; }
 }
 mpeg2dec_t *dec=mpeg2_init();
 if (!dec) { if(ref) fclose(ref); fclose(in); return 1; }
 const mpeg2_info_t *info=mpeg2_info(dec);
 uint8_t buffer[4096], expected[162];
 unsigned frames=0, ended=0, failed=0, types[4]={0}, maxdiff=0;
 uint64_t samples=0, difference=0;
 uint32_t hash=2166136261u;
 for (unsigned steps=0; steps<2000000; ++steps) {
  mpeg2_state_t state=mpeg2_parse(dec);
  if (state==STATE_BUFFER) {
   size_t n=fread(buffer,1,chunk,in);
   if (!n) break;
   mpeg2_buffer(dec,buffer,buffer+n);
  } else if (state==STATE_INVALID || state==STATE_INVALID_END) {
   failed=1; break;
  } else if (state==STATE_SLICE || state==STATE_END) {
   if (state==STATE_END) ended=1;
   if (!info->display_fbuf) continue;
   const mpeg2_sequence_t *s=info->sequence;
   if (!s || s->picture_width!=162 || s->picture_height!=98 ||
       s->frame_period!=1080000 || s->width<162 || s->height<98 ||
       s->chroma_width*2!=s->width ||
       (s->chroma_height!=s->height && s->chroma_height*2!=s->height)) {
    failed=1; break;
   }
   unsigned coding=info->display_picture->flags & PIC_MASK_CODING_TYPE;
   if (coding<1 || coding>3 || frames>=50) { failed=1; break; }
   ++types[coding];
   for (unsigned plane=0; plane<3; ++plane) {
    unsigned width=plane ? 81 : 162;
    unsigned height=plane && s->chroma_height*2==s->height ? 49 : 98;
    unsigned stride=plane ? s->chroma_width : s->width;
    for (unsigned y=0; y<height; ++y) {
     const uint8_t *row=info->display_fbuf->buf[plane]+y*stride;
     if (ref && fread(expected,1,width,ref)!=width) { failed=1; break; }
     for (unsigned x=0; x<width; ++x) {
      hash=(hash^row[x])*16777619u;
      if (ref) {
       unsigned d=(unsigned)abs((int)row[x]-(int)expected[x]);
       if (d>maxdiff) maxdiff=d;
       difference+=d; ++samples;
      }
     }
    }
   }
   ++frames;
  }
 }
 failed |= ferror(in) || !ended || frames!=50 || !types[1] || !types[2] || !types[3];
 if (ref) failed |= ferror(ref) || fgetc(ref)!=EOF || maxdiff>3 || difference*5>samples;
 if (bad) fprintf(out,"%s chunk=%u %s\n",name,chunk,failed ? "rejected" : "UNEXPECTED SUCCESS");
 else fprintf(out,"%s chunk=%u frames=%u I/P/B=%u/%u/%u yuv-fnv1a=%08x maxdiff=%u diff=%llu/%llu %s\n",
  name,chunk,frames,types[1],types[2],types[3],hash,maxdiff,
  (unsigned long long)difference,(unsigned long long)samples,failed ? "FAIL" : "PASS");
 mpeg2_close(dec); fclose(in); if(ref) fclose(ref);
 return bad ? !failed : !!failed;
}
int main(int argc,char **argv) {
#ifdef __PS4__
 (void)argc; (void)argv;
 const char *dir="/app0", *output="/data/mpeg2-smoke.txt";
#else
 if(argc!=3) return 2;
 const char *dir=argv[1], *output=argv[2];
#endif
 FILE *out=fopen(output,"w"); if(!out) return 2;
 int result=0;
 const char *names[]={"clip420","clip422","interlaced","truncated","invalid"};
 /* Odd packet boundaries exercise sequence headers split across input buffers. */
 for(unsigned i=0;i<5;++i) for(unsigned c=0;c<2;++c)
  result |= decode(dir,names[i],c ? 17 : 4096,i>=3,out);
 fprintf(out,"%s\n",result ? "FAIL" : "PASS");
 if(fclose(out)) result=1;
 return result;
}
