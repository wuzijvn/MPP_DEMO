#include "06_hwaccel_pipeline_service.hpp"

#include "00_enterprise_common.hpp"
#include "03_state_machine.hpp"
#include "04_logger.hpp"

#include <vector>

namespace stage05_enterprise {
namespace {

/*
 * 全局静态：当前运行选择的硬件像素格式。
 * 注意：这是 demo 级实现，真实多实例服务需避免全局共享带来的并发风险。
 */
static AVPixelFormat g_hw_pix_fmt = AV_PIX_FMT_NONE;

/*
 * get_format 回调：
 * - 输入：FFmpeg 给出的候选像素格式列表。
 * - 输出：优先返回 g_hw_pix_fmt；若未命中则回退到候选第一个。
 *
 * 驱动影子线：当这里回退时，常见原因是 backend/driver 不支持目标格式协商。
 */
static AVPixelFormat get_hw_format(AVCodecContext* ctx, const AVPixelFormat* pix_fmts) {
    (void)ctx;
    for (const AVPixelFormat* p = pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
        if (*p == g_hw_pix_fmt) {
            return *p;
        }
    }
    return pix_fmts[0];
}

static bool parse_hw_type(const std::string& name, AVHWDeviceType* out) {
    AVHWDeviceType t = av_hwdevice_find_type_by_name(name.c_str());
    if (t == AV_HWDEVICE_TYPE_NONE) {
        return false;
    }
    *out = t;
    return true;
}

static std::vector<AVPixelFormat> collect_hw_fmts(const AVCodec* dec, AVHWDeviceType target) {
    std::vector<AVPixelFormat> out;
    for (int i = 0;; ++i) {
        const AVCodecHWConfig* cfg = avcodec_get_hw_config(dec, i);
        if (cfg == nullptr) {
            break;
        }
        const bool has_method = (cfg->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) != 0;
        if (has_method && cfg->device_type == target) {
            out.push_back(cfg->pix_fmt);
        }
    }
    return out;
}

}  // namespace

HwaccelPipelineService::HwaccelPipelineService(const PipelineConfig& cfg,
                                               PipelineStats* stats,
                                               StateMachine* sm,
                                               Logger* logger)
    : cfg_(cfg), stats_(stats), sm_(sm), logger_(logger) {}

/*
 * prepare_decoder 职责：
 * 1) 打开输入并找到视频流；
 * 2) 准备 decoder + hwdevice + hwfmt 协商；
 * 3) 决定当前是否进入 fallback 模式。
 */
bool HwaccelPipelineService::prepare_decoder(AVFormatContext** fmt_ctx,
                                             AVCodecContext** dec_ctx,
                                             AVBufferRef** hw_dev_ctx,
                                             int* video_stream_index,
                                             std::string* fail_reason) {
    int ret = avformat_open_input(fmt_ctx, cfg_.input.c_str(), nullptr, nullptr);
    if (ret < 0) {
        *fail_reason = "avformat_open_input: " + ff_err2str(ret);
        return false;
    }
    sm_->transit(PipelineState::kInputOpened, "input opened");

    ret = avformat_find_stream_info(*fmt_ctx, nullptr);
    if (ret < 0) {
        *fail_reason = "avformat_find_stream_info: " + ff_err2str(ret);
        return false;
    }
    sm_->transit(PipelineState::kStreamReady, "stream info ready");

    *video_stream_index = av_find_best_stream(*fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (*video_stream_index < 0) {
        *fail_reason = "no video stream";
        return false;
    }

    const AVCodec* dec = avcodec_find_decoder_by_name(cfg_.decoder.c_str());
    if (dec == nullptr) {
        dec = avcodec_find_decoder((*fmt_ctx)->streams[*video_stream_index]->codecpar->codec_id);
    }
    if (dec == nullptr) {
        *fail_reason = "decoder not found";
        return false;
    }

    bool fallback = cfg_.inject_force_sw_fallback;
    std::vector<AVPixelFormat> hw_fmts;

    if (cfg_.hw_type_set) {
        AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_NONE;
        if (!parse_hw_type(cfg_.hw_type, &hw_type)) {
            *fail_reason = "unknown hw type";
            return false;
        }

        // 显式 hwdevice 模式：从 decoder capability 收集目标后端可用的硬件像素格式。
        hw_fmts = collect_hw_fmts(dec, hw_type);
        if (cfg_.inject_missing_hwfmt) {
            // 故障注入：模拟“后端无可用 hwfmt”的场景。
            hw_fmts.clear();
        }

        if (!fallback && cfg_.inject_device_create_fail) {
            // 故障注入：模拟设备创建失败。
            ret = AVERROR(EIO);
        } else {
            const char* dev = cfg_.device.empty() ? nullptr : cfg_.device.c_str();
            ret = av_hwdevice_ctx_create(hw_dev_ctx, hw_type, dev, nullptr, 0);
        }

        if (ret < 0) {
            fallback = true;
            stats_->fallback_count++;
            logger_->log(LogLevel::kWarn, "hwdevice create failed, fallback to decoder-only path: %s",
                         ff_err2str(ret).c_str());
        } else {
            sm_->transit(PipelineState::kDevicePrepared, "hw device prepared");
        }
    } else {
        logger_->log(LogLevel::kInfo,
                     "wrapper mode: no hw_type forced; decoder wrapper is the primary evidence path");
    }

    *dec_ctx = avcodec_alloc_context3(dec);
    if (*dec_ctx == nullptr) {
        *fail_reason = "avcodec_alloc_context3 failed";
        return false;
    }

    ret = avcodec_parameters_to_context(*dec_ctx, (*fmt_ctx)->streams[*video_stream_index]->codecpar);
    if (ret < 0) {
        *fail_reason = "avcodec_parameters_to_context: " + ff_err2str(ret);
        return false;
    }

    if (cfg_.hw_type_set && !fallback && !hw_fmts.empty() && *hw_dev_ctx != nullptr) {
        g_hw_pix_fmt = hw_fmts[0];
        (*dec_ctx)->get_format = get_hw_format;
        (*dec_ctx)->hw_device_ctx = av_buffer_ref(*hw_dev_ctx);
        logger_->log(LogLevel::kInfo, "use hw pix fmt=%s", pix_fmt_name(g_hw_pix_fmt));
    } else if (cfg_.hw_type_set) {
        fallback = true;
        stats_->fallback_count++;
        logger_->log(LogLevel::kWarn, "decoder opens in software/fallback mode");
    } else {
        g_hw_pix_fmt = AV_PIX_FMT_NONE;
    }

    ret = avcodec_open2(*dec_ctx, dec, nullptr);
    if (ret < 0) {
        *fail_reason = "avcodec_open2: " + ff_err2str(ret);
        return false;
    }

    sm_->transit(PipelineState::kDecoderReady, "decoder opened");
    return true;
}

/*
 * run_loop 职责：
 * 1) 运行 demux -> decode 主循环；
 * 2) 统计 packet/frame/hw/sw/transfer 指标；
 * 3) 在错误点设置 fail_reason 并走统一清理。
 */
bool HwaccelPipelineService::run_loop(AVFormatContext* fmt_ctx,
                                      AVCodecContext* dec_ctx,
                                      int video_stream_index,
                                      std::string* fail_reason) {
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* sw_frame = av_frame_alloc();
    if (pkt == nullptr || frame == nullptr || sw_frame == nullptr) {
        *fail_reason = "alloc packet/frame failed";
        av_packet_free(&pkt);
        av_frame_free(&frame);
        av_frame_free(&sw_frame);
        return false;
    }

    sm_->transit(PipelineState::kLoopRunning, "decode loop enter");

    while (stats_->frame_recv < cfg_.max_frames) {
        int ret = av_read_frame(fmt_ctx, pkt);
        if (ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            stats_->err_count++;
            *fail_reason = "av_read_frame: " + ff_err2str(ret);
            goto fail;
        }

        stats_->packet_read++;
        if (pkt->stream_index != video_stream_index) {
            av_packet_unref(pkt);
            continue;
        }

        // send 后 packet 可立即 unref，所有权转入 decoder 队列。
        ret = avcodec_send_packet(dec_ctx, pkt);
        av_packet_unref(pkt);
        if (ret < 0) {
            stats_->err_count++;
            *fail_reason = "avcodec_send_packet: " + ff_err2str(ret);
            goto fail;
        }
        stats_->packet_sent++;

        while (ret >= 0 && stats_->frame_recv < cfg_.max_frames) {
            ret = avcodec_receive_frame(dec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                stats_->err_count++;
                *fail_reason = "avcodec_receive_frame: " + ff_err2str(ret);
                goto fail;
            }

            stats_->frame_recv++;
            if (stats_->first_pts == kNoPts) {
                stats_->first_pts = frame->pts;
            }
            stats_->last_pts = frame->pts;

            const AVPixelFormat f = static_cast<AVPixelFormat>(frame->format);
            if (f == g_hw_pix_fmt && g_hw_pix_fmt != AV_PIX_FMT_NONE) {
                stats_->frame_hw++;

                if (cfg_.inject_transfer_fail) {
                    // 故障注入：模拟 hwdownload 失败计数。
                    stats_->hw_transfer_fail++;
                } else {
                    /*
                     * hwdownload：把硬件帧下载到 CPU 可访问内存。
                     * 驱动影子线：这一步通常意味着“非零拷贝”路径。
                     */
                    ret = av_hwframe_transfer_data(sw_frame, frame, 0);
                    if (ret == 0) {
                        stats_->hw_transfer_ok++;
                    } else {
                        stats_->hw_transfer_fail++;
                        logger_->log(LogLevel::kWarn, "hw transfer fail: %s", ff_err2str(ret).c_str());
                    }
                    av_frame_unref(sw_frame);
                }
            } else {
                /*
                 * 这里统计的是“CPU 可见输出帧”，不是“软件解码器是否生效”。
                 * 默认 RKMPP wrapper 模式下，这个分支命中是常见现象。
                 */
                stats_->frame_cpu_visible++;
            }

            if (cfg_.print_every > 0 && stats_->frame_recv % cfg_.print_every == 0) {
                logger_->log(LogLevel::kInfo,
                             "progress frame=%llu hw=%llu cpu_visible=%llu transfer_ok=%llu transfer_fail=%llu",
                             static_cast<unsigned long long>(stats_->frame_recv),
                             static_cast<unsigned long long>(stats_->frame_hw),
                             static_cast<unsigned long long>(stats_->frame_cpu_visible),
                             static_cast<unsigned long long>(stats_->hw_transfer_ok),
                             static_cast<unsigned long long>(stats_->hw_transfer_fail));
            }

            // frame 使用完必须 unref，避免引用积累。
            av_frame_unref(frame);
        }
    }

    sm_->transit(PipelineState::kDraining, "decode loop finished");

    av_frame_free(&sw_frame);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    return true;

fail:
    av_frame_free(&sw_frame);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    return false;
}

bool HwaccelPipelineService::run(std::string* fail_reason) {
    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* dec_ctx = nullptr;
    AVBufferRef* hw_dev_ctx = nullptr;
    int video_stream_index = -1;

    if (!prepare_decoder(&fmt_ctx, &dec_ctx, &hw_dev_ctx, &video_stream_index, fail_reason)) {
        sm_->transit(PipelineState::kFailed, "prepare decoder failed");
        if (dec_ctx != nullptr) {
            avcodec_free_context(&dec_ctx);
        }
        if (hw_dev_ctx != nullptr) {
            av_buffer_unref(&hw_dev_ctx);
        }
        if (fmt_ctx != nullptr) {
            avformat_close_input(&fmt_ctx);
        }
        return false;
    }

    const bool ok = run_loop(fmt_ctx, dec_ctx, video_stream_index, fail_reason);

    // 统一资源释放，保持与 prepare 阶段对称。
    if (dec_ctx != nullptr) {
        avcodec_free_context(&dec_ctx);
    }
    if (hw_dev_ctx != nullptr) {
        av_buffer_unref(&hw_dev_ctx);
    }
    if (fmt_ctx != nullptr) {
        avformat_close_input(&fmt_ctx);
    }

    if (ok) {
        sm_->transit(PipelineState::kStopped, "normal stop");
    } else {
        sm_->transit(PipelineState::kFailed, "loop failed");
    }
    return ok;
}

}  // namespace stage05_enterprise
