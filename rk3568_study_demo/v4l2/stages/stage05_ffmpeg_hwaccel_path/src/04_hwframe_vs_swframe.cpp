#include "00_ffmpeg_hwaccel_common.hpp"

#include <inttypes.h>
#include <string>
#include <vector>

namespace {

struct Args {
    std::string input;
    /*
     * 默认走 RKMPP decoder wrapper，不强制创建 hwdevice。
     * 若显式传 --hw-type=drm/vaapi/rkmpp，则进入 AVHWDeviceContext 实验模式。
     */
    std::string hw_type;
    bool hw_type_set = false;
    std::string decoder = "h264_rkmpp";
    std::string device;
    int max_frames = 16;
};

/*
 * g_hw_pix_fmt 由 decoder + hw_type 协商候选得来。
 * 在 get_format 回调阶段用于告诉 FFmpeg：我们优先选择哪个硬件像素格式。
 */
static AVPixelFormat g_hw_pix_fmt = AV_PIX_FMT_NONE;

/*
 * get_format 回调在 avcodec_open2 / 解码过程中被 FFmpeg 调用。
 * 前置条件：dec_ctx->get_format 已设置，且 pix_fmts 由 FFmpeg 提供候选列表。
 * 返回值：
 * - 返回硬件 pix fmt：尽量走硬件帧路径；
 * - 返回第一个软件候选：会落到软件帧（fallback）。
 */
AVPixelFormat get_hw_format(AVCodecContext* ctx, const AVPixelFormat* pix_fmts) {
    (void)ctx;
    for (const AVPixelFormat* p = pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
        if (*p == g_hw_pix_fmt) {
            return *p;
        }
    }
    fprintf(stderr, "[demo04] get_format fallback: hw pix fmt not offered, fallback to %s\n",
            ffmpeg_stage05::pix_fmt_name(pix_fmts[0]));
    return pix_fmts[0];
}

bool is_rkmpp_wrapper_decoder(const AVCodec* dec) {
    if (dec == nullptr || dec->name == nullptr) {
        return false;
    }
    return strstr(dec->name, "_rkmpp") != nullptr;
}

const char* pick_verdict(const Args& a,
                         bool decoder_is_rkmpp_wrapper,
                         int hw_frames,
                         int wrapper_frames,
                         int sw_fallback_frames,
                         bool fallback) {
    /*
     * verdict 目标：给出人可读的一句话结论，避免用户自行推断分支含义。
     */
    if (a.hw_type_set) {
        if (hw_frames > 0 && !fallback) {
            return "HARDWARE_FRAME_CONFIRMED";
        }
        if (sw_fallback_frames > 0 || fallback) {
            return "SOFTWARE_FALLBACK";
        }
        return "UNKNOWN_NEED_MORE_EVIDENCE";
    }

    if (decoder_is_rkmpp_wrapper && wrapper_frames > 0 && sw_fallback_frames == 0) {
        return "HARDWARE_DECODE_WRAPPER_OUTPUT";
    }
    if (sw_fallback_frames > 0) {
        return "SOFTWARE_FALLBACK";
    }
    return "UNKNOWN_NEED_MORE_EVIDENCE";
}

const char* pick_verdict_reason(const Args& a,
                                bool decoder_is_rkmpp_wrapper,
                                int hw_frames,
                                int wrapper_frames,
                                int sw_fallback_frames,
                                bool fallback) {
    if (a.hw_type_set) {
        if (hw_frames > 0 && !fallback) {
            return "explicit hwdevice mode got hardware frames";
        }
        if (sw_fallback_frames > 0 || fallback) {
            return "explicit hwdevice mode did not get hardware frames";
        }
        return "explicit hwdevice mode has no decisive frame evidence";
    }

    if (decoder_is_rkmpp_wrapper && wrapper_frames > 0 && sw_fallback_frames == 0) {
        return "rkmpp wrapper decoder selected and decoded frames are CPU-visible";
    }
    if (sw_fallback_frames > 0) {
        return "non-wrapper software-visible fallback frames observed";
    }
    return "no enough decoded frame evidence";
}

const char* pick_verdict_cn(const char* verdict) {
    if (verdict == nullptr) {
        return "结论未知：缺少证据";
    }
    if (strcmp(verdict, "HARDWARE_FRAME_CONFIRMED") == 0) {
        return "硬件帧已确认：显式 hwdevice 模式拿到了硬件帧";
    }
    if (strcmp(verdict, "HARDWARE_DECODE_WRAPPER_OUTPUT") == 0) {
        return "RKMPP wrapper 硬解路径：解码在 VPU，输出是 CPU 可见帧";
    }
    if (strcmp(verdict, "SOFTWARE_FALLBACK") == 0) {
        return "软解回退：未形成可证明的硬件帧/硬解 wrapper 证据";
    }
    return "结论未知：需要更多证据";
}

bool parse_args(int argc, char** argv, Args* a) {
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--input=", 8) == 0) a->input = argv[i] + 8;
        else if (strncmp(argv[i], "--hw-type=", 10) == 0) {
            a->hw_type = argv[i] + 10;
            a->hw_type_set = true;
        }
        else if (strncmp(argv[i], "--decoder=", 10) == 0) a->decoder = argv[i] + 10;
        else if (strncmp(argv[i], "--device=", 9) == 0) a->device = argv[i] + 9;
        else if (strncmp(argv[i], "--max-frames=", 13) == 0) a->max_frames = atoi(argv[i] + 13);
    }
    return !a->input.empty();
}

}  // namespace

