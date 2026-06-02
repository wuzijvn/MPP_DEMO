#ifndef STAGE01_V4L2_STATS_HPP_
#define STAGE01_V4L2_STATS_HPP_

#include <linux/videodev2.h>
#include <stdio.h>
#include <sys/time.h>

#include <map>
#include <string>

#include "stage01_v4l2_common.hpp"
#include "stage01_v4l2_types.hpp"

namespace stage01_v4l2 {

/*
===============================================================================
Stage01 Statistics and Log Interpretation
===============================================================================

你写驱动/多媒体链路时，最容易踩的坑是：
“程序看起来在跑，但不知道它到底健康不健康”。

这个文件的作用就是把状态变成可观测数字：
1) 协商结果（fmt/parm）
2) 采集行为（select/dq/qbuf）
3) 负载行为（bytes/interval/flags）
4) 异常行为（timeout/eagain/zero-bytes/backward-ts）

学习方式：
1) 先看 print_* 函数输出项；
2) 再回头理解每个 update_* 如何累计这些统计。
*/

// print_caps:
//   打印设备能力信息，作为初始化第一手证据。
//
// 重点：
//   cap.capabilities 包含“总能力”；
//   当包含 V4L2_CAP_DEVICE_CAPS 时，实际节点能力应看 cap.device_caps。
inline void print_caps(const v4l2_capability& cap) {
    uint32_t caps = cap.capabilities;
    uint32_t dev_caps = (caps & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : caps;

    // 这些信息建议每次实验保存到日志，便于后续横向对比设备差异。
    printf("device info:\n");
    printf("  driver      : %s\n", cap.driver);
    printf("  card        : %s\n", cap.card);
    printf("  bus_info    : %s\n", cap.bus_info);
    printf("  version     : %u.%u.%u\n",
           (cap.version >> 16) & 0xFF,
           (cap.version >> 8) & 0xFF,
           cap.version & 0xFF);
    printf("  capabilities: 0x%08x\n", caps);
    printf("  device_caps : 0x%08x\n", dev_caps);
}

// print_fmt_compare:
//   打印格式协商三元对照：
//   request -> S_FMT 返回 -> G_FMT 回读。
//
// 为什么必须打印：
// 1) 驱动可能改分辨率；
// 2) 驱动可能替换像素格式；
// 3) bytesperline/sizeimage 常被对齐修正。
inline void print_fmt_compare(const v4l2_format& req_fmt,
                              const v4l2_format& active_fmt,
                              const v4l2_format& g_fmt) {
    printf("format compare (request vs active S_FMT result vs G_FMT readback):\n");
    // request：用户态请求值
    printf("  request : %ux%u fourcc=%s field=%u\n",
           req_fmt.fmt.pix.width,
           req_fmt.fmt.pix.height,
           fourcc_to_string(req_fmt.fmt.pix.pixelformat).c_str(),
           req_fmt.fmt.pix.field);

    // active(S_FMT)：S_FMT 返回时驱动接受/调整后的值
    printf("  active(S_FMT): %ux%u fourcc=%s field=%u bytesperline=%u sizeimage=%u\n",
           active_fmt.fmt.pix.width,
           active_fmt.fmt.pix.height,
           fourcc_to_string(active_fmt.fmt.pix.pixelformat).c_str(),
           active_fmt.fmt.pix.field,
           active_fmt.fmt.pix.bytesperline,
           active_fmt.fmt.pix.sizeimage);

    // readback(G_FMT)：再次回读的当前生效值（最可信）
    printf("  readback(G_FMT): %ux%u fourcc=%s field=%u bytesperline=%u sizeimage=%u\n",
           g_fmt.fmt.pix.width,
           g_fmt.fmt.pix.height,
           fourcc_to_string(g_fmt.fmt.pix.pixelformat).c_str(),
           g_fmt.fmt.pix.field,
           g_fmt.fmt.pix.bytesperline,
           g_fmt.fmt.pix.sizeimage);
}

// print_parm_compare:
//   打印帧率协商三元对照：
//   request fps -> S_PARM 返回 -> G_PARM 回读。
//
// 注意：
//   很多设备（尤其 UVC）对 S_PARM 支持有限，
//   所以“请求值 != 生效值”并不一定是 bug。
inline void print_parm_compare(int req_fps,
                               const v4l2_streamparm& active_parm,
                               const v4l2_streamparm& g_parm,
                               bool s_parm_ok) {
    printf("fps compare (request vs active S_PARM result vs G_PARM readback):\n");
    printf("  request fps: %d\n", req_fps);

    if (s_parm_ok && active_parm.parm.capture.timeperframe.numerator > 0) {
        double s_fps = (double)active_parm.parm.capture.timeperframe.denominator /
                       (double)active_parm.parm.capture.timeperframe.numerator;
        printf("  active(S_PARM): tpf=%u/%u => %.3f fps\n",
               active_parm.parm.capture.timeperframe.numerator,
               active_parm.parm.capture.timeperframe.denominator,
               s_fps);
    } else {
        printf("  active(S_PARM): not available (driver may ignore/deny S_PARM)\n");
    }

    if (g_parm.parm.capture.timeperframe.numerator > 0) {
        double g_fps = (double)g_parm.parm.capture.timeperframe.denominator /
                       (double)g_parm.parm.capture.timeperframe.numerator;
        printf("  readback(G_PARM): tpf=%u/%u => %.3f fps\n",
               g_parm.parm.capture.timeperframe.numerator,
               g_parm.parm.capture.timeperframe.denominator,
               g_fps);
    } else {
        printf("  readback(G_PARM): driver returned zero denominator/numerator\n");
    }
}

// init_stats:
//   显式初始化统计结构，避免脏值。
inline void init_stats(CaptureStats* st) {
    st->select_calls = 0;
    st->select_ready = 0;
    st->select_timeout = 0;
    st->select_eintr = 0;
    st->select_error = 0;
    st->dq_ok = 0;
    st->dq_fail = 0;
    st->dq_eagain = 0;
    st->requeue_ok = 0;
    st->requeue_fail = 0;
    st->requeue_skipped = 0;
    st->bytes_total = 0;
    // min 初始化成最大无符号值，便于第一次更新正确收敛。
    st->bytes_min = 0xFFFFFFFFU;
    st->bytes_max = 0U;
    st->bytes_hist.clear();
    st->has_last_sequence = false;
    st->last_sequence = 0;
    st->sequence_gap_frames = 0;
    st->flags_hist.clear();
    st->zero_bytes_frames = 0;
    st->bytes_over_sizeimage_frames = 0;
    st->has_last_dq_host_ms = false;
    st->last_dq_host_ms = 0.0;
    st->dq_interval_count = 0;
    st->dq_interval_sum_ms = 0.0;
    st->dq_interval_min_ms = 0.0;
    st->dq_interval_max_ms = 0.0;
    st->has_last_v4l2_ts_ns = false;
    st->last_v4l2_ts_ns = 0;
    st->v4l2_ts_zero_count = 0;
    st->v4l2_ts_backward_count = 0;
    st->v4l2_ts_interval_count = 0;
    st->v4l2_ts_interval_sum_ms = 0.0;
    st->v4l2_ts_interval_min_ms = 0.0;
    st->v4l2_ts_interval_max_ms = 0.0;
    st->first_frame_ms = 0.0;
    st->last_frame_ms = 0.0;
}

// update_bytes_hist:
//   汇总单帧 bytesused 到分布统计。
inline void update_bytes_hist(CaptureStats* st, uint32_t bytesused) {
    // bytes_total 可用于后续算平均帧字节大小。
    st->bytes_total += bytesused;
    if (bytesused < st->bytes_min) st->bytes_min = bytesused;
    if (bytesused > st->bytes_max) st->bytes_max = bytesused;
    st->bytes_hist[bytesused] += 1;
}

// update_sequence_gap:
//   基于 sequence 检测“中间跳帧”数量。
//
// 示例：
//   上一帧 seq=100，本帧 seq=103，则 gap 增加 2（101/102）。
inline void update_sequence_gap(CaptureStats* st, uint32_t seq) {
    if (st->has_last_sequence) {
        // 仅统计“向前跳”的 gap；
        // 非单调回退场景另有 timestamp/backward 指标辅助判断。
        if (seq > st->last_sequence + 1U) {
            st->sequence_gap_frames += (uint64_t)(seq - st->last_sequence - 1U);
        }
    }
    st->last_sequence = seq;
    st->has_last_sequence = true;
}

// tv_to_ns:
//   把 struct timeval 转成纳秒，便于计算相邻帧 timestamp 间隔。
inline uint64_t tv_to_ns(const timeval& tv) {
    if (tv.tv_sec < 0 || tv.tv_usec < 0) return 0;
    // timeval 的微秒字段转换成纳秒时需要 *1000。
    return (uint64_t)tv.tv_sec * 1000000000ULL + (uint64_t)tv.tv_usec * 1000ULL;
}

// flags_to_text:
//   把常见 V4L2 buffer flags 转成可读串，便于日志/CSV审阅。
//
// 注意：
//   这里只覆盖“采集分析高频”标志，不追求列全所有历史 flag。
inline std::string flags_to_text(uint32_t flags) {
    std::string out;
    char tmp[96];
    snprintf(tmp, sizeof(tmp), "0x%08x", flags);
    out = tmp;

    uint32_t ts_mask = flags & V4L2_BUF_FLAG_TIMESTAMP_MASK;
    uint32_t src_mask = flags & V4L2_BUF_FLAG_TSTAMP_SRC_MASK;

    // flags 有助于判断“帧质量/时间戳来源/编码帧类型”。
    if (flags & V4L2_BUF_FLAG_ERROR) out += "|ERROR";
    if (flags & V4L2_BUF_FLAG_KEYFRAME) out += "|KEYFRAME";
    if (flags & V4L2_BUF_FLAG_PFRAME) out += "|PFRAME";
    if (flags & V4L2_BUF_FLAG_BFRAME) out += "|BFRAME";
    if (flags & V4L2_BUF_FLAG_LAST) out += "|LAST";

    if (ts_mask == V4L2_BUF_FLAG_TIMESTAMP_UNKNOWN) out += "|TS_UNKNOWN";
    if (ts_mask == V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC) out += "|TS_MONOTONIC";
    if (ts_mask == V4L2_BUF_FLAG_TIMESTAMP_COPY) out += "|TS_COPY";

    if (src_mask == V4L2_BUF_FLAG_TSTAMP_SRC_EOF) out += "|SRC_EOF";
    if (src_mask == V4L2_BUF_FLAG_TSTAMP_SRC_SOE) out += "|SRC_SOE";

    return out;
}

// maybe_warn_payload_shape:
//   统计一些 payload 异常形态，帮助快速定位链路不一致问题。
inline void maybe_warn_payload_shape(const v4l2_format& active_fmt,
                                     const v4l2_buffer& buf,
                                     CaptureStats* st) {
    if (buf.bytesused == 0) {
        st->zero_bytes_frames++;
    }

    // sizeimage 是驱动声明的单帧缓存上界之一，bytesused 超过它需要重点排查。
    uint32_t sizeimage = active_fmt.fmt.pix.sizeimage;
    if (sizeimage > 0 && buf.bytesused > sizeimage) {
        // 这类异常优先检查：
        // 1) 驱动 sizeimage 计算
        // 2) 多平面/单平面使用是否匹配
        // 3) 用户态 buffer 类型是否选错
        st->bytes_over_sizeimage_frames++;
    }
}

// update_dq_host_interval:
//   记录用户态 DQ 时间间隔统计（MONOTONIC now_ms）。
inline void update_dq_host_interval(CaptureStats* st, double now_ms_host) {
    if (st->has_last_dq_host_ms) {
        // host 侧间隔用于观测“应用实际看到的到帧节奏”。
        double d = now_ms_host - st->last_dq_host_ms;
        if (d >= 0.0) {
            st->dq_interval_count++;
            st->dq_interval_sum_ms += d;
            if (st->dq_interval_count == 1) {
                st->dq_interval_min_ms = d;
                st->dq_interval_max_ms = d;
            } else {
                if (d < st->dq_interval_min_ms) st->dq_interval_min_ms = d;
                if (d > st->dq_interval_max_ms) st->dq_interval_max_ms = d;
            }
        }
    }
    st->has_last_dq_host_ms = true;
    st->last_dq_host_ms = now_ms_host;
}

// update_v4l2_timestamp_interval:
//   记录驱动侧 timestamp 间隔统计。
inline void update_v4l2_timestamp_interval(const v4l2_buffer& buf, CaptureStats* st) {
    uint64_t ts_ns = tv_to_ns(buf.timestamp);
    if (ts_ns == 0) {
        // 某些驱动/模式下可能不给有效 timestamp，这里单独计数而不是直接忽略。
        st->v4l2_ts_zero_count++;
        return;
    }

    if (st->has_last_v4l2_ts_ns) {
        if (ts_ns < st->last_v4l2_ts_ns) {
            // 时间戳回退通常意味着驱动侧时间基异常或模式切换。
            st->v4l2_ts_backward_count++;
        } else {
            double d_ms = (double)(ts_ns - st->last_v4l2_ts_ns) / 1000000.0;
            st->v4l2_ts_interval_count++;
            st->v4l2_ts_interval_sum_ms += d_ms;
            if (st->v4l2_ts_interval_count == 1) {
                st->v4l2_ts_interval_min_ms = d_ms;
                st->v4l2_ts_interval_max_ms = d_ms;
            } else {
                if (d_ms < st->v4l2_ts_interval_min_ms) st->v4l2_ts_interval_min_ms = d_ms;
                if (d_ms > st->v4l2_ts_interval_max_ms) st->v4l2_ts_interval_max_ms = d_ms;
            }
        }
    }

    st->has_last_v4l2_ts_ns = true;
    st->last_v4l2_ts_ns = ts_ns;
}

// print_stats:
//   打印本次采集汇总。
//
// 你每次实验至少关注：
// 1) fps
// 2) timeout
// 3) dq_fail
// 4) requeue_fail/skipped
// 5) bytesused 分布
inline void print_stats(const AppConfig& cfg,
                        const v4l2_format& active_fmt,
                        const CaptureStats& st,
                        double stream_on_ms,
                        double loop_end_ms) {
    double elapsed_ms = (loop_end_ms > stream_on_ms) ? (loop_end_ms - stream_on_ms) : 0.0;
    double elapsed_sec = elapsed_ms / 1000.0;
    double fps = (elapsed_sec > 0.0) ? ((double)st.dq_ok / elapsed_sec) : 0.0;

    printf("\n================ capture summary ================\n");
    printf("config:\n");
    printf("  dev=%s req=%dx%d frames=%d warmup=%d req_fps=%d timeout_ms=%d req_bufs=%u inject=%s inject_frame=%d log_every=%d trace_csv=%s\n",
           cfg.dev.c_str(),
           cfg.req_width,
           cfg.req_height,
           cfg.total_frames,
           cfg.warmup_frames,
           cfg.req_fps,
           cfg.timeout_ms,
           cfg.req_buf_count,
           cfg.inject.c_str(),
           cfg.inject_frame,
           cfg.log_every,
           cfg.trace_csv.empty() ? "(disabled)" : cfg.trace_csv.c_str());

    printf("active format snapshot:\n");
    printf("  width=%u height=%u fourcc=%s bytesperline=%u sizeimage=%u\n",
           active_fmt.fmt.pix.width,
           active_fmt.fmt.pix.height,
           fourcc_to_string(active_fmt.fmt.pix.pixelformat).c_str(),
           active_fmt.fmt.pix.bytesperline,
           active_fmt.fmt.pix.sizeimage);

    printf("timing:\n");
    printf("  stream duration: %.3f s\n", elapsed_sec);
    printf("  fps(actual dq_ok / duration): %.3f\n", fps);

    printf("select:\n");
    printf("  calls=%llu ready=%llu timeout=%llu eintr=%llu error=%llu\n",
           (unsigned long long)st.select_calls,
           (unsigned long long)st.select_ready,
           (unsigned long long)st.select_timeout,
           (unsigned long long)st.select_eintr,
           (unsigned long long)st.select_error);

    printf("dqbuf:\n");
    printf("  ok=%llu fail=%llu eagain=%llu\n",
           (unsigned long long)st.dq_ok,
           (unsigned long long)st.dq_fail,
           (unsigned long long)st.dq_eagain);

    printf("qbuf(requeue):\n");
    printf("  ok=%llu fail=%llu skipped=%llu\n",
           (unsigned long long)st.requeue_ok,
           (unsigned long long)st.requeue_fail,
           (unsigned long long)st.requeue_skipped);

    if (st.dq_ok > 0) {
        double avg_bytes = (double)st.bytes_total / (double)st.dq_ok;
        printf("bytesused:\n");
        printf("  min=%u max=%u avg=%.2f\n", st.bytes_min, st.bytes_max, avg_bytes);
        printf("  distribution (bytesused => count):\n");
        for (std::map<uint32_t, uint64_t>::const_iterator it = st.bytes_hist.begin(); it != st.bytes_hist.end(); ++it) {
            printf("    %u => %llu\n", it->first, (unsigned long long)it->second);
        }
    } else {
        printf("bytesused:\n");
        printf("  no dequeued frames\n");
    }

    printf("sequence:\n");
    printf("  observed_gap_frames=%llu (仅作驱动行为观察，不是唯一丢帧标准)\n",
           (unsigned long long)st.sequence_gap_frames);

    printf("payload shape:\n");
    printf("  zero_bytes_frames=%llu bytes_over_sizeimage_frames=%llu\n",
           (unsigned long long)st.zero_bytes_frames,
           (unsigned long long)st.bytes_over_sizeimage_frames);

    if (st.dq_interval_count > 0) {
        double avg_ms = st.dq_interval_sum_ms / (double)st.dq_interval_count;
        printf("dq host interval(ms):\n");
        printf("  count=%llu min=%.3f max=%.3f avg=%.3f\n",
               (unsigned long long)st.dq_interval_count,
               st.dq_interval_min_ms,
               st.dq_interval_max_ms,
               avg_ms);
    } else {
        printf("dq host interval(ms):\n");
        printf("  not enough frames\n");
    }

    if (st.v4l2_ts_interval_count > 0 || st.v4l2_ts_zero_count > 0 || st.v4l2_ts_backward_count > 0) {
        printf("v4l2 timestamp interval(ms):\n");
        if (st.v4l2_ts_interval_count > 0) {
            double avg_ms = st.v4l2_ts_interval_sum_ms / (double)st.v4l2_ts_interval_count;
            printf("  count=%llu min=%.3f max=%.3f avg=%.3f\n",
                   (unsigned long long)st.v4l2_ts_interval_count,
                   st.v4l2_ts_interval_min_ms,
                   st.v4l2_ts_interval_max_ms,
                   avg_ms);
        } else {
            printf("  no valid positive interval\n");
        }
        printf("  zero_ts=%llu backward_ts=%llu\n",
               (unsigned long long)st.v4l2_ts_zero_count,
               (unsigned long long)st.v4l2_ts_backward_count);
    }

    if (!st.flags_hist.empty()) {
        printf("buffer flags distribution:\n");
        for (std::map<uint32_t, uint64_t>::const_iterator it = st.flags_hist.begin();
             it != st.flags_hist.end();
             ++it) {
            std::string text = flags_to_text(it->first);
            printf("  %s => %llu\n", text.c_str(), (unsigned long long)it->second);
        }
    }

    printf("=================================================\n\n");
}

}  // namespace stage01_v4l2

#endif  // STAGE01_V4L2_STATS_HPP_
