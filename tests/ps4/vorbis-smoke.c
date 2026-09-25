/* Exercise vorbisfile callbacks, complete decoding, EOF and exact PCM seeks. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <vorbis/vorbisfile.h>
static int seek_cb(void *p, ogg_int64_t offset, int whence) { return fseek((FILE *)p,(long)offset,whence); }
static size_t read_cb(void *p,size_t size,size_t n,void *f) { return fread(p,size,n,(FILE *)f); }
static int close_cb(void *f) { return fclose((FILE *)f); }
static long tell_cb(void *f) { return ftell((FILE *)f); }
static uint32_t hash_bytes(uint32_t hash,const char *p,long n) {
 for(long i=0;i<n;i++)hash=(hash^(unsigned char)p[i])*16777619u;
 return hash;
}
static int test(const char *path,FILE *out) {
 FILE *f=fopen(path,"rb"); if(!f)return 1;
 OggVorbis_File v;ov_callbacks cb={read_cb,seek_cb,close_cb,tell_cb};
 if(ov_open_callbacks(f,&v,NULL,0,cb)){fclose(f);return 1;}
 vorbis_info *info=ov_info(&v,-1); if(!info || !ov_seekable(&v)){ov_clear(&v);return 1;}
 int channels=info->channels; long rate=info->rate;
 ogg_int64_t total=ov_pcm_total(&v,-1);long long bytes=0;uint32_t hash=2166136261u;
 char buf[4096],start[4096];long start_size=0,n;int bitstream=0,nonzero=0;
 while((n=ov_read(&v,buf,sizeof(buf),0,2,1,&bitstream))>0) {
  if(bytes==0){start_size=n;memcpy(start,buf,n);}
  for(long i=0;i<n;i++)if(buf[i])nonzero=1;
  hash=hash_bytes(hash,buf,n);bytes+=n;
 }
 int failed=n<0 || !nonzero || bytes!=total*channels*2 || ov_pcm_tell(&v)!=total;
 fprintf(out,"rate=%ld channels=%d frames=%lld bytes=%lld hash=%08x\n",rate,channels,(long long)total,bytes,hash);
 if(ov_pcm_seek(&v,0) || (n=ov_read(&v,buf,start_size,0,2,1,&bitstream))!=start_size || memcmp(buf,start,start_size))failed=1;
 ogg_int64_t mid=total/2;
 if(ov_pcm_seek(&v,mid) || ov_pcm_tell(&v)!=mid)failed=1;
 n=ov_read(&v,buf,sizeof(buf),0,2,1,&bitstream);
 if(n<=0)failed=1;
 else fprintf(out,"mid=%lld bytes=%ld hash=%08x\n",(long long)mid,n,hash_bytes(2166136261u,buf,n));
 if(ov_pcm_seek(&v,total) || ov_read(&v,buf,sizeof(buf),0,2,1,&bitstream)!=0)failed=1;
 if(ov_pcm_seek(&v,total+1)==0)failed=1;
 ov_clear(&v);return failed;
}
int main(int argc,char **argv) {
#ifdef __PS4__
 const char *dir="/app0",*output="/data/vorbis-smoke.txt";
#else
 if(argc!=3)return 2;const char *dir=argv[1],*output=argv[2];
#endif
 FILE *out=fopen(output,"w");if(!out)return 2;
 const char *names[]={"mono.ogg","stereo.ogg"};int failed=0;
 for(int i=0;i<2;i++){char path[1024];snprintf(path,sizeof(path),"%s/%s",dir,names[i]);fprintf(out,"%s\n",names[i]);failed|=test(path,out);}
 fprintf(out,"%s\n",failed?"FAIL":"PASS");fclose(out);return failed;
}
