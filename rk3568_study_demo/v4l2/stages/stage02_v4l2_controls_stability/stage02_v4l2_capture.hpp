#ifndef STAGE02_V4L2_CAPTURE_HPP_
#define STAGE02_V4L2_CAPTURE_HPP_

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "stage02_v4l2_common.hpp"
#include "stage02_v4l2_ctrls.hpp"
#include "stage02_v4l2_types.hpp"

namespace stage02_v4l2 {

/*
===============================================================================
Stage02 Capture Core (Controls + Queue + Recovery)
===============================================================================

本文件是 Stage02 最核心实现，建议你按三条主线理解：
1) 控制面：controls 枚举与设置（功能正确性）
2) 数据面：DQ -> 队列 -> writer（吞吐与时延）
3) 恢复面：timeout 后重启策略（稳定性与可用性）

阅读方法：
1) 先看 run_stage02 的 1~10 步骤标题；
2) 再看两个分支（带 writer / 纯采集）的差异；
3) 最后回看 summary 字段，建立“指标 -> 代码来源”的映射。

你要重点建立的 4 个工程不变量：
1) 只要 DQ 成功，最终就必须 Q 回（除非进程退出）。
2) 任何时候发生 timeout，都要先区分“偶发抖动”还是“链路卡死”。
3) 采集线程不能被慢写盘拖死（所以要解耦队列 + 明确背压策略）。
4) 无论成功失败，cleanup 必须可重复执行且不泄漏资源。

你会在这个文件里看到两种“吞吐取舍”：
1) `drop-oldest`：优先实时性，允许丢旧帧。
2) `block`：优先完整性，允许采集线程等待。
这两个策略没有绝对对错，取决于业务目标（预览 vs 取证/录制）。
*/

// WriterContext:
//   写线程上下文，把主线程中需要共享的信息集中封装。
//
// 设计动机：
// - pthread_create 只能传一个 void* 参数；
// - 用结构体打包可读性更高，也便于后续扩展。
struct WriterContext {
    // 共享队列状态（生产者/消费者都读写）。
    QueueState* qs;
    // 汇总统计（writer 会累计自身指标）。
    Stage2Stats* st;
    // 线程内使用配置快照，避免跨线程读取可变对象。
    AppConfig cfg;
    // 互斥锁 + 两个条件变量：
    // - not_empty: 消费者等“有数据”
    // - not_full : 生产者等“有空位”
    pthread_mutex_t* mu;
    pthread_cond_t* cv_not_empty;
    pthread_cond_t* cv_not_full;
};

// update_host_interval:
//   统计相邻 DQ 的 host 侧时间间隔（毫秒）。
//
// 这些指标用于：
// 1) 判断采集节奏是否平稳；
// 2) 观察系统负载/调度抖动；
// 3) 对比不同 queue 策略的副作用。
inline void update_host_interval(Stage2Stats* st, double now) {
    // 只在“有上一帧时间点”时才能计算 delta。
    if (st->has_last_host_ms) {
        double d = now - st->last_host_ms;
        if (d >= 0.0) {
            st->host_interval_count++;
            st->host_interval_sum_ms += d;
            if (st->host_interval_count == 1) {
                st->host_interval_min_ms = d;
                st->host_interval_max_ms = d;
            } else {
                if (d < st->host_interval_min_ms) st->host_interval_min_ms = d;
                if (d > st->host_interval_max_ms) st->host_interval_max_ms = d;
            }
        }
    }
    st->has_last_host_ms = true;
    st->last_host_ms = now;
}

// ensure_dir:
//   确保输出目录存在。
//
// 约定：
// - 已存在目录返回 true；
// - 创建成功返回 true；
// - 其他错误返回 false。
inline bool ensure_dir(const std::string& p) {
    if (p.empty()) return false;
    if (mkdir(p.c_str(), 0755) == 0) return true;
    // 目录已存在也是成功语义。
    return errno == EEXIST;
}

// dump_raw_frame:
//   把单帧原始数据写到文件，文件名包含 frame_no + sequence。
//
// 为什么要带 seq：
// - 当你做异常帧排查时，可以把“日志中的 seq”与“落盘文件”直接对应。
inline bool dump_raw_frame(const std::string& out_dir, const FramePacket& pkt, uint64_t* dumped_files) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/frame_%06llu_seq_%u.raw",
             out_dir.c_str(),
             (unsigned long long)pkt.frame_no,
             pkt.seq);
    FILE* fp = fopen(path, "wb");
    if (!fp) return false;
    size_t w = fwrite(pkt.data.data(), 1, pkt.data.size(), fp);
    fclose(fp);
    if (w != pkt.data.size()) return false;
    // 输出计数递增用于 summary 对账。
    if (dumped_files) (*dumped_files)++;
    return true;
}

