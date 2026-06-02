#ifndef STAGE04_ENTERPRISE_COMMON_HPP_
#define STAGE04_ENTERPRISE_COMMON_HPP_

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/time.h>
}

#include <stdio.h>
#include <string>

namespace stage04_enterprise {

inline std::string ff_err2str(int errnum) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errnum, buf, sizeof(buf));
    return std::string(buf);
}

}  // namespace stage04_enterprise

#endif
