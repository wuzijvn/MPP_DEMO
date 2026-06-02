#ifndef STAGE01_V4L2_COMMON_HPP_
#define STAGE01_V4L2_COMMON_HPP_

#include <errno.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <time.h>

#include <string>

namespace stage01_v4l2 {

/*
===============================================================================
Stage01 Common Utilities
===============================================================================

本文件职责：
1) 提供“所有模块都能复用”的小工具函数；
2) 这些函数尽量保持无状态、无副作用，便于你单独理解；
3) 通过统一封装减少重复代码和低级错误。

学习建议：
1) 先理解 xioctl（系统调用层）
2) 再理解 fourcc_to_string（格式可视化）
3) 最后理解 now_ms（时序统计基础）
*/

// xioctl:
//   ioctl 的轻量封装，自动处理 EINTR。
//
// 背景：
//   系统调用可能被信号打断，返回 -1 且 errno=EINTR。
//   对这类情况直接当失败，会造成“偶发报错”。
//
// 行为：
//   仅在 EINTR 时重试；其他错误原样返回，调用方按 errno 判断。
inline int xioctl(int fd, unsigned long req, void* arg) {
    int ret;
    do {
        // 这里不做任何业务逻辑，只负责转发 ioctl。
        // “发生什么操作”完全由 req/arg 决定。
        ret = ioctl(fd, req, arg);
    } while (ret == -1 && errno == EINTR);
    // 返回值约定沿用 ioctl 原语：
    // - 成功通常为 0（具体看 ioctl 类型）
    // - 失败为 -1，errno 提供具体原因
    //
    // 实战提醒：
    // - xioctl 只处理 EINTR，不会屏蔽其他错误；
    // - 调用点仍需根据 errno 做分支处理。
    return ret;
}

// fourcc_to_string:
//   把 V4L2 fourcc（32-bit）转为 4 字符可读串。
//
// 示例：
//   V4L2_PIX_FMT_YUYV -> "YUYV"
inline std::string fourcc_to_string(uint32_t f) {
    std::string s;
    // 注意 fourcc 是 little-endian 存储。
    // 这里按字节拆出来，得到 4 个可读字符。
    s.push_back((char)(f & 0xFF));
    s.push_back((char)((f >> 8) & 0xFF));
    s.push_back((char)((f >> 16) & 0xFF));
    s.push_back((char)((f >> 24) & 0xFF));
    // 注意：
    // 某些 fourcc 字节可能不是可打印字符，这时日志会显示异常符号。
    // 这是正常现象，通常意味着格式值本身不常见或请求有误。
    return s;
}

// now_ms:
//   获取单调时钟毫秒值。
//
// 为什么用 CLOCK_MONOTONIC：
// 1) 不受系统时间（NTP/手工改时）跳变影响；
// 2) 适合做采集耗时和 fps 统计。
inline double now_ms() {
    timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        // 失败时返回 0，调用方应在统计时容错处理。
        return 0.0;
    }
    // 秒转毫秒 + 纳秒转毫秒
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

}  // namespace stage01_v4l2

#endif  // STAGE01_V4L2_COMMON_HPP_
