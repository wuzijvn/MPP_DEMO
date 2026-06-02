#include "00_enterprise_common.hpp"
#include "01_pipeline_types.hpp"
#include "02_cli_config.hpp"
#include "03_state_machine.hpp"
#include "04_logger.hpp"
#include "05_metrics_sink.hpp"
#include "06_hwaccel_pipeline_service.hpp"
#include "07_gate_evaluator.hpp"

#include <string>

/*
 * 企业级主入口职责：
 * 1) 组装配置、日志、状态机、pipeline service；
 * 2) 运行后执行 gate 判定；
 * 3) 输出结构化结果（log + metrics + terminal summary）。
 */
int main(int argc, char** argv) {
    using namespace stage05_enterprise;

    PipelineConfig cfg;
    if (!parse_cli(argc, argv, &cfg)) {
        return 1;
    }

    if (!ensure_dir(cfg.log_dir)) {
        fprintf(stderr, "create log dir failed: %s\n", cfg.log_dir.c_str());
        return 2;
    }

    Logger logger;
    const std::string log_path = cfg.log_dir + "/enterprise_pipeline.log";
    if (!logger.open(log_path)) {
        return 3;
    }

    PipelineStats stats;
    StateMachine sm(&stats);

    logger.log(LogLevel::kInfo,
               "stage05 enterprise start input=%s decoder=%s hw_type=%s device=%s max_frames=%u",
               cfg.input.c_str(), cfg.decoder.c_str(),
               cfg.hw_type_set ? cfg.hw_type.c_str() : "(not-forced)",
               cfg.device.empty() ? "(auto)" : cfg.device.c_str(),
               cfg.max_frames);
    logger.log(LogLevel::kInfo,
               "inject flags device_fail=%d force_sw=%d transfer_fail=%d missing_hwfmt=%d",
               cfg.inject_device_create_fail ? 1 : 0,
               cfg.inject_force_sw_fallback ? 1 : 0,
               cfg.inject_transfer_fail ? 1 : 0,
               cfg.inject_missing_hwfmt ? 1 : 0);

    HwaccelPipelineService service(cfg, &stats, &sm, &logger);
    std::string fail_reason;
    const bool service_ok = service.run(&fail_reason);

    std::string gate_reason;
    const bool gate_pass = evaluate_gate(cfg, stats, service_ok, &gate_reason);

    logger.log(LogLevel::kInfo,
               "summary final_state=%s transitions=%llu packet_read=%llu packet_sent=%llu frame_recv=%llu hw=%llu cpu_visible=%llu transfer_ok=%llu transfer_fail=%llu fallback_count=%llu err_count=%llu",
               state_to_string(sm.state()),
               static_cast<unsigned long long>(stats.state_transition),
               static_cast<unsigned long long>(stats.packet_read),
               static_cast<unsigned long long>(stats.packet_sent),
               static_cast<unsigned long long>(stats.frame_recv),
               static_cast<unsigned long long>(stats.frame_hw),
               static_cast<unsigned long long>(stats.frame_cpu_visible),
               static_cast<unsigned long long>(stats.hw_transfer_ok),
               static_cast<unsigned long long>(stats.hw_transfer_fail),
               static_cast<unsigned long long>(stats.fallback_count),
               static_cast<unsigned long long>(stats.err_count));

    logger.log(LogLevel::kInfo,
               "driver_shadow: default RKMPP wrapper evidence is decoder selection plus decoded frames; explicit hwdevice mode may map to /dev/dri/renderD* or backend-specific nodes, and fallback often comes from hwfmt/device negotiation failure.");

    if (!gate_pass) {
        logger.log(LogLevel::kError, "gate fail: %s; fail_reason=%s", gate_reason.c_str(),
                   fail_reason.c_str());
    } else {
        logger.log(LogLevel::kInfo, "gate pass");
    }

    const std::string metrics_path = cfg.log_dir + "/enterprise_metrics.json";
    if (!write_metrics_json(metrics_path, cfg, stats, gate_pass, gate_reason,
                            state_to_string(sm.state()))) {
        return 4;
    }

    // 终端摘要给脚本层抓取 PASS/FAIL。
    printf("[enterprise] log=%s\n", log_path.c_str());
    printf("[enterprise] metrics=%s\n", metrics_path.c_str());
    printf("[enterprise] result=%s\n", gate_pass ? "PASS" : "FAIL");

    return gate_pass ? 0 : 5;
}
