extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
}

#include <iostream>
#include <string>

/*
demo.cpp 目的:
1) 在 RK 平台上用 FFmpeg + rkmpp 解码 H.264
2) 演示硬件设备上下文(hardware device ctx)与硬件帧上下文(hw frames ctx)的基本配置
3) 演示硬件帧转到可读写的 NV12 软件帧（便于后处理/调试）
*/

/*
函数与参数速查:
1) get_hw_format(ctx, pix_fmts):
   - ctx: 当前解码器上下文（本示例未直接使用）。
   - pix_fmts: 解码器给出的候选像素格式数组（以 AV_PIX_FMT_NONE 结尾）。
   - 返回: 选择的目标像素格式（优先 NV12）。
2) log_error(msg, err):
   - msg: 错误语义标签。
   - err: FFmpeg 返回码（负值），函数内转成人可读字符串输出。
3) main(argc, argv):
   - argv[1]: 输入媒体路径，缺省为 test.mp4。
   - 主要阶段:
     a) 打开输入并找视频流
     b) 选择 rkmpp 解码器并创建硬件设备
     c) 配置 hw_frames_ctx（格式、分辨率、池大小）
     d) 打开解码器并送包取帧
     e) 必要时将硬件帧转成可读软件帧（NV12）
*/

// rkmpp 定制 FFmpeg 里可能引用的全局符号
extern "C" {
char *querystring = nullptr;
}

// 选择硬件像素格式（rkmpp 输出 NV12）
// 函数: get_hw_format
// 参数:
// - ctx: 解码器上下文（本示例里仅为回调签名需要）。
// - pix_fmts: 解码器可选像素格式列表（以 AV_PIX_FMT_NONE 结尾）。
// 返回:
// - 优先返回 AV_PIX_FMT_NV12；找不到时返回首个候选格式。
static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts)
{
    (void)ctx; // 这个 demo 不使用 ctx，仅扫描候选像素格式列表
    for (const enum AVPixelFormat *p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
        // RK3568 MPP解码器输出NV12格式
        if (*p == AV_PIX_FMT_NV12)
            return *p;
    }
    // 没找到 NV12 时退回第一个候选，避免直接失败
    return pix_fmts[0];
}

// 函数: log_error
// 参数:
// - msg: 错误语义标签，便于定位阶段。
// - err: FFmpeg 返回码。
// 作用: 把 FFmpeg 错误码转成人类可读字符串输出。
static void log_error(const char *msg, int err)
{
    char buf[256];
    av_strerror(err, buf, sizeof(buf));
    std::cerr << msg << " : " << buf << std::endl;
}