// writer_thread_main:
//   消费队列中的 FramePacket，执行异步写盘和统计。
//
// 关键语义：
// 1) 采集线程“生产”帧，writer 线程“消费”帧；
// 2) 条件变量避免忙等；
// 3) stop=true 且队列空时退出。
//
// 常见坑：
// - 忘记 signal/broadcast 导致线程卡死；
// - 队列锁范围太大，拖慢采集。
inline void* writer_thread_main(void* arg) {
    WriterContext* w = (WriterContext*)arg;
    QueueState* qs = w->qs;
    Stage2Stats* st = w->st;

    if (!w->cfg.no_save) {
        // 目录创建失败不直接退出线程，后续 dump 会统计为失败。
        // 这样做是为了“不让写目录问题影响采集主循环”。
        ensure_dir(w->cfg.out_dir);
    }

    for (;;) {
        // 阶段A：等待有帧可消费或收到停止信号。
        pthread_mutex_lock(w->mu);
        // 这里必须用 while，不能用 if：
        // 1) 条件变量允许 spurious wakeup（伪唤醒）；
        // 2) 多线程竞争下，醒来时条件可能已被其他线程改变。
        while (qs->q.empty() && !qs->stop) {
            pthread_cond_wait(w->cv_not_empty, w->mu);
        }
        if (qs->q.empty() && qs->stop) {
            pthread_mutex_unlock(w->mu);
            break;
        }
        // 阶段B：从队列取一帧，立即释放锁，减少临界区时长。
        FramePacket pkt = qs->q.front();
        qs->q.pop_front();
        // 消费后 signal not_full，唤醒可能阻塞的生产者。
        pthread_cond_signal(w->cv_not_full);
        pthread_mutex_unlock(w->mu);

        // 阶段C：更新 writer 侧吞吐统计。
        st->writer_frames++;
        st->writer_bytes += pkt.data.size();

        // 阶段D：按 dump_every 节奏抽样落盘。
        if (!w->cfg.no_save && w->cfg.dump_every > 0 && (pkt.frame_no % (uint64_t)w->cfg.dump_every) == 0U) {
            if (!dump_raw_frame(w->cfg.out_dir, pkt, &st->writer_dumped_files)) {
                st->writer_dump_fail++;
            }
        }

        // 阶段E：可选慢写盘模拟，用于 backpressure 压测。
        if (w->cfg.writer_delay_ms > 0) {
            usleep((useconds_t)w->cfg.writer_delay_ms * 1000U);
        }
    }
    return NULL;
}

// requeue_all_buffers:
//   将所有 MMAP buffer 初始入队（或恢复后重入队）。
// 
// 这是 streaming 模型的前置条件：
// - 驱动必须拿到可写 buffer，才能持续产出帧。
inline bool requeue_all_buffers(int fd, size_t nbufs, Stage2Stats* st) {
    for (size_t i = 0; i < nbufs; ++i) {
        v4l2_buffer b;
        memset(&b, 0, sizeof(b));
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = (unsigned int)i;
        if (xioctl(fd, VIDIOC_QBUF, &b) < 0) {
            st->qbuf_fail++;
            return false;
        }
        // 注意：这里统计的是“所有 QBUF 成功次数”，
        // 包含初始化入队 + 恢复重入队 + 常规回队。
        st->qbuf_ok++;
    }
    return true;
} 

