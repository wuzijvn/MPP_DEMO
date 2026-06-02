#include <stdio.h>

#include <string>

#include "00_m2m_demo_common.hpp"

namespace {

/*
 * 结构体作用：10 demo 参数。
 * 说明：该 demo 是“可运行恢复路径清单打印器”。
 */
struct Args {
    std::string dev = "/dev/video0";
    int timeout_ms = 120;
    bool verbose = false;
};

void print_usage(const char* prog) {
    printf("%s: Stage03 demo 10 - SOURCE_CHANGE/EOS/drain recovery note runner\n", prog);
    printf("Usage:\n");
    printf("  %s --dev=/dev/video0 --timeout-ms=120 --verbose\n", prog);
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

/*
 * 函数作用：打印 SOURCE_CHANGE 恢复路径。
 * 驱动影子线：
 * - SOURCE_CHANGE 常由驱动/固件在分辨率变化时上报；
 * - 用户态需释放旧 CAPTURE buffer 并重配。
 */
void print_source_change_path() {
    printf("[source_change] expected recovery path:\n");
    printf("  1) DQEVENT(V4L2_EVENT_SOURCE_CHANGE)\n");
    printf("  2) STREAMOFF(CAPTURE)\n");
    printf("  3) REQBUFS(CAPTURE,count=0) release old capture buffers\n");
    printf("  4) G_FMT or decoder metadata query new resolution\n");
    printf("  5) S_FMT(CAPTURE new size/fmt)\n");
    printf("  6) REQBUFS+QUERYBUF+MMAP(CAPTURE new buffers)\n");
    printf("  7) QBUF all CAPTURE buffers\n");
    printf("  8) STREAMON(CAPTURE) and continue DQBUF loop\n");
}

/*
 * 函数作用：打印 EOS/drain 收敛路径。
 */
void print_eos_drain_path() {
    printf("[eos_drain] expected path:\n");
    printf("  1) OUTPUT queue last input submitted\n");
    printf("  2) Continue DQBUF CAPTURE until LAST flag or drain condition\n");
    printf("  3) Stop after all in-flight frames are returned\n");
    printf("  4) STREAMOFF both queues\n");
    printf("  5) Unmap buffers and close device\n");
}

}  // namespace

/*
 * 函数作用：demo10 入口。
 * 行为：
 * - 先做一次 poll 观察当前节点是否有事件；
 * - 再输出 SOURCE_CHANGE 与 EOS/drain 的标准恢复顺序。
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

    const int pr = m2m_demo::poll_readable(fd, args.timeout_ms, args.verbose);
    if (pr == 0) {
        printf("[demo10] poll timeout observed. In real decode flow this may imply no ready event yet.\n");
    } else if (pr > 0) {
        printf("[demo10] poll got event. Real flow should DQEVENT/DQBUF then branch by flags/event type.\n");
    } else {
        fprintf(stderr, "[demo10] poll error: %s\n", strerror(errno));
    }

    print_source_change_path();
    print_eos_drain_path();

    printf("[demo10] NOTE: this demo is a runnable recovery-playbook printer, not a full codec stream implementation.\n");

    close(fd);
    return 0;
}
