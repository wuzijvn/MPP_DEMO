#ifndef STAGE02_V4L2_COMMON_HPP_
#define STAGE02_V4L2_COMMON_HPP_

#include <errno.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>

#include <string>

namespace stage02_v4l2 {

// xioctl:
//   对 ioctl 做统一封装，自动处理 EINTR。
//
// 为什么要这样做：
// 1) ioctl 在收到信号时，可能返回 -1 且 errno=EINTR；
// 2) 这种情况通常不是“真实功能失败”，而是“被中断，需重试”；
// 3) 如果不重试，程序会出现“偶发失败”，很难复现与定位。
//
// 工程经验：
// - 用户态媒体程序几乎都会有类似包装函数；
// - 这样可以把“EINTR 重试策略”集中在一个地方，避免每个调用点重复写。
inline int xioctl(int fd, unsigned long req, void* arg) {
    int ret;
    do {
        ret = ioctl(fd, req, arg);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

// fourcc_to_string:
//   把 V4L2 fourcc 整数编码转为可读字符串（4字符）。
//
// 用途：
// 1) 打印日志时可读性高（比如 YUYV/NV12/MJPG）；
// 2) 快速确认“请求格式 vs 生效格式”是否一致；
// 3) 减少调试时对十六进制值的人工换算。
inline std::string fourcc_to_string(uint32_t f) {
    std::string s;
    s.push_back((char)(f & 0xFF));
    s.push_back((char)((f >> 8) & 0xFF));
    s.push_back((char)((f >> 16) & 0xFF));
    s.push_back((char)((f >> 24) & 0xFF));
    return s;
}

// now_ms:
//   获取当前 MONOTONIC 时钟毫秒值。
//
// 为什么不用 REALTIME：
// 1) REALTIME 可能被 NTP/手工改时影响，会跳变；
// 2) MONOTONIC 单调递增，适合做耗时、帧间隔、抖动分析。
//
// 使用场景：
// - DQ 间隔统计；
// - 运行时长统计；
// - 实际 fps 计算。
inline double now_ms() {
    timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

// print_errno_hint:
//   统一打印 errno 辅助信息。
//
// 价值：
// - 把“哪个步骤失败 + errno + 错误文本”一次打印完整；
// - 便于后续写故障报告时直接引用日志证据。
inline void print_errno_hint(const char* what) {
    fprintf(stderr, "%s failed: errno=%d (%s)\n", what, errno, strerror(errno));
}

}  // namespace stage02_v4l2

#endif  // STAGE02_V4L2_COMMON_HPP_
