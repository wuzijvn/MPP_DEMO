#include <stdio.h>

#include <string>

#include "00_m2m_demo_common.hpp"

namespace {

/*
 * 结构体作用：01 demo 参数集合。
 * 知识边界：本 demo 只训练“打开节点 + 基础参数解析”，不进入任何 ioctl 状态机。
 */
struct Args {
    std::string dev = "/dev/video0";
    bool verbose = false;
};

/*
 * 函数作用：打印命令用法。
 */
void print_usage(const char* prog) {
    printf("%s: Stage03 demo 01 - open video device\n", prog);
    printf("Usage: %s --dev=/dev/video0 --verbose\n", prog);
}

/*
 * 函数作用：解析命令行参数。
 * 输入假设：参数采用 --key=value 或 --verbose。
 * 失败现象：
 * - 未知参数会直接报错并返回 false。
 */
bool parse_args(int argc, char** argv, Args* a) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return false;
        } else if (m2m_demo::parse_kv(argv[i], "--dev", &a->dev)) {
            // --dev 解析成功，无需额外动作。
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
 * 函数作用：demo01 入口。
 * 主流程：
 * 1) 解析参数；
 * 2) open 设备节点；
 * 3) 打印可观测结果；
 * 4) close 资源。
 * 驱动影子线：
 * - open() 会进入驱动 file_operations.open；
 * - close() 对应驱动 release，资源应对称回收。
 */
int main(int argc, char** argv) {
    Args args;
    if (!parse_args(argc, argv, &args)) {
        return 1;
    }

    int fd = -1;
    if (!m2m_demo::open_node(args.dev, &fd)) {
        return 1;
    }

    if (args.verbose) {
        printf("[demo01] open success: dev=%s fd=%d\n", args.dev.c_str(), fd);
    }
    printf("[demo01] PASS: open video device done\n");

    // 资源对称释放：open 成功后必须 close。
    close(fd);
    return 0;
}
