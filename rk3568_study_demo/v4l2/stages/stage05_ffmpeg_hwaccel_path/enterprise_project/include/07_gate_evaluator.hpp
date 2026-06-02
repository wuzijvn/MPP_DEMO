#ifndef STAGE05_GATE_EVALUATOR_HPP_
#define STAGE05_GATE_EVALUATOR_HPP_

#include "01_pipeline_types.hpp"

namespace stage05_enterprise {

bool evaluate_gate(const PipelineConfig& cfg,
                   const PipelineStats& stats,
                   bool service_ok,
                   std::string* reason);

}  // namespace stage05_enterprise

#endif
