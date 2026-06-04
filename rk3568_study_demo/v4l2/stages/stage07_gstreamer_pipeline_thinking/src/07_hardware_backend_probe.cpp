#include "00_stage07_gst_common.hpp"

/*
 * Demo07: 硬件后端候选与证据边界。
 *
 * 学习目标：
 * - 识别 GStreamer 中可能走硬件的 decoder/encoder/backend element。
 * - 明确“发现插件”不是“证明硬解”。
 * - 给出 RK 板、V4L2、VAAPI/OpenMAX 的下一步证据采集路径。
 */
int main(int argc, char** argv) {
    const bool require_hw = stage07::has_arg(argc, argv, "--require-hw");
    const std::vector<std::string> candidates = {
        "avdec_h264_rkmpp", "avdec_hevc_rkmpp", "avdec_vp8_rkmpp", "avdec_vp9_rkmpp",
        "mpph264enc", "v4l2h264dec", "v4l2slh264dec", "v4l2src", "v4l2sink",
        "vaapih264dec", "omxh264dec"
    };

    std::cout << "Stage07 Demo07: hardware backend candidate probe\n";
    int found = 0;
    for (size_t i = 0; i < candidates.size(); ++i) {
        const bool ok = stage07::inspect_element_exists(candidates[i]);
        if (ok) {
            ++found;
        }
        std::cout << std::left << std::setw(22) << candidates[i]
                  << " installed=" << stage07::yes_no(ok)
                  << " | " << stage07::hardware_backend_hint(candidates[i])
                  << "\n";
    }

    std::cout << "\n[evidence checklist]\n";
    std::cout << "1. gst pipeline exits with EOS or stable streaming.\n";
    std::cout << "2. selected element name is visible in command/log, not only auto-plugged guesswork.\n";
    std::cout << "3. backend logs/dmesg/device node prove hardware path, or CPU usage/fallback check supports it.\n";
    std::cout << "4. output caps/memory type are understood: SystemMemory, DMABuf, DRM PRIME, or vendor memory.\n";
    std::cout << "5. failure report states what is proven and what is not proven.\n";

    const bool pass = require_hw ? (found > 0) : true;
    std::cout << "found_hw_candidates=" << found << "\n";
    std::cout << "verdict=" << (pass ? "PASS_BACKEND_PROBE" : "FAIL_REQUIRED_HW_BACKEND_NOT_FOUND") << "\n";
    return pass ? 0 : 1;
}
