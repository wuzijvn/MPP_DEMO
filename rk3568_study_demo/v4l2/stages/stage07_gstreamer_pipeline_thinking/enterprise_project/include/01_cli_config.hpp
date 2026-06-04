#ifndef STAGE07_ENTERPRISE_CLI_CONFIG_HPP_
#define STAGE07_ENTERPRISE_CLI_CONFIG_HPP_

#include "00_enterprise_common.hpp"

namespace stage07_enterprise {

/*
 * CliConfig 描述一次 GStreamer pipeline 诊断运行。
 *
 * 关键思想：
 * - scenario 决定 pipeline 形状和预期结果。
 * - backend_element 显式记录硬件/软件 element 选择，避免“decodebin 自动选了谁”说不清。
 * - output_dir 固化日志位置，保证问题能被复现和转交。
 */
struct CliConfig {
    std::string mode = "raw-basic";
    std::string scenario = "normal";
    std::string output_dir = "logs/enterprise_default";
    std::string backend_element = "avdec_h264_rkmpp";
    std::string gst_debug = "GST_CAPS:3,GST_ELEMENT_PADS:3,pipeline:3";
    int frames = 30;
    int width = 320;
    int height = 240;
    int queue_depth = 4;
    int slow_us = 0;
    int min_caps_mentions = 0;
    long long max_elapsed_ms = 20000;
    bool require_backend = false;
    bool expect_failure = false;
    bool quiet = false;
};

bool parse_cli(int argc, char** argv, CliConfig* config, std::string* error);
void print_usage(const char* argv0);
std::string config_summary(const CliConfig& config);

}  // namespace stage07_enterprise

#endif  // STAGE07_ENTERPRISE_CLI_CONFIG_HPP_
