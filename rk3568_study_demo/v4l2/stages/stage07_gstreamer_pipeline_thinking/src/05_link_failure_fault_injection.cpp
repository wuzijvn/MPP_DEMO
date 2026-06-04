#include "00_stage07_gst_common.hpp"

/*
 * Demo05: link/caps failure 故障注入。
 *
 * 学习目标：
 * - 故意让 raw source 去输出 video/x-h264，观察 link failure。
 * - 训练“失败分层”：pipeline 拼写、element 缺失、pad template 不支持、caps 不匹配、运行时 not-negotiated。
 *
 * 真实工作映射：
 * - 板端硬解失败时，不要第一反应就说 driver 坏了。
 * - 先看 gst-launch 是否能 link，再看 caps，再看 parser/decoder/sink，再看 device node/dmesg。
 */
int main() {
    const std::string cmd =
        "gst-launch-1.0 videotestsrc num-buffers=3 ! video/x-h264 ! fakesink sync=false";

    std::cout << "Stage07 Demo05: intentional link failure and layer classification\n";
    stage07::CommandResult result = stage07::run_command_capture(cmd);
    stage07::print_command_result(result, 2400);

    const std::string layer = stage07::classify_gst_failure(result.output, result.exit_code);
    stage07::print_kv("classified_layer", layer);
    std::cout << "\n[why this failure is expected]\n";
    std::cout << "videotestsrc produces raw video buffers. It cannot directly produce compressed H.264 caps.\n";
    std::cout << "Correct paths need encoder/parser or a compressed file source before h264parse/decoder.\n";
    std::cout << "\n[driver shadow]\n";
    std::cout << "This is still user-space link/caps failure. It does not reach VPU driver ioctl path.\n";

    const bool expected = (layer == "link_or_caps_negotiation_failure");
    std::cout << "verdict=" << (expected ? "PASS_EXPECTED_LINK_FAILURE" : "FAIL_UNEXPECTED_FAILURE_CLASS") << "\n";
    return expected ? 0 : 1;
}
