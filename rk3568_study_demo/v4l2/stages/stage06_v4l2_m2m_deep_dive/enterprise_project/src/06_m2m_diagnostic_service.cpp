#include "06_m2m_diagnostic_service.hpp"

#include <sys/wait.h>

#include <algorithm>

namespace stage06_enterprise {

namespace {

std::string shell_quote(const std::string& text) {
    std::string out = "'";
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\'') {
            out += "'\\''";
        } else {
            out += text[i];
        }
    }
    out += "'";
    return out;
}

int shell_status(int rc) {
    if (rc < 0) {
        return 127;
    }
    if (WIFEXITED(rc)) {
        return WEXITSTATUS(rc);
    }
    return 128;
}

int run_command_to_file(const std::string& command, const std::string& output_path) {
    const std::string full = command + " > " + shell_quote(output_path) + " 2>&1";
    return shell_status(system(full.c_str()));
}

bool command_available(const std::string& command) {
    const std::string test = "command -v " + command + " >/dev/null 2>&1";
    return shell_status(system(test.c_str())) == 0;
}

void log_errno(Logger* logger, const std::string& tag, const std::string& prefix) {
    if (logger != NULL) {
        logger->error(tag, prefix + ": " + strerror(errno));
    }
}

}  // namespace

M2mDiagnosticService::M2mDiagnosticService(const CliConfig& config)
    : config_(config),
      fd_(-1),
      output_type_(V4L2_BUF_TYPE_VIDEO_OUTPUT),
      capture_type_(V4L2_BUF_TYPE_VIDEO_CAPTURE),
      output_granted_(0),
      capture_granted_(0),
      output_in_driver_(0),
      capture_in_driver_(0),
      output_streaming_(false),
      capture_streaming_(false) {}

int M2mDiagnosticService::run() {
    if (!prepare_output_dir()) {
        return 2;
    }
    if (!logger_.open(config_.output_dir + "/enterprise_pipeline.log")) {
        return 2;
    }

    metrics_.mode = config_.mode;
    logger_.info("config", config_summary(config_));

    const int mode_rc = (config_.mode == "rk-rkmpp") ? run_rk_rkmpp() : run_vm_vim2m();
    finalize_and_write_outputs();

    logger_.close();
    if (metrics_.gate_pass && mode_rc == 0) {
        return 0;
    }
    return 1;
}

bool M2mDiagnosticService::prepare_output_dir() {
    return ensure_dir(config_.output_dir);
}

int M2mDiagnosticService::run_vm_vim2m() {
    logger_.info("mode", "VM vim2m: real V4L2 M2M ioctl path");
    output_type_ = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    capture_type_ = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (!open_and_query_device()) {
        cleanup_vm();
        return 1;
    }

    if (config_.inject == "unsupported_format") {
        sm_.transition(PipelineState::FORMAT_NEGOTIATION,
                       "inject unsupported compressed codec fourcc into vim2m S_FMT");
        metrics_.real_ioctl_path = true;
        struct v4l2_format fmt;
        const bool ioctl_ok = stage06::try_or_set_format(fd_, output_type_,
                                                         config_.output_fourcc,
                                                         config_.width,
                                                         config_.height,
                                                         true, &fmt, config_.verbose);
        const uint32_t actual_fourcc = ioctl_ok ? fmt.fmt.pix.pixelformat : 0;
        metrics_.unsupported_format_rejected =
            !ioctl_ok || actual_fourcc != config_.output_fourcc;
        logger_.info("fault",
                     ioctl_ok
                         ? "requested=" + fourcc_to_string(config_.output_fourcc) +
                               ", actual=" + fourcc_to_string(actual_fourcc)
                         : "VIDIOC_S_FMT rejected requested unsupported format");
        sm_.transition(PipelineState::STOPPED, "unsupported-format probe finished");
        cleanup_vm();
        return metrics_.unsupported_format_rejected ? 0 : 1;
    }

    bool ok = true;
    ok = ok && negotiate_formats();
    ok = ok && setup_buffers();
    ok = ok && queue_initial_buffers();
    ok = ok && start_streaming();
    ok = ok && run_vm_queue_loop();

    drain_and_stop_vm();
    return ok ? 0 : 1;
}

