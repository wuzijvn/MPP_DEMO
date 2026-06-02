#ifndef STAGE01_V4L2_CAPTURE_HPP_
#define STAGE01_V4L2_CAPTURE_HPP_

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "stage01_v4l2_common.hpp"
#include "stage01_v4l2_stats.hpp"
#include "stage01_v4l2_types.hpp"

namespace stage01_v4l2 {

/*
===============================================================================
Stage01 Capture Core State Machine
===============================================================================

这个文件是 Stage01 最核心的学习对象。
你要达到的目标不是“背 API”，而是理解三条线：

1) 状态机线：
   open -> QUERYCAP -> 格式协商 -> 帧率协商 -> 缓冲建链 -> STREAMON -> 循环 -> 清理

2) 数据线：
   驱动写 MMAP buffer -> DQ 给用户态 -> 用户处理 -> Q 回驱动

3) 证据线：
   每个关键步骤都打印/统计，能解释“为什么成功/为什么失败”。

阅读建议：
1) 先看 run_capture 的 1~9 大步骤标题；
2) 再看每步的失败分支与 cleanup；
3) 最后看统计指标是如何更新的。
*/

inline void dump_supported_formats_and_intervals(int fd) {
    // 这个函数用于“先摸清设备能力，再做协商”。
    // 工作中调链路时，先枚举能力再请求参数是高频动作。
    printf("supported format/size/fps table:\n");
    for (unsigned int fmt_index = 0;; ++fmt_index) {
        v4l2_fmtdesc fmtdesc;
        memset(&fmtdesc, 0, sizeof(fmtdesc));
        fmtdesc.index = fmt_index;
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) < 0) {
            // 枚举结束通常会返回错误（常见 EINVAL），这里直接 break 即可。
            break;
        }
        std::string fourcc = fourcc_to_string(fmtdesc.pixelformat);
        printf("  fmt[%u]: fourcc=%s desc=\"%s\"\n", fmt_index, fourcc.c_str(), fmtdesc.description);

        for (unsigned int size_index = 0;; ++size_index) {
            v4l2_frmsizeenum fsize;
            memset(&fsize, 0, sizeof(fsize));
            fsize.index = size_index;
            fsize.pixel_format = fmtdesc.pixelformat;
            if (xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &fsize) < 0) {
                break;
            }

            if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                printf("    size[%u]: %ux%u\n",
                       size_index,
                       fsize.discrete.width,
                       fsize.discrete.height);

                for (unsigned int fi = 0;; ++fi) {
                    v4l2_frmivalenum fival;
                    memset(&fival, 0, sizeof(fival));
                    fival.index = fi;
                    fival.pixel_format = fmtdesc.pixelformat;
                    fival.width = fsize.discrete.width;
                    fival.height = fsize.discrete.height;
                    if (xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival) < 0) {
                        break;
                    }

                    if (fival.type == V4L2_FRMIVAL_TYPE_DISCRETE &&
                        fival.discrete.numerator > 0) {
                        double fps = (double)fival.discrete.denominator /
                                     (double)fival.discrete.numerator;
                        printf("      fps[%u]: %.3f (%u/%u)\n",
                               fi,
                               fps,
                               fival.discrete.denominator,
                               fival.discrete.numerator);
                    }
                }
            }
        }
    }
}

// CaptureRunResult:
//   run_capture 的返回结果。
//
// ret_code 约定：
//   0 : 达成目标帧数（dq_ok >= total_frames）
//   1 : 初始化阶段失败（open/querycap/s_fmt/reqbufs...）
//   2 : 初始化成功，但采集中途未达成目标（超时/队列耗尽/其他错误）
struct CaptureRunResult {
    int ret_code;

    // 最终生效格式（来自 G_FMT）。
    v4l2_format active_fmt;

    // 完整统计。
    CaptureStats stats;

    // 预览帧缓存（来自采集循环中的“最近稳定帧”）。
    std::vector<unsigned char> saved_frame;
    uint32_t saved_bytes;
    uint32_t saved_seq;

