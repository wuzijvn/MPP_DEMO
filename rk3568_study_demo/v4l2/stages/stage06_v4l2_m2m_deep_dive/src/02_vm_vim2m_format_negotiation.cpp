#include "00_stage06_m2m_common.hpp"

/*
 * Demo02 目标：
 * - 对 VM 的 vim2m 节点真实执行 TRY_FMT 或 S_FMT。
 * - 用 raw-to-raw fourcc（默认 RGBP -> RGBP）证明格式协商路径真实触达驱动。
 *
 * 深化点：
 * - Stage03 的格式 demo 主要证明 S_FMT 调用。
 * - Stage06 增加 capability gate、TRY/S_FMT 双模式、失败 verdict，并明确区分 vim2m raw M2M 与 codec M2M。
 */
int main(int argc, char** argv) {
    const std::string device = stage06::get_arg(argc, argv, "--device", "/dev/video0");
    const std::string output_text = stage06::get_arg(argc, argv, "--output", "RGBP");
    const std::string capture_text = stage06::get_arg(argc, argv, "--capture", "RGBP");
    const uint32_t width = stage06::get_arg_u32(argc, argv, "--width", 640);
    const uint32_t height = stage06::get_arg_u32(argc, argv, "--height", 480);
    const bool apply = stage06::get_arg_int(argc, argv, "--apply", 0) != 0;
    const bool verbose = stage06::get_arg_int(argc, argv, "--verbose", 1) != 0;

    uint32_t output_fourcc = 0;
    uint32_t capture_fourcc = 0;
    if (!stage06::parse_fourcc(output_text, &output_fourcc) ||
        !stage06::parse_fourcc(capture_text, &capture_fourcc)) {
        std::cerr << "--output/--capture must be 4 chars, example RGBP\n";
        return 2;
    }

    std::cout << "Stage06 Demo02: VM vim2m real format negotiation\n";
    stage06::print_line("device", device);
    stage06::print_line("mode", apply ? "S_FMT(apply)" : "TRY_FMT(safe probe)");
    stage06::print_line("output_fourcc", output_text);
    stage06::print_line("capture_fourcc", capture_text);
    stage06::print_line("size", std::to_string(width) + "x" + std::to_string(height));

    int fd = -1;
    if (!stage06::open_video_node(device, &fd)) {
        std::cout << "verdict=FAIL_OPEN_DEVICE\n";
        return 1;
    }

    struct v4l2_capability cap;
    if (!stage06::query_capability(fd, &cap)) {
        close(fd);
        std::cout << "verdict=FAIL_QUERYCAP\n";
        return 1;
    }
    stage06::print_capability(cap);

    const uint32_t caps = stage06::active_caps(cap);
    if (!stage06::is_m2m_capable(caps) || !stage06::is_streaming_capable(caps)) {
        close(fd);
        std::cout << "verdict=FAIL_NOT_USABLE_M2M_STREAMING_NODE\n";
        return 1;
    }

    struct v4l2_format out_fmt;
    struct v4l2_format cap_fmt;
    const bool out_ok = stage06::try_or_set_format(
        fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, output_fourcc, width, height, apply,
        &out_fmt, verbose);
    const bool cap_ok = stage06::try_or_set_format(
        fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, capture_fourcc, width, height, apply,
        &cap_fmt, verbose);

    close(fd);
    std::cout << "verdict=" << ((out_ok && cap_ok) ? "PASS_VM_FORMAT_NEGOTIATION" : "FAIL_VM_FORMAT_NEGOTIATION") << "\n";
    return (out_ok && cap_ok) ? 0 : 1;
}
