#include "./include/00_ffmpeg_demo_common.hpp"

#include <string>

namespace {

struct Args {
    std::string input;
    int max_frames = 6;
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
        fprintf(stderr, "Usage: --input=PATH [--max-frames=6]\n");
        return 1;
    }

    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* dec_ctx = nullptr;
    AVPacket* pkt = nullptr;
    AVFrame* frame = nullptr;

    int ret = avformat_open_input(&fmt_ctx, args.input.c_str(), nullptr, nullptr);
    if (ret < 0) {
        fprintf(stderr, "open input failed: %s\n", ff_err2str(ret).c_str());
        return 2;
    }
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        fprintf(stderr, "find stream info failed\n");
        safe_close_input(&fmt_ctx);
        return 3;
    }

    const int v_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (v_idx < 0) {
        safe_close_input(&fmt_ctx);
        return 4;
    }

    const AVCodec* dec =
        avcodec_find_decoder(fmt_ctx->streams[v_idx]->codecpar->codec_id);
    dec_ctx = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[v_idx]->codecpar);
    avcodec_open2(dec_ctx, dec, nullptr);

    pkt = av_packet_alloc();
    frame = av_frame_alloc();

    int frame_count = 0;
    while (frame_count < args.max_frames && av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index != v_idx) {
            safe_packet_unref(pkt);
            continue;
        }

        printf("[demo04] packet ownership: demux -> pkt(size=%d) -> decoder\n", pkt->size);
        ret = avcodec_send_packet(dec_ctx, pkt);

        /*
         * 关键点：无论 send 成功失败，本轮 packet 引用都要释放。
         * 否则会出现 packet 引用泄漏，长跑时内存上涨。
         */
        safe_packet_unref(pkt);

        if (ret < 0) {
            fprintf(stderr, "send_packet failed: %s\n", ff_err2str(ret).c_str());
            break;
        }

        while (true) {
            ret = avcodec_receive_frame(dec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                fprintf(stderr, "receive_frame failed: %s\n", ff_err2str(ret).c_str());
                goto out;
            }

            printf("[demo04] frame ownership: decoder -> frame(w=%d h=%d fmt=%d) -> app\n",
                   frame->width, frame->height, frame->format);

            /*
             * 关键点：处理完成立即 unref，释放 frame 对底层 buffer 的引用。
             */
            safe_frame_unref(frame);
            ++frame_count;
            if (frame_count >= args.max_frames) {
                break;
            }
        }
    }

out:
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&dec_ctx);
    safe_close_input(&fmt_ctx);
    printf("[demo04] summary: frame_count=%d\n", frame_count);
    printf("[demo04] PASS\n");
    return 0;
}
