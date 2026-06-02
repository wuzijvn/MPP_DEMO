#include "05_gate_evaluator.hpp"

namespace stage06_enterprise {

void GateEvaluator::evaluate(const CliConfig& config, PipelineMetrics* m) const {
    if (m == NULL) {
        return;
    }

    /*
     * gate 判定优先找“最能行动”的失败层。
     * 这比只输出 FAIL 更适合岗位工作：驱动同学和框架同学需要不同证据。
     */
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

    m->gate_pass = true;
    if (m->timeout_count > 0 || m->source_change_count > 0) {
        m->verdict = "PASS_WITH_RECOVERY_EVIDENCE";
    } else {
        m->verdict = "PASS_NORMAL_PATH";
    }
    m->failure_layer = "none";
}

}  // namespace stage06_enterprise
