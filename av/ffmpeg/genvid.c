// SPDX-License-Identifier: MIT

//
// Generate .mp4 video from generated frames
//
// $ gcc -o generate_video generate_video.c -Wall -Wextra -Wpedantic -lavcodec -lavformat -lavutil -lswresample -lswscale -lm
//

#include <stdbool.h>
#include <stdint.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#if 0
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../thirdparty/stb/stb_image_write.h"
#endif

// 800x600 @ 24 fps for 4 seconds

#define FRAME_COUNT 24 * 4
#define FRAME_W 800
#define FRAME_H 600
#define FRAME_SZ (FRAME_W * FRAME_H)

#define VID_FILE "vid_from_frames.mp4"
#define VID_FPS 24
#define VID_DURATION (FRAME_COUNT / VID_FPS)

#define ASSERT(cond)                                                                    \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            fprintf(stderr, "Assert failed: %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
        }                                                                               \
    } while (0);

#if 0
// Prompt for y/n
static bool prompt_yn(const char* prompt) {
    while (1) {
        fprintf(stdout, "%s (y/n) ", prompt);
        fflush(stdout);
        switch (getchar()) {
            case 'y':
            case 'Y':
                return true;
            case 'n':
            case 'N':
                return false;
        }
    }
}
#endif

// Everything needed for encoding
typedef struct {
    // Output container
    AVFormatContext*    oc;
    AVOutputFormat*     of;
    // Encoder
    AVCodec*            c;
    AVCodecContext*     cc;
    // Video stream
    AVStream*           st;
    // Temporary frames + packets
    AVFrame*            fr_rgba;
    AVFrame*            fr;
    AVPacket*           pkt;
    // Frame pts
    int                 next_pts;
    int                 frame_num;
    // Input frames
    uint32_t*           frames;
    // Swscale context
    struct SwsContext*  swctx;
} output_ctx_t;

// Get video frame
static AVFrame* get_vid_frame(output_ctx_t* ctx) {
    if (av_compare_ts(ctx->next_pts, ctx->cc->time_base, VID_DURATION, (AVRational){ 1, 1 }) > 0) {
        return NULL;
    }

    if (av_frame_make_writable(ctx->fr) < 0) {
        ASSERT(0);
    }

    printf("scaling frame %d\n", ctx->next_pts);
    if (ctx->frame_num >= FRAME_COUNT) {
        return NULL;
    }
    uint32_t* frame = &ctx->frames[FRAME_SZ * ctx->frame_num];
    ++ctx->frame_num;

    sws_scale(ctx->swctx, (const uint8_t* const*)&frame, ctx->fr_rgba->linesize, 0, ctx->fr->height, ctx->fr->data, ctx->fr->linesize);

    ctx->fr->pts = ctx->next_pts++;

    return ctx->fr;
}

// Encode a frame
static bool encode_frame(output_ctx_t* ctx) {
    AVFrame* fr = get_vid_frame(ctx);

    int ret;
    ret = avcodec_send_frame(ctx->cc, fr);
    ASSERT(ret >= 0);

    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx->cc, ctx->pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else {
            ASSERT(ret >= 0);
        }

        av_packet_rescale_ts(ctx->pkt, ctx->cc->time_base, ctx->st->time_base);
        ctx->pkt->stream_index = ctx->st->index;

        ret = av_interleaved_write_frame(ctx->oc, ctx->pkt);
        ASSERT(ret >= 0);
    }

    return ret == AVERROR_EOF ? 1 : 0;
}

