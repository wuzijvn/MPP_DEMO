#ifndef STAGE07_ENTERPRISE_STATE_MACHINE_HPP_
#define STAGE07_ENTERPRISE_STATE_MACHINE_HPP_

#include "00_enterprise_common.hpp"

namespace stage07_enterprise {

/*
 * 显式状态机。
 *
 * GStreamer 自身有 NULL/READY/PAUSED/PLAYING 状态；本诊断服务再包一层工具状态：
 * ValidateConfig -> ProbeTools -> BuildPipeline -> RunPipeline -> ParseEvidence -> EvaluateGate -> ExportMetrics。
 *
 * 工作意义：
 * - 真实 bring-up 报告不能只说“跑失败了”，要说明失败停在哪个阶段。
 */
enum class ServiceState {
    kInit,
    kValidateConfig,
    kProbeTools,
    kBuildPipeline,
    kRunPipeline,
    kParseEvidence,
    kEvaluateGate,
    kExportMetrics,
    kDone,
    kFailed
};

class StateMachine {
public:
    StateMachine();
    void transition(ServiceState next, const std::string& reason);
    ServiceState state() const;
    std::string history_text() const;

private:
    ServiceState state_;
    std::vector<std::string> history_;
};

std::string state_name(ServiceState state);

}  // namespace stage07_enterprise

#endif  // STAGE07_ENTERPRISE_STATE_MACHINE_HPP_
