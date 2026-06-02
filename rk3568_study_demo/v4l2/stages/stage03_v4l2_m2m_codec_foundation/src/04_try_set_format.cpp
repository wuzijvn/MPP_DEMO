#include <stdio.h>

#include <string>

#include "00_m2m_demo_common.hpp"

namespace {

/*
 * 结构体作用：04 demo 的参数集合。
 * 本 demo 的知识边界：
 * - 只验证双队列 S_FMT 协商；
 * - 不进入 REQBUFS/QBUF/DQBUF。
 */
struct Args {
    std::string dev = "/dev/video0";
    std::string in_fourcc = "H264";
    std::string out_fourcc = "NV12";
    uint32_t width = 1280;
    uint32_t height = 720;
    int mplane = 0;
    bool verbose = false;
};

void print_usage(const char* prog) {
    printf("%s: Stage03 demo 04 - try S_FMT for OUTPUT/CAPTURE\n", prog);
    printf("Usage:\n");
    printf("  %s --dev=/dev/video0 --in-fourcc=H264 --out-fourcc=NV12 --width=1280 --height=720 --mplane=0 --verbose\n", prog);
}

/*
 * 函数作用：解析命令行参数。
 */
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
 * 函数作用：demo04 入口。
 * 主流程：
 * 1) 解析 fourcc；
 * 2) open + QUERYCAP；
 * 3) S_FMT OUTPUT；
 * 4) S_FMT CAPTURE；
 * 5) close。
 * 驱动影子线：
 * - S_FMT 触达驱动格式协商路径；
 * - 常见 EINVAL 表示格式/分辨率不被当前驱动接受。
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

    v4l2_capability cap;
    if (!m2m_demo::querycap(fd, &cap)) {
        close(fd);
        return 1;
    }

    printf("[demo04] driver=%s card=%s dev=%s\n", reinterpret_cast<char*>(cap.driver),
           reinterpret_cast<char*>(cap.card), args.dev.c_str());

    // 根据 mplane 开关选择 OUTPUT/CAPTURE 队列类型。
    const uint32_t out_type = m2m_demo::output_type_from_mplane(args.mplane);
    const uint32_t cap_type = m2m_demo::capture_type_from_mplane(args.mplane);

    // 先协商 decoder 输入侧（OUTPUT）格式。
    if (!m2m_demo::set_format(fd, out_type, in_fourcc, args.width, args.height,
                              args.verbose)) {
        close(fd);
        return 1;
    }

    // 再协商 decoder 输出侧（CAPTURE）格式。
    if (!m2m_demo::set_format(fd, cap_type, out_fourcc, args.width, args.height,
                              args.verbose)) {
        close(fd);
        return 1;
    }

    printf("[demo04] PASS: S_FMT OUTPUT/CAPTURE both succeeded\n");

    close(fd);
    return 0;
}
