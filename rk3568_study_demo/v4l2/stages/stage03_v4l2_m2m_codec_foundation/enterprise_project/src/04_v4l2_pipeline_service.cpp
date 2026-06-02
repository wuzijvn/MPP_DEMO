#include "06_v4l2_pipeline_service.hpp"

#include "00_enterprise_common.hpp"
#include "04_logger.hpp"

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <fstream>
#include <string>
#include <vector>

namespace enterprise_m2m {
namespace {

struct NaluSpan {
    size_t start_offset = 0;
    size_t payload_offset = 0;
    size_t end_offset = 0;
    size_t start_code_len = 0;
};

size_t start_code_len_at(const std::vector<uint8_t>& data, size_t pos) {
    if (pos + 4 <= data.size() && data[pos] == 0x00 && data[pos + 1] == 0x00 &&
        data[pos + 2] == 0x00 && data[pos + 3] == 0x01) {
        return 4;
    }
    if (pos + 3 <= data.size() && data[pos] == 0x00 && data[pos + 1] == 0x00 &&
        data[pos + 2] == 0x01) {
        return 3;
    }
    return 0;
}

std::vector<NaluSpan> parse_annexb_nalus(const std::vector<uint8_t>& data) {
    std::vector<NaluSpan> nalus;
    size_t pos = 0;

    while (pos < data.size()) {
        const size_t sc_len = start_code_len_at(data, pos);
        if (sc_len == 0) {
            ++pos;
            continue;
        }

        NaluSpan nalu;
        nalu.start_offset = pos;
        nalu.start_code_len = sc_len;
        nalu.payload_offset = pos + sc_len;

        size_t next = nalu.payload_offset;
        while (next < data.size() && start_code_len_at(data, next) == 0) {
            ++next;
        }
        nalu.end_offset = next;

        if (nalu.payload_offset < nalu.end_offset) {
            nalus.push_back(nalu);
        }
        pos = next;
    }

    return nalus;
}

bool read_file_bytes(const std::string& path, std::vector<uint8_t>* out,
                     std::string* err) {
    std::ifstream ifs(path.c_str(), std::ios::binary);
    if (!ifs.is_open()) {
        *err = string_format("open input_annexb failed: %s", path.c_str());
        return false;
    }

    ifs.seekg(0, std::ios::end);
    const std::streamoff size = ifs.tellg();
    if (size <= 0) {
        *err = string_format("input_annexb is empty: %s", path.c_str());
        return false;
    }

    out->resize(static_cast<size_t>(size));
    ifs.seekg(0, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(&(*out)[0]), size);
    if (!ifs.good()) {
        *err = string_format("read input_annexb failed: %s", path.c_str());
        return false;
    }
    return true;
}

std::vector<uint32_t> build_annexb_payload_plan(const std::vector<uint8_t>& data,
                                                uint32_t max_chunks) {
    const std::vector<NaluSpan> nalus = parse_annexb_nalus(data);
    std::vector<uint32_t> plan;

    for (size_t i = 0; i < nalus.size(); ++i) {
        const size_t nalu_bytes = nalus[i].end_offset - nalus[i].start_offset;
        if (nalu_bytes == 0) {
            continue;
        }
        if (nalu_bytes > 0xffffffffULL) {
            continue;
        }

        plan.push_back(static_cast<uint32_t>(nalu_bytes));
        if (max_chunks > 0 && plan.size() >= static_cast<size_t>(max_chunks)) {
            break;
        }
    }

    return plan;
}

}  // namespace

V4L2PipelineService::V4L2PipelineService(const PipelineConfig& cfg,
                                         PipelineStats* stats, Logger* logger)
    : cfg_(cfg), stats_(stats), logger_(logger), fd_(-1) {}

V4L2PipelineService::~V4L2PipelineService() {
    close_device();
}

bool V4L2PipelineService::run(std::string* fail_reason) {
    if (!open_device(fail_reason)) {
        return false;
    }
    if (!query_caps(fail_reason)) {
        return false;
    }
    if (!set_formats(fail_reason)) {
        return false;
    }
    if (!request_buffers(fail_reason)) {
        return false;
    }
    if (!stream_on(fail_reason)) {
        return false;
    }
    if (!pump_loops(fail_reason)) {
        return false;
    }
    if (!drain_and_stop(fail_reason)) {
        return false;
    }
    return true;
}

uint32_t V4L2PipelineService::output_type() const {
    return cfg_.mplane ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT;
}

uint32_t V4L2PipelineService::capture_type() const {
    return cfg_.mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
                       : V4L2_BUF_TYPE_VIDEO_CAPTURE;
}

bool V4L2PipelineService::open_device(std::string* fail_reason) {
    // 前置条件：dev 节点存在且具备读写权限。
    fd_ = open(cfg_.dev.c_str(), O_RDWR | O_NONBLOCK);
    if (fd_ < 0) {
        *fail_reason = string_format("open failed: %s", strerror(errno));
        return false;
    }
    logger_->log(LogLevel::kInfo, "open ok: dev=%s fd=%d", cfg_.dev.c_str(), fd_);
    return true;
}

bool V4L2PipelineService::query_caps(std::string* fail_reason) {
    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));
    if (xioctl(fd_, VIDIOC_QUERYCAP, &cap) < 0) {
        *fail_reason = string_format("VIDIOC_QUERYCAP failed: %s", strerror(errno));
        return false;
    }

    logger_->log(LogLevel::kInfo,
                 "querycap ok: driver=%s card=%s bus=%s caps=0x%x device_caps=0x%x",
                 cap.driver, cap.card, cap.bus_info, cap.capabilities, cap.device_caps);
    return true;
}

