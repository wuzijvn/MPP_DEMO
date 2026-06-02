#include "03_state_machine.hpp"

namespace stage05_enterprise {

const char* state_to_string(PipelineState s) {
    switch (s) {
        case PipelineState::kInit:
            return "Init";
        case PipelineState::kDevicePrepared:
            return "DevicePrepared";
        case PipelineState::kInputOpened:
            return "InputOpened";
        case PipelineState::kStreamReady:
            return "StreamReady";
        case PipelineState::kDecoderReady:
            return "DecoderReady";
        case PipelineState::kLoopRunning:
            return "LoopRunning";
        case PipelineState::kDraining:
            return "Draining";
        case PipelineState::kStopped:
            return "Stopped";
        case PipelineState::kFailed:
            return "Failed";
    }
    return "Unknown";
}

StateMachine::StateMachine(PipelineStats* stats) : state_(PipelineState::kInit), stats_(stats) {}

/*
 * 状态迁移模块职责：
 * 1) 提供单一状态记录点；
 * 2) 维护 state_transition 计数，作为 gate 判断输入之一。
 */
void StateMachine::transit(PipelineState next, const char* /*reason*/) {
    state_ = next;
    if (stats_ != nullptr) {
        stats_->state_transition++;
    }
}

PipelineState StateMachine::state() const {
    return state_;
}

}  // namespace stage05_enterprise
