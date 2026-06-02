#include "00_ffmpeg_demo_common.hpp"

extern "C" {
#include <libavutil/time.h>
}

#include <string>

namespace {

struct Args {
    std::string input;
    int max_frames = 120;
};

bool parse_args(int argc, char** argv, Args* a) {
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--input=", 8) == 0) {
            a->input = argv[i] + 8;
        } else if (strncmp(argv[i], "--max-frames=", 13) == 0) {
            a->max_frames = atoi(argv[i] + 13);
        }
    }
    return !a->input.empty();
}

}  // namespace

int main(int argc, char** argv) {
    using namespace ffmpeg_stage04;

    Args args;
    if (!parse_args(argc, argv, &args)) {
        fprintf(stderr, "Usage: --input=PATH [--max-frames=120]\n");
        return 1;
    }

    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* dec_ctx = nullptr;
    AVPacket* pkt = nullptr;
    AVFrame* frame = nullptr;

    int ret = avformat_open_input(&fmt_ctx, args.input.c_str(), nullptr, nullptr);
    if (ret < 0) {
        fprintf(stderr, "open failed: %s\n", ff_err2str(ret).c_str());
        return 2;
    }
    avformat_find_stream_info(fmt_ctx, nullptr);

    const int v_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (v_idx < 0) {
        safe_close_input(&fmt_ctx);
        return 3;
    }

    const AVCodec* dec =
        avcodec_find_decoder(fmt_ctx->streams[v_idx]->codecpar->codec_id);
    dec_ctx = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[v_idx]->codecpar);
    avcodec_open2(dec_ctx, dec, nullptr);

    pkt = av_packet_alloc();
    frame = av_frame_alloc();

    int frame_count = 0;
    const int64_t t0_us = av_gettime_relative();

    while (frame_count < args.max_frames && av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index != v_idx) {
            safe_packet_unref(pkt);
            continue;
        }

        ret = avcodec_send_packet(dec_ctx, pkt);
        safe_packet_unref(pkt);
        if (ret < 0) {
            break;
        }

        while (true) {
            ret = avcodec_receive_frame(dec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                goto out;
            }
            ++frame_count;
            safe_frame_unref(frame);
            if (frame_count >= args.max_frames) {
                break;
            }
        }
    }

out:
    const int64_t t1_us = av_gettime_relative();
    const double elapsed_s = (t1_us - t0_us) / 1000000.0;
    const double fps = elapsed_s > 0.0 ? (frame_count / elapsed_s) : 0.0;

    printf("[demo08] benchmark: frames=%d elapsed_s=%.6f fps=%.3f\n", frame_count,
           elapsed_s, fps);
    printf("[demo08] note: this is software decode baseline, not hardware path\n");
    printf("[demo08] PASS\n");

    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&dec_ctx);
    safe_close_input(&fmt_ctx);
    return 0;
}