// 函数: main
// 参数:
// - argc/argv: argv[1] 可传输入媒体路径，默认 test.mp4。
// 作用: 串起“打开输入 -> 配置硬解 -> 解码 -> 取帧/转帧 -> 释放资源”全流程。
int main(int argc, char **argv)
{
    // 输入视频文件，默认 test.mp4
    std::string input = argc > 1 ? argv[1] : "test.mp4";

    AVFormatContext *fmt_ctx = nullptr;
    AVCodecContext  *codec_ctx = nullptr;
    AVBufferRef     *hw_device_ctx = nullptr;
    AVBufferRef     *hw_frames_ctx = nullptr;

    int ret;

    /* ---------------- 打开输入 ---------------- */
    ret = avformat_open_input(&fmt_ctx, input.c_str(), nullptr, nullptr);
    if (ret < 0) {
        log_error("avformat_open_input failed", ret);
        return -1;
    }

    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        log_error("avformat_find_stream_info failed", ret);
        return -1;
    }

    int video_stream_idx =
        av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_idx < 0) {
        std::cerr << "No video stream found" << std::endl;
        return -1;
    }

    AVStream *vs = fmt_ctx->streams[video_stream_idx];

    /* ---------------- 查找解码器 ---------------- */
    // 先尝试 RKMPP 硬解
    const AVCodec *codec = avcodec_find_decoder_by_name("h264_rkmpp");
    if (!codec) {
        // 没有 rkmpp 时降级到软解，保证 demo 可运行
        std::cerr << "h264_rkmpp not found, fallback to software decoder"
                  << std::endl;
        codec = avcodec_find_decoder(vs->codecpar->codec_id);
    }
    if (!codec) {
        std::cerr << "Decoder not found" << std::endl;
        return -1;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, vs->codecpar);

    // 当解码器协商像素格式时，会调用这个回调函数
    codec_ctx->get_format = get_hw_format;
    codec_ctx->thread_count = 4;

    /* ---------------- 创建 RKMPP 设备 ---------------- */
    // 关键修正：使用AV_HWDEVICE_TYPE_RKMPP而不是DRM
    ret = av_hwdevice_ctx_create(&hw_device_ctx,
                                 AV_HWDEVICE_TYPE_RKMPP,
                                 nullptr, nullptr, 0);
    if (ret < 0) {
        log_error("av_hwdevice_ctx_create failed", ret);
        return -1;
    }

    /* ---------------- 创建 HW frames ctx ---------------- */
    // hw_frames_ctx 描述“硬件帧池”的格式、尺寸、池大小等信息
    hw_frames_ctx = av_hwframe_ctx_alloc(hw_device_ctx);
    if (!hw_frames_ctx) {
        std::cerr << "av_hwframe_ctx_alloc failed" << std::endl;
        return -1;
    }

    AVHWFramesContext *frames_ctx =
        (AVHWFramesContext *)hw_frames_ctx->data;

    // 关键设置：硬件与软件侧都指定 NV12，避免格式不一致
    frames_ctx->format    = AV_PIX_FMT_NV12;  // MPP解码器输出NV12
    frames_ctx->sw_format = AV_PIX_FMT_NV12;  // 软件处理使用NV12
    frames_ctx->width     = codec_ctx->width;
    frames_ctx->height    = codec_ctx->height;
    frames_ctx->initial_pool_size = 8;        // 帧池数量（示例值）

    ret = av_hwframe_ctx_init(hw_frames_ctx);
    if (ret < 0) {
        log_error("av_hwframe_ctx_init failed", ret);
        return -1;
    }

    codec_ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ctx);

    /* ---------------- 打开解码器 ---------------- */
    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        log_error("avcodec_open2 failed", ret);
        return -1;
    }

    std::cout << "Decoder opened: " << codec->name << std::endl;

    /* ---------------- 解码循环 ---------------- */
    AVPacket *pkt = av_packet_alloc();
    AVFrame  *hw_frame = av_frame_alloc();
    AVFrame  *sw_frame = av_frame_alloc();

    int decoded = 0;

    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index != video_stream_idx) {
            av_packet_unref(pkt);
            continue;
        }

        ret = avcodec_send_packet(codec_ctx, pkt);
        av_packet_unref(pkt);
        if (ret < 0)
            break;

        // 一次 send_packet 可能产出多帧，所以要循环 receive_frame
        while (true) {
            ret = avcodec_receive_frame(codec_ctx, hw_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0) {
                log_error("receive_frame failed", ret);
                goto out;
            }

            decoded++;

            // 为软件帧分配缓冲区，准备接收从硬件帧拷贝过来的像素数据
            sw_frame->format = AV_PIX_FMT_NV12;
            sw_frame->width  = hw_frame->width;
            sw_frame->height = hw_frame->height;

            if (av_frame_get_buffer(sw_frame, 32) < 0) {
                std::cerr << "av_frame_get_buffer failed" << std::endl;
                goto out;
            }

            // 硬件帧 -> 软件帧
            // 实际工程里如需零拷贝，会尽量避免这步拷贝，改成下游直接消费硬件帧
            ret = av_hwframe_transfer_data(sw_frame, hw_frame, 0);
            if (ret < 0) {
                log_error("av_hwframe_transfer_data failed", ret);
                goto out;
            }

            std::cout << "Decoded frame " << decoded
                      << " : " << sw_frame->width
                      << "x" << sw_frame->height
                      << " NV12" << std::endl;

            av_frame_unref(hw_frame);
            av_frame_unref(sw_frame);
        }
    }

out:
    std::cout << "Total decoded frames: " << decoded << std::endl;

    // 资源释放顺序:
    // 先帧与包，再 codec/fmt，再 hw ctx
    av_frame_free(&hw_frame);
    av_frame_free(&sw_frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    av_buffer_unref(&hw_frames_ctx);
    av_buffer_unref(&hw_device_ctx);

    return 0;
}
