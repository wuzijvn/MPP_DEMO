#ifndef STAGE07_ENTERPRISE_METRICS_SINK_HPP_
#define STAGE07_ENTERPRISE_METRICS_SINK_HPP_

#include "00_enterprise_common.hpp"
#include "01_cli_config.hpp"

namespace stage07_enterprise {

/*
 * MetricsSink 输出机器可读 JSON。
 *
 * 工作意义：
 * - CI/板端回归需要结构化数据，不只需要 console log。
 * - gate_pass、failure_layer、caps_mentions、elapsed_ms 等字段可以进入日报/bring-up 表格。
 */
class MetricsSink {
public:
    bool write_json(const CliConfig& config,
                    const PipelineMetrics& metrics,
                    const std::string& state_history,
                    const std::string& output_dir) const;
};

std::string json_escape(const std::string& text);

}  // namespace stage07_enterprise

#endif  // STAGE07_ENTERPRISE_METRICS_SINK_HPP_
