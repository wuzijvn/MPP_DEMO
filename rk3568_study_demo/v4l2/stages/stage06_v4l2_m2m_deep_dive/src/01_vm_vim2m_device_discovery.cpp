#include "00_stage06_m2m_common.hpp"

/*
 * Demo01 目标：
 * - 在 VM 上真实打开 `/dev/videoX`，确认它是不是 V4L2 M2M 节点。
 * - 对 vim2m 枚举 OUTPUT/CAPTURE 支持格式，给后续真实 queue demo 选择 fourcc。
 *
 * 和 Stage03 的关系：
 * - Stage03 讲 open/querycap/enum_fmt 的基础动作。
 * - Stage06 在这里深化为“环境门禁”：如果节点不是 M2M 或不支持 STREAMING，后续 demo 不继续伪装成功。
 */
int main(int argc, char** argv) {
    const std::string device = stage06::get_arg(argc, argv, "--device", "/dev/video0");
    const bool verbose = stage06::get_arg_int(argc, argv, "--verbose", 1) != 0;
    const bool require_m2m = stage06::get_arg_int(argc, argv, "--require-m2m", 1) != 0;

    std::cout << "Stage06 Demo01: VM vim2m device discovery with real ioctl\n";
    stage06::print_line("device", device);
    stage06::print_line("boundary", "validates V4L2 M2M node capability; does not prove codec hardware decode");

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
    if (require_m2m && !stage06::is_m2m_capable(caps)) {
        close(fd);
        std::cout << "verdict=FAIL_NOT_M2M_NODE\n";
        return 1;
    }
    if (!stage06::is_streaming_capable(caps)) {
        close(fd);
        std::cout << "verdict=FAIL_NOT_STREAMING_NODE\n";
        return 1;
    }

    std::vector<uint32_t> out_formats;
    std::vector<uint32_t> cap_formats;

    std::cout << "\nOUTPUT formats from real VIDIOC_ENUM_FMT:\n";
    const bool out_ok = stage06::enum_formats(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT,
                                              &out_formats, verbose);

    std::cout << "\nCAPTURE formats from real VIDIOC_ENUM_FMT:\n";
    const bool cap_ok = stage06::enum_formats(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                              &cap_formats, verbose);

    close(fd);

    stage06::print_line("output_format_count", std::to_string(out_formats.size()));
    stage06::print_line("capture_format_count", std::to_string(cap_formats.size()));
    if (!out_ok || !cap_ok || out_formats.empty() || cap_formats.empty()) {
        std::cout << "verdict=FAIL_ENUM_FORMATS\n";
        return 1;
    }

    std::cout << "verdict=PASS_VM_M2M_DEVICE_DISCOVERY\n";
    return 0;
}
