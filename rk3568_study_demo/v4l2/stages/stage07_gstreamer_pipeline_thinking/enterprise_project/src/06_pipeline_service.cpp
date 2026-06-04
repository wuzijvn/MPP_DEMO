#include "06_pipeline_service.hpp"

namespace stage07_enterprise {

bool PipelineService::run(const CliConfig& config, PipelineMetrics* metrics) {
    if (metrics == NULL) {
        return false;
    }

    ensure_dir(config.output_dir);
    logger_.open(config.output_dir);
    logger_.info("config", config_summary(config));

    sm_.transition(ServiceState::kValidateConfig, "validate CLI and scenario");
    if (!validate_config(config, metrics)) {
        sm_.transition(ServiceState::kFailed, metrics->gate_reason);
        metrics_sink_.write_json(config, *metrics, sm_.history_text(), config.output_dir);
        return false;
    }

    sm_.transition(ServiceState::kProbeTools, "probe gst tools and backend element");
    if (!probe_tools_and_backend(config, metrics)) {
        sm_.transition(ServiceState::kEvaluateGate, "tool/backend probe failed, evaluate gate");
        gate_.evaluate(config, metrics);
        sm_.transition(ServiceState::kExportMetrics, "write metrics after probe failure");
        metrics_sink_.write_json(config, *metrics, sm_.history_text(), config.output_dir);
        return metrics->gate_pass;
    }

    sm_.transition(ServiceState::kBuildPipeline, "construct gst-launch pipeline");
    const std::string pipeline = build_pipeline(config);
    metrics->pipeline = pipeline;
    logger_.info("pipeline", pipeline);

    sm_.transition(ServiceState::kRunPipeline, "run gst-launch and capture output");
    CommandResult result = run_pipeline(config, pipeline);
    metrics->log_path = config.output_dir + "/gst_run.log";
    write_text_file(metrics->log_path, result.output);
    logger_.info("pipeline", "gst log saved to " + metrics->log_path);

    sm_.transition(ServiceState::kParseEvidence, "parse output into counters");
    parse_evidence(config, result, metrics);

    sm_.transition(ServiceState::kEvaluateGate, "evaluate objective pass/fail gate");
    gate_.evaluate(config, metrics);
    logger_.info("gate", metrics->gate_pass ? "PASS: " + metrics->gate_reason
                                            : "FAIL: " + metrics->gate_reason);

    sm_.transition(ServiceState::kExportMetrics, "write enterprise_metrics.json");
    metrics_sink_.write_json(config, *metrics, sm_.history_text(), config.output_dir);

    sm_.transition(metrics->gate_pass ? ServiceState::kDone : ServiceState::kFailed,
                   metrics->gate_reason);
    logger_.info("driver_shadow",
                 "GStreamer evidence must be mapped to plugin/rootfs, caps negotiation, codec backend, and device/driver logs.");
    return metrics->gate_pass;
}

bool PipelineService::validate_config(const CliConfig& config, PipelineMetrics* metrics) {
    if (config.output_dir.empty()) {
        metrics->gate_reason = "empty output_dir";
        return false;
    }
    if (config.frames <= 0 || config.width <= 0 || config.height <= 0) {
        metrics->gate_reason = "invalid frame size or count";
        return false;
    }
    return true;
}

bool PipelineService::probe_tools_and_backend(const CliConfig& config,
                                              PipelineMetrics* metrics) {
    const bool launch = tool_exists("gst-launch-1.0");
    const bool inspect = tool_exists("gst-inspect-1.0");
    metrics->gst_tools_available = launch && inspect;
    logger_.info("probe", "gst-launch=" + yes_no(launch) + ", gst-inspect=" + yes_no(inspect));
    if (!metrics->gst_tools_available) {
        metrics->gate_reason = "gst tools unavailable";
        return false;
    }

    metrics->backend_installed = element_exists(config.backend_element);
    logger_.info("probe", "backend_element=" + config.backend_element +
                          ", installed=" + yes_no(metrics->backend_installed));
    if (config.require_backend && !metrics->backend_installed) {
        metrics->gate_reason = "required backend missing";
        return false;
    }
    return true;
}

std::string PipelineService::build_pipeline(const CliConfig& config) const {
    /*
     * 每个 scenario 都尽量保持可读的 gst-launch 字符串。
     * 注意 capsfilter 只限制格式，转换依赖 videoconvert；这正是 Stage07 的核心知识点之一。
     */
    if (config.scenario == "caps-failure") {
        return "videotestsrc num-buffers=" + std::to_string(config.frames) +
               " ! video/x-h264 ! fakesink sync=false";
    }

    if (config.scenario == "missing-element") {
        return "videotestsrc num-buffers=" + std::to_string(config.frames) +
               " ! definitely_missing_stage07_element ! fakesink sync=false";
    }

    if (config.scenario == "hardware-probe") {
        /*
         * 这里只做 element 可见性和证据边界，不构造假码流硬解。
         * 用 raw pipeline 保证工具可跑，同时 metrics 中记录 backend_installed。
         */
        return "videotestsrc num-buffers=" + std::to_string(config.frames) +
               " ! video/x-raw,format=NV12,width=" + std::to_string(config.width) +
               ",height=" + std::to_string(config.height) +
               ",framerate=30/1 ! videoconvert ! fakesink sync=false";
    }

    std::ostringstream p;
    p << "videotestsrc num-buffers=" << config.frames
      << " ! video/x-raw,format=NV12,width=" << config.width
      << ",height=" << config.height << ",framerate=30/1";

    if (config.scenario == "slow-queue") {
        p << " ! queue max-size-buffers=" << config.queue_depth
          << " max-size-bytes=0 max-size-time=0"
          << " ! identity sleep-time=" << config.slow_us;
    }

    p << " ! videoconvert ! video/x-raw,format=I420 ! fakesink sync=false";
    return p.str();
}

CommandResult PipelineService::run_pipeline(const CliConfig& config,
                                            const std::string& pipeline) const {
    std::ostringstream cmd;
    if (config.mode == "debug-caps" || config.min_caps_mentions > 0) {
        cmd << "GST_DEBUG=" << shell_quote(config.gst_debug) << " ";
    }
    cmd << "gst-launch-1.0 " << pipeline;
    return run_command_capture(cmd.str());
}

void PipelineService::parse_evidence(const CliConfig& config,
                                     const CommandResult& result,
                                     PipelineMetrics* metrics) {
    metrics->mode = config.mode;
    metrics->scenario = config.scenario;
    metrics->backend_element = config.backend_element;
    metrics->exit_code = result.exit_code;
    metrics->elapsed_ms = result.elapsed_ms;
    metrics->eos_count = count_occurrences(result.output, "Got EOS");
    metrics->error_count = count_occurrences(result.output, "ERROR");
    metrics->warning_count = count_occurrences(result.output, "WARNING");
    metrics->caps_mentions = count_occurrences(result.output, "caps") +
                             count_occurrences(result.output, "Caps");
    metrics->link_failure_count = count_occurrences(result.output, "could not link") +
                                  count_occurrences(result.output, "can't handle caps");
    metrics->missing_element_count = count_occurrences(result.output, "no element") +
                                     count_occurrences(result.output, "no such element");
    metrics->not_negotiated_count = count_occurrences(result.output, "not-negotiated");
    metrics->failure_layer = classify_failure(result.output, result.exit_code);

    /*
     * fpsdisplaysink 的精确帧数需要读取 property 或 signal；本项目不用 C API，
     * 因此 rendered_frames_hint 只在 EOS 成功时用输入帧数作为教学提示，不作为硬 gate。
     */
    metrics->rendered_frames_hint = (result.exit_code == 0 && metrics->eos_count > 0)
        ? config.frames
        : 0;
    metrics->dropped_frames_hint = 0;

    logger_.info("metrics", "exit_code=" + std::to_string(metrics->exit_code) +
                            ", elapsed_ms=" + std::to_string(metrics->elapsed_ms) +
                            ", eos_count=" + std::to_string(metrics->eos_count) +
                            ", failure_layer=" + metrics->failure_layer);
}

}  // namespace stage07_enterprise
