#include "04_metrics_sink.hpp"

namespace stage06_enterprise {

namespace {

std::string json_escape(const std::string& text) {
    std::ostringstream oss;
    for (char ch : text) {
        if (ch == '"') {
            oss << "\\\"";
        } else if (ch == '\\') {
            oss << "\\\\";
        } else if (ch == '\n') {
            oss << "\\n";
        } else {
            oss << ch;
        }
    }
    return oss.str();
}

void write_kv(FILE* fp, const std::string& key, const std::string& value,
              bool comma) {
    fprintf(fp, "  \"%s\": \"%s\"%s\n", key.c_str(), json_escape(value).c_str(), comma ? "," : "");
}

void write_kv_num(FILE* fp, const std::string& key, int value, bool comma) {
    fprintf(fp, "  \"%s\": %d%s\n", key.c_str(), value, comma ? "," : "");
}

void write_kv_bool(FILE* fp, const std::string& key, bool value, bool comma) {
    fprintf(fp, "  \"%s\": %s%s\n", key.c_str(), value ? "true" : "false", comma ? "," : "");
}

}  // namespace

bool write_metrics_json(const std::string& path, const CliConfig& config,
                        const PipelineMetrics& m, const StateMachine& sm) {
    ensure_dir(dirname_of(path));
    FILE* fp = fopen(path.c_str(), "w");
    if (fp == NULL) {
        std::cerr << "open metrics failed: " << path << ": " << strerror(errno) << "\n";
        return false;
    }

    fprintf(fp, "{\n");
    write_kv(fp, "device", config.device, true);
    write_kv(fp, "inject", config.inject, true);
    write_kv(fp, "output_fourcc", fourcc_to_string(config.output_fourcc), true);
    write_kv(fp, "capture_fourcc", fourcc_to_string(config.capture_fourcc), true);
    write_kv_num(fp, "width", static_cast<int>(config.width), true);
    write_kv_num(fp, "height", static_cast<int>(config.height), true);
    write_kv_num(fp, "frames_requested", config.frames, true);
    write_kv_num(fp, "qbuf_output", m.qbuf_output, true);
    write_kv_num(fp, "qbuf_capture", m.qbuf_capture, true);
    write_kv_num(fp, "dqbuf_output", m.dqbuf_output, true);
    write_kv_num(fp, "dqbuf_capture", m.dqbuf_capture, true);
    write_kv_num(fp, "decoded_frames", m.decoded_frames, true);
    write_kv_num(fp, "poll_calls", m.poll_calls, true);
    write_kv_num(fp, "timeout_count", m.timeout_count, true);
    write_kv_num(fp, "bytesused_zero_count", m.bytesused_zero_count, true);
    write_kv_num(fp, "source_change_count", m.source_change_count, true);
    write_kv_num(fp, "eos_count", m.eos_count, true);
    write_kv_num(fp, "recovery_count", m.recovery_count, true);
    write_kv_num(fp, "streamoff_count", m.streamoff_count, true);
    write_kv_num(fp, "max_output_depth", m.max_output_depth, true);
    write_kv_num(fp, "max_capture_depth", m.max_capture_depth, true);
    write_kv_bool(fp, "device_opened", m.device_opened, true);
    write_kv_bool(fp, "querycap_ok", m.querycap_ok, true);
    write_kv_bool(fp, "m2m_capable", m.m2m_capable, true);
    write_kv_bool(fp, "simulated_device", m.simulated_device, true);
    write_kv_bool(fp, "gate_pass", m.gate_pass, true);
    write_kv(fp, "verdict", m.verdict, true);
    write_kv(fp, "failure_layer", m.failure_layer, true);
    write_kv(fp, "state_history", sm.history_text(), false);
    fprintf(fp, "}\n");
    fclose(fp);
    return true;
}

std::string metrics_summary_text(const PipelineMetrics& m) {
    std::ostringstream oss;
    oss << "decoded_frames=" << m.decoded_frames
        << ", qbuf_output=" << m.qbuf_output
        << ", qbuf_capture=" << m.qbuf_capture
        << ", dqbuf_output=" << m.dqbuf_output
        << ", dqbuf_capture=" << m.dqbuf_capture
        << ", poll_calls=" << m.poll_calls
        << ", timeout_count=" << m.timeout_count
        << ", bytesused_zero_count=" << m.bytesused_zero_count
        << ", m2m_capable=" << yes_no(m.m2m_capable)
        << ", source_change_count=" << m.source_change_count
        << ", recovery_count=" << m.recovery_count
        << ", gate_pass=" << yes_no(m.gate_pass)
        << ", verdict=" << m.verdict
        << ", failure_layer=" << m.failure_layer;
    return oss.str();
}

}  // namespace stage06_enterprise