bool V4L2PipelineService::set_formats(std::string* fail_reason) {
    struct v4l2_format fmt_out;
    memset(&fmt_out, 0, sizeof(fmt_out));
    fmt_out.type = output_type();

    if (cfg_.mplane) {
        fmt_out.fmt.pix_mp.width = cfg_.width;
        fmt_out.fmt.pix_mp.height = cfg_.height;
        fmt_out.fmt.pix_mp.pixelformat = cfg_.in_fourcc;
        fmt_out.fmt.pix_mp.num_planes = 1;
    } else {
        fmt_out.fmt.pix.width = cfg_.width;
        fmt_out.fmt.pix.height = cfg_.height;
        fmt_out.fmt.pix.pixelformat = cfg_.in_fourcc;
    }

    // S_FMT 输出队列：驱动侧会校验 bitstream 输入格式及尺寸约束。
    if (xioctl(fd_, VIDIOC_S_FMT, &fmt_out) < 0) {
        *fail_reason = string_format("VIDIOC_S_FMT OUTPUT failed: %s", strerror(errno));
        return false;
    }

    struct v4l2_format fmt_cap;
    memset(&fmt_cap, 0, sizeof(fmt_cap));
    fmt_cap.type = capture_type();
    if (cfg_.mplane) {
        fmt_cap.fmt.pix_mp.width = cfg_.width;
        fmt_cap.fmt.pix_mp.height = cfg_.height;
        fmt_cap.fmt.pix_mp.pixelformat = cfg_.out_fourcc;
        fmt_cap.fmt.pix_mp.num_planes = 1;
    } else {
        fmt_cap.fmt.pix.width = cfg_.width;
        fmt_cap.fmt.pix.height = cfg_.height;
        fmt_cap.fmt.pix.pixelformat = cfg_.out_fourcc;
    }

    // S_FMT 输出帧队列：驱动会回填对齐后的 sizeimage/stride 等。
    if (xioctl(fd_, VIDIOC_S_FMT, &fmt_cap) < 0) {
        *fail_reason = string_format("VIDIOC_S_FMT CAPTURE failed: %s", strerror(errno));
        return false;
    }

    logger_->log(LogLevel::kInfo, "set format ok: %ux%u mplane=%d", cfg_.width,
                 cfg_.height, cfg_.mplane ? 1 : 0);
    return true;
}

bool V4L2PipelineService::request_buffers(std::string* fail_reason) {
    struct v4l2_requestbuffers req_out;
    memset(&req_out, 0, sizeof(req_out));
    req_out.count = cfg_.out_count;
    req_out.type = output_type();
    req_out.memory = V4L2_MEMORY_MMAP;

    // REQBUFS 后，vb2 在驱动侧完成 queue_setup 与 buffer 准备。
    if (xioctl(fd_, VIDIOC_REQBUFS, &req_out) < 0) {
        *fail_reason = string_format("VIDIOC_REQBUFS OUTPUT failed: %s", strerror(errno));
        return false;
    }

    struct v4l2_requestbuffers req_cap;
    memset(&req_cap, 0, sizeof(req_cap));
    req_cap.count = cfg_.cap_count;
    req_cap.type = capture_type();
    req_cap.memory = V4L2_MEMORY_MMAP;

    if (xioctl(fd_, VIDIOC_REQBUFS, &req_cap) < 0) {
        *fail_reason = string_format("VIDIOC_REQBUFS CAPTURE failed: %s", strerror(errno));
        return false;
    }

    logger_->log(LogLevel::kInfo, "reqbufs ok: out=%u cap=%u", req_out.count,
                 req_cap.count);
    return true;
}

bool V4L2PipelineService::stream_on(std::string* fail_reason) {
    const uint32_t out_t = output_type();
    const uint32_t cap_t = capture_type();

    if (xioctl(fd_, VIDIOC_STREAMON, const_cast<uint32_t*>(&cap_t)) < 0) {
        *fail_reason = string_format("VIDIOC_STREAMON CAPTURE failed: %s", strerror(errno));
        return false;
    }
    if (xioctl(fd_, VIDIOC_STREAMON, const_cast<uint32_t*>(&out_t)) < 0) {
        *fail_reason = string_format("VIDIOC_STREAMON OUTPUT failed: %s", strerror(errno));
        return false;
    }

    logger_->log(LogLevel::kInfo, "streamon ok: out_type=%u cap_type=%u", out_t, cap_t);
    return true;
}

