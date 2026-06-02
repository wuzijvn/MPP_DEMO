#ifndef STAGE04_ENTERPRISE_PIPELINE_TYPES_HPP_
#define STAGE04_ENTERPRISE_PIPELINE_TYPES_HPP_

#include <stdint.h>
#include <string>

namespace stage04_enterprise {

enum class PipelineState {
    kInit = 0,
    kInputOpened,
    kStreamReady,
    kDecoderReady,
    kDecoding,
    kCompleted,
    kFailed,
};

struct Config {
    std::string input;
    std::string log_dir = "./logs";
    int max_frames = 120;
    bool inject_send_fail = false;
    bool inject_receive_fail = false;
    bool verbose = false;
};

struct Metrics {
    uint64_t packet_in = 0;
    uint64_t frame_out = 0;
    uint64_t error_count = 0;
    uint64_t state_transition = 0;
    double elapsed_s = 0.0;
    double fps = 0.0;
};

inline const char* state_to_cstr(PipelineState s) {
    switch (s) {
        case PipelineState::kInit:
            return "INIT";
        case PipelineState::kInputOpened:
            return "INPUT_OPENED";
        case PipelineState::kStreamReady:
            return "STREAM_READY";
        case PipelineState::kDecoderReady:
            return "DECODER_READY";
        case PipelineState::kDecoding:
            return "DECODING";
        case PipelineState::kCompleted:
            return "COMPLETED";
        case PipelineState::kFailed:
            return "FAILED";
        default:
            return "UNKNOWN";
    }
}

}  // namespace stage04_enterprise

#endif