    CaptureRunResult() : ret_code(1), saved_bytes(0), saved_seq(0) {
        memset(&active_fmt, 0, sizeof(active_fmt));
        init_stats(&stats);
    }
};

// run_capture:
//   执行完整 V4L2 采集流程，并返回统计结果。
//
// 流程顺序（岗位面试高频问法）：
// 1) open
// 2) VIDIOC_QUERYCAP
// 3) VIDIOC_S_FMT + VIDIOC_G_FMT
// 4) VIDIOC_S_PARM + VIDIOC_G_PARM
// 5) VIDIOC_REQBUFS + VIDIOC_QUERYBUF + mmap
// 6) VIDIOC_QBUF(全部初始入队)
// 7) VIDIOC_STREAMON
// 8) (select + VIDIOC_DQBUF + VIDIOC_QBUF)*N
// 9) VIDIOC_STREAMOFF + munmap + close
inline CaptureRunResult run_capture(const AppConfig& cfg) {
    CaptureRunResult rr;

    // fd: 设备文件描述符。
    // stream_on: 标记是否成功开流，cleanup 时据此决定是否 STREAMOFF。
    // fd=-1 表示“尚未打开”，cleanup 用它判断是否需要 close。
    int fd = -1;
    bool stream_on = false;
    // trace CSV 可选开启，用于逐帧细粒度分析。
    FILE* trace_fp = NULL;

    // trace CSV 辅助状态：
    // 用于按帧记录 host/v4l2 timestamp 间隔。
    bool trace_has_prev_host = false;
    double trace_prev_host_ms = 0.0;
    bool trace_has_prev_v4l2_ts = false;
    uint64_t trace_prev_v4l2_ts_ns = 0;

    // type: 多个 ioctl 需要传 buffer type。
    // 这里是 single-planar capture 节点。
    // type 在 STREAMON/OFF、QBUF/DQBUF 等 ioctl 中都会重复使用。
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    // 用户态维护的 mmap 缓冲信息。
    // bufs 存放每个 MMAP 缓冲的用户态地址和长度。
    std::vector<Buffer> bufs;

    // fmt_req: 请求格式
    // fmt_active: S_FMT 返回后的“驱动调整后格式”
    // fmt_g: G_FMT 回读格式（推荐以它为最终准）
    v4l2_format fmt_req;
    v4l2_format fmt_active;
    v4l2_format fmt_g;
    memset(&fmt_req, 0, sizeof(fmt_req));
    memset(&fmt_active, 0, sizeof(fmt_active));
    memset(&fmt_g, 0, sizeof(fmt_g));

    // 帧率相关：
    // parm_active: S_PARM 返回值
    // parm_g: G_PARM 回读值
    v4l2_streamparm parm_active;
    v4l2_streamparm parm_g;
    memset(&parm_active, 0, sizeof(parm_active));
    memset(&parm_g, 0, sizeof(parm_g));

    // 标记 S_PARM 是否成功，用于后续日志解释。
    bool s_parm_ok = false;
    uint32_t requested_fourcc = V4L2_PIX_FMT_YUYV;

    // ------------------------------------------------------------------------
    // 1) open: 打开视频节点
    // ------------------------------------------------------------------------
    // O_RDWR: 我们需要 ioctl 配置 + 流式 I/O。
    fd = open(cfg.dev.c_str(), O_RDWR);
    if (fd < 0) {
        // 常见原因：
        // 1) 节点不存在（No such file）
        // 2) 权限不足（Permission denied）
        // 3) 节点被占用/异常
        perror("open video device failed");
        goto cleanup;
    }

    // ------------------------------------------------------------------------
    // 2) VIDIOC_QUERYCAP: 查询能力
    // ------------------------------------------------------------------------
    {
        v4l2_capability cap;
        memset(&cap, 0, sizeof(cap));
        if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
            // QUERYCAP 失败时，不要继续后续流程，基础能力未知。
            perror("VIDIOC_QUERYCAP");
            goto cleanup;
        }
        print_caps(cap);

        // capability 选择规则：
        // 若 cap.capabilities 含 V4L2_CAP_DEVICE_CAPS，
        // 则当前节点有效能力看 cap.device_caps。
        uint32_t effective_caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;

        // 本示例要求：
        // 1) VIDEO_CAPTURE（是采集节点）
        // 2) STREAMING（支持 QBUF/DQBUF 流式模型）
        bool has_capture = (effective_caps & V4L2_CAP_VIDEO_CAPTURE) != 0;
        bool has_streaming = (effective_caps & V4L2_CAP_STREAMING) != 0;

        if (!has_capture || !has_streaming) {
            // 这通常说明你打开了错误节点类型。
            fprintf(stderr, "device does not support capture+streaming (effective_caps=0x%08x)\n", effective_caps);
            goto cleanup;
        }
    }

    if (cfg.dump_formats) {
        dump_supported_formats_and_intervals(fd);
    }

    if (!cfg.trace_csv.empty()) {
        trace_fp = fopen(cfg.trace_csv.c_str(), "w");
        if (!trace_fp) {
            // trace 打不开不应该影响主功能，继续运行。
            perror("fopen trace csv (continue without trace)");
        } else {
            fprintf(trace_fp,
                    "frame_no,sequence,index,bytesused,buffer_length,flags_hex,flags_text,"
                    "v4l2_ts_sec,v4l2_ts_usec,host_dq_ms,host_delta_ms,v4l2_delta_ms\n");
            fflush(trace_fp);
            printf("trace csv enabled: %s\n", cfg.trace_csv.c_str());
        }
    }

    // ------------------------------------------------------------------------
    // 3) 格式协商：S_FMT + G_FMT
    // ------------------------------------------------------------------------
    // 先填“请求值”，后续对比 S_FMT 返回和 G_FMT 回读。
    fmt_req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt_req.fmt.pix.width = (unsigned int)cfg.req_width;
    fmt_req.fmt.pix.height = (unsigned int)cfg.req_height;

    // 故障注入 bad-fmt：请求一个大概率不支持的 fourcc。
    // 正常路径使用 YUYV。
    if (!cfg.req_pixfmt.empty() &&
        cfg.req_pixfmt.size() == 4 &&
        cfg.inject != "bad-fmt") {
        requested_fourcc = v4l2_fourcc((unsigned char)cfg.req_pixfmt[0],
                                       (unsigned char)cfg.req_pixfmt[1],
                                       (unsigned char)cfg.req_pixfmt[2],
                                       (unsigned char)cfg.req_pixfmt[3]);
    }
    fmt_req.fmt.pix.pixelformat = (cfg.inject == "bad-fmt") ? v4l2_fourcc('B', 'A', 'D', '!') : requested_fourcc;
    // FIELD_NONE 常用于 progressive 场景。
    fmt_req.fmt.pix.field = V4L2_FIELD_NONE;

    // TRY_FMT 不改设备状态，仅做可行性探测。
    // 这个 ioctl 在工作中非常重要：先试探，再提交 S_FMT。
    {
        v4l2_format fmt_try = fmt_req;
        if (xioctl(fd, VIDIOC_TRY_FMT, &fmt_try) == 0) {
            printf("try fmt: req=%ux%u/%s -> driver-suggest=%ux%u/%s bytesperline=%u sizeimage=%u\n",
                   fmt_req.fmt.pix.width,
                   fmt_req.fmt.pix.height,
                   fourcc_to_string(fmt_req.fmt.pix.pixelformat).c_str(),
                   fmt_try.fmt.pix.width,
                   fmt_try.fmt.pix.height,
                   fourcc_to_string(fmt_try.fmt.pix.pixelformat).c_str(),
                   fmt_try.fmt.pix.bytesperline,
                   fmt_try.fmt.pix.sizeimage);
        } else {
            // TRY_FMT 失败通常可继续，让后续 S_FMT 再给最终结论。
            perror("VIDIOC_TRY_FMT (continue)");
        }
    }

    // S_FMT 是“请求并可能被调整”。
    fmt_active = fmt_req;
    if (xioctl(fd, VIDIOC_S_FMT, &fmt_active) < 0) {
        perror("VIDIOC_S_FMT");
        fprintf(stderr, "hint: if inject=bad-fmt this failure is expected\n");
        goto cleanup;
    }

    // G_FMT 用于最终确认当前生效格式。
    fmt_g.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd, VIDIOC_G_FMT, &fmt_g) < 0) {
        perror("VIDIOC_G_FMT");
        goto cleanup;
    }

    print_fmt_compare(fmt_req, fmt_active, fmt_g);

    // 仅当需要导出 PPM 时才强制限制 YUYV（因为当前 PPM 转换器只实现了 YUYV）。
    if (cfg.save_preview) {
        if (fmt_g.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV) {
            fprintf(stderr, "preview ppm path currently supports YUYV only, got fourcc=%s\n",
                    fourcc_to_string(fmt_g.fmt.pix.pixelformat).c_str());
            fprintf(stderr, "tip: use --pixfmt=YUYV for preview export, or run with --no-save for pure capture stats\n");
            goto cleanup;
        }

        // YUYV 两像素一组，宽度应为偶数。
        if ((fmt_g.fmt.pix.width % 2U) != 0U) {
            fprintf(stderr, "active width is odd (%u), YUYV converter requires even width\n", fmt_g.fmt.pix.width);
            goto cleanup;
        }
    }

    // ------------------------------------------------------------------------
    // 4) 帧率协商：S_PARM + G_PARM
    // ------------------------------------------------------------------------
    // 注意：很多设备并不严格支持 S_PARM，失败可继续。
    memset(&parm_active, 0, sizeof(parm_active));
    parm_active.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm_active.parm.capture.timeperframe.numerator = 1;
    parm_active.parm.capture.timeperframe.denominator = (unsigned int)cfg.req_fps;
    if (xioctl(fd, VIDIOC_S_PARM, &parm_active) < 0) {
        perror("VIDIOC_S_PARM (continue)");
    } else {
        s_parm_ok = true;
    }

    memset(&parm_g, 0, sizeof(parm_g));
    parm_g.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd, VIDIOC_G_PARM, &parm_g) < 0) {
        perror("VIDIOC_G_PARM (continue)");
    }

    print_parm_compare(cfg.req_fps, parm_active, parm_g, s_parm_ok);

    // ------------------------------------------------------------------------
    // 5) 缓冲队列初始化：REQBUFS -> QUERYBUF -> MMAP -> 初始 QBUF
    // ------------------------------------------------------------------------
    {
        v4l2_requestbuffers req;
        memset(&req, 0, sizeof(req));

        // req.count：希望驱动分配多少个 buffer。
        // 工程常见起点是 4，兼顾时延和稳定性。
        // req.count 是“请求值”，驱动可调整为其他值。
        req.count = cfg.req_buf_count;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
            perror("VIDIOC_REQBUFS");
            goto cleanup;
        }

        // 驱动可能下调数量。少于2通常无法稳定轮转。
        if (req.count < 2) {
            fprintf(stderr, "insufficient buffer count after REQBUFS: %u\n", req.count);
            goto cleanup;
        }

        bufs.resize(req.count);
        for (unsigned int i = 0; i < req.count; ++i) {
            v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            // QUERYBUF 拿到每个缓冲的 offset/length。
            if (xioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
                perror("VIDIOC_QUERYBUF");
                goto cleanup;
            }

            // mmap 建立“驱动缓冲 -> 用户态地址”映射。
            void* start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
            if (start == MAP_FAILED) {
                perror("mmap");
                goto cleanup;
            }

            bufs[i].start = start;
            bufs[i].length = buf.length;
        }

        // 初始时必须把所有空缓冲入队，驱动才有地方写采集数据。
        for (unsigned int i = 0; i < req.count; ++i) {
            v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
                perror("VIDIOC_QBUF (initial)");
                goto cleanup;
            }
        }
    }

    // ------------------------------------------------------------------------
    // 6) STREAMON: 开始流式采集
    // ------------------------------------------------------------------------
    if (xioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        perror("VIDIOC_STREAMON");
        goto cleanup;
    }
    stream_on = true;

    // ------------------------------------------------------------------------
    // 7) 主循环：select -> DQBUF -> (处理) -> QBUF
    // ------------------------------------------------------------------------
    {
        double stream_on_ms = now_ms();
        double loop_end_ms = stream_on_ms;

        while ((int)rr.stats.dq_ok < cfg.total_frames) {
            rr.stats.select_calls++;

            // select 等待设备就绪，避免忙轮询占满 CPU。
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);

            timeval tv;
            tv.tv_sec = cfg.timeout_ms / 1000;
            tv.tv_usec = (cfg.timeout_ms % 1000) * 1000;

            int r = select(fd + 1, &fds, NULL, NULL, &tv);
            if (r < 0) {
                if (errno == EINTR) {
                    rr.stats.select_eintr++;
                    continue;
                }
                rr.stats.select_error++;
                perror("select");
                break;
            }

            // r==0 表示超时，不是 errno 错误。
            if (r == 0) {
                rr.stats.select_timeout++;
                fprintf(stderr, "select timeout (%d ms), dq_ok=%llu\n",
                        cfg.timeout_ms,
                        (unsigned long long)rr.stats.dq_ok);
                break;
            }
            rr.stats.select_ready++;

            v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            // DQBUF：取出一个“已填好数据”的缓冲。
            // 成功后 buf.index 指向 bufs[] 映射区。
            if (xioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
                rr.stats.dq_fail++;
                if (errno == EAGAIN) {
                    // 非阻塞语义下常见可重试错误。
                    rr.stats.dq_eagain++;
                    continue;
                }
                perror("VIDIOC_DQBUF");
                break;
            }

            // 到这里说明拿到了一帧有效缓冲，开始更新统计与日志证据。
            rr.stats.dq_ok++;
            update_bytes_hist(&rr.stats, buf.bytesused);
            update_sequence_gap(&rr.stats, buf.sequence);
            rr.stats.flags_hist[buf.flags] += 1;
            maybe_warn_payload_shape(fmt_g, buf, &rr.stats);
            update_v4l2_timestamp_interval(buf, &rr.stats);

            double now = now_ms();
            if (rr.stats.first_frame_ms <= 0.0) rr.stats.first_frame_ms = now;
            rr.stats.last_frame_ms = now;
            loop_end_ms = now;
            update_dq_host_interval(&rr.stats, now);

            double host_delta_ms = -1.0;
            if (trace_has_prev_host) {
                host_delta_ms = now - trace_prev_host_ms;
            }
            trace_prev_host_ms = now;
            trace_has_prev_host = true;

            uint64_t v4l2_ts_ns = tv_to_ns(buf.timestamp);
            double v4l2_delta_ms = -1.0;
            if (v4l2_ts_ns != 0 && trace_has_prev_v4l2_ts && v4l2_ts_ns >= trace_prev_v4l2_ts_ns) {
                v4l2_delta_ms = (double)(v4l2_ts_ns - trace_prev_v4l2_ts_ns) / 1000000.0;
            }
            if (v4l2_ts_ns != 0) {
                trace_prev_v4l2_ts_ns = v4l2_ts_ns;
                trace_has_prev_v4l2_ts = true;
            }

            if (trace_fp) {
                // trace CSV 记录“逐帧证据”，适合后续画图分析抖动。
                std::string flags_text = flags_to_text(buf.flags);
                double host_dq_ms = now - stream_on_ms;
                fprintf(trace_fp,
                        "%llu,%u,%u,%u,%zu,0x%08x,\"%s\",%lld,%lld,%.3f,%.3f,%.3f\n",
                        (unsigned long long)rr.stats.dq_ok,
                        buf.sequence,
                        buf.index,
                        buf.bytesused,
                        bufs[buf.index].length,
                        buf.flags,
                        flags_text.c_str(),
                        (long long)buf.timestamp.tv_sec,
                        (long long)buf.timestamp.tv_usec,
                        host_dq_ms,
                        host_delta_ms,
                        v4l2_delta_ms);
                if ((rr.stats.dq_ok % 50U) == 0U) {
                    fflush(trace_fp);
                }
            }

            // 打点日志：前几帧 + 每50帧 + 最后一帧。
            bool print_this_frame =
                (rr.stats.dq_ok <= 5U) ||
                (rr.stats.dq_ok == (uint64_t)cfg.total_frames) ||
                ((cfg.log_every > 0) && (rr.stats.dq_ok % (uint64_t)cfg.log_every) == 0U);
            if (print_this_frame) {
                std::string flag_text = flags_to_text(buf.flags);
                printf("frame=%llu seq=%u index=%u bytesused=%u ts=%lld.%06lld flags=%s\n",
                       (unsigned long long)rr.stats.dq_ok,
                       buf.sequence,
                       buf.index,
                       buf.bytesused,
                       (long long)buf.timestamp.tv_sec,
                       (long long)buf.timestamp.tv_usec,
                       flag_text.c_str());
            }

            // 保存最近稳定帧：
            // 超过 warmup 后，每帧覆盖一次，最终得到“最后一帧稳定图”。
            if (cfg.save_preview && (int)rr.stats.dq_ok > cfg.warmup_frames) {
                const unsigned char* src = (const unsigned char*)bufs[buf.index].start;
                rr.saved_frame.assign(src, src + buf.bytesused);
                rr.saved_bytes = buf.bytesused;
                rr.saved_seq = buf.sequence;
            }

            bool skip_requeue_now = false;
            if (cfg.inject == "skip-requeue" && (int)rr.stats.dq_ok >= cfg.inject_frame) {
                // 故障注入：从 inject_frame 开始不回队。
                // 预期：可用缓冲逐步耗尽，最终 timeout 或 dq 失败。
                skip_requeue_now = true;
            }

            if (skip_requeue_now) {
                rr.stats.requeue_skipped++;
                fprintf(stderr,
                        "[inject] skip requeue at frame=%llu (seq=%u, skipped=%llu)\n",
                        (unsigned long long)rr.stats.dq_ok,
                        buf.sequence,
                        (unsigned long long)rr.stats.requeue_skipped);
            } else {
                // 关键语义：DQ 成功的 buffer 必须尽快 Q 回去，形成环。
                if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
                    rr.stats.requeue_fail++;
                    perror("VIDIOC_QBUF (requeue)");
                    break;
                }
                rr.stats.requeue_ok++;
            }
        }

        if (loop_end_ms < stream_on_ms) {
            loop_end_ms = now_ms();
        }

        print_stats(cfg, fmt_g, rr.stats, stream_on_ms, loop_end_ms);
    }

    rr.active_fmt = fmt_g;
    rr.ret_code = (rr.stats.dq_ok >= (uint64_t)cfg.total_frames) ? 0 : 2;

cleanup:
    // ------------------------------------------------------------------------
    // 8) 清理：STREAMOFF -> munmap -> close
    // ------------------------------------------------------------------------
    if (stream_on) {
        if (xioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
            perror("VIDIOC_STREAMOFF");
        }
    }

    for (size_t i = 0; i < bufs.size(); ++i) {
        if (bufs[i].start && bufs[i].start != MAP_FAILED) {
            munmap(bufs[i].start, bufs[i].length);
            bufs[i].start = NULL;
            bufs[i].length = 0;
        }
    }

    if (fd >= 0) {
        close(fd);
        fd = -1;
    }

    if (trace_fp) {
        fflush(trace_fp);
        fclose(trace_fp);
        trace_fp = NULL;
    }

    return rr;
}

}  // namespace stage01_v4l2

#endif  // STAGE01_V4L2_CAPTURE_HPP_
