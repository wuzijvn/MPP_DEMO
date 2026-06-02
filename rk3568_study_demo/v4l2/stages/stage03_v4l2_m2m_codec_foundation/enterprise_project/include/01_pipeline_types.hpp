#ifndef STAGE03_PIPELINE_TYPES_HPP_
#define STAGE03_PIPELINE_TYPES_HPP_

#include <stdint.h>

#include <string>

namespace enterprise_m2m {

enum class PipelineState {
    kInit = 0,
    kDeviceOpened,
    kCapsQueried,
    kFormatsSet,
    kBuffersRequested,
    kStreaming,
    kDraining,
    kStopped,
    kFailed,
};

struct PipelineConfig {
    std::string dev = "/dev/video0";
    uint32_t in_fourcc = 0;
    uint32_t out_fourcc = 0;
    uint32_t width = 1280;
    uint32_t height = 720;
    uint32_t out_count = 4;
    uint32_t cap_count = 4;
    uint32_t timeout_ms = 200;
    uint32_t loops = 8;
    uint32_t output_bytesused = 16;
    uint32_t max_input_chunks = 0;
    bool mplane = false;
    bool inject_timeout = false;
    bool inject_source_change = false;
    bool inject_dqbuf_eagain = false;
    bool verbose = false;
    std::string log_dir = "./logs";
    std::string input_annexb;
};

struct PipelineStats {
    uint64_t qbuf_out = 0;
    uint64_t qbuf_cap = 0;
    uint64_t dqbuf_out_ok = 0;
    uint64_t dqbuf_cap_ok = 0;
    uint64_t dqbuf_eagain = 0;
    uint64_t poll_timeout = 0;
    uint64_t source_change = 0;
    uint64_t eos_count = 0;
    uint64_t state_transition = 0;
    uint64_t payload_bytes_total = 0;
    uint64_t payload_chunks_total = 0;
    uint64_t real_payload_mode = 0;
};

inline const char* state_to_string(PipelineState s) {
    switch (s) {
        case PipelineState::kInit:
            return "INIT";
        case PipelineState::kDeviceOpened:
            return "DEVICE_OPENED";
        case PipelineState::kCapsQueried:
            return "CAPS_QUERIED";
        case PipelineState::kFormatsSet:
            return "FORMATS_SET";
        case PipelineState::kBuffersRequested:
            return "BUFFERS_REQUESTED";
        case PipelineState::kStreaming:
            return "STREAMING";
        case PipelineState::kDraining:
            return "DRAINING";
        case PipelineState::kStopped:
            return "STOPPED";
        case PipelineState::kFailed:
            return "FAILED";
        default:
            return "UNKNOWN";
    }
}

}  // namespace enterprise_m2m

#endif  // STAGE03_PIPELINE_TYPES_HPP_
