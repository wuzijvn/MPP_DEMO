#include "02_cli_config.hpp"

#include <string.h>

#include <stdio.h>

namespace stage05_enterprise {
namespace {

void print_usage(const char* prog) {
    printf("%s usage:\n", prog);
    printf("  --input=PATH (required)\n");
    printf("  [--decoder=h264_rkmpp]\n");
    printf("  [--hw-type=drm|vaapi|rkmpp] optional hwdevice experiment\n");
    printf("  [--device=PATH] optional backend device\n");
    printf("  [--max-frames=120]\n");
    printf("  [--print-every=10]\n");
    printf("  [--log-dir=./logs/run_xxx]\n");
    printf("  [--inject-device-create-fail]\n");
    printf("  [--inject-force-sw-fallback]\n");
    printf("  [--inject-transfer-fail]\n");
    printf("  [--inject-missing-hwfmt]\n");
}

}  // namespace

/*
 * CLI 解析模块职责：
 * 1) 把命令行参数映射到 PipelineConfig；
 * 2) 提供最小输入校验（--input 必填）；
 * 3) 把故障注入开关显式化，方便矩阵脚本批量执行。
 */
bool parse_cli(int argc, char** argv, PipelineConfig* cfg) {
    if (cfg == nullptr) {
        return false;
    }

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            print_usage(argv[0]);
            return false;
        }
        if (strncmp(a, "--input=", 8) == 0) {
            cfg->input = a + 8;
            continue;
        }
        if (strncmp(a, "--decoder=", 10) == 0) {
            cfg->decoder = a + 10;
            continue;
        }
        if (strncmp(a, "--hw-type=", 10) == 0) {
            cfg->hw_type = a + 10;
            cfg->hw_type_set = true;
            continue;
        }
        if (strncmp(a, "--device=", 9) == 0) {
            cfg->device = a + 9;
            continue;
        }
        if (strncmp(a, "--max-frames=", 13) == 0) {
            cfg->max_frames = static_cast<uint32_t>(atoi(a + 13));
            continue;
        }
        if (strncmp(a, "--print-every=", 14) == 0) {
            cfg->print_every = static_cast<uint32_t>(atoi(a + 14));
            continue;
        }
        if (strncmp(a, "--log-dir=", 10) == 0) {
            cfg->log_dir = a + 10;
            continue;
        }

        // 故障注入开关：用于验证 gate 是否能准确识别异常路径。
        if (strcmp(a, "--inject-device-create-fail") == 0) {
            cfg->inject_device_create_fail = true;
            continue;
        }
        if (strcmp(a, "--inject-force-sw-fallback") == 0) {
            cfg->inject_force_sw_fallback = true;
            continue;
        }
        if (strcmp(a, "--inject-transfer-fail") == 0) {
            cfg->inject_transfer_fail = true;
            continue;
        }
        if (strcmp(a, "--inject-missing-hwfmt") == 0) {
            cfg->inject_missing_hwfmt = true;
            continue;
        }

        fprintf(stderr, "unknown arg: %s\n", a);
        print_usage(argv[0]);
        return false;
    }

    if (cfg->input.empty()) {
        print_usage(argv[0]);
        fprintf(stderr, "--input is required\n");
        return false;
    }
    return true;
}

}  // namespace stage05_enterprise
