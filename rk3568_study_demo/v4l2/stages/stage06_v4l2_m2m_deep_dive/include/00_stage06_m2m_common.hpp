#ifndef STAGE06_M2M_COMMON_HPP_
#define STAGE06_M2M_COMMON_HPP_

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
#include <sys/types.h>
#include <unistd.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace stage06 {

/*
 * Stage06 公共工具头。
 *
 * 教学边界：
 * - 这里放跨 demo 复用的小工具，例如参数解析、fourcc 打印、ioctl 重试。
 * - 不把 V4L2 M2M 主流程藏在这里，核心状态机仍留在每个 demo 文件中，方便你按知识点阅读。
 *
 * 驱动影子线：
 * - open/ioctl/poll 这些调用最终会进入内核 V4L2 驱动、videobuf2、v4l2-mem2mem 等路径。
 * - 本头文件只提供用户态封装，不假设具体 RK3568、MPP 或某个厂商驱动私有 ABI。
 */

inline int xioctl(int fd, unsigned long request, void* arg) {
    int ret;
    do {
        /*
         * EINTR 表示系统调用被信号打断。
         * 媒体程序里 ioctl 经常处于长流程状态机中，直接失败会制造假错误，
         * 所以这里统一重试。真正的 EINVAL/EIO/ENOTTY 等错误交给调用方解释。
         */
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

inline bool has_arg(int argc, char** argv, const std::string& key) {
    for (int i = 1; i < argc; ++i) {
        if (key == argv[i]) {
            return true;
        }
    }
    return false;
}

inline std::string get_arg(int argc, char** argv, const std::string& key,
                           const std::string& def) {
    const std::string prefix = key + "=";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.compare(0, prefix.size(), prefix) == 0) {
            return arg.substr(prefix.size());
        }
    }
    return def;
}

inline int get_arg_int(int argc, char** argv, const std::string& key, int def) {
    const std::string value = get_arg(argc, argv, key, "");
    if (value.empty()) {
        return def;
    }
    return atoi(value.c_str());
}

inline uint32_t get_arg_u32(int argc, char** argv, const std::string& key,
                            uint32_t def) {
    const int value = get_arg_int(argc, argv, key, static_cast<int>(def));
    if (value < 0) {
        return def;
    }
    return static_cast<uint32_t>(value);
}

inline bool parse_fourcc(const std::string& text, uint32_t* out) {
    if (text.size() != 4 || out == NULL) {
        return false;
    }
    *out = v4l2_fourcc(text[0], text[1], text[2], text[3]);
    return true;
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

inline uint32_t output_type(bool mplane) {
    return mplane ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT;
}

inline uint32_t capture_type(bool mplane) {
    return mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
}

inline std::string buffer_type_name(uint32_t type) {
    switch (type) {
        case V4L2_BUF_TYPE_VIDEO_OUTPUT:
            return "VIDEO_OUTPUT";
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
            return "VIDEO_CAPTURE";
        case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
            return "VIDEO_OUTPUT_MPLANE";
        case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
            return "VIDEO_CAPTURE_MPLANE";
        default:
            return "UNKNOWN_TYPE";
    }
}

inline bool open_video_node(const std::string& device, int* out_fd) {
    if (out_fd == NULL) {
        return false;
    }
    /*
     * O_NONBLOCK 是 codec 测试工具常用选择：
     * - DQBUF 没有 ready buffer 时返回 EAGAIN；
     * - 程序用 poll 决定何时再 DQBUF；
     * - 这和驱动 waitqueue/wakeup 模型对应。
     */
    const int fd = open(device.c_str(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        std::cerr << "open " << device << " failed: " << strerror(errno) << "\n";
        return false;
    }
    *out_fd = fd;
    return true;
}

inline bool query_capability(int fd, struct v4l2_capability* cap) {
    if (cap == NULL) {
        return false;
    }
    memset(cap, 0, sizeof(*cap));
    if (xioctl(fd, VIDIOC_QUERYCAP, cap) < 0) {
        std::cerr << "VIDIOC_QUERYCAP failed: " << strerror(errno) << "\n";
        return false;
    }
    return true;
}

inline std::string yes_no(bool v) {
    return v ? "yes" : "no";
}

inline void print_line(const std::string& tag, const std::string& message) {
    std::cout << std::left << std::setw(18) << tag << " : " << message << "\n";
}

inline void print_counter(const std::string& name, int value) {
    std::cout << std::left << std::setw(28) << name << " = " << value << "\n";
}

inline bool write_text_file(const std::string& path, const std::string& body) {
    FILE* fp = fopen(path.c_str(), "w");
    if (fp == NULL) {
        std::cerr << "fopen " << path << " failed: " << strerror(errno) << "\n";
        return false;
    }
    const size_t written = fwrite(body.data(), 1, body.size(), fp);
    fclose(fp);
    if (written != body.size()) {
        std::cerr << "write incomplete: " << path << "\n";
        return false;
    }
    return true;
}

struct QueueCounters {
    int qbuf_output = 0;
    int qbuf_capture = 0;
    int dqbuf_output = 0;
    int dqbuf_capture = 0;
    int poll_calls = 0;
    int timeout_count = 0;
    int source_change_count = 0;
    int eos_count = 0;
    int recovery_count = 0;
};

inline void print_counters(const QueueCounters& c) {
    print_counter("qbuf_output", c.qbuf_output);
    print_counter("qbuf_capture", c.qbuf_capture);
    print_counter("dqbuf_output", c.dqbuf_output);
    print_counter("dqbuf_capture", c.dqbuf_capture);
    print_counter("poll_calls", c.poll_calls);
    print_counter("timeout_count", c.timeout_count);
    print_counter("source_change_count", c.source_change_count);
    print_counter("eos_count", c.eos_count);
    print_counter("recovery_count", c.recovery_count);
}

}  // namespace stage06

#endif  // STAGE06_M2M_COMMON_HPP_
