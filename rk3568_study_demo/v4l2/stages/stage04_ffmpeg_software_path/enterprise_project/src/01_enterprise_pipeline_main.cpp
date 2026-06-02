#include "00_enterprise_common.hpp"
#include "01_pipeline_types.hpp"

#include <errno.h>
#include <sys/stat.h>

#include <fstream>

namespace stage04_enterprise {

bool ensure_dir(const std::string& dir) {
    if (mkdir(dir.c_str(), 0755) == 0 || errno == EEXIST) {
        return true;
    }
    return false;
}

void log_line(std::ofstream& ofs, const std::string& s) {
    ofs << s << "\n";
    printf("%s\n", s.c_str());
}

void transit(PipelineState* cur, PipelineState next, Metrics* m, std::ofstream& ofs,
             const char* reason) {
    std::string line = std::string("[state] ") + state_to_cstr(*cur) + " -> " +
                       state_to_cstr(next) + " reason=" + (reason ? reason : "-");
    log_line(ofs, line);
    *cur = next;
    m->state_transition++;
}

bool write_metrics_json(const std::string& path, const Config& cfg, const Metrics& m,
                        bool pass) {
    std::ofstream ofs(path.c_str());
    if (!ofs.is_open()) {
        return false;
    }
    ofs << "{\n";
    ofs << "  \"pass\": " << (pass ? "true" : "false") << ",\n";
    ofs << "  \"input\": \"" << cfg.input << "\",\n";
    ofs << "  \"max_frames\": " << cfg.max_frames << ",\n";
    ofs << "  \"inject_send_fail\": " << (cfg.inject_send_fail ? "true" : "false")
        << ",\n";
    ofs << "  \"inject_receive_fail\": "
        << (cfg.inject_receive_fail ? "true" : "false") << ",\n";
    ofs << "  \"packet_in\": " << m.packet_in << ",\n";
    ofs << "  \"frame_out\": " << m.frame_out << ",\n";
    ofs << "  \"error_count\": " << m.error_count << ",\n";
    ofs << "  \"state_transition\": " << m.state_transition << ",\n";
    ofs << "  \"elapsed_s\": " << m.elapsed_s << ",\n";
    ofs << "  \"fps\": " << m.fps << "\n";
    ofs << "}\n";
    return true;
}

bool parse_args(int argc, char** argv, Config* cfg) {
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--input=", 8) == 0) {
            cfg->input = argv[i] + 8;
        } else if (strncmp(argv[i], "--log-dir=", 10) == 0) {
            cfg->log_dir = argv[i] + 10;
        } else if (strncmp(argv[i], "--max-frames=", 13) == 0) {
            cfg->max_frames = atoi(argv[i] + 13);
        } else if (strcmp(argv[i], "--inject-send-fail") == 0) {
            cfg->inject_send_fail = true;
        } else if (strcmp(argv[i], "--inject-receive-fail") == 0) {
            cfg->inject_receive_fail = true;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            cfg->verbose = true;
        }
    }
    return !cfg->input.empty();
}

}  // namespace stage04_enterprise

