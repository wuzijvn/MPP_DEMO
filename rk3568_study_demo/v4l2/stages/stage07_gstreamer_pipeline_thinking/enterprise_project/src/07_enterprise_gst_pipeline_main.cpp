#include "01_cli_config.hpp"
#include "06_pipeline_service.hpp"

/*
 * Stage07 企业级 GStreamer pipeline 诊断服务入口。
 *
 * 输出：
 * - stdout: 适合人工快速阅读的 summary。
 * - enterprise_pipeline.log: 结构化运行日志。
 * - gst_run.log: 原始 gst-launch 输出。
 * - enterprise_metrics.json: gate 和 counters。
 *
 * 岗位映射：
 * - 这类工具可用于 SoC 板端 codec bring-up、pipeline 失败分层、CI smoke test 和 debug report。
 */
int main(int argc, char** argv) {
    stage07_enterprise::CliConfig config;
    std::string error;
    if (!stage07_enterprise::parse_cli(argc, argv, &config, &error)) {
        std::cerr << "parse_cli failed: " << error << "\n";
        stage07_enterprise::print_usage(argv[0]);
        return 2;
    }

    stage07_enterprise::PipelineMetrics metrics;
    stage07_enterprise::PipelineService service;
    const bool ok = service.run(config, &metrics);

    std::cout << "Stage07 Enterprise GStreamer Pipeline Diagnostic Service\n";
    stage07_enterprise::print_kv("config", stage07_enterprise::config_summary(config));
    stage07_enterprise::print_kv("pipeline", metrics.pipeline);
    stage07_enterprise::print_kv("gst_tools_available", stage07_enterprise::yes_no(metrics.gst_tools_available));
    stage07_enterprise::print_kv("backend_installed", stage07_enterprise::yes_no(metrics.backend_installed));
    stage07_enterprise::print_kv("exit_code", std::to_string(metrics.exit_code));
    stage07_enterprise::print_kv("elapsed_ms", std::to_string(metrics.elapsed_ms));
    stage07_enterprise::print_kv("eos_count", std::to_string(metrics.eos_count));
    stage07_enterprise::print_kv("caps_mentions", std::to_string(metrics.caps_mentions));
    stage07_enterprise::print_kv("failure_layer", metrics.failure_layer);
    stage07_enterprise::print_kv("gate_pass", stage07_enterprise::yes_no(metrics.gate_pass));
    stage07_enterprise::print_kv("gate_reason", metrics.gate_reason);
    stage07_enterprise::print_kv("metrics_json", config.output_dir + "/enterprise_metrics.json");
    stage07_enterprise::print_kv("gst_log", metrics.log_path);
    std::cout << "verdict=" << (ok ? "PASS_ENTERPRISE_GST_PIPELINE" : "FAIL_ENTERPRISE_GST_PIPELINE") << "\n";
    return ok ? 0 : 1;
}
