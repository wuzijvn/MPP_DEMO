#ifndef STAGE05_HWACCEL_PIPELINE_SERVICE_HPP_
#define STAGE05_HWACCEL_PIPELINE_SERVICE_HPP_

#include "00_enterprise_common.hpp"
#include "01_pipeline_types.hpp"

namespace stage05_enterprise {

class Logger;
class StateMachine;

class HwaccelPipelineService {
public:
    HwaccelPipelineService(const PipelineConfig& cfg,
                           PipelineStats* stats,
                           StateMachine* sm,
                           Logger* logger);

    bool run(std::string* fail_reason);

private:
    bool prepare_decoder(AVFormatContext** fmt_ctx,
                         AVCodecContext** dec_ctx,
                         AVBufferRef** hw_dev_ctx,
                         int* video_stream_index,
                         std::string* fail_reason);

    bool run_loop(AVFormatContext* fmt_ctx,
                  AVCodecContext* dec_ctx,
                  int video_stream_index,
                  std::string* fail_reason);

    const PipelineConfig cfg_;
    PipelineStats* stats_;
    StateMachine* sm_;
    Logger* logger_;
};

}  // namespace stage05_enterprise

#endif
