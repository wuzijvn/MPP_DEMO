#ifndef STAGE03_CLI_CONFIG_HPP_
#define STAGE03_CLI_CONFIG_HPP_

#include "01_pipeline_types.hpp"

namespace enterprise_m2m {

bool parse_cli(int argc, char** argv, PipelineConfig* cfg);
void print_usage(const char* prog);

}  // namespace enterprise_m2m

#endif  // STAGE03_CLI_CONFIG_HPP_