// restart_stream:
//   timeout 恢复路径：STREAMOFF -> 全量 QBUF -> STREAMON。
//
// 适用场景：
// - 采集链路短暂卡死，需要尝试软重启。
//
// 注意：
// - 这不是万能恢复，只是“低成本第一步”；
// - 若多次失败，通常需要更深层排查（驱动/硬件/供电/链路）。
inline bool restart_stream(int fd, size_t nbufs, Stage2Stats* st) {
    v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    // 顺序不能反：必须先停流，再重建入队，再开流。
    if (xioctl(fd, VIDIOC_STREAMOFF, &t) < 0) {
        return false;
    }
    // 根据 V4L2 streaming 语义，STREAMOFF 会让队列回到“可重建”状态，
    // 这里重新全量 QBUF，随后 STREAMON 尝试恢复采集环。
    if (!requeue_all_buffers(fd, nbufs, st)) return false;
    if (xioctl(fd, VIDIOC_STREAMON, &t) < 0) {
        return false;
    }
    return true;
}

// to_fourcc:
//   把 4 字符字符串转 fourcc 整数。
//
// 前提：
// - parse_args 已保证字符串长度为 4。
inline uint32_t to_fourcc(const std::string& s) {
    return v4l2_fourcc((unsigned char)s[0], (unsigned char)s[1], (unsigned char)s[2], (unsigned char)s[3]);
}

// print_summary:
//   打印 Stage2 汇总报告。
//
// 建议你读这个 summary 的顺序：
// 1) timing + fps
// 2) select/dq/qbuf 基础健康度
// 3) error/recovery
// 4) queue/backpressure
// 5) host interval 抖动
inline void print_summary(const AppConfig& cfg,
                          const v4l2_format& active_fmt,
                          const Stage2Stats& st,
                          const QueueState& qs,
                          double start_ms,
                          double end_ms) {
    double sec = (end_ms > start_ms) ? (end_ms - start_ms) / 1000.0 : 0.0;
    double fps = (sec > 0.0) ? ((double)st.dq_ok / sec) : 0.0;

    printf("\n================ stage02 summary ================\n");
    printf("config:\n");
    printf("  dev=%s req=%dx%d pixfmt=%s fps=%d timeout_ms=%d req_bufs=%u duration_sec=%d total_frames=%d\n",
           cfg.dev.c_str(),
           cfg.req_width,
           cfg.req_height,
           cfg.req_pixfmt.c_str(),
           cfg.req_fps,
           cfg.timeout_ms,
           cfg.req_buf_count,
           cfg.duration_sec,
           cfg.total_frames);
    printf("  queue_depth=%d queue_policy=%s writer_delay_ms=%d dump_every=%d no_save=%d out_dir=%s\n",
           cfg.queue_depth,
           cfg.queue_policy.c_str(),
           cfg.writer_delay_ms,
           cfg.dump_every,
           cfg.no_save ? 1 : 0,
           cfg.out_dir.c_str());
    printf("  recover_on_timeout=%d max_recoveries=%d\n",
           cfg.recover_on_timeout ? 1 : 0,
           cfg.max_recoveries);

    printf("active format:\n");
    printf("  %ux%u fourcc=%s bytesperline=%u sizeimage=%u\n",
           active_fmt.fmt.pix.width,
           active_fmt.fmt.pix.height,
           fourcc_to_string(active_fmt.fmt.pix.pixelformat).c_str(),
           active_fmt.fmt.pix.bytesperline,
           active_fmt.fmt.pix.sizeimage);

    printf("timing:\n");
    printf("  duration=%.3f s fps=%.3f\n", sec, fps);

    printf("select:\n");
    printf("  calls=%llu ready=%llu timeout=%llu eintr=%llu error=%llu\n",
           (unsigned long long)st.select_calls,
           (unsigned long long)st.select_ready,
           (unsigned long long)st.select_timeout,
           (unsigned long long)st.select_eintr,
           (unsigned long long)st.select_error);

    printf("capture io:\n");
    printf("  dq_ok=%llu dq_fail=%llu dq_eagain=%llu qbuf_ok=%llu qbuf_fail=%llu\n",
           (unsigned long long)st.dq_ok,
           (unsigned long long)st.dq_fail,
           (unsigned long long)st.dq_eagain,
           (unsigned long long)st.qbuf_ok,
           (unsigned long long)st.qbuf_fail);

    printf("error/recovery:\n");
    printf("  error_flag_frames=%llu zero_bytes_frames=%llu recoveries_attempted=%llu recoveries_ok=%llu recoveries_fail=%llu\n",
           (unsigned long long)st.error_flag_frames,
           (unsigned long long)st.zero_bytes_frames,
           (unsigned long long)st.recoveries_attempted,
           (unsigned long long)st.recoveries_ok,
           (unsigned long long)st.recoveries_fail);

    printf("writer:\n");
    printf("  frames=%llu bytes=%llu dumped_files=%llu dump_fail=%llu\n",
           (unsigned long long)st.writer_frames,
           (unsigned long long)st.writer_bytes,
           (unsigned long long)st.writer_dumped_files,
           (unsigned long long)st.writer_dump_fail);

    printf("queue/backpressure:\n");
    printf("  peak_depth=%llu dropped_oldest=%llu blocked_waits=%llu\n",
           (unsigned long long)qs.peak_depth,
           (unsigned long long)qs.dropped_oldest,
           (unsigned long long)qs.blocked_waits);

    if (st.host_interval_count > 0) {
        double avg = st.host_interval_sum_ms / (double)st.host_interval_count;
        printf("host interval(ms): count=%llu min=%.3f max=%.3f avg=%.3f\n",
               (unsigned long long)st.host_interval_count,
               st.host_interval_min_ms,
               st.host_interval_max_ms,
               avg);
    } else {
        printf("host interval(ms): not enough samples\n");
    }

    printf("flags distribution:\n");
    for (std::map<uint32_t, uint64_t>::const_iterator it = st.flags_hist.begin();
         it != st.flags_hist.end();
         ++it) {
        printf("  0x%08x => %llu\n", it->first, (unsigned long long)it->second);
    }
    printf("===============================================\n\n");
}

