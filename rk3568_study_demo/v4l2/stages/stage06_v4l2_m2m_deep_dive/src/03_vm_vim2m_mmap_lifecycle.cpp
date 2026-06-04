#include "00_stage06_m2m_common.hpp"

/*
 * Demo03 目标：
 * - 在 vim2m 上真实执行 S_FMT -> REQBUFS -> QUERYBUF -> mmap -> munmap。
 * - 验证用户态确实拿到驱动/vb2 提供的 MMAP buffer。
 *
 * 深化点：
 * - 不再打印“假 mmap 指针”。
 * - 失败路径会对已映射资源做对称释放，训练真实 media 程序的 cleanup 思维。
 */
int main(int argc, char** argv) {
    const std::string device = stage06::get_arg(argc, argv, "--device", "/dev/video0");
    const std::string output_text = stage06::get_arg(argc, argv, "--output", "RGBP");
    const std::string capture_text = stage06::get_arg(argc, argv, "--capture", "RGBP");
    const uint32_t width = stage06::get_arg_u32(argc, argv, "--width", 640);
    const uint32_t height = stage06::get_arg_u32(argc, argv, "--height", 480);
    const uint32_t output_count = stage06::get_arg_u32(argc, argv, "--output-count", 3);
    const uint32_t capture_count = stage06::get_arg_u32(argc, argv, "--capture-count", 4);
    const bool verbose = stage06::get_arg_int(argc, argv, "--verbose", 1) != 0;

    uint32_t output_fourcc = 0;
    uint32_t capture_fourcc = 0;
    if (!stage06::parse_fourcc(output_text, &output_fourcc) ||
        !stage06::parse_fourcc(capture_text, &capture_fourcc)) {
        std::cerr << "--output/--capture must be 4 chars\n";
        return 2;
    }

    std::cout << "Stage06 Demo03: VM vim2m real MMAP buffer lifecycle\n";
    stage06::print_line("device", device);
    stage06::print_line("format", output_text + " -> " + capture_text);

    int fd = -1;
    std::vector<stage06::MappedBuffer> output_buffers;
    std::vector<stage06::MappedBuffer> capture_buffers;
    stage06::QueueCounters counters;

    if (!stage06::open_video_node(device, &fd)) {
        std::cout << "verdict=FAIL_OPEN_DEVICE\n";
        return 1;
    }

    bool ok = true;
    ok = ok && stage06::try_or_set_format(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT,
                                          output_fourcc, width, height, true, NULL, verbose);
    ok = ok && stage06::try_or_set_format(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                          capture_fourcc, width, height, true, NULL, verbose);

    uint32_t output_granted = 0;
    uint32_t capture_granted = 0;
    ok = ok && stage06::request_buffers(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT,
                                        output_count, &output_granted, verbose);
    ok = ok && stage06::request_buffers(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                        capture_count, &capture_granted, verbose);

    output_buffers.resize(output_granted);
    capture_buffers.resize(capture_granted);

    for (uint32_t i = 0; ok && i < output_granted; ++i) {
        ok = stage06::querybuf_map(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, i,
                                   &output_buffers[i], verbose);
        if (ok) {
            counters.mapped_output++;
        }
    }
    for (uint32_t i = 0; ok && i < capture_granted; ++i) {
        ok = stage06::querybuf_map(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, i,
                                   &capture_buffers[i], verbose);
        if (ok) {
            counters.mapped_capture++;
        }
    }

    stage06::print_counters(counters);
    stage06::unmap_all(&capture_buffers, verbose);
    stage06::unmap_all(&output_buffers, verbose);
    stage06::release_buffers_best_effort(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, verbose);
    stage06::release_buffers_best_effort(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, verbose);
    close(fd);

    std::cout << "verdict=" << (ok ? "PASS_VM_MMAP_LIFECYCLE" : "FAIL_VM_MMAP_LIFECYCLE") << "\n";
    return ok ? 0 : 1;
}
