#include "00_ffmpeg_demo_common.hpp"

#include <string>

namespace {

struct Args {
    std::string input;
    int inject_step = 0;
};

bool parse_args(int argc, char** argv, Args* a) {
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--input=", 8) == 0) {
            a->input = argv[i] + 8;
        } else if (strncmp(argv[i], "--inject-step=", 14) == 0) {
            a->inject_step = atoi(argv[i] + 14);
        }
    }
    return !a->input.empty();
}

}  // namespace

int main(int argc, char** argv) {
    using namespace ffmpeg_stage04;

    Args args;
    if (!parse_args(argc, argv, &args)) {
        fprintf(stderr, "Usage: --input=PATH [--inject-step=N]\n");
        return 1;
    }

    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* dec_ctx = nullptr;
    AVPacket* pkt = nullptr;
    AVFrame* frame = nullptr;

    int ret = 0;

    ret = avformat_open_input(&fmt_ctx, args.input.c_str(), nullptr, nullptr);
    if (ret < 0) {
        fprintf(stderr, "step1 open failed\n");
        goto fail;
    }
    printf("[demo07] step1 open ok\n");
    if (args.inject_step == 1) {
        fprintf(stderr, "[demo07] inject fail at step1\n");
        goto fail;
    }

    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        fprintf(stderr, "step2 find_stream_info failed\n");
        goto fail;
    }
    printf("[demo07] step2 stream info ok\n");
    if (args.inject_step == 2) {
        fprintf(stderr, "[demo07] inject fail at step2\n");
        goto fail;
    }

    {
        const int v_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1,
                                              nullptr, 0);
        if (v_idx < 0) {
            fprintf(stderr, "step3 no video stream\n");
            goto fail;
        }
        const AVCodec* dec =
            avcodec_find_decoder(fmt_ctx->streams[v_idx]->codecpar->codec_id);
        dec_ctx = avcodec_alloc_context3(dec);
        if (dec_ctx == nullptr) {
            goto fail;
        }
        avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[v_idx]->codecpar);
        ret = avcodec_open2(dec_ctx, dec, nullptr);
        if (ret < 0) {
            fprintf(stderr, "step3 open decoder failed\n");
            goto fail;
        }
    }
    printf("[demo07] step3 decoder open ok\n");
    if (args.inject_step == 3) {
        fprintf(stderr, "[demo07] inject fail at step3\n");
        goto fail;
    }

    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    if (pkt == nullptr || frame == nullptr) {
        goto fail;
    }

    printf("[demo07] step4 packet/frame alloc ok\n");
    if (args.inject_step == 4) {
        fprintf(stderr, "[demo07] inject fail at step4\n");
        goto fail;
    }

    printf("[demo07] PASS\n");
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&dec_ctx);
    safe_close_input(&fmt_ctx);
    return 0;

fail:
    /*
     * 关键教学点：goto 清理路径保证资源释放对称，避免多分支重复释放逻辑。
     * 驱动影子线：用户态泄漏会放大“疑似驱动问题”的噪音，先确保用户态释放对称。
     */
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&dec_ctx);
    safe_close_input(&fmt_ctx);
    printf("[demo07] FAIL path cleanup done\n");
    return 2;
}
