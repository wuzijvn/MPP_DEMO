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
    std::string mode = "vm-vim2m";
    std::string device = "/dev/video0";
    std::string output_dir = "logs/enterprise_default";
    std::string inject = "none";
    std::string input;
    std::string decoder = "h264_rkmpp";
    uint32_t output_fourcc = v4l2_fourcc('R', 'G', 'B', 'P');
    uint32_t capture_fourcc = v4l2_fourcc('R', 'G', 'B', 'P');
    uint32_t width = 640;
    uint32_t height = 480;
    int frames = 8;
    int output_depth = 3;
    int capture_depth = 4;
    int timeout_at = -1;
    int source_change_at = -1;
    int bytesused_zero_at = -1;
    int min_decoded_frames = 4;
    int allowed_timeouts = 0;
    int timeout_ms = 1000;
    bool require_device = true;
    bool require_rkmpp = false;
    bool recover = true;
    bool verbose = true;
};

bool parse_cli(int argc, char** argv, CliConfig* config, std::string* error);
void print_usage(const char* argv0);
std::string config_summary(const CliConfig& config);

}  // namespace stage06_enterprise

#endif  // STAGE06_ENTERPRISE_CLI_CONFIG_HPP_
