#include "02_state_machine.hpp"

namespace stage07_enterprise {

std::string state_name(ServiceState state) {
    switch (state) {
        case ServiceState::kInit:
            return "Init";
        case ServiceState::kValidateConfig:
            return "ValidateConfig";
        case ServiceState::kProbeTools:
            return "ProbeTools";
        case ServiceState::kBuildPipeline:
            return "BuildPipeline";
        case ServiceState::kRunPipeline:
            return "RunPipeline";
        case ServiceState::kParseEvidence:
            return "ParseEvidence";
        case ServiceState::kEvaluateGate:
            return "EvaluateGate";
        case ServiceState::kExportMetrics:
            return "ExportMetrics";
        case ServiceState::kDone:
            return "Done";
        case ServiceState::kFailed:
            return "Failed";
    }
    return "Unknown";
}

StateMachine::StateMachine() : state_(ServiceState::kInit) {
    history_.push_back("Init: service created");
}

void StateMachine::transition(ServiceState next, const std::string& reason) {
    std::ostringstream line;
    line << state_name(state_) << " -> " << state_name(next) << ": " << reason;
    history_.push_back(line.str());
    state_ = next;
}

ServiceState StateMachine::state() const {
    return state_;
}

std::string StateMachine::history_text() const {
    std::ostringstream oss;
    for (size_t i = 0; i < history_.size(); ++i) {
        oss << history_[i] << "\n";
    }
    return oss.str();
}

}  // namespace stage07_enterprise
