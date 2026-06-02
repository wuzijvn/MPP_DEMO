#ifndef STAGE05_PIPELINE_TYPES_HPP_
#define STAGE05_PIPELINE_TYPES_HPP_

#include <stdint.h>

#include <string>

namespace stage05_enterprise {

const int64_t kNoPts = static_cast<int64_t>(0x8000000000000000ULL);

enum class PipelineState {
    kInit = 0,
    kDevicePrepared,
    kInputOpened,
    kStreamReady,
    kDecoderReady,
    kLoopRunning,
    kDraining,
    kStopped,
    kFailed,
};

struct PipelineConfig {
    std::string input;
    /*
     * 默认走 RKMPP decoder wrapper，不强制绑定 AVHWDeviceContext。
     * hw_type 只有在命令行显式 --hw-type=... 时才生效，用于对比 DRM/VAAPI/RKMPP
     * hwdevice 行为，避免把不可用 VAAPI 当作板端默认路径。
     */
    std::string decoder = "h264_rkmpp";
    std::string hw_type;
    bool hw_type_set = false;
    std::string device;
    std::string log_dir = "./logs/run_default";

    uint32_t max_frames = 120;
    uint32_t print_every = 10;

    bool inject_device_create_fail = false;
    bool inject_force_sw_fallback = false;
    bool inject_transfer_fail = false;
    bool inject_missing_hwfmt = false;
};

struct PipelineStats {
    uint64_t state_transition = 0;

    uint64_t packet_read = 0;
    uint64_t packet_sent = 0;
    uint64_t frame_recv = 0;

    uint64_t frame_hw = 0;
    /*
     * frame_cpu_visible 统计“CPU 可见内存形态”的输出帧数量。
     * 注意：它不等价于“软件解码器在工作”。
     * 在默认 RKMPP wrapper 模式下，即便是 VPU 硬解，FFmpeg 也可能返回 CPU 可见帧格式。
     */
    uint64_t frame_cpu_visible = 0;
    uint64_t hw_transfer_ok = 0;
    uint64_t hw_transfer_fail = 0;

    uint64_t fallback_count = 0;
    uint64_t err_count = 0;

    int64_t first_pts = kNoPts;
    int64_t last_pts = kNoPts;
};

}  // namespace stage05_enterprise

#endif
