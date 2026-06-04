#include "00_stage07_gst_common.hpp"

/*
 * Demo02: raw video caps negotiation。
 *
 * 学习目标：
 * - 看懂 `video/x-raw,format=NV12,width=...,height=...,framerate=...` 的约束作用。
 * - 理解 capsfilter 不是“转换器”，它只是声明/限制格式；真正转换发生在 videoconvert。
 * - 把用户态 caps 映射到驱动侧 format negotiation：ENUM_FMT/TRY_FMT/S_FMT。
 *
 * 驱动影子线：
 * - GStreamer caps 中的 format/width/height/framerate 对应 V4L2 pixelformat、尺寸、timeperframe。
 * - 对真实硬件路径，还会牵涉 stride/alignment/modifier/profile/level 等限制。
 */
int main(int argc, char** argv) {
    const int width = stage07::get_arg_int(argc, argv, "--width", 320);
    const int height = stage07::get_arg_int(argc, argv, "--height", 240);
    const int frames = stage07::get_arg_int(argc, argv, "--frames", 8);
    const std::string in_format = stage07::get_arg(argc, argv, "--in-format", "NV12");
    const std::string out_format = stage07::get_arg(argc, argv, "--out-format", "I420");

    std::ostringstream cmd;
    cmd << "gst-launch-1.0 -q "
        << "videotestsrc num-buffers=" << frames << " "
        << "! video/x-raw,format=" << in_format
        << ",width=" << width << ",height=" << height << ",framerate=30/1 "
        << "! videoconvert "
        << "! video/x-raw,format=" << out_format
        << " ! fakesink sync=false";

    std::cout << "Stage07 Demo02: raw caps negotiation\n";
    stage07::print_kv("input_caps", "video/x-raw,format=" + in_format);
    stage07::print_kv("output_caps", "video/x-raw,format=" + out_format);
    stage07::print_kv("resolution", std::to_string(width) + "x" + std::to_string(height));

    stage07::CommandResult result = stage07::run_command_capture(cmd.str());
    stage07::print_command_result(result, 2000);

    std::cout << "\n[walkthrough]\n";
    std::cout << "videotestsrc produces raw test frames; first capsfilter requests the producer shape.\n";
    std::cout << "videoconvert is the transform element that can change pixel layout.\n";
    std::cout << "second capsfilter requests the downstream shape; fakesink consumes buffers without display.\n";
    std::cout << "\n[driver shadow]\n";
    std::cout << "On a V4L2-backed hardware element, the same negotiation pressure becomes TRY_FMT/S_FMT and queue buffer layout.\n";
    std::cout << "If caps cannot be linked, check plugin pad template first, then driver-supported formats.\n";
    std::cout << "verdict=" << (result.ok() ? "PASS_CAPS_NEGOTIATION" : "FAIL_CAPS_NEGOTIATION") << "\n";
    return result.ok() ? 0 : 1;
}
