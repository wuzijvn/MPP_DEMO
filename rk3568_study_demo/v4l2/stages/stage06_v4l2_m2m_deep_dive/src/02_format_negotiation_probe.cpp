#include "00_stage06_m2m_common.hpp"

namespace {

void fill_format(struct v4l2_format* fmt, uint32_t type, uint32_t fourcc,
                 uint32_t width, uint32_t height) {
    memset(fmt, 0, sizeof(*fmt));
    fmt->type = type;
    if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
        /*
         * 多平面格式：codec M2M 设备常见。
         * 注意：压缩 OUTPUT 格式很多驱动仍用 num_planes=1，表示一个 bitstream payload 平面。
         */
        fmt->fmt.pix_mp.width = width;
        fmt->fmt.pix_mp.height = height;
        fmt->fmt.pix_mp.pixelformat = fourcc;
        fmt->fmt.pix_mp.num_planes = 1;
    } else {
        fmt->fmt.pix.width = width;
        fmt->fmt.pix.height = height;
        fmt->fmt.pix.pixelformat = fourcc;
    }
}

void print_format_result(const std::string& title, const struct v4l2_format& fmt) {
    std::cout << title << "\n";
    stage06::print_line("type", stage06::buffer_type_name(fmt.type));
    if (V4L2_TYPE_IS_MULTIPLANAR(fmt.type)) {
        stage06::print_line("width", std::to_string(fmt.fmt.pix_mp.width));
        stage06::print_line("height", std::to_string(fmt.fmt.pix_mp.height));
        stage06::print_line("fourcc", stage06::fourcc_to_string(fmt.fmt.pix_mp.pixelformat));
        stage06::print_line("num_planes", std::to_string(fmt.fmt.pix_mp.num_planes));
        for (uint32_t i = 0; i < fmt.fmt.pix_mp.num_planes && i < VIDEO_MAX_PLANES; ++i) {
            std::ostringstream oss;
            oss << "sizeimage=" << fmt.fmt.pix_mp.plane_fmt[i].sizeimage
                << ", bytesperline=" << fmt.fmt.pix_mp.plane_fmt[i].bytesperline;
            stage06::print_line("plane" + std::to_string(i), oss.str());
        }
    } else {
        stage06::print_line("width", std::to_string(fmt.fmt.pix.width));
        stage06::print_line("height", std::to_string(fmt.fmt.pix.height));
        stage06::print_line("fourcc", stage06::fourcc_to_string(fmt.fmt.pix.pixelformat));
        stage06::print_line("sizeimage", std::to_string(fmt.fmt.pix.sizeimage));
        stage06::print_line("bytesperline", std::to_string(fmt.fmt.pix.bytesperline));
    }
}

bool negotiate_one(int fd, uint32_t type, uint32_t fourcc, uint32_t width,
                   uint32_t height, bool apply) {
    struct v4l2_format fmt;
    fill_format(&fmt, type, fourcc, width, height);

    /*
     * TRY_FMT：不改变驱动 streaming 状态，是学习和 bring-up 阶段更安全的探测方式。
     * S_FMT：真正修改队列格式，生产工具一般会在 STREAMOFF 且 buffer 未申请时调用。
     */
    const unsigned long request = apply ? VIDIOC_S_FMT : VIDIOC_TRY_FMT;
    if (stage06::xioctl(fd, request, &fmt) < 0) {
        std::cerr << (apply ? "VIDIOC_S_FMT" : "VIDIOC_TRY_FMT") << " "
                  << stage06::buffer_type_name(type) << " failed: "
                  << strerror(errno) << "\n";
        return false;
    }

    print_format_result(apply ? "S_FMT result:" : "TRY_FMT result:", fmt);
    return true;
}

