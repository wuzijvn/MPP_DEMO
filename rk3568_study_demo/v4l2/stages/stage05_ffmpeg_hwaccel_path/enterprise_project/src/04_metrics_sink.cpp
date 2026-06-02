#include "05_metrics_sink.hpp"

#include <stdio.h>

namespace stage05_enterprise {

/*
 * metrics 输出模块职责：
 * 1) 把 gate、配置、运行计数写成 JSON；
 * 2) 给 CI 或批处理脚本提供机器可解析结果；
 * 3) 避免只靠终端日志做人眼判断。
 */
bool write_metrics_json(const std::string& path,
                        const PipelineConfig& cfg,
                        const PipelineStats& stats,
                        bool gate_pass,
                        const std::string& gate_reason,
                        const std::string& final_state) {
    FILE* fp = fopen(path.c_str(), "w");
    if (fp == nullptr) {
        fprintf(stderr, "open metrics file failed: %s\n", path.c_str());
        return false;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"input\": \"%s\",\n", cfg.input.c_str());
    fprintf(fp, "  \"decoder\": \"%s\",\n", cfg.decoder.c_str());
    fprintf(fp, "  \"hw_type\": \"%s\",\n", cfg.hw_type_set ? cfg.hw_type.c_str() : "");
    fprintf(fp, "  \"hw_type_set\": %s,\n", cfg.hw_type_set ? "true" : "false");
    fprintf(fp, "  \"device\": \"%s\",\n", cfg.device.c_str());
    fprintf(fp, "  \"max_frames\": %u,\n", cfg.max_frames);
    fprintf(fp, "  \"inject\": {\n");
    fprintf(fp, "    \"device_create_fail\": %s,\n", cfg.inject_device_create_fail ? "true" : "false");
    fprintf(fp, "    \"force_sw_fallback\": %s,\n", cfg.inject_force_sw_fallback ? "true" : "false");
    fprintf(fp, "    \"transfer_fail\": %s,\n", cfg.inject_transfer_fail ? "true" : "false");
    fprintf(fp, "    \"missing_hwfmt\": %s\n", cfg.inject_missing_hwfmt ? "true" : "false");
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"stats\": {\n");
    fprintf(fp, "    \"state_transition\": %llu,\n", static_cast<unsigned long long>(stats.state_transition));
    fprintf(fp, "    \"packet_read\": %llu,\n", static_cast<unsigned long long>(stats.packet_read));
    fprintf(fp, "    \"packet_sent\": %llu,\n", static_cast<unsigned long long>(stats.packet_sent));
    fprintf(fp, "    \"frame_recv\": %llu,\n", static_cast<unsigned long long>(stats.frame_recv));
    fprintf(fp, "    \"frame_hw\": %llu,\n", static_cast<unsigned long long>(stats.frame_hw));
    fprintf(fp, "    \"frame_cpu_visible\": %llu,\n", static_cast<unsigned long long>(stats.frame_cpu_visible));
    fprintf(fp, "    \"hw_transfer_ok\": %llu,\n", static_cast<unsigned long long>(stats.hw_transfer_ok));
    fprintf(fp, "    \"hw_transfer_fail\": %llu,\n", static_cast<unsigned long long>(stats.hw_transfer_fail));
    fprintf(fp, "    \"fallback_count\": %llu,\n", static_cast<unsigned long long>(stats.fallback_count));
    fprintf(fp, "    \"err_count\": %llu,\n", static_cast<unsigned long long>(stats.err_count));
    fprintf(fp, "    \"first_pts\": %lld,\n", static_cast<long long>(stats.first_pts));
    fprintf(fp, "    \"last_pts\": %lld\n", static_cast<long long>(stats.last_pts));
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"final_state\": \"%s\",\n", final_state.c_str());
    fprintf(fp, "  \"gate\": {\n");
    fprintf(fp, "    \"pass\": %s,\n", gate_pass ? "true" : "false");
    fprintf(fp, "    \"reason\": \"%s\"\n", gate_reason.c_str());
    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");

    fclose(fp);
    return true;
}

}  // namespace stage05_enterprise
