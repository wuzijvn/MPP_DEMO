#include "00_stage06_m2m_common.hpp"

#include <algorithm>

using stage06::QueueCounters;

namespace {

struct Step {
    int id;
    std::string userspace_action;
    std::string driver_shadow;
    std::string visible_evidence;
};

void print_step(const Step& step) {
    std::cout << std::setw(2) << step.id << " | " << std::left
              << std::setw(34) << step.userspace_action << " | "
              << std::setw(42) << step.driver_shadow << " | "
              << step.visible_evidence << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    const std::string codec = stage06::get_arg(argc, argv, "--codec", "H264");
    const std::string raw = stage06::get_arg(argc, argv, "--raw", "NV12");
    const int frames = std::max(1, stage06::get_arg_int(argc, argv, "--frames", 4));
    const int source_change_at = stage06::get_arg_int(argc, argv, "--source-change-at", 3);

    /*
     * 这个 demo 不打开真实设备。
     * 目的：先把 stateful decoder ioctl 顺序背后的所有权链路讲清楚。
     * 工作中你看到 FFmpeg V4L2 M2M、GStreamer v4l2codecs、或者自研测试工具时，
     * 大体都要回答：哪个 ioctl 让 buffer 从用户态交给驱动？哪个 ioctl 把完成结果拿回来？
     */
    std::cout << "Stage06 Demo01: stateful decoder ioctl sequence map\n";
    stage06::print_line("codec", codec);
    stage06::print_line("capture_raw", raw);
    stage06::print_line("frames", std::to_string(frames));
    stage06::print_line("source_change_at", std::to_string(source_change_at));
    std::cout << "\n";

    const Step steps[] = {
        {1, "open(/dev/videoX)", "file_operations.open 创建 codec session", "fd >= 0"},
        {2, "VIDIOC_QUERYCAP", "驱动返回 capabilities", "capabilities 含 M2M/STREAMING"},
        {3, "S_FMT OUTPUT " + codec, "压缩码流队列格式协商", "OUTPUT pixelformat=" + codec},
        {4, "S_FMT CAPTURE " + raw, "解码帧队列格式协商", "CAPTURE pixelformat=" + raw},
        {5, "REQBUFS/QUERYBUF/MMAP", "vb2 分配/准备 queue buffer", "用户态获得 mmap 地址"},
        {6, "QBUF OUTPUT bytesused>0", "压缩 packet 所有权交给驱动", "OUTPUT queued"},
        {7, "QBUF CAPTURE empty", "空 frame buffer 所有权交给驱动", "CAPTURE queued"},
        {8, "STREAMON both queues", "v4l2-mem2mem 可调度 job", "streaming=on"},
        {9, "poll + DQBUF loop", "IRQ/worker 完成后 wakeup", "DQBUF 返回 sequence/timestamp"},
        {10, "SOURCE_CHANGE", "驱动提示分辨率/stride 改变", "需要重配 CAPTURE"},
        {11, "EOS/drain", "驱动排空内部 DPB/引用帧", "最后一个 buffer 带 LAST"},
        {12, "STREAMOFF + munmap + close", "停止硬件 job 并释放 session", "资源对称释放"},
    };

    std::cout << "id | userspace action                   | driver shadow                             | visible evidence\n";
    std::cout << "---+------------------------------------+-------------------------------------------+------------------------------\n";
    for (const Step& step : steps) {
        print_step(step);
    }

    QueueCounters counters;
    std::cout << "\n模拟 decode loop：\n";
    for (int i = 1; i <= frames; ++i) {
        /*
         * OUTPUT QBUF：用户态把一段压缩码流交给驱动。
         * 前置条件：该 OUTPUT buffer 属于用户态，且 bytesused 必须是实际 payload 大小。
         * 失败典型：bytesused=0 会导致驱动收到空包，可能 EINVAL、无输出、或 DQBUF timeout。
         */
        counters.qbuf_output++;
        std::cout << "[frame " << i << "] QBUF OUTPUT compressed_packet bytesused>0 -> DRIVER\n";

        /*
         * CAPTURE QBUF：用户态把空 raw frame buffer 交给驱动。
         * 前置条件：CAPTURE queue 已经 REQBUFS/QUERYBUF/MMAP；buffer 不再被 CPU 写。
         * 驱动影子线：vb2 buffer 进入 queued/active 状态，等待硬件填充。
         */
        counters.qbuf_capture++;
        std::cout << "[frame " << i << "] QBUF CAPTURE empty_frame_buffer -> DRIVER\n";

        counters.poll_calls++;
        std::cout << "[frame " << i << "] poll waits for driver wakeup\n";

        if (source_change_at > 0 && i == source_change_at) {
            counters.source_change_count++;
            counters.recovery_count++;
            std::cout << "[frame " << i << "] EVENT SOURCE_CHANGE: STREAMOFF CAPTURE -> REQBUFS new size -> STREAMON CAPTURE\n";
        }

        counters.dqbuf_output++;
        counters.dqbuf_capture++;
        std::cout << "[frame " << i << "] DQBUF OUTPUT done + DQBUF CAPTURE decoded_frame -> USER\n";
    }
    counters.eos_count = 1;

    std::cout << "\n关键计数器：\n";
    stage06::print_counters(counters);
    std::cout << "\nverdict=SEQUENCE_MAP_READY\n";
    std::cout << "下一步：运行 02_format_negotiation_probe，观察 TRY_FMT/S_FMT 如何把请求格式变成驱动接受的格式。\n";
    return 0;
}
