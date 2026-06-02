#include "./include/00_ffmpeg_demo_common.hpp"

#include <string>

namespace {

struct Args {
    std::string input;
    std::string output = "./logs/demo06_first_frame.yuv";
};

bool parse_args(int argc, char** argv, Args* a) {
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--input=", 8) == 0) {
            a->input = argv[i] + 8;
        } else if (strncmp(argv[i], "--output=", 9) == 0) {
            a->output = argv[i] + 9;
        }
    }
    return !a->input.empty();
}

/*
 * 按 linesize 写单个平面。
 * 前置条件：base 指向有效 plane 数据，linesize >= w。
 */
bool save_plane(FILE* fp, const uint8_t* base, int linesize, int w, int h) {
    for (int y = 0; y < h; ++y) {
        if (fwrite(base + y * linesize, 1, w, fp) != static_cast<size_t>(w)) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace ffmpeg_stage04;

    Args args;
    if (!parse_args(argc, argv, &args)) {
        fprintf(stderr, "Usage: --input=PATH [--output=PATH]\n");
        return 1;
    }

    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* dec_ctx = nullptr;
    AVPacket* pkt = nullptr;
    AVFrame* frame = nullptr;

    int ret = avformat_open_input(&fmt_ctx, args.input.c_str(), nullptr, nullptr);
    if (ret < 0) {
        return 2;
    }
    avformat_find_stream_info(fmt_ctx, nullptr);

    const int v_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (v_idx < 0) {
        safe_close_input(&fmt_ctx);
        return 3;
    }

    const AVCodec* dec = avcodec_find_decoder(fmt_ctx->streams[v_idx]->codecpar->codec_id);
    dec_ctx = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[v_idx]->codecpar);
    avcodec_open2(dec_ctx, dec, nullptr);

    pkt = av_packet_alloc();
    frame = av_frame_alloc();

    bool saved = false;
    while (!saved && av_read_frame(fmt_ctx, pkt) >= 0) {
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

            if (frame->format != AV_PIX_FMT_YUV420P) {
                printf("[demo06] note: frame format=%d not YUV420P, skip save in this demo\n",
                       frame->format);
                safe_frame_unref(frame);
                continue;
            }

            FILE* fp = fopen(args.output.c_str(), "wb");
            if (fp == nullptr) {
                perror("fopen output");
                goto out;
            }

            const int w = frame->width;
            const int h = frame->height;
            bool ok = true;
            ok &= save_plane(fp, frame->data[0], frame->linesize[0], w, h);
            ok &= save_plane(fp, frame->data[1], frame->linesize[1], w / 2, h / 2);
            ok &= save_plane(fp, frame->data[2], frame->linesize[2], w / 2, h / 2);
            fclose(fp);

            if (!ok) {
                fprintf(stderr, "write yuv failed\n");
                goto out;
            }

            printf("[demo06] saved first YUV420P frame to %s (%dx%d)\n", args.output.c_str(),
                   w, h);
            saved = true;
            safe_frame_unref(frame);
            break;
        }
    }

out:
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&dec_ctx);
    safe_close_input(&fmt_ctx);
    printf("[demo06] %s\n", saved ? "PASS" : "FAIL(no suitable frame)");
    return saved ? 0 : 4;
}
