#ifndef STAGE05_CLI_CONFIG_HPP_
#define STAGE05_CLI_CONFIG_HPP_

#include "01_pipeline_types.hpp"

namespace stage05_enterprise {

bool parse_cli(int argc, char** argv, PipelineConfig* cfg);

}  // namespace stage05_enterprise

#endif
