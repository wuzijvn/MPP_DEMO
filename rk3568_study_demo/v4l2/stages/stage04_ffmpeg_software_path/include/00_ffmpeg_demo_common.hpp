#ifndef STAGE04_FFMPEG_DEMO_COMMON_HPP_
#define STAGE04_FFMPEG_DEMO_COMMON_HPP_

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

namespace ffmpeg_stage04 {

/*
 * 函数作用：把 FFmpeg 错误码转成可读字符串。
 * 工作场景：所有 av* API 失败时统一打印错误，便于快速定位层级。
 */
inline std::string ff_err2str(int errnum) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errnum, buf, sizeof(buf));
    return std::string(buf);
}

/*
 * 函数作用：打印输入文件媒体信息（流索引、类型、codec id）。
 * 说明：这是学习阶段最常用的 preflight 可见性输出。
 */
inline void print_stream_brief(AVFormatContext* fmt_ctx) {
    printf("[stream-brief] nb_streams=%u\n", fmt_ctx ? fmt_ctx->nb_streams : 0);
    if (fmt_ctx == nullptr) {
        return;
    }
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; ++i) {
        AVStream* st = fmt_ctx->streams[i];
        const AVCodecParameters* cp = st->codecpar;
        const char* type = av_get_media_type_string(cp->codec_type);
        if (type == nullptr) {
            type = "unknown";
        }
        printf("  - stream[%u]: type=%s codec_id=%d time_base=%d/%d\n", i, type,
               cp->codec_id, st->time_base.num, st->time_base.den);
    }
}

/*
 * 函数作用：安全关闭输入文件。
 * 前置条件：fmt_ctx 可能为 nullptr。
 * 调用后状态：fmt_ctx 内部资源会释放。
 */
inline void safe_close_input(AVFormatContext** fmt_ctx) {
    if (fmt_ctx != nullptr && *fmt_ctx != nullptr) {
        avformat_close_input(fmt_ctx);
    }
}

/*
 * 函数作用：统一释放 AVPacket。
 * 说明：packet 内容由 FFmpeg 内部分配，必须调用 av_packet_unref 回收引用。
 */
inline void safe_packet_unref(AVPacket* pkt) {
    if (pkt != nullptr) {
        av_packet_unref(pkt);
    }
}

/*
 * 函数作用：统一释放 AVFrame。
 */
inline void safe_frame_unref(AVFrame* frame) {
    if (frame != nullptr) {
        av_frame_unref(frame);
    }
}

}  // namespace ffmpeg_stage04

#endif  // STAGE04_FFMPEG_DEMO_COMMON_HPP_
