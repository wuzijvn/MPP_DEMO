#include "05_gate_evaluator.hpp"

namespace stage06_enterprise {

void GateEvaluator::evaluate(const CliConfig& config, PipelineMetrics* m) const {
    if (m == NULL) {
        return;
    }

    if (config.mode == "rk-rkmpp") {
        if (!m->rk_evidence_collected) {
            m->gate_pass = false;
            m->verdict = "FAIL_RK_EVIDENCE_NOT_COLLECTED";
            m->failure_layer = "rk_board_probe";
            return;
        }
        if (config.require_rkmpp && !m->rk_decoder_seen) {
            m->gate_pass = false;
            m->verdict = "FAIL_RKMPP_DECODER_NOT_FOUND";
            m->failure_layer = "ffmpeg_rkmpp_backend";
            return;
        }
        if (!config.input.empty() && m->rk_decoder_seen && !m->rk_decode_command_ok) {
            m->gate_pass = false;
            m->verdict = "FAIL_RKMPP_DECODE_COMMAND";
            m->failure_layer = "rk_hardware_decode";
            return;
        }
        m->gate_pass = true;
        m->verdict = "PASS_RK_HARDWARE_PATH_EVIDENCE";
        m->failure_layer = "none";
        return;
    }

    /*
     * gate 判定优先找“最能行动”的失败层。
     * 这比只输出 FAIL 更适合岗位工作：驱动同学和框架同学需要不同证据。
     */
    if (!m->real_ioctl_path) {
        m->gate_pass = false;
        m->verdict = "FAIL_REAL_IOCTL_PATH_NOT_REACHED";
        m->failure_layer = "test_coverage";
        return;
    }
    if (config.inject == "unsupported_format") {
        m->gate_pass = m->unsupported_format_rejected;
        m->verdict = m->unsupported_format_rejected
                         ? "PASS_FAULT_UNSUPPORTED_FORMAT_REJECTED"
                         : "FAIL_UNSUPPORTED_FORMAT_ACCEPTED";
        m->failure_layer = m->unsupported_format_rejected ? "none" : "format_negotiation";
        return;
    }
    if (config.require_device && !m->device_opened) {
        m->gate_pass = false;
        m->verdict = "FAIL_DEVICE_REQUIRED";
        m->failure_layer = "device_node";
        return;
    }
    if (config.require_device && !m->m2m_capable) {
        m->gate_pass = false;
        m->verdict = "FAIL_M2M_CAPABILITY_REQUIRED";
        m->failure_layer = "device_capability";
        return;
    }
    if (config.require_device && !m->streaming_capable) {
        m->gate_pass = false;
        m->verdict = "FAIL_STREAMING_CAPABILITY_REQUIRED";
        m->failure_layer = "device_capability";
        return;
    }
    if (m->bytesused_zero_count > 0) {
        m->gate_pass = false;
        m->verdict = "FAIL_OUTPUT_BYTESUSED_ZERO";
        m->failure_layer = "v4l2_queue_payload";
        return;
    }
    if (m->timeout_count > config.allowed_timeouts) {
        m->gate_pass = false;
        m->verdict = "FAIL_TIMEOUT_OVER_LIMIT";
        m->failure_layer = "driver_or_hardware_completion";
        return;
    }
    if (m->decoded_frames < config.min_decoded_frames) {
        m->gate_pass = false;
        m->verdict = "FAIL_DECODED_FRAME_BELOW_GATE";
        m->failure_layer = "pipeline_progress";
        return;
    }
    if (m->qbuf_capture == 0 || m->qbuf_output == 0) {
        m->gate_pass = false;
        m->verdict = "FAIL_QUEUE_NOT_EXERCISED";
        m->failure_layer = "test_coverage";
        return;
    }
    if (m->mapped_capture == 0 || m->mapped_output == 0) {
        m->gate_pass = false;
        m->verdict = "FAIL_MMAP_NOT_EXERCISED";
        m->failure_layer = "test_coverage";
        return;
    }

    m->gate_pass = true;
    if (m->timeout_count > 0 || m->source_change_count > 0) {
        m->verdict = "PASS_WITH_RECOVERY_EVIDENCE";
    } else {
        m->verdict = "PASS_NORMAL_PATH";
    }
    m->failure_layer = "none";
}

}  // namespace stage06_enterprise
