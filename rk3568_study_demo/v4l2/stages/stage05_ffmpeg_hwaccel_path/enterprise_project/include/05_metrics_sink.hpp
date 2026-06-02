#ifndef STAGE05_METRICS_SINK_HPP_
#define STAGE05_METRICS_SINK_HPP_

#include "01_pipeline_types.hpp"

namespace stage05_enterprise {

bool write_metrics_json(const std::string& path,
                        const PipelineConfig& cfg,
                        const PipelineStats& stats,
                        bool gate_pass,
                        const std::string& gate_reason,
                        const std::string& final_state);

}  // namespace stage05_enterprise

#endif
