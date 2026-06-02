#ifndef STAGE02_V4L2_TYPES_HPP_
#define STAGE02_V4L2_TYPES_HPP_

#include <linux/videodev2.h>
#include <stdint.h>
#include <sys/time.h>

#include <deque>
#include <map>
#include <string>
#include <vector>

namespace stage02_v4l2 {

/*
===============================================================================
Stage02 Data Model
===============================================================================

本文件定义 Stage02 所有核心数据结构。
与 Stage01 相比，新增重点在于：
1) controls 元信息结构（ControlInfo / ControlSetRequest）
2) 线程间传递结构（FramePacket / QueueState）
3) 稳定性指标结构（Stage2Stats）
*/

// Buffer:
//   描述一个通过 MMAP 映射到用户态的驱动缓冲。
//
// 字段语义：
// - start: mmap 返回的起始地址
// - length: 该缓冲区总长度（通常来自 QUERYBUF.length）
//
// 典型踩坑：
// 1) 把它当 malloc 内存来 free（错误）；
// 2) 忘记在清理阶段 munmap，导致资源泄漏。
struct Buffer {
    void* start;
    size_t length;
};

// ControlInfo:
//   保存一个 V4L2 控制项的静态描述信息（由 QUERYCTRL 获得）。
//
// 这些字段用于：
// 1) 列表展示（让你看见设备“可调什么”）；
// 2) 参数校验（是否越界、是否步长匹配）；
// 3) 自动化脚本映射（按 name 或 id 设置）。
struct ControlInfo {
    uint32_t id;
    std::string name;
    uint32_t type;
    int32_t minimum;
    int32_t maximum;
    int32_t step;
    int32_t default_value;
    uint32_t flags;
};

// ControlSetRequest:
//   命令行传入的单个控制项设置请求（KEY=VAL）。
//
// KEY 支持：
// 1) 控制项归一化名称（如 brightness）
// 2) 十六进制 id（如 0x00980900）
struct ControlSetRequest {
    std::string key;
    int value;
};

// AppConfig:
//   Stage2 运行配置（默认值 + 命令行解析结果）。
//
// 相比 Stage1，本阶段新增重点：
// 1) controls 配置（list/set）
// 2) 线程队列与 backpressure 策略
// 3) 超时恢复策略
// 4) 时间驱动的稳定性测试模式
struct AppConfig {
    std::string dev;
    int req_width;
    int req_height;
    std::string req_pixfmt;
    int req_fps;
    int timeout_ms;
    unsigned int req_buf_count;

    // 采集停止条件：
    // 1) 若 duration_sec > 0，按时间停止（稳定性跑测常用）
    // 2) 否则按 total_frames 停止
    int duration_sec;
    int total_frames;

    // 控制项相关。
    bool list_ctrls;
    std::vector<ControlSetRequest> set_ctrls;

    // 线程队列与写盘策略。
    int queue_depth;
    std::string queue_policy;  // drop-oldest | block
    int writer_delay_ms;       // 模拟慢写盘延迟
    bool no_save;
    int dump_every;            // 每 N 帧保存一次 raw，0 表示不保存
    std::string out_dir;
    int log_every;

    // 发生 timeout 时是否尝试自动恢复（STREAMOFF+RESTART）。
    bool recover_on_timeout;
    int max_recoveries;
};

// FramePacket:
//   capture 线程送给 writer 线程的“单帧载体”。
//
// 为什么需要它：
// 1) 采集线程 DQ 后要尽快 Q 回，不能卡在写盘；
// 2) 所以复制必要数据后入队，让 writer 异步消费。
//
// 字段中保留 seq/flags/timestamp 是为了后续定位异常帧。
struct FramePacket {
    std::vector<unsigned char> data;
    uint64_t frame_no;
    uint32_t seq;
    uint32_t bytesused;
    uint32_t flags;
    timeval v4l2_ts;
    double host_dq_ms;
};

// QueueState:
//   capture->writer 的共享队列状态。
//
// policy:
// - drop-oldest: 队列满时丢最旧帧，优先实时性
// - block      : 队列满时阻塞生产者，优先完整性
//
// 统计字段含义：
// - dropped_oldest: 因为队列满而主动丢帧次数
// - blocked_waits : 生产者等待队列空位次数
// - peak_depth    : 运行期间队列峰值深度
//
// 这些值是“是否背压、背压多严重”的直接证据。
struct QueueState {
    std::deque<FramePacket> q;
    size_t max_depth;
    std::string policy;
    bool stop;

