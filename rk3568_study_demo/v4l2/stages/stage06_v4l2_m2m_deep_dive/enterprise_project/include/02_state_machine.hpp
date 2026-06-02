#ifndef STAGE06_ENTERPRISE_STATE_MACHINE_HPP_
#define STAGE06_ENTERPRISE_STATE_MACHINE_HPP_

#include "00_enterprise_common.hpp"

namespace stage06_enterprise {

enum class PipelineState {
    INIT,
    OPEN_DEVICE,
    QUERYCAP,
    FORMAT_NEGOTIATION,
    BUFFER_SETUP,
    STREAMING,
    RUNNING,
    SOURCE_CHANGE,
    DRAINING,
    RECOVERY,
    STOPPED,
    FAILED
};

struct StateTransition {
    PipelineState from;
    PipelineState to;
    std::string reason;
};

/*
 * StateMachine 显式记录用户态 pipeline 状态变化。
 *
 * 为什么单独成模块：
 * - 工作中定位 V4L2 M2M 问题，最怕“当前到底 STREAMON 了吗、buffer 还属于谁”说不清。
 * - 状态机日志能把现象变成可复盘证据。
 *
 * 驱动影子线：
 * - 每个用户态 state 对应驱动 context/session/queue/job 的某个阶段。
 */
class StateMachine {
public:
    StateMachine();
    void transition(PipelineState next, const std::string& reason);
    PipelineState current() const;
    const std::vector<StateTransition>& history() const;
    std::string history_text() const;

private:
    PipelineState current_;
    std::vector<StateTransition> history_;
};

std::string state_name(PipelineState state);

}  // namespace stage06_enterprise

#endif  // STAGE06_ENTERPRISE_STATE_MACHINE_HPP_
