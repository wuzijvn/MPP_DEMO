#include "00_ffmpeg_demo_common.hpp"

#include <string>

namespace {

struct Args {
    std::string input;
    bool verbose = false;
};

void print_usage(const char* prog) {
    printf("%s: stage04 demo01 open input and find stream\n", prog);
    printf("Usage: %s --input=PATH [--verbose]\n", prog);
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
        fprintf(stderr, "unknown arg: %s\n", argv[i]);
        return false;
    }

    if (args->input.empty()) {
        fprintf(stderr, "--input is required\n");
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace ffmpeg_stage04;

    Args args;
    if (!parse_args(argc, argv, &args)) {
        return 1;
    }

    AVFormatContext* fmt_ctx = nullptr;

    /*
     * 前置条件：输入路径可读，且 FFmpeg 支持对应容器 demuxer。
     * 调用后状态：成功时 fmt_ctx 打开并绑定输入格式上下文。
     */
    int ret = avformat_open_input(&fmt_ctx, args.input.c_str(), nullptr, nullptr);
    if (ret < 0) {
        fprintf(stderr, "avformat_open_input failed: %s\n", ff_err2str(ret).c_str());
        return 2;
    }

    /*
     * 作用：探测流信息（codec 参数、时基等），后续 decode 必需。
     * 驱动影子线：这一步仍是纯用户态，不触发 codec 硬件节点。
     */
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        fprintf(stderr, "avformat_find_stream_info failed: %s\n",
                ff_err2str(ret).c_str());
        safe_close_input(&fmt_ctx);
        return 3;
    }

    printf("[demo01] input=%s\n", args.input.c_str());
    print_stream_brief(fmt_ctx);

    const int v_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1,
                                          nullptr, 0);
    if (v_idx < 0) {
        printf("[demo01] no video stream found\n");
    } else {
        AVStream* st = fmt_ctx->streams[v_idx];
        printf("[demo01] best_video_stream=%d codec_id=%d %dx%d\n", v_idx,
               st->codecpar->codec_id, st->codecpar->width, st->codecpar->height);
    }

    if (args.verbose) {
        // av_dump_format 用于打印完整容器/流摘要，便于排查输入问题。
        av_dump_format(fmt_ctx, 0, args.input.c_str(), 0);
    }

    // 资源释放对称：open_input 对应 close_input。
    safe_close_input(&fmt_ctx);
    printf("[demo01] PASS\n");
    return 0;
}