void print_simulated_result(uint32_t out_fourcc, uint32_t cap_fourcc,
                            uint32_t width, uint32_t height, bool mplane) {
    std::cout << "SIMULATION_MODE: device unavailable or --simulate=1\n";
    stage06::print_line("meaning", "不声称真实驱动接受该格式，只演示协商报告结构");
    stage06::print_line("OUTPUT type", stage06::buffer_type_name(stage06::output_type(mplane)));
    stage06::print_line("OUTPUT fourcc", stage06::fourcc_to_string(out_fourcc));
    stage06::print_line("CAPTURE type", stage06::buffer_type_name(stage06::capture_type(mplane)));
    stage06::print_line("CAPTURE fourcc", stage06::fourcc_to_string(cap_fourcc));
    stage06::print_line("requested size", std::to_string(width) + "x" + std::to_string(height));
    stage06::print_line("driver may adjust", "coded size / stride / sizeimage / plane count");
}

}  // namespace

int main(int argc, char** argv) {
    const std::string device = stage06::get_arg(argc, argv, "--device", "/dev/video0");
    const bool mplane = stage06::get_arg_int(argc, argv, "--mplane", 1) != 0;
    const bool apply = stage06::get_arg_int(argc, argv, "--apply", 0) != 0;
    const bool simulate = stage06::get_arg_int(argc, argv, "--simulate", 0) != 0;
    const bool require_device = stage06::get_arg_int(argc, argv, "--require-device", 0) != 0;
    const uint32_t width = stage06::get_arg_u32(argc, argv, "--width", 1280);
    const uint32_t height = stage06::get_arg_u32(argc, argv, "--height", 720);

    uint32_t out_fourcc = V4L2_PIX_FMT_H264;
    uint32_t cap_fourcc = V4L2_PIX_FMT_NV12;
    const std::string output_text = stage06::get_arg(argc, argv, "--output", "H264");
    const std::string capture_text = stage06::get_arg(argc, argv, "--capture", "NV12");
    if (!stage06::parse_fourcc(output_text, &out_fourcc)) {
        std::cerr << "--output must be 4 chars, example H264\n";
        return 2;
    }
    if (!stage06::parse_fourcc(capture_text, &cap_fourcc)) {
        std::cerr << "--capture must be 4 chars, example NV12\n";
        return 2;
    }

    std::cout << "Stage06 Demo02: V4L2 M2M format negotiation probe\n";
    stage06::print_line("device", device);
    stage06::print_line("mode", apply ? "S_FMT(apply)" : "TRY_FMT(safe probe)");
    stage06::print_line("mplane", stage06::yes_no(mplane));
    std::cout << "\n";

    if (simulate) {
        print_simulated_result(out_fourcc, cap_fourcc, width, height, mplane);
        std::cout << "verdict=SIMULATED_FORMAT_NEGOTIATION\n";
        return 0;
    }

    int fd = -1;
    if (!stage06::open_video_node(device, &fd)) {
        if (require_device) {
            std::cout << "verdict=DEVICE_REQUIRED_BUT_UNAVAILABLE\n";
            return 1;
        }
        print_simulated_result(out_fourcc, cap_fourcc, width, height, mplane);
        std::cout << "verdict=SIMULATED_FORMAT_NEGOTIATION_DEVICE_MISSING\n";
        return 0;
    }

    struct v4l2_capability cap;
    if (stage06::query_capability(fd, &cap)) {
        stage06::print_line("driver", reinterpret_cast<const char*>(cap.driver));
        stage06::print_line("card", reinterpret_cast<const char*>(cap.card));
        stage06::print_line("bus_info", reinterpret_cast<const char*>(cap.bus_info));
        std::cout << "\n";
    }

    const bool ok_out = negotiate_one(fd, stage06::output_type(mplane), out_fourcc,
                                      width, height, apply);
    std::cout << "\n";
    const bool ok_cap = negotiate_one(fd, stage06::capture_type(mplane), cap_fourcc,
                                      width, height, apply);
    close(fd);

    std::cout << "\nverdict=" << ((ok_out && ok_cap) ? "FORMAT_NEGOTIATION_OK" : "FORMAT_NEGOTIATION_FAILED") << "\n";
    return (ok_out && ok_cap) ? 0 : 1;
}
