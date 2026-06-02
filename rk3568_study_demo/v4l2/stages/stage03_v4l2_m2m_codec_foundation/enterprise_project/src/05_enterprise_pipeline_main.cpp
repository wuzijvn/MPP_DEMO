#include "00_enterprise_common.hpp"
#include "01_pipeline_types.hpp"
#include "02_cli_config.hpp"
#include "03_state_machine.hpp"
#include "04_logger.hpp"
#include "05_metrics_sink.hpp"
#include "06_v4l2_pipeline_service.hpp"

#include <stdio.h>

#include <string>

namespace enterprise_m2m {
namespace {

bool evaluate_pass(const PipelineStats& s, const PipelineConfig& cfg,
                   std::string* reason) {
    if (s.state_transition < 6) {
        *reason = "state transition too few, pipeline may not complete";
        return false;
    }
    const uint64_t expected_loops =
        cfg.input_annexb.empty() ? static_cast<uint64_t>(cfg.loops)
                                 : s.payload_chunks_total;
    if (s.qbuf_out < expected_loops || s.qbuf_cap < expected_loops) {
        *reason = "qbuf counters lower than expected loops";
        return false;
    }
    if (!cfg.inject_timeout && s.poll_timeout > 0) {
        *reason = "unexpected poll timeout in normal mode";
        return false;
    }
    if (!cfg.inject_source_change && s.source_change > 0) {
        *reason = "unexpected source_change in normal mode";
        return false;
    }
    if (!cfg.input_annexb.empty() && s.real_payload_mode == 0) {
        *reason = "input_annexb provided but real_payload_mode not enabled";
        return false;
    }
    if (!cfg.input_annexb.empty() && (s.payload_bytes_total == 0 || s.payload_chunks_total == 0)) {
        *reason = "real payload mode requires non-zero payload bytes/chunks";
        return false;
    }
    *reason = "pass";
    return true;
}

}  // namespace
}  // namespace enterprise_m2m

int main(int argc, char** argv) {
    using namespace enterprise_m2m;

    PipelineConfig cfg;
    if (!parse_cli(argc, argv, &cfg)) {
        return 1;
    }

    if (!ensure_dir(cfg.log_dir)) {
        fprintf(stderr, "log dir create failed: %s\n", cfg.log_dir.c_str());
        return 1;
    }

    Logger logger;
    const std::string log_path = cfg.log_dir + "/enterprise_pipeline.log";
    if (!logger.open(log_path)) {
        return 1;
    }

    PipelineStats stats;
    StateMachine sm(&stats);

    logger.log(LogLevel::kInfo,
               "enterprise stage03 start: dev=%s loops=%u timeout_ms=%u input_annexb=%s output_bytesused=%u max_input_chunks=%u inject_timeout=%d inject_source_change=%d inject_dqbuf_eagain=%d",
               cfg.dev.c_str(), cfg.loops, cfg.timeout_ms,
               cfg.input_annexb.empty() ? "-" : cfg.input_annexb.c_str(),
               cfg.output_bytesused, cfg.max_input_chunks,
               cfg.inject_timeout ? 1 : 0,
               cfg.inject_source_change ? 1 : 0, cfg.inject_dqbuf_eagain ? 1 : 0);

    sm.transit(PipelineState::kDeviceOpened, "enter service bootstrap");
    sm.transit(PipelineState::kCapsQueried, "prepare querycap stage");
    sm.transit(PipelineState::kFormatsSet, "prepare format stage");
    sm.transit(PipelineState::kBuffersRequested, "prepare vb2 queue stage");
    sm.transit(PipelineState::kStreaming, "prepare streaming stage");

    V4L2PipelineService svc(cfg, &stats, &logger);
    std::string fail_reason;
    const bool service_ok = svc.run(&fail_reason);

    if (service_ok) {
        sm.transit(PipelineState::kDraining, "service finished and enters drain");
        sm.transit(PipelineState::kStopped, "graceful stop");
    } else {
        sm.transit(PipelineState::kFailed, fail_reason.c_str());
    }

    std::string gate_reason;
    const bool pass_gate = service_ok && evaluate_pass(stats, cfg, &gate_reason);

    logger.log(LogLevel::kInfo,
               "summary: state=%s transitions=%llu qbuf_out=%llu qbuf_cap=%llu dq_out_ok=%llu dq_cap_ok=%llu dq_eagain=%llu poll_timeout=%llu source_change=%llu eos=%llu real_payload_mode=%llu payload_chunks=%llu payload_bytes=%llu",
               state_to_string(sm.state()),
               static_cast<unsigned long long>(stats.state_transition),
               static_cast<unsigned long long>(stats.qbuf_out),
               static_cast<unsigned long long>(stats.qbuf_cap),
               static_cast<unsigned long long>(stats.dqbuf_out_ok),
               static_cast<unsigned long long>(stats.dqbuf_cap_ok),
               static_cast<unsigned long long>(stats.dqbuf_eagain),
               static_cast<unsigned long long>(stats.poll_timeout),
               static_cast<unsigned long long>(stats.source_change),
               static_cast<unsigned long long>(stats.eos_count),
               static_cast<unsigned long long>(stats.real_payload_mode),
               static_cast<unsigned long long>(stats.payload_chunks_total),
               static_cast<unsigned long long>(stats.payload_bytes_total));

    logger.log(LogLevel::kInfo,
               "driver_shadow: userspace stage maps to driver s_fmt/reqbufs/qbuf/dqbuf/streamon/streamoff, and SOURCE_CHANGE requires CAPTURE reconfigure contract.");

    if (!pass_gate) {
        if (gate_reason.empty()) {
            gate_reason = fail_reason.empty() ? "unknown" : fail_reason;
        }
        logger.log(LogLevel::kError, "gate fail: %s", gate_reason.c_str());
    } else {
        logger.log(LogLevel::kInfo, "gate pass");
    }

    const std::string metrics_path = cfg.log_dir + "/enterprise_metrics.json";
    if (!write_metrics_json(metrics_path, cfg, stats, pass_gate, gate_reason)) {
        logger.log(LogLevel::kError, "write metrics failed: %s", metrics_path.c_str());
        return 1;
    }

    printf("[enterprise] log=%s\n", log_path.c_str());
    printf("[enterprise] metrics=%s\n", metrics_path.c_str());
    printf("[enterprise] result=%s\n", pass_gate ? "PASS" : "FAIL");

    return pass_gate ? 0 : 2;
}
