#ifndef STAGE05_FFMPEG_HWACCEL_COMMON_HPP_
#define STAGE05_FFMPEG_HWACCEL_COMMON_HPP_

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

namespace ffmpeg_stage05 {

inline std::string ff_err2str(int errnum) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errnum, buf, sizeof(buf));
    return std::string(buf);
}

inline const char* pix_fmt_name(AVPixelFormat fmt) {
    const char* n = av_get_pix_fmt_name(fmt);
    return n != nullptr ? n : "unknown";
}

inline void safe_close_input(AVFormatContext** fmt_ctx) {
    if (fmt_ctx != nullptr && *fmt_ctx != nullptr) {
        avformat_close_input(fmt_ctx);
    }
}

inline void print_hw_types() {
    printf("[hwtypes] available hw device types from FFmpeg build:\n");
    AVHWDeviceType t = AV_HWDEVICE_TYPE_NONE;
    while ((t = av_hwdevice_iterate_types(t)) != AV_HWDEVICE_TYPE_NONE) {
        printf("  - %s\n", av_hwdevice_get_type_name(t));
    }
}

inline std::vector<AVPixelFormat> collect_decoder_hw_configs(const AVCodec* dec,
                                                              AVHWDeviceType target) {
    std::vector<AVPixelFormat> out;
    for (int i = 0;; ++i) {
        const AVCodecHWConfig* cfg = avcodec_get_hw_config(dec, i);
        if (cfg == nullptr) {
            break;
        }
        const bool is_hw = (cfg->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) != 0;
        if (is_hw && cfg->device_type == target) {
            out.push_back(cfg->pix_fmt);
        }
    }
    return out;
}

inline bool parse_hw_type(const std::string& name, AVHWDeviceType* out) {
    AVHWDeviceType t = av_hwdevice_find_type_by_name(name.c_str());
    if (t == AV_HWDEVICE_TYPE_NONE) {
        return false;
    }
    *out = t;
    return true;
}

}  // namespace ffmpeg_stage05

#endif  // STAGE05_FFMPEG_HWACCEL_COMMON_HPP_