int main() {
    const size_t bufsz = 4 * FRAME_COUNT * FRAME_W * FRAME_H;
    printf("Creating %d frames (%luMB)\n", FRAME_COUNT , (long)(bufsz / 1024 / 1024));
    uint32_t* buf = malloc(bufsz);
    if (buf == NULL) {
        return EXIT_FAILURE;
    }

    // Draw white frames with black grid
    for (int i = 0; i < FRAME_COUNT; ++i) {
        uint32_t* frame = &buf[i * FRAME_SZ];
        memset(frame, 0xFFFFFFFF, 4 * FRAME_W * FRAME_H);
        for (int y = 0; y < FRAME_H; ++y) {
            for (int x = 0; x < FRAME_W; ++x) {
                if ((y + i) % FRAME_COUNT == 0 || (x + i) % FRAME_COUNT == 0) {
                    frame[y * FRAME_W + x] = 0xFF000000;
                }
            }
        }
    }

    // Write frames to disk
#if 0
    if (prompt_yn("Write frames to disk?")) {
        printf("Writing %d frames...\n", FRAME_COUNT);
        char fname[32];
        for (int i = 0; i < FRAME_COUNT; ++i) {
            printf("%d ", i + 1);
            fflush(stdout);
            snprintf(fname, sizeof(fname), "frame-%03d.png", i);
            uint32_t* frame = &buf[i * FRAME_SZ];
            stbi_write_bmp(fname, FRAME_W, FRAME_H, 4, frame);
        }
        printf("\n");
    }
#endif

    output_ctx_t out = { 0 };

    out.frames = buf;

    // Allocate container
    avformat_alloc_output_context2(&out.oc, NULL, NULL, VID_FILE);
    ASSERT(out.oc);

    // Container format
    out.of = (AVOutputFormat*)out.oc->oformat;

    // Find h264 encoder
    out.c = (AVCodec*)avcodec_find_encoder(AV_CODEC_ID_H264);
    ASSERT(out.c);

    // Set up encoder
    out.cc = avcodec_alloc_context3(out.c);
    ASSERT(out.cc);
    out.cc->codec_id = out.c->id;
    out.cc->bit_rate = 400000;
    out.cc->width = FRAME_W;
    out.cc->height = FRAME_H;
    out.cc->time_base = (AVRational){ 1, VID_FPS };
    out.cc->gop_size = 12;
    out.cc->pix_fmt = AV_PIX_FMT_YUV420P;
    if (out.of->flags & AVFMT_GLOBALHEADER) {
        out.cc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // Open encoder
    if (avcodec_open2(out.cc, out.c, NULL) < 0) {
        ASSERT(0 && "Failed to open video encoder");
    }

    // Add new video stream
    out.st = avformat_new_stream(out.oc, NULL);
    ASSERT(out.st);
    out.st->time_base = out.cc->time_base;
    out.st->id = out.oc->nb_streams - 1;

    // Copy stream params
    if (avcodec_parameters_from_context(out.st->codecpar, out.cc) < 0) {
        ASSERT(0 && "Failed to copy code parameters");
    }

    // Output frame
    out.fr = av_frame_alloc();
    ASSERT(out.fr);
    out.fr->format = out.cc->pix_fmt;
    out.fr->width = out.cc->width;
    out.fr->height = out.cc->height;
    if (av_frame_get_buffer(out.fr, 0) < 0) {
        ASSERT(0 && "Failed to allocate buffer for frame");
    }

    // Input frame
    out.fr_rgba = av_frame_alloc();
    ASSERT(out.fr_rgba);
    out.fr_rgba->format = AV_PIX_FMT_RGB32;
    out.fr_rgba->width = out.cc->width;
    out.fr_rgba->height = out.cc->height;
    if (av_frame_get_buffer(out.fr_rgba, 0) < 0) {
        ASSERT(0 && "Failed to allocate buffer for frame");
    }

    // Temp packet
    out.pkt = av_packet_alloc();
    ASSERT(out.pkt);

    // Create swscale context
    out.swctx = sws_getContext(out.cc->width, out.cc->height, out.fr_rgba->format,
                               out.cc->width, out.cc->height, out.fr->format,
                               SWS_BICUBIC, NULL, NULL, NULL);
    ASSERT(out.swctx);

    // Dump debug info
    av_dump_format(out.oc, 0, VID_FILE, 1);

    // Open output file
    if (!(out.of->flags & AVFMT_NOFILE)) {
        if (avio_open(&out.oc->pb, VID_FILE, AVIO_FLAG_WRITE) < 0) {
            ASSERT(0 && "Failed to open output file");
        }
    }

    // Write file header
    if (avformat_write_header(out.oc, NULL) < 0) {
        ASSERT(0 && "Failed to write file header");
    }

    // Encode frames
    bool encoding = true;
    while (encoding) {
        encoding = !encode_frame(&out);
    }

    // Write file trailer
    if (av_write_trailer(out.oc) < 0) {
        ASSERT(0 && "Failed to write file trailer");
    }

    // Clean up
    av_frame_free(&out.fr);
    av_packet_free(&out.pkt);
    avcodec_free_context(&out.cc);
    if (!(out.of->flags & AVFMT_NOFILE)) {
        avio_closep(&out.oc->pb);
    }
    avformat_free_context(out.oc);
}

