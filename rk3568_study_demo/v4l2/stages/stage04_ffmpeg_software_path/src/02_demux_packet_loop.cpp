#include "00_ffmpeg_demo_common.hpp"

#include <string>

namespace {

struct Args {
    std::string input;
    int max_packets = 30;
    bool verbose = false;
};

void print_usage(const char* prog) {
    printf("%s: stage04 demo02 demux packet loop\n", prog);
    printf("Usage: %s --input=PATH [--max-packets=30] [--verbose]\n", prog);
}

bool parse_args(int argc, char** argv, Args* args) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return false;
        }
        if (strcmp(argv[i], "--verbose") == 0) {
            args->verbose = true;
            continue;
        }
        if (strncmp(argv[i], "--input=", 8) == 0) {
            args->input = argv[i] + 8;
            continue;
        }
        if (strncmp(argv[i], "--max-packets=", 14) == 0) {
            args->max_packets = atoi(argv[i] + 14);
            continue;
        }
        fprintf(stderr, "unknown arg: %s\n", argv[i]);
        return false;
    }
    return !args->input.empty();
}

}  // namespace

int main(int argc, char** argv) {
    using namespace ffmpeg_stage04;

    Args args;
    if (!parse_args(argc, argv, &args)) {
        return 1;
    }

    AVFormatContext* fmt_ctx = nullptr;
    int ret = avformat_open_input(&fmt_ctx, args.input.c_str(), nullptr, nullptr);
    if (ret < 0) {
        fprintf(stderr, "open input failed: %s\n", ff_err2str(ret).c_str());
        return 2;
    }
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        fprintf(stderr, "find stream info failed: %s\n", ff_err2str(ret).c_str());
        safe_close_input(&fmt_ctx);
        return 3;
    }

    AVPacket* pkt = av_packet_alloc();
    if (pkt == nullptr) {
        safe_close_input(&fmt_ctx);
        return 4;
    }

    int read_count = 0;
    int v_count = 0;
    int a_count = 0;

    while (read_count < args.max_packets) {
        /*
         * av_read_frame 前置条件：fmt_ctx 有效。
         * 成功后：pkt 持有一包压缩数据（当前函数持有引用）。
         * 约束：每轮必须 av_packet_unref，避免 packet 引用泄漏。
         */
        ret = av_read_frame(fmt_ctx, pkt);
        if (ret == AVERROR_EOF) {
            printf("[demo02] EOF reached\n");
            break;
        }
        if (ret < 0) {
            fprintf(stderr, "av_read_frame failed: %s\n", ff_err2str(ret).c_str());
            break;
        }

        const AVStream* st = fmt_ctx->streams[pkt->stream_index];
        if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            ++v_count;
        } else if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            ++a_count;
        }

        if (args.verbose) {
            printf("[demo02] packet#%d stream=%d size=%d pts=%lld dts=%lld flags=0x%x\n",
                   read_count, pkt->stream_index, pkt->size,
                   static_cast<long long>(pkt->pts), static_cast<long long>(pkt->dts),
                   pkt->flags);
        }

        safe_packet_unref(pkt);
        ++read_count;
    }

    printf("[demo02] summary: read_count=%d video_packets=%d audio_packets=%d\n",
           read_count, v_count, a_count);

    av_packet_free(&pkt);
    safe_close_input(&fmt_ctx);
    printf("[demo02] PASS\n");
    return 0;
}
