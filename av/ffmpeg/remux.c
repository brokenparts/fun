// SPDX-License-Identifier: MIT

//
// Remux media from one container to another
//
// $ gcc -o remux remux.c -Wall -Wextra -Wpedantic -lavformat -lavcodec lavutil -lm
//

#include <stdio.h>

#include <libavformat/avformat.h>

#define LOG(...) printf(__VA_ARGS__); fflush(stdout)
#define ERROR(...) LOG(__VA_ARGS__); exit(1)
#define ASSERT(cond) if (!(cond)) { ERROR("ASSERT FAILED %s:%d %s\n", __FILE__, __LINE__, #cond); }
#define AVCALL(code)    {                                           \
    int avcall__ret = code;                                         \
    if (avcall__ret < 0) {                                          \
        char avcall__err[255];                                      \
        av_strerror(avcall__ret, avcall__err, sizeof(avcall__err)); \
        LOG("AVCALL %s returned %s\n", #code, avcall__err);         \
        exit(1);                                                    \
    }                                                               \
}

typedef struct {
    const char*         path;
    AVFormatContext*    container;
} media_container_t;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        ERROR("Usage: %s <input> <output>\n", argv[0]);
    }

    media_container_t inp = { 0 }; inp.path = argv[1];
    media_container_t out = { 0 }; out.path = argv[2];

    // Open input
    AVCALL(avformat_open_input(&inp.container, inp.path, NULL, NULL));
    AVCALL(avformat_find_stream_info(inp.container, NULL));

    // Open output
    AVCALL(avformat_alloc_output_context2(&out.container, NULL, NULL, out.path));
    AVCALL(avio_open(&out.container->pb, out.path, AVIO_FLAG_WRITE));

    // Copy media stream layout
    for (size_t i = 0; i < inp.container->nb_streams; ++i) {
        AVStream* inp_stream = inp.container->streams[i];
        AVStream* out_stream = avformat_new_stream(out.container, NULL); ASSERT(out_stream);
        // Copy parameters from input stream
        AVCALL(avcodec_parameters_copy(out_stream->codecpar, inp_stream->codecpar));
        out_stream->time_base = inp_stream->time_base;
    }

    // Dump input and output formats
    av_dump_format(inp.container, 0, inp.path, 0);
    av_dump_format(out.container, 0, out.path, 1);

    // Write output file header
    AVCALL(avformat_write_header(out.container, NULL));

    // Transfer packets
    AVPacket* pkt = av_packet_alloc(); ASSERT(pkt);
    while (1) {
        // Read
        int ret = av_read_frame(inp.container, pkt);
        if (ret == AVERROR_EOF) {
            break;
        }
        AVCALL(ret);

#if 0
        // This would need to be done if the input and output streams had a different
        // time_base value
        AVStream* inp_stream = inp.container->streams[pkt->stream_index];
        AVStream* out_stream = out.container->streams[pkt->stream_index];
        av_packet_rescale_ts(pkt, inp_stream->time_base, out_stream->time_base);
#endif

        // Write
        AVCALL(av_interleaved_write_frame(out.container, pkt));
        
        av_packet_unref(pkt);
    }

    // Write output file trailer
    AVCALL(av_write_trailer(out.container));

    // Clean up
    av_packet_free(&pkt);
    avio_closep(&out.container->pb);
    avformat_free_context(out.container);
    avformat_close_input(&inp.container);
}