inline int run_stage02(const AppConfig& cfg) {
    /*
    ===========================================================================
    run_stage02: 配置参数如何映射到运行行为
    ===========================================================================

    你可以把命令行参数和代码作用点对照起来记：

    1) 设备与格式：
       --pixfmt / width / height -> S_FMT/G_FMT
       --fps                     -> S_PARM（可能被忽略）

    2) 停止条件：
       --duration-sec>0          -> 按绝对时间截止
       --frames                  -> 按 dq_ok 帧数截止

    3) 背压与写盘：
       --queue-depth             -> qs.max_depth
       --queue-policy            -> drop-oldest 或 block
       --writer-delay-ms         -> writer 线程 sleep 模拟慢盘
       --dump-every / --out-dir  -> 抽样写 raw

    4) 稳定性恢复：
       --recover-on-timeout      -> timeout 后是否尝试重启
       --max-recoveries          -> 最多重启次数
    */

    // --------------------------------------------------------------------
    // 0) 资源与运行状态变量
    // --------------------------------------------------------------------
    // fd=-1 表示尚未打开，cleanup 时据此判断是否 close。
    int fd = -1;
    // stream_on 标记流是否已开启，避免重复 STREAMOFF。
    bool stream_on = false;
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    std::vector<Buffer> bufs;
    v4l2_format active_fmt;
    memset(&active_fmt, 0, sizeof(active_fmt));

    Stage2Stats st;
    init_stage2_stats(&st);
    QueueState qs;
    // 队列深度和策略来自命令行配置。
    qs.max_depth = (size_t)cfg.queue_depth;
    qs.policy = cfg.queue_policy;

    pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cv_not_empty = PTHREAD_COND_INITIALIZER;
    pthread_cond_t cv_not_full = PTHREAD_COND_INITIALIZER;
    pthread_t writer_tid;
    bool writer_started = false;
    WriterContext wctx;
    std::vector<ControlInfo> ctrls;

    double start_ms = 0.0;
    double end_ms = 0.0;
    int ret_code = 1;

    // --------------------------------------------------------------------
    // 1) open 设备节点
    // --------------------------------------------------------------------
    fd = open(cfg.dev.c_str(), O_RDWR);
    if (fd < 0) {
        perror("open video device");
        goto cleanup;
    }

    // --------------------------------------------------------------------
    // 2) QUERYCAP：确认节点支持 capture + streaming
    // --------------------------------------------------------------------
    {
        v4l2_capability cap;
        memset(&cap, 0, sizeof(cap));
        if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
            perror("VIDIOC_QUERYCAP");
            goto cleanup;
        }
        uint32_t eff = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;
        bool has_cap = (eff & V4L2_CAP_VIDEO_CAPTURE) != 0;
        bool has_stream = (eff & V4L2_CAP_STREAMING) != 0;
        if (!has_cap || !has_stream) {
            // 这里属于“节点能力不匹配”：
            // 常见是打开了错误视频节点（例如 metadata/输出节点）。
            fprintf(stderr, "device does not support capture+streaming\n");
            goto cleanup;
        }
    }

    // --------------------------------------------------------------------
    // 3) controls 枚举 + 可选设置
    // --------------------------------------------------------------------
    ctrls = enumerate_controls(fd);
    if (cfg.list_ctrls) {
        // list-only 模式：只枚举控件并退出，不进入采集。
        print_controls_table(ctrls);
        ret_code = 0;
        goto cleanup;
    }

    if (!apply_control_requests(fd, cfg.set_ctrls, ctrls)) {
        // 控件失败不一定要终止采集：
        // 实战里经常先继续采集，再结合日志判断是否必须修正控件策略。
        fprintf(stderr, "warning: some controls failed to apply; continue capture\n");
    }

    // --------------------------------------------------------------------
    // 4) S_FMT / G_FMT：格式协商与生效值回读
    // --------------------------------------------------------------------
    {
        v4l2_format fmt;
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = (unsigned int)cfg.req_width;
        fmt.fmt.pix.height = (unsigned int)cfg.req_height;
        fmt.fmt.pix.pixelformat = to_fourcc(cfg.req_pixfmt);
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
        if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
            // 常见根因：
            // 1) 像素格式不支持（EINVAL）
            // 2) 分辨率组合不支持（EINVAL）
            // 下一步：先 list formats/size，再选合法组合。
            perror("VIDIOC_S_FMT");
            goto cleanup;
        }
        // active_fmt 使用 G_FMT 回读结果，作为后续统计输出依据。
        active_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(fd, VIDIOC_G_FMT, &active_fmt) < 0) {
            perror("VIDIOC_G_FMT");
            goto cleanup;
        }
    }

    // --------------------------------------------------------------------
    // 5) S_PARM：帧率请求（可能被驱动忽略）
    // --------------------------------------------------------------------
    {
        v4l2_streamparm p;
        memset(&p, 0, sizeof(p));
        p.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        p.parm.capture.timeperframe.numerator = 1;
        p.parm.capture.timeperframe.denominator = (unsigned int)cfg.req_fps;
        if (xioctl(fd, VIDIOC_S_PARM, &p) < 0) {
            // 很多设备对 S_PARM 支持有限，这里允许继续。
            perror("VIDIOC_S_PARM (continue)");
        }
    }

    // --------------------------------------------------------------------
    // 6) REQBUFS -> QUERYBUF -> MMAP：建立用户态缓冲映射
    // --------------------------------------------------------------------
    {
        v4l2_requestbuffers req;
        memset(&req, 0, sizeof(req));
        req.count = cfg.req_buf_count;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
            perror("VIDIOC_REQBUFS");
            goto cleanup;
        }
        if (req.count < 2) {
            // 缓冲太少会导致环路不稳定，通常不具备持续采集意义。
            fprintf(stderr, "REQBUFS too low: %u\n", req.count);
            goto cleanup;
        }

        bufs.resize(req.count);
        for (unsigned int i = 0; i < req.count; ++i) {
            v4l2_buffer b;
            memset(&b, 0, sizeof(b));
            b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            b.memory = V4L2_MEMORY_MMAP;
            b.index = i;
            if (xioctl(fd, VIDIOC_QUERYBUF, &b) < 0) {
                perror("VIDIOC_QUERYBUF");
                goto cleanup;
            }
            void* start = mmap(NULL, b.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, b.m.offset);
            if (start == MAP_FAILED) {
                perror("mmap");
                goto cleanup;
            }
            bufs[i].start = start;
            bufs[i].length = b.length;
        }
    }

    // --------------------------------------------------------------------
    // 7) 初始 QBUF（全部入队）
    // --------------------------------------------------------------------
    if (!requeue_all_buffers(fd, bufs.size(), &st)) {
        perror("initial QBUF");
        goto cleanup;
    }

    // --------------------------------------------------------------------
    // 8) 分支A：带 writer 线程（推荐真实稳定性测试路径）
    // --------------------------------------------------------------------
    if (!cfg.no_save || cfg.writer_delay_ms > 0) {
        // 进入本分支的条件：
        // 1) 需要真正写盘（no_save=false），或
        // 2) 即使不写盘，也要模拟慢 writer（writer_delay_ms>0）。
        // 
        // 目的：让采集线程和“消费侧”解耦，从而观察背压行为。
        // 当需要写盘或模拟慢写盘时，启用 writer 线程路径。
        wctx.qs = &qs;
        wctx.st = &st;
        wctx.cfg = cfg;
        wctx.mu = &mu;
        wctx.cv_not_empty = &cv_not_empty;
        wctx.cv_not_full = &cv_not_full;
        if (pthread_create(&writer_tid, NULL, writer_thread_main, &wctx) != 0) {
            // 线程起不来时，说明环境资源或参数配置有问题，无法做解耦测试。
            fprintf(stderr, "pthread_create writer failed\n");
            goto cleanup;
        }
        writer_started = true;
// 这里是要开始数据传输了？
        // STREAMON 必须在初始 QBUF 后执行。
        if (xioctl(fd, VIDIOC_STREAMON, &type) < 0) {
            perror("VIDIOC_STREAMON");
            goto cleanup;
        }
        stream_on = true;

        start_ms = now_ms();
        // 两种停止模式：
        // 1) duration_sec>0: 按时间停止
        // 2) 否则按帧数停止
        uint64_t target_frames = (cfg.duration_sec > 0) ? (uint64_t)-1 : (uint64_t)cfg.total_frames;
        double deadline_ms = (cfg.duration_sec > 0) ? (start_ms + (double)cfg.duration_sec * 1000.0) : 0.0;

        while (1) {
            // ------------------------------------------------------------
            // A-1) 停止条件判断（先判停，再做 select）
            // ------------------------------------------------------------
            if (cfg.duration_sec > 0) {
                if (now_ms() >= deadline_ms) break;
            } else {
                if (st.dq_ok >= target_frames) break;
            }

            st.select_calls++;
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            timeval tv;
            tv.tv_sec = cfg.timeout_ms / 1000;
            tv.tv_usec = (cfg.timeout_ms % 1000) * 1000;
            // select 等待设备可读，避免忙轮询。
            int r = select(fd + 1, &fds, NULL, NULL, &tv);
            if (r < 0) {
                if (errno == EINTR) {
                    st.select_eintr++;
                    continue;
                }
                st.select_error++;
                perror("select");
                break;
            }
            if (r == 0) {
                // timeout 分支：先记账，再按配置尝试恢复。
                // 
                // 这里是 Stage02 的关键教学点：
                // 1) timeout 不是“立刻崩溃”，而是先尝试软恢复；
                // 2) 恢复次数受上限约束，避免无限重启掩盖真实故障。
                st.select_timeout++;
                fprintf(stderr, "select timeout, dq_ok=%llu\n", (unsigned long long)st.dq_ok);
                bool recovered = false;
                if (cfg.recover_on_timeout &&
                    st.recoveries_attempted < (uint64_t)cfg.max_recoveries) {
                    st.recoveries_attempted++;
                    recovered = restart_stream(fd, bufs.size(), &st);
                    if (recovered) st.recoveries_ok++;
                    else st.recoveries_fail++;
                }
                if (!recovered) break;
                // 恢复成功则继续下一轮 select。
                continue;
            }
            st.select_ready++;

            v4l2_buffer b;
            memset(&b, 0, sizeof(b));
            b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            b.memory = V4L2_MEMORY_MMAP;
            if (xioctl(fd, VIDIOC_DQBUF, &b) < 0) {
                st.dq_fail++;
                if (errno == EAGAIN) {
                    // 可重试的短暂“暂无帧”情况。
                    st.dq_eagain++;
                    continue;
                }
                perror("VIDIOC_DQBUF");
                break;
            }

            // ------------------------------------------------------------
            // A-2) DQ 成功后的指标采样
            // ------------------------------------------------------------
            st.dq_ok++;
            st.bytes_hist[b.bytesused] += 1;
            st.flags_hist[b.flags] += 1;
            if (b.bytesused == 0) st.zero_bytes_frames++;
            if (b.flags & V4L2_BUF_FLAG_ERROR) st.error_flag_frames++;

            double host_ms = now_ms();
            update_host_interval(&st, host_ms);

            // 复制数据到线程队列：
            // 采集线程不能持有驱动缓冲太久，尽快复制后归还最稳妥。
            // 
            // 这里故意“先复制再 Q 回”：
            // - 好处：实现简单，线程边界清晰；
            // - 代价：有一次 memcpy（后续 Stage03/04 会讲零拷贝优化）。
            FramePacket pkt;
            pkt.frame_no = st.dq_ok;
            pkt.seq = b.sequence;
            pkt.bytesused = b.bytesused;
            pkt.flags = b.flags;
            pkt.v4l2_ts = b.timestamp;
            pkt.host_dq_ms = host_ms - start_ms;
            const unsigned char* src = (const unsigned char*)bufs[b.index].start;
            pkt.data.assign(src, src + b.bytesused);

            pthread_mutex_lock(&mu);
            if (qs.policy == "drop-oldest") {
                // 保实时：队列满时丢旧帧，保证新帧更容易进入。
                //
                // 典型应用：直播预览，宁可丢历史帧，也要尽量贴近当前画面。
                while (qs.q.size() >= qs.max_depth) {
                    qs.q.pop_front();
                    qs.dropped_oldest++;
                }
            } else {  // block
                // 保完整：队列满时阻塞采集线程等待空位。
                //
                // 典型应用：离线处理/取证录制，宁可增加时延，也不轻易丢帧。
                while (qs.q.size() >= qs.max_depth && !qs.stop) {
                    qs.blocked_waits++;
                    pthread_cond_wait(&cv_not_full, &mu);
                }
            }
            qs.q.push_back(pkt);
            if (qs.q.size() > qs.peak_depth) qs.peak_depth = qs.q.size();
            pthread_cond_signal(&cv_not_empty);
            pthread_mutex_unlock(&mu);

            if (cfg.log_every > 0 && (st.dq_ok <= 5 || (st.dq_ok % (uint64_t)cfg.log_every) == 0U)) {
                printf("frame=%llu seq=%u bytes=%u qdepth=%zu flags=0x%08x\n",
                       (unsigned long long)st.dq_ok,
                       b.sequence,
                       b.bytesused,
                       qs.q.size(),
                       b.flags);
            }

            // DQ 成功的 buffer 需要尽快 Q 回去形成环。
            if (xioctl(fd, VIDIOC_QBUF, &b) < 0) {
                st.qbuf_fail++;
                perror("VIDIOC_QBUF");
                // 回队失败意味着缓冲环受损，通常应停止本轮采集。
                break;
            }
            st.qbuf_ok++;
        }

        end_ms = now_ms();

        // 通知 writer 线程收尾退出。
        //
        // 注意 stop 的时机：
        // 1) 主循环结束后置 stop=true；
        // 2) broadcast 唤醒所有等待线程；
        // 3) join 等待 writer 把剩余队列消费完再退出。
        pthread_mutex_lock(&mu);
        qs.stop = true;
        pthread_cond_broadcast(&cv_not_empty);
        pthread_cond_broadcast(&cv_not_full);
        pthread_mutex_unlock(&mu);
        pthread_join(writer_tid, NULL);
        writer_started = false;

        print_summary(cfg, active_fmt, st, qs, start_ms, end_ms);
        ret_code = 0;
        goto cleanup;
    }

    // --------------------------------------------------------------------
    // 9) 分支B：纯采集统计路径（不启 writer 线程）
    // --------------------------------------------------------------------
    //
    // 场景：
    // - 只想看采集链路本身，不引入写盘干扰。
    if (xioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        perror("VIDIOC_STREAMON");
        goto cleanup;
    }
    stream_on = true;

    start_ms = now_ms();
    {
        uint64_t target_frames = (cfg.duration_sec > 0) ? (uint64_t)-1 : (uint64_t)cfg.total_frames;
        double deadline_ms = (cfg.duration_sec > 0) ? (start_ms + (double)cfg.duration_sec * 1000.0) : 0.0;
        while (1) {
            // 与分支A相同：先判断停止条件。
            if (cfg.duration_sec > 0) {
                if (now_ms() >= deadline_ms) break;
            } else {
                if (st.dq_ok >= target_frames) break;
            }

            st.select_calls++;
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            timeval tv;
            tv.tv_sec = cfg.timeout_ms / 1000;
            tv.tv_usec = (cfg.timeout_ms % 1000) * 1000;
            int r = select(fd + 1, &fds, NULL, NULL, &tv);
            if (r <= 0) {
                // 纯采集分支不做恢复重启，直接退出。
                // 这样你可以更纯粹观察“设备原始稳定性”。
                if (r < 0 && errno == EINTR) {
                    st.select_eintr++;
                    continue;
                }
                if (r == 0) st.select_timeout++;
                else st.select_error++;
                break;
            }
            st.select_ready++;

            v4l2_buffer b;
            memset(&b, 0, sizeof(b));
            b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            b.memory = V4L2_MEMORY_MMAP;
            if (xioctl(fd, VIDIOC_DQBUF, &b) < 0) {
                st.dq_fail++;
                if (errno == EAGAIN) {
                    st.dq_eagain++;
                    continue;
                }
                // 纯采集路径这里也遵循“非 EAGAIN 直接退出”策略。
                break;
            }
            // 纯采集分支不走队列，但统计口径与分支A尽量保持一致，
            // 便于你做 AB 对比（只差是否启用 writer/backpressure）。
            st.dq_ok++;
            st.bytes_hist[b.bytesused] += 1;
            st.flags_hist[b.flags] += 1;
            if (b.flags & V4L2_BUF_FLAG_ERROR) st.error_flag_frames++;
            if (b.bytesused == 0) st.zero_bytes_frames++;

            double host_ms = now_ms();
            update_host_interval(&st, host_ms);
            st.writer_frames++;
            st.writer_bytes += b.bytesused;

            if (cfg.log_every > 0 && (st.dq_ok <= 5 || (st.dq_ok % (uint64_t)cfg.log_every) == 0U)) {
                printf("frame=%llu seq=%u bytes=%u flags=0x%08x\n",
                       (unsigned long long)st.dq_ok,
                       b.sequence,
                       b.bytesused,
                       b.flags);
            }

            if (xioctl(fd, VIDIOC_QBUF, &b) < 0) {
                st.qbuf_fail++;
                break;
            }
            st.qbuf_ok++;
        }
    }
    end_ms = now_ms();
    print_summary(cfg, active_fmt, st, qs, start_ms, end_ms);
    ret_code = 0;

