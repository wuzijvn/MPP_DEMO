#include "06_m2m_diagnostic_service.hpp"

#include <algorithm>

namespace stage06_enterprise {

M2mDiagnosticService::M2mDiagnosticService(const CliConfig& config)
    : config_(config), fd_(-1), output_in_driver_(0), capture_in_driver_(0) {}

int M2mDiagnosticService::run() {
    if (!prepare_output_dir()) {
        return 2;
    }
    if (!logger_.open(config_.output_dir + "/enterprise_pipeline.log")) {
        return 2;
    }

    logger_.info("config", config_summary(config_));
    try_probe_device();
    negotiate_formats_simulated();
    setup_buffers_simulated();
    const bool loop_ok = run_queue_loop();
    drain_and_stop();
    finalize_and_write_outputs();

    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
    logger_.close();
    return (loop_ok && metrics_.gate_pass) ? 0 : 1;
}

bool M2mDiagnosticService::prepare_output_dir() {
    return ensure_dir(config_.output_dir);
}

void M2mDiagnosticService::try_probe_device() {
    sm_.transition(PipelineState::OPEN_DEVICE, "open video node if available");
    logger_.info("state", "INIT -> OPEN_DEVICE");

    fd_ = open(config_.device.c_str(), O_RDWR | O_NONBLOCK);
    if (fd_ < 0) {
        metrics_.simulated_device = true;
        logger_.warn("device", "open failed, continue in simulated queue mode: " + std::string(strerror(errno)));
        if (config_.require_device) {
            sm_.transition(PipelineState::FAILED, "device required but open failed");
        }
        return;
    }

    metrics_.device_opened = true;
    logger_.info("device", "opened " + config_.device);

    sm_.transition(PipelineState::QUERYCAP, "VIDIOC_QUERYCAP evidence collection");
    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));
    if (xioctl(fd_, VIDIOC_QUERYCAP, &cap) == 0) {
        metrics_.querycap_ok = true;
        const uint32_t caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;
        metrics_.m2m_capable = (caps & V4L2_CAP_VIDEO_M2M) || (caps & V4L2_CAP_VIDEO_M2M_MPLANE);
        logger_.info("querycap", std::string("driver=") + reinterpret_cast<const char*>(cap.driver) +
                     ", card=" + reinterpret_cast<const char*>(cap.card) +
                     ", m2m_capable=" + yes_no(metrics_.m2m_capable));
        if (!metrics_.m2m_capable) {
            metrics_.simulated_device = true;
            logger_.warn("querycap", "opened node is not a V4L2 M2M codec device; continue in simulated codec queue mode");
        }
    } else {
        metrics_.simulated_device = true;
        logger_.warn("querycap", std::string("VIDIOC_QUERYCAP failed: ") + strerror(errno));
    }
}

void M2mDiagnosticService::negotiate_formats_simulated() {
    sm_.transition(PipelineState::FORMAT_NEGOTIATION,
                   "simulate OUTPUT/CAPTURE S_FMT and record requested formats");
    std::ostringstream oss;
    oss << "OUTPUT=" << fourcc_to_string(config_.output_fourcc)
        << ", CAPTURE=" << fourcc_to_string(config_.capture_fourcc)
        << ", size=" << config_.width << "x" << config_.height
        << "; real driver may adjust coded size, stride and sizeimage";
    logger_.info("format", oss.str());
}

void M2mDiagnosticService::setup_buffers_simulated() {
    sm_.transition(PipelineState::BUFFER_SETUP,
                   "simulate REQBUFS/QUERYBUF/MMAP for both queues");
    logger_.info("buffer", "OUTPUT buffers=" + std::to_string(config_.output_depth) +
                 ", CAPTURE buffers=" + std::to_string(config_.capture_depth));
    sm_.transition(PipelineState::STREAMING, "simulate STREAMON both queues");
    logger_.info("stream", "STREAMON OUTPUT + CAPTURE");
}

