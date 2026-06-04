#include "04_metrics_sink.hpp"

namespace stage07_enterprise {

std::string json_escape(const std::string& text) {
    std::ostringstream oss;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        switch (c) {
            case '\\':
                oss << "\\\\";
                break;
            case '"':
                oss << "\\\"";
                break;
            case '\n':
                oss << "\\n";
                break;
            case '\r':
                oss << "\\r";
                break;
            case '\t':
                oss << "\\t";
                break;
            default:
                oss << c;
                break;
        }
    }
    return oss.str();
}

bool MetricsSink::write_json(const CliConfig& config,
                             const PipelineMetrics& m,
                             const std::string& state_history,
                             const std::string& output_dir) const {
    ensure_dir(output_dir);
    const std::string path = output_dir + "/enterprise_metrics.json";
    std::ostringstream oss;
    oss << "{\n"
        << "  \"mode\": \"" << json_escape(config.mode) << "\",\n"
        << "  \"scenario\": \"" << json_escape(config.scenario) << "\",\n"
        << "  \"backend_element\": \"" << json_escape(config.backend_element) << "\",\n"
        << "  \"pipeline\": \"" << json_escape(m.pipeline) << "\",\n"
        << "  \"exit_code\": " << m.exit_code << ",\n"
        << "  \"elapsed_ms\": " << m.elapsed_ms << ",\n"
        << "  \"eos_count\": " << m.eos_count << ",\n"
        << "  \"error_count\": " << m.error_count << ",\n"
        << "  \"warning_count\": " << m.warning_count << ",\n"
        << "  \"caps_mentions\": " << m.caps_mentions << ",\n"
        << "  \"link_failure_count\": " << m.link_failure_count << ",\n"
        << "  \"missing_element_count\": " << m.missing_element_count << ",\n"
        << "  \"not_negotiated_count\": " << m.not_negotiated_count << ",\n"
        << "  \"rendered_frames_hint\": " << m.rendered_frames_hint << ",\n"
        << "  \"dropped_frames_hint\": " << m.dropped_frames_hint << ",\n"
        << "  \"gst_tools_available\": " << (m.gst_tools_available ? "true" : "false") << ",\n"
        << "  \"backend_installed\": " << (m.backend_installed ? "true" : "false") << ",\n"
        << "  \"expected_failure\": " << (m.expected_failure ? "true" : "false") << ",\n"
        << "  \"gate_pass\": " << (m.gate_pass ? "true" : "false") << ",\n"
        << "  \"failure_layer\": \"" << json_escape(m.failure_layer) << "\",\n"
        << "  \"gate_reason\": \"" << json_escape(m.gate_reason) << "\",\n"
        << "  \"log_path\": \"" << json_escape(m.log_path) << "\",\n"
        << "  \"state_history\": \"" << json_escape(state_history) << "\"\n"
        << "}\n";
    return write_text_file(path, oss.str());
}

}  // namespace stage07_enterprise
