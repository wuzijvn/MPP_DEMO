#include "00_ffmpeg_demo_common.hpp"

#include <string>

namespace {

struct Args {
    std::string input;
    int max_packets = 20;
};

bool parse_args(int argc, char** argv, Args* a) {
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--input=", 8) == 0) {
            a->input = argv[i] + 8;
        } else if (strncmp(argv[i], "--max-packets=", 14) == 0) {
            a->max_packets = atoi(argv[i] + 14);
        }
    }
    return !a->input.empty();
}

/*
 * 把时间戳按流时基换算到秒：
 * seconds = ts * av_q2d(time_base)
 */
double to_seconds(int64_t ts, AVRational tb) {
    if (ts == AV_NOPTS_VALUE) {
        return -1.0;
    }
    return ts * av_q2d(tb);
}

}  // namespace

int main(int argc, char** argv) {
    using namespace ffmpeg_stage04;

    Args args;
    if (!parse_args(argc, argv, &args)) {
        fprintf(stderr, "Usage: --input=PATH [--max-packets=20]\n");
        return 1;
    }

    AVFormatContext* fmt_ctx = nullptr;
    AVPacket* pkt = nullptr;

    int ret = avformat_open_input(&fmt_ctx, args.input.c_str(), nullptr, nullptr);
    if (ret < 0) {
        fprintf(stderr, "open failed: %s\n", ff_err2str(ret).c_str());
        return 2;
    }
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        safe_close_input(&fmt_ctx);
        return 3;
    }

    const int v_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (v_idx < 0) {
        safe_close_input(&fmt_ctx);
        return 4;
    }

    AVStream* v_st = fmt_ctx->streams[v_idx];
    const AVRational tb = v_st->time_base;

    pkt = av_packet_alloc();
    if (pkt == nullptr) {
        safe_close_input(&fmt_ctx);
        return 5;
    }

    int shown = 0;
    while (shown < args.max_packets && av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index != v_idx) {
            safe_packet_unref(pkt);
            continue;
        }

        const double pts_s = to_seconds(pkt->pts, tb);
        const double dts_s = to_seconds(pkt->dts, tb);

        printf("[demo05] pkt#%d pts=%lld(%.6fs) dts=%lld(%.6fs) duration=%lld tb=%d/%d\n",
               shown, static_cast<long long>(pkt->pts), pts_s,
               static_cast<long long>(pkt->dts), dts_s,
               static_cast<long long>(pkt->duration), tb.num, tb.den);

        safe_packet_unref(pkt);
        ++shown;
    }

    av_packet_free(&pkt);
    safe_close_input(&fmt_ctx);
    printf("[demo05] PASS\n");
    return 0;
}