bool M2mDiagnosticService::open_and_query_device() {
    sm_.transition(PipelineState::OPEN_DEVICE, "open VM vim2m video node");
    if (!stage06::open_video_node(config_.device, &fd_)) {
        metrics_.simulated_device = false;
        sm_.transition(PipelineState::FAILED, "open failed; VM track does not simulate device");
        return false;
    }
    metrics_.device_opened = true;
    logger_.info("device", "opened " + config_.device);

    sm_.transition(PipelineState::QUERYCAP, "VIDIOC_QUERYCAP and capability gate");
    struct v4l2_capability cap;
    if (!stage06::query_capability(fd_, &cap)) {
        sm_.transition(PipelineState::FAILED, "VIDIOC_QUERYCAP failed");
        return false;
    }
    metrics_.querycap_ok = true;
    metrics_.real_ioctl_path = true;
    const uint32_t caps = stage06::active_caps(cap);
    metrics_.m2m_capable = stage06::is_m2m_capable(caps);
    metrics_.streaming_capable = stage06::is_streaming_capable(caps);
    logger_.info("querycap",
                 std::string("driver=") + reinterpret_cast<const char*>(cap.driver) +
                     ", card=" + reinterpret_cast<const char*>(cap.card) +
                     ", active_caps=" + stage06::hex_u32(caps) +
                     ", m2m_capable=" + yes_no(metrics_.m2m_capable) +
                     ", streaming_capable=" + yes_no(metrics_.streaming_capable));

    if (!metrics_.m2m_capable || !metrics_.streaming_capable) {
        sm_.transition(PipelineState::FAILED, "VM track requires a real streaming V4L2 M2M node");
        logger_.error("querycap", "not a streaming M2M node; refusing simulated fallback");
        return false;
    }
    return true;
}

bool M2mDiagnosticService::negotiate_formats() {
    sm_.transition(PipelineState::FORMAT_NEGOTIATION,
                   "real VIDIOC_S_FMT on OUTPUT and CAPTURE queues");
    logger_.info("format", "OUTPUT=" + fourcc_to_string(config_.output_fourcc) +
                             ", CAPTURE=" + fourcc_to_string(config_.capture_fourcc) +
                             ", size=" + std::to_string(config_.width) +
                             "x" + std::to_string(config_.height));

    if (!stage06::try_or_set_format(fd_, output_type_, config_.output_fourcc,
                                    config_.width, config_.height, true, NULL,
                                    config_.verbose)) {
        sm_.transition(PipelineState::FAILED, "OUTPUT S_FMT failed");
        return false;
    }
    if (!stage06::try_or_set_format(fd_, capture_type_, config_.capture_fourcc,
                                    config_.width, config_.height, true, NULL,
                                    config_.verbose)) {
        sm_.transition(PipelineState::FAILED, "CAPTURE S_FMT failed");
        return false;
    }
    return true;
}

bool M2mDiagnosticService::setup_buffers() {
    sm_.transition(PipelineState::BUFFER_SETUP,
                   "real REQBUFS, QUERYBUF and mmap on both queues");
    if (!stage06::request_buffers(fd_, output_type_,
                                  static_cast<uint32_t>(config_.output_depth),
                                  &output_granted_, config_.verbose)) {
        sm_.transition(PipelineState::FAILED, "OUTPUT REQBUFS failed");
        return false;
    }
    if (!stage06::request_buffers(fd_, capture_type_,
                                  static_cast<uint32_t>(config_.capture_depth),
                                  &capture_granted_, config_.verbose)) {
        sm_.transition(PipelineState::FAILED, "CAPTURE REQBUFS failed");
        return false;
    }

    output_buffers_.resize(output_granted_);
    capture_buffers_.resize(capture_granted_);
    for (uint32_t i = 0; i < output_granted_; ++i) {
        if (!stage06::querybuf_map(fd_, output_type_, i, &output_buffers_[i],
                                   config_.verbose)) {
            sm_.transition(PipelineState::FAILED, "OUTPUT QUERYBUF/MMAP failed");
            return false;
        }
        metrics_.mapped_output++;
    }
    for (uint32_t i = 0; i < capture_granted_; ++i) {
        if (!stage06::querybuf_map(fd_, capture_type_, i, &capture_buffers_[i],
                                   config_.verbose)) {
            sm_.transition(PipelineState::FAILED, "CAPTURE QUERYBUF/MMAP failed");
            return false;
        }
        metrics_.mapped_capture++;
    }
    return true;
}

