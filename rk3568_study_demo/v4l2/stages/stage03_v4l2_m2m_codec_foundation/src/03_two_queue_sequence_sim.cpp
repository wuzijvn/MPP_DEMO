#include <stdio.h>

#include <fstream>
#include <string>

#include "00_m2m_demo_common.hpp"

namespace {

/*
 * 结构体作用：03 demo 参数集合。
 * 知识边界：仅做双队列所有权“可视化模拟”，不访问真实设备节点。
 */
struct Args {
    std::string out_dir = "./logs/sim_sequence";
    std::string output_tag = "demo03";
    int dq_loops = 3;
};

void print_usage(const char* prog) {
    printf("%s: Stage03 demo 03 - two queue sequence simulation\n", prog);
    printf("Usage: %s --out-dir=./logs/sim_sequence --output-tag=demo03 --dq-loops=3\n",
           prog);
}

/*
 * 函数作用：解析参数。
 */
bool parse_args(int argc, char** argv, Args* a) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return false;
        } else if (m2m_demo::parse_kv(argv[i], "--out-dir", &a->out_dir)) {
        } else if (m2m_demo::parse_kv(argv[i], "--output-tag", &a->output_tag)) {
        } else if (m2m_demo::parse_kv_int(argv[i], "--dq-loops", &a->dq_loops)) {
        } else {
            fprintf(stderr, "unknown arg: %s\n", argv[i]);
            return false;
        }
    }
    return true;
}

}  // namespace

/*
 * 函数作用：demo03 入口。
 * 数据流目标：把状态机顺序“打印成可见证据”，便于新同学建立心智模型。
 */
int main(int argc, char** argv) {
    Args args;
    if (!parse_args(argc, argv, &args)) {
        return 1;
    }

    printf("[demo03] two-queue ownership sequence simulation\n");
    printf("1) S_FMT OUTPUT\n");
    printf("2) S_FMT CAPTURE\n");
    printf("3) REQBUFS OUTPUT/CAPTURE\n");
    printf("4) QBUF CAPTURE (empty buffers -> driver)\n");
    printf("5) QBUF OUTPUT (input buffers -> driver)\n");
    printf("6) STREAMON both queues\n");

    for (int i = 0; i < args.dq_loops; ++i) {
        // 所有权方向：驱动产出 CAPTURE buffer 后，交给用户态处理。
        printf("7.%d DQBUF CAPTURE (driver -> user)\n", i + 1);
        // 用户态处理完成后再归还驱动，形成循环。
        printf("7.%d QBUF CAPTURE (user -> driver)\n", i + 1);
    }

    printf("8) STREAMOFF both queues\n");

    if (!m2m_demo::ensure_dir(args.out_dir)) {
        return 1;
    }

    const std::string report = args.out_dir + "/" + args.output_tag + "_report.md";
    std::ofstream ofs(report.c_str());
    if (!ofs.is_open()) {
        fprintf(stderr, "open report failed: %s\n", report.c_str());
        return 1;
    }

    ofs << "# demo03 two-queue sequence report\n\n";
    ofs << "dq_loops=" << args.dq_loops << "\n";
    ofs << "pass_condition=ownership_sequence_visible\n";
    ofs.close();

    printf("[demo03] PASS: report=%s\n", report.c_str());
    return 0;
}
