#include "02_state_machine.hpp"

namespace stage06_enterprise {

StateMachine::StateMachine() : current_(PipelineState::INIT) {}

void StateMachine::transition(PipelineState next, const std::string& reason) {
    StateTransition t;
    t.from = current_;
    t.to = next;
    t.reason = reason;
    history_.push_back(t);
    current_ = next;
}

PipelineState StateMachine::current() const {
    return current_;
}

const std::vector<StateTransition>& StateMachine::history() const {
    return history_;
}

std::string StateMachine::history_text() const {
    std::ostringstream oss;
    for (size_t i = 0; i < history_.size(); ++i) {
        oss << i << ": " << state_name(history_[i].from) << " -> "
            << state_name(history_[i].to) << " | " << history_[i].reason << "\n";
    }
    return oss.str();
}

std::string state_name(PipelineState state) {
    switch (state) {
        case PipelineState::INIT:
            return "INIT";
        case PipelineState::OPEN_DEVICE:
            return "OPEN_DEVICE";
        case PipelineState::QUERYCAP:
            return "QUERYCAP";
        case PipelineState::FORMAT_NEGOTIATION:
            return "FORMAT_NEGOTIATION";
        case PipelineState::BUFFER_SETUP:
            return "BUFFER_SETUP";
        case PipelineState::STREAMING:
            return "STREAMING";
        case PipelineState::RUNNING:
            return "RUNNING";
        case PipelineState::SOURCE_CHANGE:
            return "SOURCE_CHANGE";
        case PipelineState::DRAINING:
            return "DRAINING";
        case PipelineState::RECOVERY:
            return "RECOVERY";
        case PipelineState::STOPPED:
            return "STOPPED";
        case PipelineState::FAILED:
            return "FAILED";
    }
    return "UNKNOWN";
}

}  // namespace stage06_enterprise
