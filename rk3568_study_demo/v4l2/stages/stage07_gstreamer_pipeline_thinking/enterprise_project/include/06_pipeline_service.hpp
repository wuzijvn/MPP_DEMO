#ifndef STAGE07_ENTERPRISE_PIPELINE_SERVICE_HPP_
#define STAGE07_ENTERPRISE_PIPELINE_SERVICE_HPP_

#include "00_enterprise_common.hpp"
#include "01_cli_config.hpp"
#include "02_state_machine.hpp"
#include "03_logger.hpp"
#include "04_metrics_sink.hpp"
#include "05_gate_evaluator.hpp"

namespace stage07_enterprise {

/*
 * PipelineService 是企业项目核心数据路径。
 *
 * 主要职责：
 * - 根据 scenario 构造 GStreamer pipeline；
 * - 运行 gst-launch 并保存日志；
 * - 解析输出得到 metrics；
 * - 调用 gate 和 metrics sink。
 *
 * 驱动影子线：
 * - 这里记录 backend_element 和 failure_layer，是为了让问题能转交到正确层：
 *   GStreamer plugin/rootfs、caps negotiation、codec backend、V4L2/MPP/VAAPI driver、display/memory path。
 */
class PipelineService {
public:
    bool run(const CliConfig& config, PipelineMetrics* metrics);

private:
    bool validate_config(const CliConfig& config, PipelineMetrics* metrics);
    bool probe_tools_and_backend(const CliConfig& config, PipelineMetrics* metrics);
    std::string build_pipeline(const CliConfig& config) const;
    CommandResult run_pipeline(const CliConfig& config, const std::string& pipeline) const;
    void parse_evidence(const CliConfig& config,
                        const CommandResult& result,
                        PipelineMetrics* metrics);

    StateMachine sm_;
    Logger logger_;
    MetricsSink metrics_sink_;
    GateEvaluator gate_;
};

}  // namespace stage07_enterprise

#endif  // STAGE07_ENTERPRISE_PIPELINE_SERVICE_HPP_
