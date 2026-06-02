#include <stdio.h>

#include <string>
#include <vector>

#include "00_m2m_demo_common.hpp"

namespace {

/*
 * 结构体作用：06 demo 参数。
 * 本 demo 知识边界：
 * 1) 讲 QBUF/DQBUF ownership loop；
 * 2) 覆盖 poll timeout 观测；
 * 3) 不做真实码流解析，只用占位 payload 触发状态机。
 */
struct Args {
    std::string dev = "/dev/video0";
    std::string in_fourcc = "H264";
    std::string out_fourcc = "NV12";
    uint32_t width = 1280;
    uint32_t height = 720;
    uint32_t out_count = 4;
    uint32_t cap_count = 4;
    uint32_t max_loops = 8;
    int timeout_ms = 800;
    int mplane = 0;
    bool verbose = false;
};

void print_usage(const char* prog) {
    printf("%s: Stage03 demo 06 - QBUF/DQBUF ownership loop\n", prog);
    printf("Usage:\n");
    printf("  %s --dev=/dev/video0 --in-fourcc=H264 --out-fourcc=NV12 --width=1280 --height=720 --out-count=4 --cap-count=4 --max-loops=8 --timeout-ms=800 --mplane=0 --verbose\n", prog);
}

bool parse_args(int argc, char** argv, Args* a) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return false;
        } else if (m2m_demo::parse_kv(argv[i], "--dev", &a->dev)) {
        } else if (m2m_demo::parse_kv(argv[i], "--in-fourcc", &a->in_fourcc)) {
        } else if (m2m_demo::parse_kv(argv[i], "--out-fourcc", &a->out_fourcc)) {
        } else if (m2m_demo::parse_kv_u32(argv[i], "--width", &a->width)) {
        } else if (m2m_demo::parse_kv_u32(argv[i], "--height", &a->height)) {
        } else if (m2m_demo::parse_kv_u32(argv[i], "--out-count", &a->out_count)) {
        } else if (m2m_demo::parse_kv_u32(argv[i], "--cap-count", &a->cap_count)) {
        } else if (m2m_demo::parse_kv_u32(argv[i], "--max-loops", &a->max_loops)) {
        } else if (m2m_demo::parse_kv_int(argv[i], "--timeout-ms", &a->timeout_ms)) {
        } else if (m2m_demo::parse_kv_int(argv[i], "--mplane", &a->mplane)) {
        } else if (strcmp(argv[i], "--verbose") == 0) {
            a->verbose = true;
        } else {
            fprintf(stderr, "unknown arg: %s\n", argv[i]);
            return false;
        }
    }
    return true;
}

}  // namespace

/*
 * 函数作用：demo06 入口。
 * 数据流：
 * 1) 双队列 S_FMT + REQBUFS + QUERYBUF+MMAP；
 * 2) QBUF CAPTURE（空 buffer）-> QBUF OUTPUT（占位输入）；
 * 3) STREAMON 双队列；
 * 4) poll -> DQBUF -> QBUF(requeue) 循环；
 * 5) STREAMOFF + munmap + close。
 */
