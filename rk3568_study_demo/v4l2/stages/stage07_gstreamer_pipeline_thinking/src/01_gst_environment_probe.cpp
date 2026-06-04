#include "00_stage07_gst_common.hpp"

/*
 * Demo01: GStreamer 环境与 element 探测。
 *
 * 学习目标：
 * - 确认 gst-launch-1.0/gst-inspect-1.0 是否可用。
 * - 识别基础 pipeline element 与 SoC 相关硬件后端候选。
 * - 建立一个重要边界：插件存在不等于硬件路径已经跑通。
 *
 * 工作场景：
 * - 新板 bring-up 的第一步不是直接写应用，而是先列插件、列设备、列后端。
 * - 如果 element 缺失，问题通常在 rootfs/package/plugin registry。
 * - 如果 element 存在但 pipeline 失败，再进入 caps、码流、driver/device 节点排查。
 */
int main() {
    std::cout << "Stage07 Demo01: GStreamer environment and backend probe\n";

    const bool has_launch = stage07::tool_exists("gst-launch-1.0");
    const bool has_inspect = stage07::tool_exists("gst-inspect-1.0");
    stage07::print_kv("gst-launch-1.0", stage07::yes_no(has_launch));
    stage07::print_kv("gst-inspect-1.0", stage07::yes_no(has_inspect));

    if (!has_launch || !has_inspect) {
        std::cout << "verdict=FAIL_GSTREAMER_TOOLS_MISSING\n";
        return 1;
    }

    stage07::CommandResult version = stage07::run_command_capture("gst-launch-1.0 --version");
    std::cout << "\n[version]\n" << version.output << "\n";

    const std::vector<std::string> base_elements = {
        "videotestsrc", "capsfilter", "queue", "identity",
        "videoconvert", "fakesink", "filesrc", "decodebin", "fpsdisplaysink"
    };
    std::cout << "[base element matrix]\n";
    for (size_t i = 0; i < base_elements.size(); ++i) {
        const bool ok = stage07::inspect_element_exists(base_elements[i]);
        stage07::print_kv(base_elements[i], stage07::yes_no(ok));
    }

    const std::vector<std::string> hardware_candidates = {
        "avdec_h264_rkmpp", "avdec_hevc_rkmpp", "avdec_vp9_rkmpp",
        "mpph264enc", "v4l2h264dec", "v4l2slh264dec",
        "vaapih264dec", "omxh264dec"
    };
    int found_hw_candidates = 0;
    std::cout << "\n[hardware/backend candidate matrix]\n";
    for (size_t i = 0; i < hardware_candidates.size(); ++i) {
        const bool ok = stage07::inspect_element_exists(hardware_candidates[i]);
        if (ok) {
            ++found_hw_candidates;
        }
        std::cout << std::left << std::setw(24) << hardware_candidates[i]
                  << " : installed=" << stage07::yes_no(ok)
                  << " | hint=" << stage07::hardware_backend_hint(hardware_candidates[i])
                  << "\n";
    }

    std::cout << "\n[driver shadow]\n";
    std::cout << "element_installed=yes only proves user-space plugin visibility.\n";
    std::cout << "hardware_proof requires a successful pipeline plus backend logs, device node or dmesg evidence.\n";
    std::cout << "found_hw_candidates=" << found_hw_candidates << "\n";
    std::cout << "verdict=PASS_GSTREAMER_ENVIRONMENT_PROBE\n";
    return 0;
}
