#include <stdio.h>

#include <string>

#include "00_m2m_demo_common.hpp"

namespace {

/*
 * 结构体作用：08 demo 参数。
 * 知识边界：仅观察 poll timeout 行为，不绑定完整解码主循环。
 */
struct Args {
    std::string dev = "/dev/video0";
    int timeout_ms = 50;
    bool verbose = false;
};

void print_usage(const char* prog) {
    printf("%s: Stage03 demo 08 - poll timeout observation\n", prog);
    printf("Usage:\n");
    printf("  %s --dev=/dev/video0 --timeout-ms=50 --verbose\n", prog);
}

bool parse_args(int argc, char** argv, Args* a) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return false;
        } else if (m2m_demo::parse_kv(argv[i], "--dev", &a->dev)) {
        } else if (m2m_demo::parse_kv_int(argv[i], "--timeout-ms", &a->timeout_ms)) {
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
 * 函数作用：demo08 入口。
 * 驱动影子线：
 * - poll timeout 常用于区分“暂时无数据”与“可能卡住”；
 * - poll event 常由中断/状态变化唤醒。
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

    const int ret = m2m_demo::poll_readable(fd, args.timeout_ms, args.verbose);
    if (ret == 0) {
        printf("[demo08] PASS: poll timeout observed (ret=0).\n");
    } else if (ret > 0) {
        printf("[demo08] INFO: poll got event (ret=%d), environment has pending events.\n",
               ret);
    } else {
        printf("[demo08] FAIL: poll error=%s\n", strerror(errno));
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
