#ifndef STAGE03_ENTERPRISE_COMMON_HPP_
#define STAGE03_ENTERPRISE_COMMON_HPP_

#include <errno.h>
#include <linux/videodev2.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <string>

namespace enterprise_m2m {

inline int xioctl(int fd, unsigned long request, void* arg) {
    int ret = 0;
    do {
        // 被信号中断时重试，避免把 EINTR 误判成业务失败。
        ret = ioctl(fd, request, arg);
    } while (ret < 0 && errno == EINTR);
    return ret;
}

inline bool ensure_dir(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    // 逐级创建目录，保证 logs/run_xxx 这种多层路径一次可用。
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        const char ch = path[i];
        cur.push_back(ch);
        if (ch == '/') {
            if (!cur.empty() && mkdir(cur.c_str(), 0755) != 0 && errno != EEXIST) {
                fprintf(stderr, "mkdir failed: %s: %s\n", cur.c_str(), strerror(errno));
                return false;
            }
        }
    }

    if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "mkdir failed: %s: %s\n", path.c_str(), strerror(errno));
        return false;
    }
    return true;
}

inline std::string now_datetime_compact() {
    char buf[32] = {0};
    const time_t t = time(nullptr);
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tmv);
    return std::string(buf);
}

inline std::string now_datetime_iso() {
    char buf[40] = {0};
    const time_t t = time(nullptr);
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S%z", &tmv);
    return std::string(buf);
}

inline std::string string_vformat(const char* fmt, va_list ap) {
    char buf[2048];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    return std::string(buf);
}

inline std::string string_format(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const std::string out = string_vformat(fmt, ap);
    va_end(ap);
    return out;
}

}  // namespace enterprise_m2m

#endif  // STAGE03_ENTERPRISE_COMMON_HPP_
