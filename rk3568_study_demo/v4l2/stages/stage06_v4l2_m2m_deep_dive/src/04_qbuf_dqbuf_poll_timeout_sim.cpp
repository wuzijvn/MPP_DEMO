#include "00_stage06_m2m_common.hpp"

#include <algorithm>

int main(int argc, char** argv) {
    const int frames = std::max(1, stage06::get_arg_int(argc, argv, "--frames", 8));
    const int timeout_at = stage06::get_arg_int(argc, argv, "--timeout-at", 5);
    const bool recover = stage06::get_arg_int(argc, argv, "--recover", 1) != 0;
    const int output_depth = std::max(1, stage06::get_arg_int(argc, argv, "--output-depth", 3));
    const int capture_depth = std::max(1, stage06::get_arg_int(argc, argv, "--capture-depth", 4));

    stage06::QueueCounters counters;
    int output_in_driver = 0;
    int capture_in_driver = 0;
    int max_output_depth = 0;
    int max_capture_depth = 0;

    std::cout << "Stage06 Demo04: QBUF/DQBUF + poll timeout simulation\n";
    stage06::print_line("frames", std::to_string(frames));
    stage06::print_line("timeout_at", std::to_string(timeout_at));
    stage06::print_line("recover", stage06::yes_no(recover));
    std::cout << "\n";

    for (int frame = 1; frame <= frames; ++frame) {
        /*
         * OUTPUT/CAPTURE 都需要提前保持一定队列深度。
         * 如果 CAPTURE 空 buffer 不够，硬件即使收到压缩码流也可能无法输出 decoded frame，
         * 用户态最后看到的就是 poll 不 ready 或 DQBUF timeout。
         */
        if (output_in_driver < output_depth) {
            counters.qbuf_output++;
            output_in_driver++;
            std::cout << "[frame " << frame << "] QBUF OUTPUT, output_depth=" << output_in_driver << "\n";
        }
        if (capture_in_driver < capture_depth) {
            counters.qbuf_capture++;
            capture_in_driver++;
            std::cout << "[frame " << frame << "] QBUF CAPTURE, capture_depth=" << capture_in_driver << "\n";
        }

        max_output_depth = std::max(max_output_depth, output_in_driver);
        max_capture_depth = std::max(max_capture_depth, capture_in_driver);
        counters.poll_calls++;

        if (timeout_at > 0 && frame == timeout_at) {
            counters.timeout_count++;
            std::cout << "[frame " << frame << "] poll timeout: no completed CAPTURE buffer before deadline\n";
            std::cout << "           layer hint: bitstream/header/bytesused/IRQ/runtime-PM/firmware/job scheduling\n";
            if (recover) {
                counters.recovery_count++;
                std::cout << "           recovery: STREAMOFF both queues -> drain logs -> STREAMON and requeue buffers\n";
                output_in_driver = 0;
                capture_in_driver = 0;
                continue;
            }
            std::cout << "verdict=DQBUF_TIMEOUT_NOT_RECOVERED\n";
            stage06::print_counters(counters);
            return 1;
        }

        /*
         * poll ready 后再 DQBUF。
         * DQBUF OUTPUT 表示压缩输入 buffer 已消费；DQBUF CAPTURE 表示 raw frame 已完成。
         */
        if (output_in_driver > 0) {
            counters.dqbuf_output++;
            output_in_driver--;
        }
        if (capture_in_driver > 0) {
            counters.dqbuf_capture++;
            capture_in_driver--;
        }
        std::cout << "[frame " << frame << "] DQBUF OUTPUT + CAPTURE, output_depth="
                  << output_in_driver << ", capture_depth=" << capture_in_driver << "\n";
    }

    std::cout << "\n性能/稳定性观察指标：\n";
    stage06::print_counter("max_output_depth", max_output_depth);
    stage06::print_counter("max_capture_depth", max_capture_depth);
    stage06::print_counters(counters);
    std::cout << "verdict=" << (counters.timeout_count == 0 ? "QUEUE_LOOP_OK" : "TIMEOUT_DETECTED_AND_RECOVERED") << "\n";
    return 0;
}
