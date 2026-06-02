#include "00_ffmpeg_demo_common.hpp"

#include <string>

namespace {

struct Args {
    std::string input;
    int max_frames = 10;
    bool verbose = false;
};

void print_usage(const char* prog) {
    printf("%s: stage04 demo03 decode packet to frame\n", prog);
    printf("Usage: %s --input=PATH [--max-frames=10] [--verbose]\n", prog);
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
        if (strncmp(argv[i], "--max-frames=", 13) == 0) {
            args->max_frames = atoi(argv[i] + 13);
            continue;
        }
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
        fprintf(stderr, "find stream info failed: %s\n", ff_err2str(ret).c_str());
        safe_close_input(&fmt_ctx);
        return 3;
    }

    const int video_stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1,
                                                 nullptr, 0);
    if (video_stream < 0) {
        fprintf(stderr, "no video stream found\n");
        safe_close_input(&fmt_ctx);
        return 4;
    }

    AVStream* v_st = fmt_ctx->streams[video_stream];
    const AVCodec* dec = avcodec_find_decoder(v_st->codecpar->codec_id);
    if (dec == nullptr) {
        fprintf(stderr, "decoder not found for codec_id=%d\n", v_st->codecpar->codec_id);
        safe_close_input(&fmt_ctx);
        return 5;
    }

    dec_ctx = avcodec_alloc_context3(dec);
    if (dec_ctx == nullptr) {
        safe_close_input(&fmt_ctx);
        return 6;
    }

    ret = avcodec_parameters_to_context(dec_ctx, v_st->codecpar);
    if (ret < 0) {
        fprintf(stderr, "parameters_to_context failed: %s\n", ff_err2str(ret).c_str());
        avcodec_free_context(&dec_ctx);
        safe_close_input(&fmt_ctx);
        return 7;
    }

    ret = avcodec_open2(dec_ctx, dec, nullptr);
    if (ret < 0) {
        fprintf(stderr, "avcodec_open2 failed: %s\n", ff_err2str(ret).c_str());
        avcodec_free_context(&dec_ctx);
        safe_close_input(&fmt_ctx);
        return 8;
    }

    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    if (pkt == nullptr || frame == nullptr) {
        av_frame_free(&frame);
        av_packet_free(&pkt);
        avcodec_free_context(&dec_ctx);
        safe_close_input(&fmt_ctx);
        return 9;
    }

    int out_frames = 0;

    while (out_frames < args.max_frames) {
        ret = av_read_frame(fmt_ctx, pkt);
        if (ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            fprintf(stderr, "av_read_frame failed: %s\n", ff_err2str(ret).c_str());
            break;
        }

        if (pkt->stream_index != video_stream) {
            safe_packet_unref(pkt);
            continue;
        }

        /*
         * send/receive 模型：
         * - send_packet 把压缩包送入 decoder 内部队列；
         * - receive_frame 从 decoder 取原始帧。
         *
         * 所有权关键点：send 后即可 unref packet。
         */
        ret = avcodec_send_packet(dec_ctx, pkt);
        safe_packet_unref(pkt);
        if (ret < 0) {
            fprintf(stderr, "avcodec_send_packet failed: %s\n", ff_err2str(ret).c_str());
            break;
        }

        while (ret >= 0) {
            ret = avcodec_receive_frame(dec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                fprintf(stderr, "avcodec_receive_frame failed: %s\n",
                        ff_err2str(ret).c_str());
                goto cleanup;
            }

            printf("[demo03] frame#%d w=%d h=%d format=%d pts=%lld key=%d\n", out_frames,
                   frame->width, frame->height, frame->format,
                   static_cast<long long>(frame->pts), frame->key_frame);

            if (args.verbose) {
                printf("[demo03] linesize: [%d,%d,%d]\n", frame->linesize[0],
                       frame->linesize[1], frame->linesize[2]);
            }

            // frame 处理完立即 unref，释放底层图像 buffer 引用。
            safe_frame_unref(frame);
            ++out_frames;
            if (out_frames >= args.max_frames) {
                break;
            }
        }
    }

cleanup:
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&dec_ctx);
    safe_close_input(&fmt_ctx);
    printf("[demo03] summary: out_frames=%d\n", out_frames);
    printf("[demo03] PASS\n");
    return 0;
}
