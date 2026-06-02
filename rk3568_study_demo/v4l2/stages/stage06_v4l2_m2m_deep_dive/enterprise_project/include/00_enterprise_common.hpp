#ifndef STAGE06_ENTERPRISE_COMMON_HPP_
#define STAGE06_ENTERPRISE_COMMON_HPP_

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace stage06_enterprise {

/*
 * 企业项目公共头。
 *
 * 定位：
 * - 这是 Stage06 的“工作化”项目，不是生产 VPU 驱动，也不假装真实解码成功。
 * - 它把 V4L2 M2M decode 的 ioctl/state/counter/gate 组织成一个可运行诊断服务。
 *
 * 驱动影子线：
 * - 用户态状态机中的 OPEN/QUERYCAP/S_FMT/QBUF/DQBUF/SOURCE_CHANGE/STREAMOFF
 *   分别映射到驱动 file_operations、V4L2 ioctl ops、videobuf2 queue、
 *   v4l2-mem2mem job 调度、IRQ/worker completion 和恢复路径。
 */

inline int xioctl(int fd, unsigned long request, void* arg) {
    int ret;
    do {
        ret = ioctl(fd, request, arg);
    } while (ret < 0 && errno == EINTR);
    return ret;
}

inline bool ensure_dir(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    if (mkdir(path.c_str(), 0755) == 0 || errno == EEXIST) {
        return true;
    }
    std::cerr << "mkdir failed: " << path << ": " << strerror(errno) << "\n";
    return false;
}

inline std::string fourcc_to_string(uint32_t fourcc) {
    char s[5];
    s[0] = static_cast<char>(fourcc & 0xff);
    s[1] = static_cast<char>((fourcc >> 8) & 0xff);
    s[2] = static_cast<char>((fourcc >> 16) & 0xff);
    s[3] = static_cast<char>((fourcc >> 24) & 0xff);
    s[4] = '\0';
    return std::string(s);
}

inline bool parse_fourcc(const std::string& text, uint32_t* out) {
    if (text.size() != 4 || out == NULL) {
        return false;
    }
    *out = v4l2_fourcc(text[0], text[1], text[2], text[3]);
    return true;
}

inline std::string yes_no(bool value) {
    return value ? "yes" : "no";
}

inline std::string dirname_of(const std::string& path) {
    const std::string::size_type pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return ".";
    }
    if (pos == 0) {
        return "/";
    }
    return path.substr(0, pos);
}

}  // namespace stage06_enterprise

#endif  // STAGE06_ENTERPRISE_COMMON_HPP_