bool M2mDiagnosticService::run_queue_loop() {
    sm_.transition(PipelineState::RUNNING,
                   "enter QBUF/poll/DQBUF loop with counters and fault injection");

    for (int frame = 1; frame <= config_.frames; ++frame) {
        if (frame == config_.source_change_at) {
            handle_source_change(frame);
            if (!config_.recover && config_.inject == "source_change_no_reconfigure") {
                metrics_.timeout_count++;
                logger_.error("source_change", "CAPTURE queue was not reconfigured; simulated DQBUF timeout");
                sm_.transition(PipelineState::FAILED, "source-change recovery missing");
                return false;
            }
        }

        if (!handle_bytesused_zero(frame)) {
            sm_.transition(PipelineState::FAILED, "OUTPUT bytesused invalid");
            return false;
        }

        /*
         * QBUF OUTPUT：压缩码流进入驱动。
         * 所有权方向：USER -> DRIVER。
         * 这里用 counter 模拟，真实工具会填 struct v4l2_buffer 并调用 VIDIOC_QBUF。
         */
        if (output_in_driver_ < config_.output_depth) {
            metrics_.qbuf_output++;
            output_in_driver_++;
        }

        /*
         * QBUF CAPTURE：空 raw frame buffer 进入驱动。
         * 所有权方向：USER -> DRIVER。
         * 如果 CAPTURE queue 太浅，硬件会被输出 buffer 背压拖住。
         */
        if (capture_in_driver_ < config_.capture_depth) {
            metrics_.qbuf_capture++;
            capture_in_driver_++;
        }

        metrics_.max_output_depth = std::max(metrics_.max_output_depth, output_in_driver_);
        metrics_.max_capture_depth = std::max(metrics_.max_capture_depth, capture_in_driver_);
        metrics_.poll_calls++;
        logger_.info("loop", "frame=" + std::to_string(frame) +
                     " qbuf_out_depth=" + std::to_string(output_in_driver_) +
                     " qbuf_cap_depth=" + std::to_string(capture_in_driver_));

        if (frame == config_.timeout_at) {
            if (!handle_timeout(frame)) {
                sm_.transition(PipelineState::FAILED, "timeout not recovered");
                return false;
            }
            continue;
        }

        /*
         * DQBUF：驱动把完成 buffer 交还用户态。
         * OUTPUT DQBUF 表示 bitstream payload 已被消费；
         * CAPTURE DQBUF 表示 decoded frame 完成，可能包含 sequence/timestamp/flags。
         */
        if (output_in_driver_ > 0) {
            metrics_.dqbuf_output++;
            output_in_driver_--;
        }
        if (capture_in_driver_ > 0) {
            metrics_.dqbuf_capture++;
            capture_in_driver_--;
            metrics_.decoded_frames++;
        }
    }
    return true;
}

void M2mDiagnosticService::handle_source_change(int frame_index) {
    metrics_.source_change_count++;
    sm_.transition(PipelineState::SOURCE_CHANGE,
                   "V4L2_EVENT_SOURCE_CHANGE at frame " + std::to_string(frame_index));
    logger_.warn("source_change", "frame=" + std::to_string(frame_index) +
                 " requires CAPTURE STREAMOFF/REQBUFS/S_FMT/STREAMON");

    if (config_.recover) {
        metrics_.recovery_count++;
        metrics_.streamoff_count++;
        output_in_driver_ = 0;
        capture_in_driver_ = 0;
        sm_.transition(PipelineState::RECOVERY,
                       "reconfigure CAPTURE queue after SOURCE_CHANGE");
        logger_.info("recovery", "CAPTURE queue reconfigured and buffers will be requeued");
        sm_.transition(PipelineState::RUNNING, "return to decode loop after source-change recovery");
    }
}

bool M2mDiagnosticService::handle_timeout(int frame_index) {
    metrics_.timeout_count++;
    logger_.warn("timeout", "poll/DQBUF timeout at frame=" + std::to_string(frame_index) +
                 "; likely layers: bitstream, queue, IRQ, firmware, runtime PM");

    if (!config_.recover) {
        logger_.error("timeout", "recovery disabled, stop pipeline");
        return false;
    }

    metrics_.recovery_count++;
    metrics_.streamoff_count++;
    sm_.transition(PipelineState::RECOVERY,
                   "STREAMOFF both queues and requeue after timeout");
    output_in_driver_ = 0;
    capture_in_driver_ = 0;
    logger_.info("recovery", "simulated STREAMOFF -> cleanup active buffers -> STREAMON");
    sm_.transition(PipelineState::RUNNING, "return to decode loop after timeout recovery");
    return true;
}

bool M2mDiagnosticService::handle_bytesused_zero(int frame_index) {
    if (frame_index != config_.bytesused_zero_at) {
        return true;
    }
    metrics_.bytesused_zero_count++;
    logger_.error("bytesused", "OUTPUT bytesused=0 injected at frame=" + std::to_string(frame_index));
    logger_.error("bytesused", "driver may see empty packet; do not blame VPU before fixing payload length");
    return false;
}

void M2mDiagnosticService::drain_and_stop() {
    sm_.transition(PipelineState::DRAINING, "send EOS and continue DQBUF until LAST");
    metrics_.eos_count++;
    for (int i = 0; i < 2; ++i) {
        metrics_.poll_calls++;
        metrics_.dqbuf_capture++;
        logger_.info("drain", "DQBUF delayed CAPTURE frame from DPB slot " + std::to_string(i));
    }
    sm_.transition(PipelineState::STOPPED, "STREAMOFF both queues and release buffers");
    metrics_.streamoff_count++;
    logger_.info("stream", "STREAMOFF OUTPUT + CAPTURE; release simulated buffers");
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
