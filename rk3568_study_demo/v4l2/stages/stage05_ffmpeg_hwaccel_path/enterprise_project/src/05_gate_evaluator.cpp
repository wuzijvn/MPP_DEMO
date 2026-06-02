#include "07_gate_evaluator.hpp"

namespace stage05_enterprise {

/*
 * gate 模块职责：
 * 用“客观计数”判断 PASS/FAIL，避免主观口径。
 */
bool evaluate_gate(const PipelineConfig& cfg,
                   const PipelineStats& stats,
                   bool service_ok,
                   std::string* reason) {
    if (!service_ok) {
        *reason = "service run failed";
        return false;
    }
    if (stats.state_transition < 6) {
        *reason = "state transition count too low";
        return false;
    }
    if (stats.packet_read == 0 || stats.frame_recv == 0) {
        *reason = "no decode evidence packet/frame";
        return false;
    }

    /*
     * 两种证据口径：
     * 1) 显式 hwdevice 模式：必须看到 hw frame，适合 VAAPI/DRM 这类 hwcontext 实验；
     * 2) 默认 RKMPP wrapper 模式：h264_rkmpp/hevc_rkmpp decoder 本身就是硬解 wrapper，
     *    FFmpeg 可能返回 CPU 可见 frame，因此以 decoder wrapper + 成功输出 frame 作为通过证据。
     */
    if (cfg.hw_type_set) {
        if (!cfg.inject_force_sw_fallback && stats.frame_hw == 0) {
            *reason = "no hardware frame observed in explicit hwdevice mode";
            return false;
        }

        // 非 transfer 注入模式下，若有硬件帧则应至少成功一次下载。
        if (!cfg.inject_transfer_fail && stats.frame_hw > 0 && stats.hw_transfer_ok == 0) {
            *reason = "hardware frame exists but no successful transfer evidence";
            return false;
        }
    } else {
        if (!cfg.inject_force_sw_fallback &&
            cfg.decoder.find("_rkmpp") == std::string::npos) {
            *reason = "wrapper mode requires an *_rkmpp decoder as hardware evidence";
            return false;
        }
    }

    *reason = "pass";
    return true;
}

}  // namespace stage05_enterprise