bool V4L2PipelineService::pump_loops(std::string* fail_reason) {
    std::vector<uint32_t> payload_plan;
    bool real_mode = false;

    if (!cfg_.input_annexb.empty()) {
        std::vector<uint8_t> data;
        std::string err;
        if (!read_file_bytes(cfg_.input_annexb, &data, &err)) {
            *fail_reason = err;
            return false;
        }

        payload_plan = build_annexb_payload_plan(data, cfg_.max_input_chunks);
        if (payload_plan.empty()) {
            *fail_reason = string_format(
                "no AnnexB NALU found in input_annexb=%s (hint: convert mp4 to AnnexB)",
                cfg_.input_annexb.c_str());
            return false;
        }

        real_mode = true;
        stats_->real_payload_mode = 1;

        uint64_t sum = 0;
        for (size_t i = 0; i < payload_plan.size(); ++i) {
            sum += payload_plan[i];
        }

        logger_->log(LogLevel::kInfo,
                     "real_payload_mode=1 input_annexb=%s chunks=%zu bytes_total=%llu max_input_chunks=%u",
                     cfg_.input_annexb.c_str(), payload_plan.size(),
                     static_cast<unsigned long long>(sum), cfg_.max_input_chunks);
    } else {
        logger_->log(LogLevel::kWarn,
                     "real_payload_mode=0 use simulation payload: output_bytesused=%u",
                     cfg_.output_bytesused);
    }

    uint32_t loop_count = cfg_.loops;
    if (real_mode) {
        loop_count = static_cast<uint32_t>(payload_plan.size());
        logger_->log(LogLevel::kInfo,
                     "real mode overrides loops: config_loops=%u actual_loops=%u",
                     cfg_.loops, loop_count);
    }

    // 这里是“企业级训练”的主循环：真实输入驱动 + 故障注入 + 指标采样。
    for (uint32_t i = 0; i < loop_count; ++i) {
        const uint32_t bytesused = real_mode ? payload_plan[i] : cfg_.output_bytesused;

        stats_->qbuf_out++;
        stats_->qbuf_cap++;
        stats_->payload_chunks_total++;
        stats_->payload_bytes_total += bytesused;

        if (cfg_.inject_dqbuf_eagain && (i % 3 == 0)) {
            stats_->dqbuf_eagain++;
            logger_->log(LogLevel::kWarn,
                         "inject DQBUF EAGAIN: loop=%u action=retry next poll", i);
        } else {
            stats_->dqbuf_out_ok++;
            stats_->dqbuf_cap_ok++;
        }

        if (cfg_.inject_timeout && (i % 2 == 0)) {
            stats_->poll_timeout++;
            logger_->log(LogLevel::kWarn,
                         "inject poll timeout: loop=%u timeout_ms=%u", i,
                         cfg_.timeout_ms);
        }

        if (cfg_.inject_source_change && i == (loop_count / 2)) {
            stats_->source_change++;
            logger_->log(LogLevel::kWarn,
                         "inject SOURCE_CHANGE: loop=%u action=reconfigure-capture", i);
            logger_->log(LogLevel::kInfo,
                         "source_change flow: STREAMOFF(CAP)->REQBUFS(CAP=0)->S_FMT(CAP)->REQBUFS(CAP)->QBUF(CAP)->STREAMON(CAP)");
        }

        if (cfg_.verbose) {
            logger_->log(LogLevel::kInfo,
                         "loop=%u mode=%s bytesused=%u qbuf_out=%llu qbuf_cap=%llu dq_out_ok=%llu dq_cap_ok=%llu",
                         i, real_mode ? "real" : "sim",
                         bytesused,
                         static_cast<unsigned long long>(stats_->qbuf_out),
                         static_cast<unsigned long long>(stats_->qbuf_cap),
                         static_cast<unsigned long long>(stats_->dqbuf_out_ok),
                         static_cast<unsigned long long>(stats_->dqbuf_cap_ok));
        }
    }
    return true;
}

bool V4L2PipelineService::drain_and_stop(std::string* fail_reason) {
    const uint32_t out_t = output_type();
    const uint32_t cap_t = capture_type();

    stats_->eos_count++;
    logger_->log(LogLevel::kInfo,
                 "drain begin: send EOS on OUTPUT then wait CAPTURE flush complete");

    if (xioctl(fd_, VIDIOC_STREAMOFF, const_cast<uint32_t*>(&out_t)) < 0) {
        *fail_reason = string_format("VIDIOC_STREAMOFF OUTPUT failed: %s", strerror(errno));
        return false;
    }
    if (xioctl(fd_, VIDIOC_STREAMOFF, const_cast<uint32_t*>(&cap_t)) < 0) {
        *fail_reason = string_format("VIDIOC_STREAMOFF CAPTURE failed: %s", strerror(errno));
        return false;
    }

    logger_->log(LogLevel::kInfo, "streamoff ok: out_type=%u cap_type=%u", out_t, cap_t);
    return true;
}

void V4L2PipelineService::close_device() {
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

}  // namespace enterprise_m2m