cleanup:
    // --------------------------------------------------------------------
    // 10) 清理阶段：STREAMOFF -> munmap -> close -> writer join
    // --------------------------------------------------------------------
    //
    // 原则：
    // - 无论中途在哪失败，都要尽可能释放资源；
    // - 这对长期运行服务尤其关键，避免资源泄漏累积成线上事故。
    if (stream_on) {
        // STREAMOFF 失败一般不再阻断 cleanup 继续执行。
        xioctl(fd, VIDIOC_STREAMOFF, &type);
    }
    for (size_t i = 0; i < bufs.size(); ++i) {
        if (bufs[i].start && bufs[i].start != MAP_FAILED) {
            munmap(bufs[i].start, bufs[i].length);
        }
    }
    if (fd >= 0) close(fd);
    if (writer_started) {
        // 防御性收尾：若异常路径提前跳到 cleanup，确保 writer 可退出。
        //
        // 为什么要再做一遍 stop+broadcast？
        // - 因为可能在 writer 线程已启动但主循环未正常收尾时跳转到 cleanup；
        // - 这一步保证不会留下僵死线程。
        pthread_mutex_lock(&mu);
        qs.stop = true;
        pthread_cond_broadcast(&cv_not_empty);
        pthread_cond_broadcast(&cv_not_full);
        pthread_mutex_unlock(&mu);
        pthread_join(writer_tid, NULL);
    }
    return ret_code;
}

}  // namespace stage02_v4l2

#endif  // STAGE02_V4L2_CAPTURE_HPP_
