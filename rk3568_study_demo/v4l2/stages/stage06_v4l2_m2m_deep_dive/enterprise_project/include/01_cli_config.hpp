#ifndef STAGE06_ENTERPRISE_CLI_CONFIG_HPP_
#define STAGE06_ENTERPRISE_CLI_CONFIG_HPP_

#include "00_enterprise_common.hpp"

namespace stage06_enterprise {

/*
 * CliConfig 保存一次诊断运行的全部输入条件。
 *
 * 生命周期：
 * - main() 创建；
 * - parse_cli() 填充并校验；
 * - M2mDiagnosticService 只读使用。
 *
 * 工作场景：
 * - 真实 bring-up 工具必须把测试输入写进日志/JSON，否则复现问题时会丢关键信息。
 */
struct CliConfig {
    std::string device = "/dev/video0";
    std::string output_dir = "logs/enterprise_default";
    std::string inject = "none";
    uint32_t output_fourcc = V4L2_PIX_FMT_H264;
    uint32_t capture_fourcc = V4L2_PIX_FMT_NV12;
    uint32_t width = 1280;
    uint32_t height = 720;
    int frames = 12;
    int output_depth = 3;
    int capture_depth = 4;
    int timeout_at = -1;
    int source_change_at = -1;
    int bytesused_zero_at = -1;
    int min_decoded_frames = 6;
    int allowed_timeouts = 0;
    bool require_device = false;
    bool recover = true;
    bool verbose = true;
};

bool parse_cli(int argc, char** argv, CliConfig* config, std::string* error);
void print_usage(const char* argv0);
std::string config_summary(const CliConfig& config);

}  // namespace stage06_enterprise

#endif  // STAGE06_ENTERPRISE_CLI_CONFIG_HPP_
