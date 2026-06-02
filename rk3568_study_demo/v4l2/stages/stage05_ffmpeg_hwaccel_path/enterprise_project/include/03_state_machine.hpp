#ifndef STAGE05_STATE_MACHINE_HPP_
#define STAGE05_STATE_MACHINE_HPP_

#include "01_pipeline_types.hpp"

namespace stage05_enterprise {

const char* state_to_string(PipelineState s);

class StateMachine {
public:
    explicit StateMachine(PipelineStats* stats);

    void transit(PipelineState next, const char* reason);
    PipelineState state() const;

private:
    PipelineState state_;
    PipelineStats* stats_;
};

}  // namespace stage05_enterprise

#endif