int main(int argc, char** argv) {
    Args args;
    if (!parse_args(argc, argv, &args)) {
        return 1;
    }

    uint32_t in_fourcc = 0;
    uint32_t out_fourcc = 0;
    if (!m2m_demo::parse_fourcc(args.in_fourcc, &in_fourcc) ||
        !m2m_demo::parse_fourcc(args.out_fourcc, &out_fourcc)) {
        fprintf(stderr, "fourcc must be 4 chars\n");
        return 1;
    }

    int fd = -1;
    if (!m2m_demo::open_node(args.dev, &fd)) {
        return 1;
    }

    const uint32_t out_type = m2m_demo::output_type_from_mplane(args.mplane);
    const uint32_t cap_type = m2m_demo::capture_type_from_mplane(args.mplane);

    if (!m2m_demo::set_format(fd, out_type, in_fourcc, args.width, args.height,
                              args.verbose) ||
        !m2m_demo::set_format(fd, cap_type, out_fourcc, args.width, args.height,
                              args.verbose)) {
        close(fd);
        return 1;
    }

    uint32_t out_granted = 0;
    uint32_t cap_granted = 0;
    if (!m2m_demo::reqbufs(fd, out_type, V4L2_MEMORY_MMAP, args.out_count, &out_granted,
                           args.verbose) ||
        !m2m_demo::reqbufs(fd, cap_type, V4L2_MEMORY_MMAP, args.cap_count, &cap_granted,
                           args.verbose)) {
        close(fd);
        return 1;
    }

    std::vector<m2m_demo::MappedBuffer> out_bufs(out_granted);
    std::vector<m2m_demo::MappedBuffer> cap_bufs(cap_granted);

    // 建立 OUTPUT 映射。
    for (uint32_t i = 0; i < out_granted; ++i) {
        if (!m2m_demo::querybuf_map_single_planar(fd, out_type, i, &out_bufs[i],
                                                   args.verbose)) {
            m2m_demo::unmap_all(&out_bufs, args.verbose);
            m2m_demo::unmap_all(&cap_bufs, args.verbose);
            close(fd);
            return 1;
        }
    }

    // 建立 CAPTURE 映射。
    for (uint32_t i = 0; i < cap_granted; ++i) {
        if (!m2m_demo::querybuf_map_single_planar(fd, cap_type, i, &cap_bufs[i],
                                                   args.verbose)) {
            m2m_demo::unmap_all(&out_bufs, args.verbose);
            m2m_demo::unmap_all(&cap_bufs, args.verbose);
            close(fd);
            return 1;
        }
    }

    /*
     * 所有权转移 1：CAPTURE 空 buffer 先交给驱动。
     * decoder 场景里，驱动需要先拿到可写输出帧缓冲。
     */
    for (uint32_t i = 0; i < cap_granted; ++i) {
        if (!m2m_demo::qbuf_single_planar(fd, cap_type, i, 0, args.verbose)) {
            m2m_demo::unmap_all(&out_bufs, args.verbose);
            m2m_demo::unmap_all(&cap_bufs, args.verbose);
            close(fd);
            return 1;
        }
    }

    /*
     * 所有权转移 2：OUTPUT 输入 buffer 交给驱动。
     * 注意：此处 16 字节占位 payload 仅用于流程训练，不代表真实码流。
     */
    for (uint32_t i = 0; i < out_granted; ++i) {
        const uint32_t bytesused = 16;
        memset(out_bufs[i].addr, 0, bytesused);
        if (!m2m_demo::qbuf_single_planar(fd, out_type, i, bytesused, args.verbose)) {
            m2m_demo::unmap_all(&out_bufs, args.verbose);
            m2m_demo::unmap_all(&cap_bufs, args.verbose);
            close(fd);
            return 1;
        }
    }

    // 开启双队列流。
    if (!m2m_demo::stream_on(fd, out_type, args.verbose) ||
        !m2m_demo::stream_on(fd, cap_type, args.verbose)) {
        m2m_demo::stream_off_best_effort(fd, cap_type, args.verbose);
        m2m_demo::stream_off_best_effort(fd, out_type, args.verbose);
        m2m_demo::unmap_all(&out_bufs, args.verbose);
        m2m_demo::unmap_all(&cap_bufs, args.verbose);
        close(fd);
        return 1;
    }

    uint32_t dq_cap_ok = 0;
    uint32_t dq_out_ok = 0;
    uint32_t poll_timeouts = 0;

    for (uint32_t loop = 0; loop < args.max_loops; ++loop) {
        const int pr = m2m_demo::poll_readable(fd, args.timeout_ms, args.verbose);
        if (pr == 0) {
            ++poll_timeouts;
            printf("[demo06] poll timeout at loop=%u\n", loop);
            break;
        }
        if (pr < 0) {
            fprintf(stderr, "poll failed: %s\n", strerror(errno));
            break;
        }

        // 所有权转移 3：驱动 -> 用户态（CAPTURE 完成帧）。
        v4l2_buffer cap_dq;
        if (m2m_demo::dqbuf_single_planar(fd, cap_type, &cap_dq, args.verbose)) {
            ++dq_cap_ok;

            // 所有权转移 4：用户态 -> 驱动（CAPTURE 回收再投递）。
            if (!m2m_demo::qbuf_single_planar(fd, cap_type, cap_dq.index, 0,
                                              args.verbose)) {
                fprintf(stderr, "requeue capture failed at loop=%u\n", loop);
                break;
            }
        }

        // 所有权转移 5：驱动 -> 用户态（OUTPUT 消费完成）。
        v4l2_buffer out_dq;
        if (m2m_demo::dqbuf_single_planar(fd, out_type, &out_dq, args.verbose)) {
            ++dq_out_ok;

            // 重新填充占位输入，再次回投 OUTPUT。
            const uint32_t bytesused = 16;
            memset(out_bufs[out_dq.index].addr, 0, bytesused);
            if (!m2m_demo::qbuf_single_planar(fd, out_type, out_dq.index, bytesused,
                                              args.verbose)) {
                fprintf(stderr, "requeue output failed at loop=%u\n", loop);
                break;
            }
        }
    }

    printf("[demo06] summary: dq_cap_ok=%u dq_out_ok=%u poll_timeouts=%u\n", dq_cap_ok,
           dq_out_ok, poll_timeouts);

    // 对称清理：先停流，再释放映射，再 close。
    m2m_demo::stream_off_best_effort(fd, cap_type, args.verbose);
    m2m_demo::stream_off_best_effort(fd, out_type, args.verbose);
    m2m_demo::unmap_all(&out_bufs, args.verbose);
    m2m_demo::unmap_all(&cap_bufs, args.verbose);
    close(fd);
    return 0;
}