bool M2mDiagnosticService::queue_initial_buffers() {
    for (uint32_t i = 0; i < capture_granted_; ++i) {
        if (!stage06::qbuf(fd_, capture_type_, i, 0, config_.verbose)) {
            sm_.transition(PipelineState::FAILED, "initial CAPTURE QBUF failed");
            return false;
        }
        metrics_.qbuf_capture++;
        capture_in_driver_++;
    }

    for (uint32_t i = 0; i < output_granted_; ++i) {
        stage06::fill_pattern(&output_buffers_[i], i);
        uint32_t bytesused = static_cast<uint32_t>(output_buffers_[i].length);
        if (config_.bytesused_zero_at > 0 && i == 0) {
            bytesused = 0;
            metrics_.bytesused_zero_count++;
            logger_.error("bytesused", "inject OUTPUT bytesused=0 through real VIDIOC_QBUF");
        }
        if (!stage06::qbuf(fd_, output_type_, i, bytesused, config_.verbose)) {
            sm_.transition(PipelineState::FAILED, "initial OUTPUT QBUF failed");
            return false;
        }
        metrics_.qbuf_output++;
        output_in_driver_++;
    }

    metrics_.max_output_depth = std::max(metrics_.max_output_depth, output_in_driver_);
    metrics_.max_capture_depth = std::max(metrics_.max_capture_depth, capture_in_driver_);
    return true;
}

bool M2mDiagnosticService::start_streaming() {
    sm_.transition(PipelineState::STREAMING,
                   "real VIDIOC_STREAMON on CAPTURE then OUTPUT");
    if (!stage06::stream_on(fd_, capture_type_, config_.verbose)) {
        sm_.transition(PipelineState::FAILED, "CAPTURE STREAMON failed");
        return false;
    }
    capture_streaming_ = true;
    metrics_.streamon_count++;

    if (!stage06::stream_on(fd_, output_type_, config_.verbose)) {
        sm_.transition(PipelineState::FAILED, "OUTPUT STREAMON failed");
        return false;
    }
    output_streaming_ = true;
    metrics_.streamon_count++;
    return true;
}

bool M2mDiagnosticService::run_vm_queue_loop() {
    sm_.transition(PipelineState::RUNNING,
                   "real poll, DQBUF and requeue loop on vim2m");
    for (int frame = 1; frame <= config_.frames; ++frame) {
        if (frame == config_.source_change_at) {
            if (!handle_source_change(frame)) {
                sm_.transition(PipelineState::FAILED, "source-change path failed");
                return false;
            }
        }
        if (frame == config_.timeout_at) {
            if (!handle_timeout_probe(frame)) {
                sm_.transition(PipelineState::FAILED, "timeout recovery path failed");
                return false;
            }
        }

        metrics_.poll_calls++;
        const int pr = stage06::poll_device(fd_, config_.timeout_ms, config_.verbose);
        if (pr == 0) {
            metrics_.timeout_count++;
            logger_.warn("poll", "timeout at frame=" + std::to_string(frame));
            if (!config_.recover) {
                return false;
            }
            continue;
        }
        if (pr < 0) {
            log_errno(&logger_, "poll", "poll failed");
            return false;
        }

        struct v4l2_buffer cap_dq;
        if (stage06::dqbuf(fd_, capture_type_, &cap_dq, config_.verbose)) {
            metrics_.dqbuf_capture++;
            metrics_.decoded_frames++;
            if (capture_in_driver_ > 0) {
                capture_in_driver_--;
            }
            if (!stage06::qbuf(fd_, capture_type_, cap_dq.index, 0, config_.verbose)) {
                sm_.transition(PipelineState::FAILED, "CAPTURE requeue failed");
                return false;
            }
            metrics_.qbuf_capture++;
            capture_in_driver_++;
        } else if (errno != EAGAIN) {
            log_errno(&logger_, "dqbuf", "CAPTURE DQBUF failed");
            return false;
        }

        struct v4l2_buffer out_dq;
        if (stage06::dqbuf(fd_, output_type_, &out_dq, config_.verbose)) {
            metrics_.dqbuf_output++;
            if (output_in_driver_ > 0) {
                output_in_driver_--;
            }
            stage06::fill_pattern(&output_buffers_[out_dq.index],
                                  static_cast<uint32_t>(frame + output_granted_));
            if (!stage06::qbuf(fd_, output_type_, out_dq.index,
                               static_cast<uint32_t>(output_buffers_[out_dq.index].length),
                               config_.verbose)) {
                sm_.transition(PipelineState::FAILED, "OUTPUT requeue failed");
                return false;
            }
            metrics_.qbuf_output++;
            output_in_driver_++;
        } else if (errno != EAGAIN) {
            log_errno(&logger_, "dqbuf", "OUTPUT DQBUF failed");
            return false;
        }

        metrics_.max_output_depth = std::max(metrics_.max_output_depth, output_in_driver_);
        metrics_.max_capture_depth = std::max(metrics_.max_capture_depth, capture_in_driver_);
    }
    return true;
}

