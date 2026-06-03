#include "00_stage06_m2m_common.hpp"

namespace {

void cleanup(int fd, std::vector<stage06::MappedBuffer>* out,
             std::vector<stage06::MappedBuffer>* cap, bool verbose) {
    if (fd >= 0) {
        stage06::stream_off_best_effort(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, verbose);
        stage06::stream_off_best_effort(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, verbose);
    }
    stage06::unmap_all(cap, verbose);
    stage06::unmap_all(out, verbose);
    if (fd >= 0) {
        stage06::release_buffers_best_effort(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, verbose);
        stage06::release_buffers_best_effort(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, verbose);
        close(fd);
    }
}

}  // namespace

/*
 * Demo04 目标：
 * - 在 vim2m 上真实执行 QBUF -> STREAMON -> poll -> DQBUF -> requeue。
 * - 这是真实 VM V4L2 M2M 队列主路径，不再只用 counter 打印。
 *
 * 注意：
 * - vim2m 是 raw-to-raw M2M，不是 H.264/H.265 codec decoder。
 * - 该 demo 证明队列所有权和 poll/DQBUF 行为，不证明 RK VPU 硬解。
 */
int main(int argc, char** argv) {
    const std::string device = stage06::get_arg(argc, argv, "--device", "/dev/video0");
    const std::string output_text = stage06::get_arg(argc, argv, "--output", "RGBP");
    const std::string capture_text = stage06::get_arg(argc, argv, "--capture", "RGBP");
    const uint32_t width = stage06::get_arg_u32(argc, argv, "--width", 640);
    const uint32_t height = stage06::get_arg_u32(argc, argv, "--height", 480);
    const uint32_t output_count = stage06::get_arg_u32(argc, argv, "--output-count", 3);
    const uint32_t capture_count = stage06::get_arg_u32(argc, argv, "--capture-count", 4);
    const int loops = stage06::get_arg_int(argc, argv, "--loops", 8);
    const int timeout_ms = stage06::get_arg_int(argc, argv, "--timeout-ms", 1000);
    const bool verbose = stage06::get_arg_int(argc, argv, "--verbose", 1) != 0;

    uint32_t output_fourcc = 0;
    uint32_t capture_fourcc = 0;
    if (!stage06::parse_fourcc(output_text, &output_fourcc) ||
        !stage06::parse_fourcc(capture_text, &capture_fourcc)) {
        std::cerr << "--output/--capture must be 4 chars\n";
        return 2;
    }

    std::cout << "Stage06 Demo04: VM vim2m real QBUF/DQBUF/poll queue loop\n";
    stage06::print_line("device", device);
    stage06::print_line("format", output_text + " -> " + capture_text);
    stage06::print_line("loops", std::to_string(loops));

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
        if (ok) counters.mapped_output++;
    }
    for (uint32_t i = 0; ok && i < capture_granted; ++i) {
        ok = stage06::querybuf_map(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, i,
                                   &capture_buffers[i], verbose);
        if (ok) counters.mapped_capture++;
    }

    for (uint32_t i = 0; ok && i < capture_granted; ++i) {
        ok = stage06::qbuf(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, i, 0, verbose);
        if (ok) counters.qbuf_capture++;
    }
    for (uint32_t i = 0; ok && i < output_granted; ++i) {
        stage06::fill_pattern(&output_buffers[i], i);
        ok = stage06::qbuf(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, i,
                           static_cast<uint32_t>(output_buffers[i].length), verbose);
        if (ok) counters.qbuf_output++;
    }

    ok = ok && stage06::stream_on(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, verbose);
    if (ok) counters.streamon_count++;
    ok = ok && stage06::stream_on(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, verbose);
    if (ok) counters.streamon_count++;

    for (int loop = 0; ok && loop < loops; ++loop) {
        counters.poll_calls++;
        const int pr = stage06::poll_device(fd, timeout_ms, verbose);
        if (pr == 0) {
            counters.timeout_count++;
            std::cout << "poll timeout at loop=" << loop << "\n";
            break;
        }
        if (pr < 0) {
            std::cerr << "poll failed: " << strerror(errno) << "\n";
            ok = false;
            break;
        }

        struct v4l2_buffer cap_dq;
        if (stage06::dqbuf(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, &cap_dq, verbose)) {
            counters.dqbuf_capture++;
            if (!stage06::qbuf(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, cap_dq.index, 0, verbose)) {
                ok = false;
                break;
            }
            counters.qbuf_capture++;
        } else if (errno != EAGAIN) {
            std::cerr << "DQBUF CAPTURE failed: " << strerror(errno) << "\n";
            ok = false;
            break;
        }

        struct v4l2_buffer out_dq;
        if (stage06::dqbuf(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, &out_dq, verbose)) {
            counters.dqbuf_output++;
            stage06::fill_pattern(&output_buffers[out_dq.index], static_cast<uint32_t>(loop + output_granted));
            if (!stage06::qbuf(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, out_dq.index,
                               static_cast<uint32_t>(output_buffers[out_dq.index].length), verbose)) {
                ok = false;
                break;
            }
            counters.qbuf_output++;
        } else if (errno != EAGAIN) {
            std::cerr << "DQBUF OUTPUT failed: " << strerror(errno) << "\n";
            ok = false;
            break;
        }
    }

    stage06::print_counters(counters);
    cleanup(fd, &output_buffers, &capture_buffers, verbose);

    std::cout << "verdict="
              << ((ok && counters.dqbuf_capture > 0 && counters.dqbuf_output > 0)
                      ? "PASS_VM_REAL_QUEUE_LOOP"
                      : "FAIL_VM_REAL_QUEUE_LOOP")
              << "\n";
    return (ok && counters.dqbuf_capture > 0 && counters.dqbuf_output > 0) ? 0 : 1;
}
