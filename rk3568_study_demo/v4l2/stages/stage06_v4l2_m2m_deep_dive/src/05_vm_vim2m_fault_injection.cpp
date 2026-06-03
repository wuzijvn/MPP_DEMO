#include "00_stage06_m2m_common.hpp"

/*
 * Demo05 目标：
 * - 在真实 vim2m 节点上做可控故障注入，不再只打印模拟故障。
 * - 默认注入 OUTPUT bytesused=0，观察真实 QBUF errno/verdict。
 *
 * 可选注入：
 * - --inject=bytesused_zero：真实调用 QBUF，预期常见结果是 EINVAL 或队列无法推进。
 * - --inject=unsupported_format：真实调用 S_FMT，预期格式协商失败。
 * - --inject=poll_without_streamon：真实 poll 未启动队列的 fd，观察 timeout 或事件。
 */
int main(int argc, char** argv) {
    const std::string device = stage06::get_arg(argc, argv, "--device", "/dev/video0");
    const std::string inject = stage06::get_arg(argc, argv, "--inject", "bytesused_zero");
    const bool verbose = stage06::get_arg_int(argc, argv, "--verbose", 1) != 0;

    std::cout << "Stage06 Demo05: VM vim2m real fault injection\n";
    stage06::print_line("device", device);
    stage06::print_line("inject", inject);

    int fd = -1;
    if (!stage06::open_video_node(device, &fd)) {
        std::cout << "verdict=FAIL_OPEN_DEVICE\n";
        return 1;
    }

    if (inject == "unsupported_format") {
        struct v4l2_format fmt;
        const bool ok = stage06::try_or_set_format(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT,
                                                   V4L2_PIX_FMT_H264, 1280, 720,
                                                   true, &fmt, verbose);
        const uint32_t actual = ok ? fmt.fmt.pix.pixelformat : 0;
        const bool rejected_or_adjusted = !ok || actual != V4L2_PIX_FMT_H264;
        if (ok) {
            stage06::print_line("requested_fourcc", "H264");
            stage06::print_line("actual_fourcc", stage06::fourcc_to_string(actual));
        }
        close(fd);
        std::cout << "verdict="
                  << (rejected_or_adjusted ? "PASS_FAULT_UNSUPPORTED_FORMAT_CAUGHT"
                                           : "UNEXPECTED_PASS_UNSUPPORTED_FORMAT")
                  << "\n";
        return rejected_or_adjusted ? 0 : 1;
    }

    if (inject == "poll_without_streamon") {
        const int ret = stage06::poll_device(fd, 50, verbose);
        close(fd);
        std::cout << "verdict=" << (ret == 0 ? "PASS_FAULT_POLL_TIMEOUT_OBSERVED" : "PASS_FAULT_POLL_EVENT_OR_ERROR_OBSERVED") << "\n";
        return 0;
    }

    if (inject != "bytesused_zero") {
        std::cerr << "unknown --inject value\n";
        close(fd);
        return 2;
    }

    bool ok = true;
    ok = ok && stage06::try_or_set_format(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT,
                                          v4l2_fourcc('R', 'G', 'B', 'P'), 640, 480,
                                          true, NULL, verbose);
    ok = ok && stage06::try_or_set_format(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                          v4l2_fourcc('R', 'G', 'B', 'P'), 640, 480,
                                          true, NULL, verbose);
    uint32_t output_granted = 0;
    ok = ok && stage06::request_buffers(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, 1,
                                        &output_granted, verbose);
    std::vector<stage06::MappedBuffer> output_buffers(output_granted);
    if (ok && output_granted > 0) {
        ok = stage06::querybuf_map(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0,
                                   &output_buffers[0], verbose);
    }

    bool qbuf_zero_ok = false;
    if (ok) {
        /*
         * 故障点：OUTPUT bytesused=0。
         * codec decoder 里这通常意味着空 packet；vim2m raw M2M 也可用于观察驱动是否拒绝。
         */
        qbuf_zero_ok = stage06::qbuf(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, 0, 0, verbose);
        if (qbuf_zero_ok) {
            stage06::print_line("fault_result", "driver accepted bytesused=0; next diagnostic should check progress/poll");
        } else {
            stage06::print_line("fault_result", std::string("driver rejected bytesused=0 errno=") + strerror(errno));
        }
    }

    stage06::unmap_all(&output_buffers, verbose);
    stage06::release_buffers_best_effort(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, verbose);
    close(fd);
    std::cout << "verdict=" << (qbuf_zero_ok ? "PASS_FAULT_BYTESUSED_ZERO_REACHED_DRIVER" : "PASS_FAULT_BYTESUSED_ZERO_REJECTED_BY_DRIVER") << "\n";
    return ok ? 0 : 1;
}