bool M2mDiagnosticService::handle_source_change(int frame_index) {
    metrics_.source_change_count++;
    sm_.transition(PipelineState::SOURCE_CHANGE,
                   "exercise CAPTURE reconfiguration sequence at frame " +
                       std::to_string(frame_index));
    logger_.warn("source_change",
                 "vim2m has no codec SOURCE_CHANGE event; this path still runs real CAPTURE STREAMOFF/REQBUFS/S_FMT/STREAMON");

    if (!config_.recover) {
        metrics_.timeout_count++;
        logger_.error("source_change", "recovery disabled; stop after source-change fault");
        return false;
    }

    if (capture_streaming_) {
        if (!stage06::stream_off(fd_, capture_type_, config_.verbose)) {
            return false;
        }
        capture_streaming_ = false;
        metrics_.streamoff_count++;
    }
    capture_in_driver_ = 0;
    stage06::unmap_all(&capture_buffers_, config_.verbose);
    stage06::release_buffers_best_effort(fd_, capture_type_, config_.verbose);

    if (!stage06::try_or_set_format(fd_, capture_type_, config_.capture_fourcc,
                                    config_.width, config_.height, true, NULL,
                                    config_.verbose)) {
        return false;
    }
    if (!stage06::request_buffers(fd_, capture_type_,
                                  static_cast<uint32_t>(config_.capture_depth),
                                  &capture_granted_, config_.verbose)) {
        return false;
    }
    capture_buffers_.resize(capture_granted_);
    for (uint32_t i = 0; i < capture_granted_; ++i) {
        if (!stage06::querybuf_map(fd_, capture_type_, i, &capture_buffers_[i],
                                   config_.verbose)) {
            return false;
        }
        metrics_.mapped_capture++;
        if (!stage06::qbuf(fd_, capture_type_, i, 0, config_.verbose)) {
            return false;
        }
        metrics_.qbuf_capture++;
        capture_in_driver_++;
    }
    if (!stage06::stream_on(fd_, capture_type_, config_.verbose)) {
        return false;
    }
    capture_streaming_ = true;
    metrics_.streamon_count++;
    metrics_.recovery_count++;
    metrics_.max_capture_depth = std::max(metrics_.max_capture_depth, capture_in_driver_);

    sm_.transition(PipelineState::RECOVERY,
                   "CAPTURE queue reconfigured with real ioctl sequence");
    sm_.transition(PipelineState::RUNNING, "return to vim2m queue loop");
    return true;
}

