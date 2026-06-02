#ifndef STAGE03_METRICS_SINK_HPP_
#define STAGE03_METRICS_SINK_HPP_

#include "01_pipeline_types.hpp"

#include <string>

namespace enterprise_m2m {

bool write_metrics_json(const std::string& path, const PipelineConfig& cfg,
                        const PipelineStats& stats, bool pass,
                        const std::string& fail_reason);

}  // namespace enterprise_m2m

#endif  // STAGE03_METRICS_SINK_HPP_
