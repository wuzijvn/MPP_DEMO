#include <stdio.h>

#include <string>

#include "00_m2m_demo_common.hpp"

namespace {

/*
 * 结构体作用：07 demo 参数。
 * 知识边界：只聚焦 STREAMON/STREAMOFF 对称切换，不做 DQBUF 主循环。
 */
struct Args {
    std::string dev = "/dev/video0";
    std::string in_fourcc = "H264";
    std::string out_fourcc = "NV12";
    uint32_t width = 1280;
    uint32_t height = 720;
    uint32_t out_count = 2;
    uint32_t cap_count = 2;
    int mplane = 0;
    bool verbose = false;
};

void print_usage(const char* prog) {
    printf("%s: Stage03 demo 07 - STREAMON/STREAMOFF minimal sequence\n", prog);
    printf("Usage:\n");
    printf("  %s --dev=/dev/video0 --in-fourcc=H264 --out-fourcc=NV12 --width=1280 --height=720 --out-count=2 --cap-count=2 --mplane=0 --verbose\n", prog);
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
 * 函数作用：demo07 入口。
 * 主流程：S_FMT -> REQBUFS -> STREAMON 双队列 -> STREAMOFF 双队列。
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

    // 不进入队列循环，仅验证状态切换接口是否可达。
    if (!m2m_demo::stream_on(fd, out_type, args.verbose) ||
        !m2m_demo::stream_on(fd, cap_type, args.verbose)) {
        // 半成功回滚：尽量把已开启队列关掉。
        m2m_demo::stream_off_best_effort(fd, cap_type, args.verbose);
        m2m_demo::stream_off_best_effort(fd, out_type, args.verbose);
        close(fd);
        return 1;
    }

    // 成功路径：双队列都做 STREAMOFF，保证对称。
    m2m_demo::stream_off_best_effort(fd, cap_type, args.verbose);
    m2m_demo::stream_off_best_effort(fd, out_type, args.verbose);
    close(fd);

    printf("[demo07] PASS: STREAMON/STREAMOFF both queues done. out=%u cap=%u\n",
           out_granted, cap_granted);
    return 0;
}