bool M2mDiagnosticService::handle_timeout_probe(int frame_index) {
    metrics_.poll_calls++;
    const int pr = stage06::poll_device(fd_, 0, config_.verbose);
    if (pr == 0) {
        metrics_.timeout_count++;
        logger_.warn("timeout", "zero-timeout poll observed no ready buffer at frame=" +
                                  std::to_string(frame_index));
    } else {
        logger_.warn("timeout", "timeout injection called real poll(0), but device already had readiness");
    }

    if (!config_.recover) {
        return false;
    }

    sm_.transition(PipelineState::RECOVERY,
                   "real STREAMOFF, requeue and STREAMON after timeout probe");
    if (output_streaming_) {
        if (!stage06::stream_off(fd_, output_type_, config_.verbose)) {
            return false;
        }
        output_streaming_ = false;
        metrics_.streamoff_count++;
    }
    if (capture_streaming_) {
        if (!stage06::stream_off(fd_, capture_type_, config_.verbose)) {
            return false;
        }
        capture_streaming_ = false;
        metrics_.streamoff_count++;
    }

    output_in_driver_ = 0;
    capture_in_driver_ = 0;
    for (uint32_t i = 0; i < capture_granted_; ++i) {
        if (!stage06::qbuf(fd_, capture_type_, i, 0, config_.verbose)) {
            return false;
        }
        metrics_.qbuf_capture++;
        capture_in_driver_++;
    }
    for (uint32_t i = 0; i < output_granted_; ++i) {
        stage06::fill_pattern(&output_buffers_[i],
                              static_cast<uint32_t>(frame_index + i + 1000));
        if (!stage06::qbuf(fd_, output_type_, i,
                           static_cast<uint32_t>(output_buffers_[i].length),
                           config_.verbose)) {
            return false;
        }
        metrics_.qbuf_output++;
        output_in_driver_++;
    }
    if (!stage06::stream_on(fd_, capture_type_, config_.verbose)) {
        return false;
    }
    capture_streaming_ = true;
    metrics_.streamon_count++;
    if (!stage06::stream_on(fd_, output_type_, config_.verbose)) {
        return false;
    }
    output_streaming_ = true;
    metrics_.streamon_count++;
    metrics_.recovery_count++;
    sm_.transition(PipelineState::RUNNING, "return after timeout recovery");
    return true;
}

void M2mDiagnosticService::drain_and_stop_vm() {
    sm_.transition(PipelineState::DRAINING,
                   "vim2m has no codec EOS; record drain boundary then stop queues");
    metrics_.eos_count++;
    cleanup_vm();
    sm_.transition(PipelineState::STOPPED,
                   "STREAMOFF, munmap, REQBUFS count=0 and close finished");
}

void M2mDiagnosticService::cleanup_vm() {
    if (fd_ >= 0) {
        if (output_streaming_) {
            if (stage06::stream_off(fd_, output_type_, config_.verbose)) {
                metrics_.streamoff_count++;
            }
            output_streaming_ = false;
        }
        if (capture_streaming_) {
            if (stage06::stream_off(fd_, capture_type_, config_.verbose)) {
                metrics_.streamoff_count++;
            }
            capture_streaming_ = false;
        }
    }
    stage06::unmap_all(&capture_buffers_, config_.verbose);
    stage06::unmap_all(&output_buffers_, config_.verbose);
    if (fd_ >= 0) {
        stage06::release_buffers_best_effort(fd_, capture_type_, config_.verbose);
        stage06::release_buffers_best_effort(fd_, output_type_, config_.verbose);
        close(fd_);
        fd_ = -1;
    }
    output_granted_ = 0;
    capture_granted_ = 0;
    output_in_driver_ = 0;
    capture_in_driver_ = 0;
}