    // backpressure 统计。
    uint64_t dropped_oldest;
    uint64_t blocked_waits;
    uint64_t peak_depth;

    QueueState()
        : max_depth(64),
          policy("drop-oldest"),
          stop(false),
          dropped_oldest(0),
          blocked_waits(0),
          peak_depth(0) {}
};

// Stage2Stats:
//   稳定性跑测统计总表。
//
// 建议你每次实验至少记录：
// 1) dq_ok/dq_fail/select_timeout
// 2) recoveries_ok/fail
// 3) dropped_oldest 或 blocked_waits
// 4) host interval min/max/avg
//
// 这些指标能支持你写出“发生了什么、为什么、如何改进”的结论。
struct Stage2Stats {
    // 基础运行统计。
    uint64_t select_calls;
    uint64_t select_ready;
    uint64_t select_timeout;
    uint64_t select_eintr;
    uint64_t select_error;

    uint64_t dq_ok;
    uint64_t dq_fail;
    uint64_t dq_eagain;

    uint64_t qbuf_ok;
    uint64_t qbuf_fail;

    // 错误分类与恢复。
    uint64_t error_flag_frames;
    uint64_t zero_bytes_frames;
    uint64_t recoveries_attempted;
    uint64_t recoveries_ok;
    uint64_t recoveries_fail;

    // writer 统计。
    uint64_t writer_frames;
    uint64_t writer_bytes;
    uint64_t writer_dumped_files;
    uint64_t writer_dump_fail;

    // 时间统计（host dq interval）。
    bool has_last_host_ms;
    double last_host_ms;
    uint64_t host_interval_count;
    double host_interval_sum_ms;
    double host_interval_min_ms;
    double host_interval_max_ms;

    // 其他分布统计。
    std::map<uint32_t, uint64_t> bytes_hist;
    std::map<uint32_t, uint64_t> flags_hist;
};

// init_stage2_stats:
//   对统计结构做显式初始化，避免脏值污染结论。
//
// 工程上不要依赖“默认构造可能是0”这种不稳定假设，
// 显式初始化更可靠，也便于后续扩展字段。
inline void init_stage2_stats(Stage2Stats* st) {
    // 显式“逐字段清零”虽然稍长，但最清晰、最不易漏字段。
    st->select_calls = 0;
    st->select_ready = 0;
    st->select_timeout = 0;
    st->select_eintr = 0;
    st->select_error = 0;
    st->dq_ok = 0;
    st->dq_fail = 0;
    st->dq_eagain = 0;
    st->qbuf_ok = 0;
    st->qbuf_fail = 0;
    st->error_flag_frames = 0;
    st->zero_bytes_frames = 0;
    st->recoveries_attempted = 0;
    st->recoveries_ok = 0;
    st->recoveries_fail = 0;
    st->writer_frames = 0;
    st->writer_bytes = 0;
    st->writer_dumped_files = 0;
    st->writer_dump_fail = 0;
    st->has_last_host_ms = false;
    st->last_host_ms = 0.0;
    st->host_interval_count = 0;
    st->host_interval_sum_ms = 0.0;
    st->host_interval_min_ms = 0.0;
    st->host_interval_max_ms = 0.0;
    st->bytes_hist.clear();
    st->flags_hist.clear();
}

}  // namespace stage02_v4l2

#endif  // STAGE02_V4L2_TYPES_HPP_
