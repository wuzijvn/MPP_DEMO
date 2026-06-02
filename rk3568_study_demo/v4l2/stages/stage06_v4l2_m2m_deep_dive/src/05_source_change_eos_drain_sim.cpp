#include "00_stage06_m2m_common.hpp"

#include <algorithm>

int main(int argc, char** argv) {
    const int frames = std::max(2, stage06::get_arg_int(argc, argv, "--frames", 10));
    const int source_change_at = stage06::get_arg_int(argc, argv, "--source-change-at", 4);
    const int eos_at = stage06::get_arg_int(argc, argv, "--eos-at", frames);
    const bool bad_reconfigure = stage06::get_arg_int(argc, argv, "--bad-reconfigure", 0) != 0;

    stage06::QueueCounters counters;
    bool capture_configured = true;
    bool draining = false;

    std::cout << "Stage06 Demo05: SOURCE_CHANGE + EOS/drain simulation\n";
    stage06::print_line("frames", std::to_string(frames));
    stage06::print_line("source_change_at", std::to_string(source_change_at));
    stage06::print_line("eos_at", std::to_string(eos_at));
    stage06::print_line("bad_reconfigure", stage06::yes_no(bad_reconfigure));
    std::cout << "\n";

    for (int frame = 1; frame <= frames; ++frame) {
        counters.qbuf_output++;
        counters.qbuf_capture++;
        counters.poll_calls++;
        std::cout << "[frame " << frame << "] RUNNING: QBUF OUTPUT/CAPTURE -> poll\n";

        if (source_change_at > 0 && frame == source_change_at) {
            counters.source_change_count++;
            capture_configured = false;
            std::cout << "[frame " << frame << "] V4L2_EVENT_SOURCE_CHANGE: decoded size/stride may change\n";
            std::cout << "           required: STREAMOFF CAPTURE -> DQBUF remaining -> REQBUFS 0 -> S_FMT/REQBUFS -> STREAMON\n";
            if (bad_reconfigure) {
                counters.timeout_count++;
                std::cout << "           injected bug: continue DQBUF without CAPTURE reconfigure -> timeout risk\n";
                std::cout << "verdict=SOURCE_CHANGE_RECONFIGURE_MISSING\n";
                stage06::print_counters(counters);
                return 1;
            }
            counters.recovery_count++;
            capture_configured = true;
            std::cout << "           recovery ok: CAPTURE queue reconfigured and buffers requeued\n";
        }

        if (!capture_configured) {
            counters.timeout_count++;
            std::cout << "[frame " << frame << "] CAPTURE not configured, cannot DQBUF decoded frame\n";
            break;
        }

        counters.dqbuf_output++;
        counters.dqbuf_capture++;
        std::cout << "[frame " << frame << "] DQBUF OUTPUT consumed + CAPTURE decoded\n";

        if (frame == eos_at) {
            draining = true;
            counters.eos_count++;
            std::cout << "[frame " << frame << "] EOS queued: stop feeding OUTPUT, keep DQBUF until LAST/drain done\n";
            break;
        }
    }

    if (draining) {
        /*
         * drain 的核心：不再输入新 packet，但要继续取出驱动/固件内部可能缓存的参考帧。
         * H.264/H.265 有 DPB，最后输入 EOS 不代表最后输出帧已经立刻返回。
         */
        for (int drain = 1; drain <= 2; ++drain) {
            counters.poll_calls++;
            counters.dqbuf_capture++;
            std::cout << "[drain " << drain << "] poll + DQBUF CAPTURE delayed reference frame\n";
        }
        std::cout << "[drain done] LAST buffer observed, STREAMOFF both queues\n";
    }

    std::cout << "\n关键计数器：\n";
    stage06::print_counters(counters);
    std::cout << "verdict=SOURCE_CHANGE_EOS_DRAIN_HANDLED\n";
    return 0;
}