int M2mDiagnosticService::run_rk_rkmpp() {
    logger_.info("mode", "RK board: RKMPP/FFmpeg hardware evidence path");
    sm_.transition(PipelineState::OPEN_DEVICE,
                   "collect RK board device and media evidence without V4L2 M2M ioctl");

    const bool ffmpeg_ok = command_available("ffmpeg");
    metrics_.ffmpeg_available = ffmpeg_ok;
    logger_.info("rk", "ffmpeg_available=" + yes_no(ffmpeg_ok));

    run_command_to_file("uname -a", config_.output_dir + "/uname.txt");
    run_command_to_file("ls -l /dev/video* /dev/dri/* 2>/dev/null || true",
                        config_.output_dir + "/device_nodes.txt");
    run_command_to_file("v4l2-ctl --list-devices 2>/dev/null || true",
                        config_.output_dir + "/v4l2_list_devices.txt");
    run_command_to_file("ffmpeg -hide_banner -decoders 2>/dev/null | grep -Ei 'rkmpp|v4l2m2m|h264|hevc' || true",
                        config_.output_dir + "/ffmpeg_decoders.txt");
    run_command_to_file("dmesg | grep -Ei 'rkvdec|mpp|vpu|v4l2|codec|firmware|iommu|dma|timeout|reset' | tail -n 160 || true",
                        config_.output_dir + "/dmesg_media_hints.txt");
    metrics_.rk_evidence_collected = true;

    if (ffmpeg_ok) {
        const std::string grep_cmd =
            "ffmpeg -hide_banner -decoders 2>/dev/null | grep -q " +
            shell_quote(config_.decoder);
        metrics_.rk_decoder_seen = shell_status(system(grep_cmd.c_str())) == 0;
    }
    logger_.info("rk", "decoder=" + config_.decoder +
                       ", decoder_seen=" + yes_no(metrics_.rk_decoder_seen));

    if (!config_.input.empty() && ffmpeg_ok && metrics_.rk_decoder_seen) {
        sm_.transition(PipelineState::RUNNING,
                       "run FFmpeg RKMPP decode command for hardware evidence");
        const std::string cmd = "ffmpeg -hide_banner -v verbose -c:v " +
                                shell_quote(config_.decoder) + " -i " +
                                shell_quote(config_.input) +
                                " -frames:v 8 -f null -";
        const int rc = run_command_to_file(cmd, config_.output_dir + "/ffmpeg_rkmpp_decode.log");
        metrics_.rk_decode_command_ok = (rc == 0);
        logger_.info("rk", "decode_command_ok=" + yes_no(metrics_.rk_decode_command_ok));
    } else {
        stage06::write_text_file(
            config_.output_dir + "/ffmpeg_rkmpp_decode.log",
            "decode command not run: provide --input, ensure ffmpeg exists, and ensure decoder is listed\n");
    }

    std::ostringstream report;
    report << "# RKMPP Hardware Path Report\n\n";
    report << "- mode: rk-rkmpp\n";
    report << "- ffmpeg_available: " << yes_no(metrics_.ffmpeg_available) << "\n";
    report << "- decoder: " << config_.decoder << "\n";
    report << "- decoder_seen: " << yes_no(metrics_.rk_decoder_seen) << "\n";
    report << "- input: " << (config_.input.empty() ? "(not provided)" : config_.input) << "\n";
    report << "- decode_command_ok: " << yes_no(metrics_.rk_decode_command_ok) << "\n\n";
    report << "## Boundary\n";
    report << "This RK path does not call VM vim2m and does not force V4L2 M2M codec ioctls onto ISP/camera nodes.\n";
    report << "Hardware proof should come from RKMPP/FFmpeg logs, decoder listing, dmesg and board device evidence.\n\n";
    report << "## Evidence Files\n";
    report << "- uname.txt\n";
    report << "- device_nodes.txt\n";
    report << "- v4l2_list_devices.txt\n";
    report << "- ffmpeg_decoders.txt\n";
    report << "- dmesg_media_hints.txt\n";
    report << "- ffmpeg_rkmpp_decode.log\n";
    stage06::write_text_file(config_.output_dir + "/rk_rkmpp_report.md", report.str());

    sm_.transition(PipelineState::STOPPED, "RK hardware evidence collection finished");
    return 0;
}

void M2mDiagnosticService::finalize_and_write_outputs() {
    gate_.evaluate(config_, &metrics_);
    logger_.info("gate", metrics_summary_text(metrics_));
    write_metrics_json(config_.output_dir + "/enterprise_metrics.json", config_, metrics_, sm_);
    std::cout << "enterprise_log=" << config_.output_dir << "/enterprise_pipeline.log\n";
    std::cout << "enterprise_metrics=" << config_.output_dir << "/enterprise_metrics.json\n";
    std::cout << "enterprise_verdict=" << metrics_.verdict << "\n";
}

}  // namespace stage06_enterprise
