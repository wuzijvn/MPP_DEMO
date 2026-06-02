#include "00_ffmpeg_hwaccel_common.hpp"

#include <string>

namespace {

struct Args {
    std::string input;
    std::string hw_type;
    bool hw_type_set = false;
    std::string decoder = "h264_rkmpp";
    int max_frames = 8;
};

bool parse_args(int argc, char** argv, Args* a) {
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--input=", 8) == 0) a->input = argv[i] + 8;
        else if (strncmp(argv[i], "--hw-type=", 10) == 0) {
            a->hw_type = argv[i] + 10;
            a->hw_type_set = true;
        }
        else if (strncmp(argv[i], "--decoder=", 10) == 0) a->decoder = argv[i] + 10;
        else if (strncmp(argv[i], "--max-frames=", 13) == 0) a->max_frames = atoi(argv[i] + 13);
    }
    return !a->input.empty();
}

}  // namespace

/*
 * demo05 目标：
 * 把“hwdownload 会引入 copy-back”这个概念讲清楚，强调证据口径。
 *
 * 说明：该 demo 是解释型可执行程序，真正 frame 级证据请结合 demo04 输出。
 */
int main(int argc, char** argv) {
    using namespace ffmpeg_stage05;

    Args a;
    if (!parse_args(argc, argv, &a)) {
        fprintf(stderr, "Usage: --input=PATH [--decoder=h264_rkmpp] [--hw-type=drm|vaapi|rkmpp] [--max-frames=8]\n");
        return 1;
    }

    printf("[demo05] teaching intent: this demo explains why hwdownload introduces copy-back.\n");
    printf("[demo05] run demo04 first to gather real hw/sw frame evidence.\n");
    printf("[demo05] params input=%s decoder=%s hw_type=%s max_frames=%d\n",
           a.input.c_str(), a.decoder.c_str(),
           a.hw_type_set ? a.hw_type.c_str() : "(not-forced)", a.max_frames);

    printf("[demo05] expected evidence:\n");
    printf("  1) hwdevice mode: if hw frame appears, transfer_data causes device->cpu copy\n");
    printf("  2) rkmpp wrapper mode: decoded frames may be CPU-visible; hard-decode proof comes from decoder selection and benchmark/dmesg\n");
    printf("  3) if only software decoder is selected, fallback happened or backend unavailable\n");

    // 驱动影子线：copy-back 意味着后续链路不再是纯硬件零拷贝，CPU/带宽压力会上升。
    printf("[demo05] driver_shadow: hwdownload often breaks zero-copy chain and increases memory bandwidth pressure.\n");

    printf("[demo05] PASS\n");
    return 0;
}
