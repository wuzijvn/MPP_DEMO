#include <stdio.h>

#include <string>
#include <vector>

#include "00_m2m_demo_common.hpp"

namespace {

/*
 * 结构体作用：05 demo 参数。
 * 本 demo 知识边界：
 * 1) 只讲 REQBUFS -> QUERYBUF -> MMAP 生命周期；
 * 2) 不进入 STREAMON/QBUF/DQBUF。
 */
struct Args {
    std::string dev = "/dev/video0";
    std::string in_fourcc = "H264";
    std::string out_fourcc = "NV12";
    uint32_t width = 1280;
    uint32_t height = 720;
    uint32_t out_count = 4;
    uint32_t cap_count = 4;
    int mplane = 0;
    bool verbose = false;
};

void print_usage(const char* prog) {
    printf("%s: Stage03 demo 05 - REQBUFS + QUERYBUF + MMAP lifecycle\n", prog);
    printf("Usage:\n");
    printf("  %s --dev=/dev/video0 --in-fourcc=H264 --out-fourcc=NV12 --width=1280 --height=720 --out-count=4 --cap-count=4 --mplane=0 --verbose\n", prog);
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
 * 函数作用：demo05 入口。
 * 主流程：
 * 1) S_FMT 双队列；
 * 2) REQBUFS 双队列；
 * 3) QUERYBUF+MMAP 双队列；
 * 4) 对称释放（munmap + close）。
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

    // 前置条件：先完成格式协商，再申请 buffer。
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

    // 存储每个 buffer 的映射信息，后续用于对称 unmap。
    std::vector<m2m_demo::MappedBuffer> out_bufs;
    std::vector<m2m_demo::MappedBuffer> cap_bufs;
    out_bufs.resize(out_granted);
    cap_bufs.resize(cap_granted);

    // OUTPUT 队列逐个 QUERYBUF+MMAP。
    for (uint32_t i = 0; i < out_granted; ++i) {
        if (!m2m_demo::querybuf_map_single_planar(fd, out_type, i, &out_bufs[i],
                                                   args.verbose)) {
            // 失败清理：已映射 OUTPUT buffer 必须全部回收。
            m2m_demo::unmap_all(&out_bufs, args.verbose);
            close(fd);
            return 1;
        }
    }

    // CAPTURE 队列逐个 QUERYBUF+MMAP。
    for (uint32_t i = 0; i < cap_granted; ++i) {
        if (!m2m_demo::querybuf_map_single_planar(fd, cap_type, i, &cap_bufs[i],
                                                   args.verbose)) {
            // 两边都可能已有映射，必须对称释放。
            m2m_demo::unmap_all(&out_bufs, args.verbose);
            m2m_demo::unmap_all(&cap_bufs, args.verbose);
            close(fd);
            return 1;
        }
    }

    printf("[demo05] PASS: REQBUFS+QUERYBUF+MMAP done. out=%u cap=%u\n", out_granted,
           cap_granted);

    // 成功路径清理：先 unmap 两边，再 close(fd)。
    m2m_demo::unmap_all(&out_bufs, args.verbose);
    m2m_demo::unmap_all(&cap_bufs, args.verbose);
    close(fd);
    return 0;
}