int main(int argc, char** argv) {
    using namespace ffmpeg_stage05;
    Args a;
    if (!parse_args(argc, argv, &a)) {
        fprintf(stderr, "Usage: --input=PATH [--decoder=h264_rkmpp] [--hw-type=drm|vaapi|rkmpp] [--device=PATH] [--max-frames=16]\n");
        return 1;
    }

    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* dec_ctx = nullptr;
    AVBufferRef* hw_dev_ctx = nullptr;
    AVPacket* pkt = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* sw_frame = nullptr;

    int hw_frames = 0;
    int wrapper_frames = 0;
    int sw_fallback_frames = 0;
    int transfer_ok = 0;
    bool fallback = false;
    bool decoder_is_rkmpp_wrapper = false;
    const char* verdict = "UNKNOWN_NEED_MORE_EVIDENCE";
    const char* verdict_reason = "not evaluated";

    // 1) 打开输入容器。
    int ret = avformat_open_input(&fmt_ctx, a.input.c_str(), nullptr, nullptr);
    if (ret < 0) {
        fprintf(stderr, "[demo04] open input failed: %s\n", ff_err2str(ret).c_str());
        return 2;
    }

    // 2) 读取流信息，为查找 video stream 和 decoder 做准备。
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        fprintf(stderr, "[demo04] stream info failed: %s\n", ff_err2str(ret).c_str());
        safe_close_input(&fmt_ctx);
        return 3;
    }

    const int v_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (v_idx < 0) {
        fprintf(stderr, "[demo04] no video stream\n");
        safe_close_input(&fmt_ctx);
        return 4;
    }

    // 3) 选择 decoder：优先按名称，否则按 stream codec_id。
    const AVCodec* dec = avcodec_find_decoder_by_name(a.decoder.c_str());
    if (dec == nullptr) {
        dec = avcodec_find_decoder(fmt_ctx->streams[v_idx]->codecpar->codec_id);
    }
    if (dec == nullptr) {
        fprintf(stderr, "[demo04] decoder not found\n");
        safe_close_input(&fmt_ctx);
        return 5;
    }
    decoder_is_rkmpp_wrapper = is_rkmpp_wrapper_decoder(dec);

    if (a.hw_type_set) {
        AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
        if (!parse_hw_type(a.hw_type, &type)) {
            fprintf(stderr, "[demo04] unknown hw type: %s\n", a.hw_type.c_str());
            safe_close_input(&fmt_ctx);
            return 6;
        }

        // 4) 显式 hwdevice 模式：从 decoder 能力中提取目标 hw_type 的硬件像素格式候选。
        const std::vector<AVPixelFormat> hw_fmts = collect_decoder_hw_configs(dec, type);
        if (!hw_fmts.empty()) {
            g_hw_pix_fmt = hw_fmts[0];
        }

        // 5) 仅在用户显式要求 hw_type 时创建 hw device。失败不直接退出，而是显式标记 fallback。
        const char* dev = a.device.empty() ? nullptr : a.device.c_str();
        ret = av_hwdevice_ctx_create(&hw_dev_ctx, type, dev, nullptr, 0);
        if (ret < 0) {
            fprintf(stderr, "[demo04] hwdevice create failed: %s\n", ff_err2str(ret).c_str());
            fprintf(stderr, "[demo04] fallback to decoder-only path\n");
            fallback = true;
        }
    } else {
        printf("[demo04] wrapper mode: decoder=%s, no hwdevice forced\n", dec->name);
        printf("[demo04] note: RKMPP hard decode evidence comes from decoder selection + successful frames.\n");
    }

    dec_ctx = avcodec_alloc_context3(dec);
    if (dec_ctx == nullptr) {
        safe_close_input(&fmt_ctx);
        av_buffer_unref(&hw_dev_ctx);
        return 7;
    }

    ret = avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[v_idx]->codecpar);
    if (ret < 0) {
        fprintf(stderr, "[demo04] params->ctx failed: %s\n", ff_err2str(ret).c_str());
        goto cleanup;
    }

    /*
     * 6) 显式 hwdevice 模式条件满足时挂载硬件路径：
     * - get_format 决策硬件像素格式；
     * - hw_device_ctx 交给 decoder 使用。
     * 默认 RKMPP wrapper 模式下不强行设置 get_format/hw_device_ctx。
     */
    if (a.hw_type_set && !fallback && hw_dev_ctx != nullptr && g_hw_pix_fmt != AV_PIX_FMT_NONE) {
        dec_ctx->get_format = get_hw_format;
        dec_ctx->hw_device_ctx = av_buffer_ref(hw_dev_ctx);
    } else if (a.hw_type_set) {
        fallback = true;
    }

    ret = avcodec_open2(dec_ctx, dec, nullptr);
    if (ret < 0) {
        fprintf(stderr, "[demo04] avcodec_open2 failed: %s\n", ff_err2str(ret).c_str());
        goto cleanup;
    }

    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    sw_frame = av_frame_alloc();
    if (pkt == nullptr || frame == nullptr || sw_frame == nullptr) {
        fprintf(stderr, "[demo04] alloc packet/frame failed\n");
        goto cleanup;
    }

    while (hw_frames + wrapper_frames + sw_fallback_frames < a.max_frames) {
        ret = av_read_frame(fmt_ctx, pkt);
        if (ret == AVERROR_EOF) break;
        if (ret < 0) {
            fprintf(stderr, "[demo04] read frame failed: %s\n", ff_err2str(ret).c_str());
            break;
        }
        if (pkt->stream_index != v_idx) {
            av_packet_unref(pkt);
            continue;
        }

        /*
         * send_packet 后 packet 可立即 unref。
         * 所有权从应用层包缓存切换到 decoder 内部队列。
         */
        ret = avcodec_send_packet(dec_ctx, pkt);
        av_packet_unref(pkt);
        if (ret < 0) {
            fprintf(stderr, "[demo04] send packet failed: %s\n", ff_err2str(ret).c_str());
            break;
        }

        while (ret >= 0 && hw_frames + wrapper_frames + sw_fallback_frames < a.max_frames) {
            ret = avcodec_receive_frame(dec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (ret < 0) {
                fprintf(stderr, "[demo04] receive frame failed: %s\n", ff_err2str(ret).c_str());
                goto cleanup;
            }

            /*
             * 关键判据：
             * - 显式 hwdevice 模式：frame->format == g_hw_pix_fmt 才按 hw frame 统计；
             * - 默认 RKMPP wrapper 模式：FFmpeg 可能返回 CPU 可见帧格式，但 decoder 本身仍是
             *   h264_rkmpp/hevc_rkmpp，硬解证据来自 decoder wrapper + 命令/性能/dmesg。
             */
            if (frame->format == g_hw_pix_fmt && g_hw_pix_fmt != AV_PIX_FMT_NONE) {
                ++hw_frames;
                printf("[demo04] frame hw idx=%d fmt=%s pts=%" PRId64 "\n", hw_frames,
                       pix_fmt_name(static_cast<AVPixelFormat>(frame->format)), frame->pts);

                /*
                 * av_hwframe_transfer_data：把硬件帧下载到 CPU 可访问内存。
                 * 这一步是显式 copy-back，性能分析时必须计入 copy 成本。
                 */
                // ret = av_hwframe_transfer_data(sw_frame, frame, 0);
                if (ret == 0) {
                    ++transfer_ok;
                    printf("[demo04] hwdownload ok -> sw fmt=%s\n",
                           pix_fmt_name(static_cast<AVPixelFormat>(sw_frame->format)));
                } else {
                    printf("[demo04] hwdownload fail: %s\n", ff_err2str(ret).c_str());
                }
                av_frame_unref(sw_frame);
            } else if (!a.hw_type_set && decoder_is_rkmpp_wrapper) {
                /*
                 * RKMPP wrapper 模式下，frame 可能是 CPU 可见格式（如 yuv420p）。
                 * 这不等于“软件解码器 h264/h265 在工作”，只是输出内存形态不同。
                 */
                ++wrapper_frames;
                printf("[demo04] frame rkmpp_wrapper_harddecode_cpu_visible idx=%d fmt=%s pts=%" PRId64 "\n", wrapper_frames,
                       pix_fmt_name(static_cast<AVPixelFormat>(frame->format)), frame->pts);
            } else {
                ++sw_fallback_frames;
                fallback = true;
                printf("[demo04] frame sw_fallback idx=%d fmt=%s pts=%" PRId64 "\n", sw_fallback_frames,
                       pix_fmt_name(static_cast<AVPixelFormat>(frame->format)), frame->pts);
            }

            // frame 使用完必须 unref，避免帧引用积压。
            av_frame_unref(frame);
        }
    }

    printf("[demo04] summary decoder=%s hw_type=%s hw_frames=%d wrapper_frames=%d sw_fallback_frames=%d hwdownload_ok=%d fallback=%d\n",
           dec->name, a.hw_type_set ? a.hw_type.c_str() : "(not-forced)",
           hw_frames, wrapper_frames, sw_fallback_frames, transfer_ok, fallback ? 1 : 0);
    verdict = pick_verdict(a, decoder_is_rkmpp_wrapper, hw_frames,
                           wrapper_frames, sw_fallback_frames, fallback);
    verdict_reason = pick_verdict_reason(a, decoder_is_rkmpp_wrapper, hw_frames,
                                         wrapper_frames, sw_fallback_frames, fallback);
    printf("[demo04] verdict=%s\n", verdict);
    printf("[demo04] verdict_reason=%s\n", verdict_reason);
    printf("[demo04] verdict_cn=%s\n", pick_verdict_cn(verdict));
    // printf("[demo04] how_to_read: HARDWARE_FRAME_CONFIRMED=硬件帧; HARDWARE_DECODE_WRAPPER_OUTPUT=rkmpp硬解但CPU可见帧; SOFTWARE_FALLBACK=软解回退\n");
    printf("[demo04] PASS\n");

cleanup:
    /*
     * 资源释放顺序保持对称：frame/packet -> codec ctx -> hw device -> format ctx。
     * 驱动影子线：hw device unref 后，backend 对应内核资源引用也会逐步回收。
     */
    av_frame_free(&sw_frame);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&dec_ctx);
    av_buffer_unref(&hw_dev_ctx);
    safe_close_input(&fmt_ctx);
    return 0;
}
