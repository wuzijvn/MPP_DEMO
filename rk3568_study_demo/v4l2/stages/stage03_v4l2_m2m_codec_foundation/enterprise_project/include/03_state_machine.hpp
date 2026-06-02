#ifndef STAGE03_STATE_MACHINE_HPP_
#define STAGE03_STATE_MACHINE_HPP_

#include "00_enterprise_common.hpp"
#include "01_pipeline_types.hpp"

namespace enterprise_m2m {

class StateMachine {
public:
    explicit StateMachine(PipelineStats* stats)
        : stats_(stats), state_(PipelineState::kInit) {}

    PipelineState state() const { return state_; }

    void transit(PipelineState next, const char* reason) {
        // 企业级日志要求记录状态迁移，方便 postmortem 复盘。
        fprintf(stdout, "[state] %s -> %s reason=%s\n", state_to_string(state_),
                state_to_string(next), reason ? reason : "-");
        state_ = next;
        if (stats_ != nullptr) {
            stats_->state_transition++;
        }
    }

private:
    PipelineStats* stats_;
    PipelineState state_;
};

}  // namespace enterprise_m2m

#endif  // STAGE03_STATE_MACHINE_HPP_
