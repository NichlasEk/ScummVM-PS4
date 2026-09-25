/* Compare decoded fixed-point PCM on the host and PS4 using the same input. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <mad.h>
_Static_assert(sizeof(int) == 4 && sizeof(long) == 8, "libmad LP64 build");
int main(int argc, char **argv) {
#ifdef __PS4__
 const char *input = "/app0/sample.mp3", *output = "/data/mad-smoke.txt";
#else
 if (argc != 3) return 2;
 const char *input = argv[1], *output = argv[2];
#endif
 FILE *out = fopen(output, "w");
 if (!out) return 2;
 FILE *in = fopen(input, "rb");
 if (!in) { fprintf(out, "FAIL: open\n"); fclose(out); return 1; }
 fseek(in, 0, SEEK_END); long size = ftell(in); rewind(in);
 if (size <= 0 || size > 64 * 1024 * 1024) { fclose(in); fclose(out); return 1; }
 unsigned char *data = calloc((size_t)size + MAD_BUFFER_GUARD, 1);
 if (!data || fread(data, 1, size, in) != (size_t)size) {
  free(data); fclose(in); fclose(out); return 1;
 }
 fclose(in);
 struct mad_stream stream; struct mad_frame frame; struct mad_synth synth;
 mad_stream_init(&stream); mad_frame_init(&frame); mad_synth_init(&synth);
 mad_stream_buffer(&stream, data, size + MAD_BUFFER_GUARD);
 uint32_t hash = 2166136261u;
 unsigned frames = 0, samples = 0, recoverable = 0, nonzero = 0;
 int fatal = 0;
 while (1) {
  if (mad_frame_decode(&frame, &stream)) {
   if (stream.error == MAD_ERROR_BUFLEN) break;
   if (MAD_RECOVERABLE(stream.error)) { ++recoverable; continue; }
   fprintf(out, "FAIL: %s\n", mad_stream_errorstr(&stream)); fatal = 1; break;
  }
  mad_synth_frame(&synth, &frame); ++frames;
  for (unsigned i = 0; i < synth.pcm.length; ++i) {
   for (unsigned ch = 0; ch < synth.pcm.channels; ++ch) {
    uint32_t sample = (uint32_t)synth.pcm.samples[ch][i];
    if (sample) ++nonzero;
    for (unsigned b = 0; b < 4; ++b) hash = (hash ^ ((sample >> (b*8)) & 255u)) * 16777619u;
    ++samples;
   }
  }
 }
 fprintf(out, "frames=%u samples=%u nonzero=%u recoverable=%u rate=%u channels=%u PCM-fnv1a=%08x\n", frames, samples, nonzero, recoverable, synth.pcm.samplerate, synth.pcm.channels, hash);
 int failed = fatal || !frames || !nonzero;
 fprintf(out, "%s\n", failed ? "FAIL" : "PASS");
 mad_synth_finish(&synth); mad_frame_finish(&frame); mad_stream_finish(&stream);
 free(data); fclose(out); return failed;
}
