#ifndef STAGE05_ENTERPRISE_COMMON_HPP_
#define STAGE05_ENTERPRISE_COMMON_HPP_

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <string>
#include <vector>

namespace stage05_enterprise {

inline std::string ff_err2str(int errnum) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errnum, buf, sizeof(buf));
    return std::string(buf);
}

inline const char* pix_fmt_name(AVPixelFormat fmt) {
    const char* n = av_get_pix_fmt_name(fmt);
    return n != nullptr ? n : "unknown";
}

inline const char* now_text() {
    static thread_local char buf[64];
    time_t t = time(nullptr);
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
    return buf;
}

inline bool ensure_dir(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    std::string cmd = "mkdir -p '" + path + "'";
    return ::system(cmd.c_str()) == 0;
}

}  // namespace stage05_enterprise

#endif
