#ifndef STAGE01_V4L2_TYPES_HPP_
#define STAGE01_V4L2_TYPES_HPP_

#include <stdint.h>

#include <map>
#include <string>

namespace stage01_v4l2 {

/*
===============================================================================
Stage01 Data Model
===============================================================================

本文件只做一件事：定义“数据长什么样”。

为什么要单独拆文件：
1) 让业务逻辑和数据定义分离，阅读压力更小；
2) 你可以先看结构体，再看流程代码；
3) 当字段扩展时，不会污染核心流程函数。

阅读顺序：
Buffer -> AppConfig -> CaptureStats
*/

// Buffer:
//   描述一个通过 MMAP 映射到用户态的驱动缓冲。
//
// 字段说明：
//   start : mmap 返回的用户态虚拟地址。
//   length: 该映射区长度（字节）。
//
// 注意：
// 1) start 来自 mmap，不是 malloc，释放必须用 munmap。
// 2) length 取自 VIDIOC_QUERYBUF 返回的 buf.length。
struct Buffer {
    void* start;
    size_t length;
};

// 说明：
// - Buffer 是“驱动共享内存映射描述”，不是帧内容副本；
// - 真正帧内容通常在 DQ 后通过 buf.index 间接定位到这里。

// AppConfig:
//   程序运行期配置，来自默认值 + 命令行。
//
// 这些配置对应你工作里常见可调参数：
// - 分辨率/帧率
// - 缓冲数
// - 超时
// - 故障注入开关
struct AppConfig {
    // 设备节点，比如 /dev/video0。
    std::string dev;

    // 请求分辨率（S_FMT request）。
    int req_width;
    int req_height;

    // 预览导出路径（raw + ppm）。
    // raw 保存 YUYV 原始数据，ppm 保存可视化图。
    std::string out_raw;
    std::string out_ppm;

    // 采集目标帧数。
    int total_frames;

    // 预热帧数：前 N 帧不用于最终导出。
    // 原因：UVC 开流初期曝光/白平衡可能还在收敛。
    int warmup_frames;

    // 请求帧率（S_PARM request）。
    int req_fps;

    // 可选：请求像素格式 fourcc（默认 YUYV）。
    // 例如:
    // - YUYV
    // - NV12
    // - MJPG
    // 若为空字符串则走默认 YUYV。
    std::string req_pixfmt;

    // select 等待超时（毫秒）。
    int timeout_ms;

    // 请求驱动分配的 MMAP buffer 数。
    unsigned int req_buf_count;

    // 故障注入模式：
    // none         正常流程
    // bad-node     使用错误设备节点
    // bad-fmt      使用错误 fourcc 触发 S_FMT 失败
    // skip-requeue 从指定帧开始不回队，观察队列耗尽
    std::string inject;

    // skip-requeue 的触发帧号。
    int inject_frame;

    // 是否导出预览帧到 raw/ppm。
    bool save_preview;

    // 是否打印设备可支持的格式/分辨率/帧率枚举（帮助你做格式协商实验）。
    bool dump_formats;

    // 可选：逐帧 trace CSV 输出路径。
    //
    // 为空表示不输出。
    // 若设置该路径，主循环会把每帧关键元数据写入 CSV：
    // frame_no/sequence/bytesused/flags/timestamp/dq_interval 等。
    std::string trace_csv;

    // 控制逐帧日志打印频率。
    //
    // 规则：
    // - 前 5 帧始终打印
    // - 最后一帧始终打印
    // - 其余每 log_every 帧打印一次
    int log_every;
};

// CaptureStats:
//   采集过程统计，用于性能基线和故障定位。
//
// 统计结构设计原则：
// 1) 尽量“只加不减”字段，便于长期积累指标体系；
// 2) 每个字段都应该能回答一个具体排障问题；
// 3) 字段命名尽量直白，让你看 summary 时不用反复跳转代码。
struct CaptureStats {
    // select 统计：
    // calls   : 总调用次数
    // ready   : 返回可读次数（r > 0）
    // timeout : 超时次数（r == 0）
    // eintr   : 被信号中断次数（errno=EINTR）
    // error   : 其他错误次数
    uint64_t select_calls;
    uint64_t select_ready;
    uint64_t select_timeout;
    uint64_t select_eintr;
    uint64_t select_error;

    // DQBUF 统计：
    // ok      : 成功出队帧数
    // fail    : 失败总数
    // eagain  : fail 中 errno=EAGAIN 的次数
    uint64_t dq_ok;
    uint64_t dq_fail;
    uint64_t dq_eagain;

    // 回队统计：
    // ok      : 正常 QBUF 回队次数
    // fail    : 回队失败次数
    // skipped : 人工故障注入“漏回队”次数
    uint64_t requeue_ok;
    uint64_t requeue_fail;
    uint64_t requeue_skipped;

    // bytesused 统计：
    // bytes_total: 所有成功帧 bytesused 求和
    // bytes_min/max: 最小/最大 bytesused
    // bytes_hist: bytesused -> count 分布
    uint64_t bytes_total;
    uint32_t bytes_min;
    uint32_t bytes_max;
    std::map<uint32_t, uint64_t> bytes_hist;

    // sequence 跳变统计：
    // 用于观察“疑似丢帧”趋势。
    // 注意：不是所有驱动都保证 sequence 严格单调可用。
    bool has_last_sequence;
    uint32_t last_sequence;
    uint64_t sequence_gap_frames;

    // buffer flags 分布统计（flags 值 -> 次数）。
    // 用于观测 ERROR/KEYFRAME/TIMESTAMP_* 等驱动语义标记。
    std::map<uint32_t, uint64_t> flags_hist;

    // DQ 时发现的异常 payload 统计。
    uint64_t zero_bytes_frames;
    uint64_t bytes_over_sizeimage_frames;

    // DQ 时刻（用户态 MONOTONIC）间隔统计，反映“到帧节奏”抖动。
    bool has_last_dq_host_ms;
    double last_dq_host_ms;
    uint64_t dq_interval_count;
    double dq_interval_sum_ms;
    double dq_interval_min_ms;
    double dq_interval_max_ms;

    // v4l2_buffer.timestamp 间隔统计（由驱动填充）。
    // 需要结合 flags 的 TIMESTAMP_MASK/TSTAMP_SRC_MASK 一起解释。
    bool has_last_v4l2_ts_ns;
    uint64_t last_v4l2_ts_ns;
    uint64_t v4l2_ts_zero_count;
    uint64_t v4l2_ts_backward_count;
    uint64_t v4l2_ts_interval_count;
    double v4l2_ts_interval_sum_ms;
    double v4l2_ts_interval_min_ms;
    double v4l2_ts_interval_max_ms;

    // 首帧/末帧时间戳（毫秒，CLOCK_MONOTONIC）。
    double first_frame_ms;
    double last_frame_ms;
};

/*
字段速查（实战最常看）：
1) select_timeout: 设备是否“长时间没给帧”
2) dq_fail/requeue_fail: 队列环是否被破坏
3) bytes_hist: 帧大小是否稳定
4) sequence_gap_frames: 是否疑似掉帧
5) dq_interval_*: 用户态看到的到帧节奏抖动
6) v4l2_ts_*: 驱动时间戳是否健康
*/

}  // namespace stage01_v4l2

#endif  // STAGE01_V4L2_TYPES_HPP_
