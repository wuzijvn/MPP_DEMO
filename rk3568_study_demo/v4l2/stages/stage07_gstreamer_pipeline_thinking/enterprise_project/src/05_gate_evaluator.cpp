#include "05_gate_evaluator.hpp"

namespace stage07_enterprise {

void GateEvaluator::evaluate(const CliConfig& config, PipelineMetrics* metrics) const {
    if (metrics == NULL) {
        return;
    }

    metrics->expected_failure = config.expect_failure;

    if (!metrics->gst_tools_available) {
        metrics->gate_pass = false;
        metrics->gate_reason = "gst tools missing";
        return;
    }

    if (config.require_backend && !metrics->backend_installed) {
        metrics->gate_pass = false;
        metrics->gate_reason = "required backend element is not installed";
        return;
    }

    if (config.expect_failure) {
        const bool failed = metrics->exit_code != 0;
        const bool known_layer = metrics->failure_layer != "unknown_gstreamer_failure";
        metrics->gate_pass = failed && known_layer;
        metrics->gate_reason = metrics->gate_pass
            ? "expected failure reproduced and classified"
            : "expected failure did not occur or was not classified";
        return;
    }

    if (metrics->exit_code != 0) {
        metrics->gate_pass = false;
        metrics->gate_reason = "pipeline exited with non-zero status";
        return;
    }

    if (metrics->eos_count <= 0) {
        metrics->gate_pass = false;
        metrics->gate_reason = "pipeline did not report EOS";
        return;
    }

    if (metrics->elapsed_ms > config.max_elapsed_ms) {
        metrics->gate_pass = false;
        metrics->gate_reason = "elapsed time exceeds gate threshold";
        return;
    }

    if (metrics->caps_mentions < config.min_caps_mentions) {
        metrics->gate_pass = false;
        metrics->gate_reason = "caps evidence below requested threshold";
        return;
    }

    metrics->gate_pass = true;
    metrics->gate_reason = "normal pipeline reached EOS within gate thresholds";
}

}  // namespace stage07_enterprise
