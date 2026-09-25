/* Demux synthetic Ogg/Theora clips, decode and hash cropped Y/U/V pixels.
 * The build script independently compares these hashes to FFmpeg decoding. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ogg/ogg.h>
#include <theora/theoradec.h>

static int decode(const char *path, const char *name, FILE *out, int expect_bad) {
    FILE *in = fopen(path, "rb");
    if (!in) return 1;
    ogg_sync_state sync;
    ogg_stream_state stream;
    ogg_page page;
    ogg_packet packet;
    th_info info;
    th_comment comment;
    th_setup_info *setup = NULL;
    th_dec_ctx *decoder = NULL;
    int have_stream = 0, headers = 0, frames = 0, eos = 0, failed = 0;
    uint32_t hash = 2166136261u;
    double end = 0;
    ogg_sync_init(&sync);
    th_info_init(&info);
    th_comment_init(&comment);
    for (;;) {
        int status = ogg_sync_pageout(&sync, &page);
        if (status < 0) { failed = 1; break; }
        if (!status) {
            char *buffer = ogg_sync_buffer(&sync, 4096);
            if (!buffer) { failed = 1; break; }
            size_t bytes = fread(buffer, 1, 4096, in);
            if (!bytes) break;
            if (ogg_sync_wrote(&sync, bytes)) { failed = 1; break; }
            continue;
        }
        if (!have_stream) {
            if (ogg_stream_init(&stream, ogg_page_serialno(&page))) { failed = 1; break; }
            have_stream = 1;
        }
        if (ogg_stream_pagein(&stream, &page)) { failed = 1; break; }
        while ((status = ogg_stream_packetout(&stream, &packet)) > 0) {
            if (headers < 3) {
                if (th_decode_headerin(&info, &comment, &setup, &packet) <= 0) { failed = 1; break; }
                if (++headers == 3) {
                    decoder = th_decode_alloc(&info, setup);
                    th_setup_free(setup); setup = NULL;
                    if (!decoder) { failed = 1; break; }
                }
            } else {
                ogg_int64_t granule;
                int result = th_decode_packetin(decoder, &packet, &granule);
                if (result != 0 && result != TH_DUPFRAME) { failed = 1; break; }
                th_ycbcr_buffer yuv;
                if (th_decode_ycbcr_out(decoder, yuv)) { failed = 1; break; }
                for (int plane = 0; plane < 3; ++plane) {
                    int sx = plane && !(info.pixel_fmt & 1);
                    int sy = plane && !(info.pixel_fmt & 2);
                    unsigned width = info.pic_width >> sx, height = info.pic_height >> sy;
                    unsigned x0 = info.pic_x >> sx, y0 = info.pic_y >> sy;
                    for (unsigned y = 0; y < height; ++y) {
                        const unsigned char *row = yuv[plane].data + (y + y0) * yuv[plane].stride + x0;
                        for (unsigned x = 0; x < width; ++x) hash = (hash ^ row[x]) * 16777619u;
                    }
                }
                ++frames;
                end = th_granule_time(decoder, granule);
            }
            eos |= packet.e_o_s != 0;
        }
        if (failed || status < 0) { failed = 1; break; }
    }
    failed |= ferror(in) || !eos || frames != 24 || headers != 3;
    if (expect_bad)
        fprintf(out, "%s %s\n", name, failed ? "rejected" : "UNEXPECTED SUCCESS");
    else
        fprintf(out, "%s %ux%u fmt=%d frames=%d yuv-fnv1a=%08x end=%.3f%s\n",
                name, info.pic_width, info.pic_height, info.pixel_fmt, frames, hash, end, failed ? " FAIL" : "");
    if (decoder) th_decode_free(decoder);
    if (setup) th_setup_free(setup);
    th_comment_clear(&comment); th_info_clear(&info);
    if (have_stream) ogg_stream_clear(&stream);
    ogg_sync_clear(&sync); fclose(in);
    return expect_bad ? !failed : failed;
}
int main(int argc, char **argv) {
#ifdef __PS4__
    const char *directory = "/app0", *output = "/data/theora-smoke.txt";
#else
    if (argc != 3) return 2;
    const char *directory = argv[1], *output = argv[2];
#endif
    FILE *out = fopen(output, "w");
    if (!out) return 2;
    const char *names[] = {"clip420.ogv", "clip422.ogv", "clip444.ogv", "truncated.ogv", "invalid.ogv"};
    int result = 0;
    for (unsigned i = 0; i < 5; ++i) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", directory, names[i]);
        result |= decode(path, names[i], out, i >= 3);
    }
    fprintf(out, "%s\n", result ? "FAIL" : "PASS");
    fclose(out); return result;
}
