#ifndef STAGE06_ENTERPRISE_METRICS_SINK_HPP_
#define STAGE06_ENTERPRISE_METRICS_SINK_HPP_

#include "00_enterprise_common.hpp"
#include "01_cli_config.hpp"
#include "02_state_machine.hpp"

namespace stage06_enterprise {

/*
 * PipelineMetrics 是本项目的机器可读验收证据。
 *
 * 关键字段：
 * - qbuf/dqbuf counter：证明队列所有权是否往返；
 * - timeout/source_change/eos/recovery：证明故障和恢复路径是否被覆盖；
 * - max queue depth：用于性能/背压分析；
 * - gate_pass/verdict：用于自动化回归。
 */
struct PipelineMetrics {
    int qbuf_output = 0;
    int qbuf_capture = 0;
    int dqbuf_output = 0;
    int dqbuf_capture = 0;
    int decoded_frames = 0;
    int poll_calls = 0;
    int timeout_count = 0;
    int bytesused_zero_count = 0;
    int source_change_count = 0;
    int eos_count = 0;
    int recovery_count = 0;
    int streamoff_count = 0;
    int max_output_depth = 0;
    int max_capture_depth = 0;
    bool device_opened = false;
    bool querycap_ok = false;
    bool m2m_capable = false;
    bool simulated_device = false;
    bool gate_pass = false;
    std::string verdict = "NOT_EVALUATED";
    std::string failure_layer = "none";
};

bool write_metrics_json(const std::string& path, const CliConfig& config,
                        const PipelineMetrics& metrics,
                        const StateMachine& sm);
std::string metrics_summary_text(const PipelineMetrics& metrics);

}  // namespace stage06_enterprise

#endif  // STAGE06_ENTERPRISE_METRICS_SINK_HPP_
