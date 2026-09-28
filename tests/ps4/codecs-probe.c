/* PS4 decoder smoke test. Fixtures are synthesized by check-ps4-codecs.sh. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <FLAC/stream_decoder.h>
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>
#include <a52dec/a52.h>
#include "codec-fixtures.h"

static size_t flac_pos, pcm_pos;
static int flac_error;
static FLAC__StreamDecoderReadStatus read_flac(const FLAC__StreamDecoder *d,
        FLAC__byte *out, size_t *bytes, void *user) {
    (void)d; (void)user;
    size_t left = sizeof(fixture_flac) - flac_pos;
    if (*bytes > left) *bytes = left;
    memcpy(out, fixture_flac + flac_pos, *bytes);
    flac_pos += *bytes;
    return *bytes ? FLAC__STREAM_DECODER_READ_STATUS_CONTINUE : FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
}
static FLAC__StreamDecoderWriteStatus write_flac(const FLAC__StreamDecoder *d,
        const FLAC__Frame *frame, const FLAC__int32 *const samples[], void *user) {
    (void)d; (void)user;
    if (frame->header.channels != 2 || frame->header.bits_per_sample != 16 ||
        frame->header.sample_rate != 48000) return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    for (unsigned i = 0; i < frame->header.blocksize; ++i) {
        for (unsigned ch = 0; ch < 2; ++ch) {
            if (pcm_pos + 2 > sizeof(fixture_pcm)) return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
            int16_t expected = (int16_t)(fixture_pcm[pcm_pos] | (fixture_pcm[pcm_pos + 1] << 8));
            pcm_pos += 2;
            if (samples[ch][i] != expected) return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        }
    }
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}
static void error_flac(const FLAC__StreamDecoder *d, FLAC__StreamDecoderErrorStatus e, void *user) {
    (void)d; (void)e; (void)user; flac_error = 1;
}
static int test_flac(void) {
    FLAC__StreamDecoder *d = FLAC__stream_decoder_new();
    if (!d) return 0;
    int ok = FLAC__stream_decoder_init_stream(d, read_flac, NULL, NULL, NULL,
        NULL, write_flac, NULL, error_flac, NULL) == FLAC__STREAM_DECODER_INIT_STATUS_OK;
    ok = ok && FLAC__stream_decoder_process_until_end_of_stream(d);
    ok = ok && !flac_error && pcm_pos == sizeof(fixture_pcm);
    FLAC__stream_decoder_finish(d);
    FLAC__stream_decoder_delete(d);
    return ok;
}
static int test_vpx(void) {
    vpx_codec_ctx_t codec;
    memset(&codec, 0, sizeof(codec));
    if (vpx_codec_dec_init(&codec, vpx_codec_vp8_dx(), NULL, 0)) return 0;
    unsigned n = fixture_ivf[32] | (fixture_ivf[33] << 8) |
        (fixture_ivf[34] << 16) | ((unsigned)fixture_ivf[35] << 24);
    int ok = n <= sizeof(fixture_ivf) - 44 &&
        vpx_codec_decode(&codec, fixture_ivf + 44, n, NULL, 0) == VPX_CODEC_OK;
    vpx_codec_iter_t it = NULL;
    vpx_image_t *img = ok ? vpx_codec_get_frame(&codec, &it) : NULL;
    ok = img && img->d_w == 64 && img->d_h == 48;
    if (ok) {
        int y = img->planes[0][0], u = img->planes[1][0], v = img->planes[2][0];
        ok = y > 70 && y < 90 && u > 80 && u < 100 && v > 230 && v < 250;
    }
    vpx_codec_destroy(&codec);
    return ok;
}
static int test_a52(void) {
    a52_state_t *d = a52_init(0);
    if (!d) return 0;
    int flags, rate, bitrate;
    /* liba52's historical API lacks const qualifiers. */
    uint8_t frame[sizeof(fixture_ac3)];
    memcpy(frame, fixture_ac3, sizeof(frame));
    int size = a52_syncinfo(frame, &flags, &rate, &bitrate);
    flags = A52_STEREO;
    sample_t level = 1.0f;
    int ok = size > 0 && (size_t)size <= sizeof(frame) && rate == 48000 &&
        !a52_frame(d, frame, &flags, &level, 0.0f);
    double energy = 0;
    if (ok) a52_dynrng(d, NULL, NULL);
    for (int b = 0; ok && b < 6; ++b) {
        ok = !a52_block(d);
        sample_t *samples = a52_samples(d);
        for (int i = 0; ok && i < 512; ++i) {
            ok = isfinite(samples[i]) && fabsf(samples[i]) < 1.0f;
            energy += samples[i] * samples[i];
        }
    }
    a52_free(d);
    return ok && energy > 0.1;
}
int main(void) {
    int flac = test_flac(), vpx = test_vpx(), a52 = test_a52();
    printf("PS4 CODECS: FLAC exact PCM=%s VP8 frame=%s A52 audio=%s\n",
        flac ? "PASS" : "FAIL", vpx ? "PASS" : "FAIL", a52 ? "PASS" : "FAIL");
    fflush(stdout);
    return flac && vpx && a52 ? 0 : 1;
}