int main(int argc, char** argv) {
    using namespace stage04_enterprise;

    Config cfg;
    if (!parse_args(argc, argv, &cfg)) {
        fprintf(stderr,
                "Usage: --input=PATH [--log-dir=DIR] [--max-frames=120] [--inject-send-fail] [--inject-receive-fail] [--verbose]\n");
        return 1;
    }

    if (!ensure_dir(cfg.log_dir)) {
        fprintf(stderr, "mkdir failed: %s\n", cfg.log_dir.c_str());
        return 2;
    }

    std::ofstream log_ofs((cfg.log_dir + "/enterprise_pipeline.log").c_str());
    if (!log_ofs.is_open()) {
        return 3;
    }

    Metrics m;
    PipelineState st = PipelineState::kInit;

    AVFormatContext* fmt = nullptr;
    AVCodecContext* dec_ctx = nullptr;
    AVPacket* pkt = nullptr;
    AVFrame* frame = nullptr;

    int ret = 0;
    int video_idx = -1;

    const int64_t t0 = av_gettime_relative();

    ret = avformat_open_input(&fmt, cfg.input.c_str(), nullptr, nullptr);
    if (ret < 0) {
        m.error_count++;
        goto fail;
    }
    transit(&st, PipelineState::kInputOpened, &m, log_ofs, "open input ok");

    ret = avformat_find_stream_info(fmt, nullptr);
    if (ret < 0) {
        m.error_count++;
        goto fail;
    }
    video_idx = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_idx < 0) {
        m.error_count++;
        goto fail;
    }
    transit(&st, PipelineState::kStreamReady, &m, log_ofs, "stream info ready");

    {
        const AVCodec* dec = avcodec_find_decoder(fmt->streams[video_idx]->codecpar->codec_id);
        dec_ctx = avcodec_alloc_context3(dec);
        avcodec_parameters_to_context(dec_ctx, fmt->streams[video_idx]->codecpar);
        ret = avcodec_open2(dec_ctx, dec, nullptr);
        if (ret < 0) {
            m.error_count++;
            goto fail;
        }
    }
    transit(&st, PipelineState::kDecoderReady, &m, log_ofs, "decoder open ok");

    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    if (pkt == nullptr || frame == nullptr) {
        m.error_count++;
        goto fail;
    }

    transit(&st, PipelineState::kDecoding, &m, log_ofs, "decode loop start");

    while (m.frame_out < static_cast<uint64_t>(cfg.max_frames) &&
           av_read_frame(fmt, pkt) >= 0) {
        if (pkt->stream_index != video_idx) {
            av_packet_unref(pkt);
            continue;
        }

        m.packet_in++;

        if (cfg.inject_send_fail && (m.packet_in % 31 == 0)) {
            m.error_count++;
            log_line(log_ofs, "[enterprise09] inject send fail");
            av_packet_unref(pkt);
            continue;
        }

        ret = avcodec_send_packet(dec_ctx, pkt);
        av_packet_unref(pkt);
        if (ret < 0) {
            m.error_count++;
            continue;
        }

        while (true) {
            if (cfg.inject_receive_fail && (m.frame_out > 0) && (m.frame_out % 37 == 0)) {
                m.error_count++;
                log_line(log_ofs, "[enterprise09] inject receive fail");
                break;
            }

            ret = avcodec_receive_frame(dec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                m.error_count++;
                break;
            }
            m.frame_out++;
            if (cfg.verbose) {
                char line[256];
                snprintf(line, sizeof(line),
                         "[enterprise09] frame_out=%llu w=%d h=%d fmt=%d pts=%lld",
                         static_cast<unsigned long long>(m.frame_out), frame->width,
                         frame->height, frame->format,
                         static_cast<long long>(frame->pts));
                log_line(log_ofs, line);
            }
            av_frame_unref(frame);
            if (m.frame_out >= static_cast<uint64_t>(cfg.max_frames)) {
                break;
            }
        }
    }

    transit(&st, PipelineState::kCompleted, &m, log_ofs, "decode finished");

    {
        const int64_t t1 = av_gettime_relative();
        m.elapsed_s = (t1 - t0) / 1000000.0;
        m.fps = m.elapsed_s > 0.0 ? (m.frame_out / m.elapsed_s) : 0.0;
    }

    {
        char sum[256];
        snprintf(sum, sizeof(sum),
                 "[enterprise09] summary: packet_in=%llu frame_out=%llu error_count=%llu state_transition=%llu elapsed_s=%.6f fps=%.3f",
                 static_cast<unsigned long long>(m.packet_in),
                 static_cast<unsigned long long>(m.frame_out),
                 static_cast<unsigned long long>(m.error_count),
                 static_cast<unsigned long long>(m.state_transition), m.elapsed_s, m.fps);
        log_line(log_ofs, sum);
    }

    write_metrics_json(cfg.log_dir + "/enterprise_metrics.json", cfg, m, true);

    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&dec_ctx);
    if (fmt) {
        avformat_close_input(&fmt);
    }

    printf("[enterprise09] result=PASS\n");
    return 0;

fail:
    transit(&st, PipelineState::kFailed, &m, log_ofs, "pipeline failed");
    {
        const int64_t t1 = av_gettime_relative();
        m.elapsed_s = (t1 - t0) / 1000000.0;
        m.fps = m.elapsed_s > 0.0 ? (m.frame_out / m.elapsed_s) : 0.0;
    }
    write_metrics_json(cfg.log_dir + "/enterprise_metrics.json", cfg, m, false);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&dec_ctx);
    if (fmt) {
        avformat_close_input(&fmt);
    }
    printf("[enterprise09] result=FAIL\n");
    return 4;
}
