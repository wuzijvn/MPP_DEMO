#ifndef STAGE03_V4L2_PIPELINE_SERVICE_HPP_
#define STAGE03_V4L2_PIPELINE_SERVICE_HPP_

#include "01_pipeline_types.hpp"

#include <string>

namespace enterprise_m2m {

class Logger;

class V4L2PipelineService {
public:
    V4L2PipelineService(const PipelineConfig& cfg, PipelineStats* stats, Logger* logger);
    ~V4L2PipelineService();

    bool run(std::string* fail_reason);

private:
    bool open_device(std::string* fail_reason);
    bool query_caps(std::string* fail_reason);
    bool set_formats(std::string* fail_reason);
    bool request_buffers(std::string* fail_reason);
    bool stream_on(std::string* fail_reason);
    bool pump_loops(std::string* fail_reason);
    bool drain_and_stop(std::string* fail_reason);
    void close_device();

    uint32_t output_type() const;
    uint32_t capture_type() const;

    PipelineConfig cfg_;
    PipelineStats* stats_;
    Logger* logger_;
    int fd_;
};

}  // namespace enterprise_m2m

#endif  // STAGE03_V4L2_PIPELINE_SERVICE_HPP_
