#include "01_cli_config.hpp"

namespace stage06_enterprise {

namespace {

bool starts_with(const std::string& text, const std::string& prefix) {
    return text.compare(0, prefix.size(), prefix) == 0;
}

std::string value_after(const std::string& arg, const std::string& key) {
    return arg.substr(key.size() + 1);
}

int to_int(const std::string& text, int def) {
    if (text.empty()) {
        return def;
    }
    return atoi(text.c_str());
}

}  // namespace

bool parse_cli(int argc, char** argv, CliConfig* config, std::string* error) {
    if (config == NULL) {
        return false;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            exit(0);
        } else if (starts_with(arg, "--device=")) {
            config->device = value_after(arg, "--device");
        } else if (starts_with(arg, "--mode=")) {
            config->mode = value_after(arg, "--mode");
        } else if (starts_with(arg, "--output-dir=")) {
            config->output_dir = value_after(arg, "--output-dir");
        } else if (starts_with(arg, "--inject=")) {
            config->inject = value_after(arg, "--inject");
        } else if (starts_with(arg, "--input=")) {
            config->input = value_after(arg, "--input");
        } else if (starts_with(arg, "--decoder=")) {
            config->decoder = value_after(arg, "--decoder");
        } else if (starts_with(arg, "--frames=")) {
            config->frames = to_int(value_after(arg, "--frames"), config->frames);
        } else if (starts_with(arg, "--output-depth=")) {
            config->output_depth = to_int(value_after(arg, "--output-depth"), config->output_depth);
        } else if (starts_with(arg, "--capture-depth=")) {
            config->capture_depth = to_int(value_after(arg, "--capture-depth"), config->capture_depth);
        } else if (starts_with(arg, "--timeout-at=")) {
            config->timeout_at = to_int(value_after(arg, "--timeout-at"), config->timeout_at);
        } else if (starts_with(arg, "--source-change-at=")) {
            config->source_change_at = to_int(value_after(arg, "--source-change-at"), config->source_change_at);
        } else if (starts_with(arg, "--bytesused-zero-at=")) {
            config->bytesused_zero_at = to_int(value_after(arg, "--bytesused-zero-at"), config->bytesused_zero_at);
        } else if (starts_with(arg, "--min-decoded-frames=")) {
            config->min_decoded_frames = to_int(value_after(arg, "--min-decoded-frames"), config->min_decoded_frames);
        } else if (starts_with(arg, "--allowed-timeouts=")) {
            config->allowed_timeouts = to_int(value_after(arg, "--allowed-timeouts"), config->allowed_timeouts);
        } else if (starts_with(arg, "--width=")) {
            config->width = static_cast<uint32_t>(to_int(value_after(arg, "--width"), config->width));
        } else if (starts_with(arg, "--height=")) {
            config->height = static_cast<uint32_t>(to_int(value_after(arg, "--height"), config->height));
        } else if (starts_with(arg, "--timeout-ms=")) {
            config->timeout_ms = to_int(value_after(arg, "--timeout-ms"), config->timeout_ms);
        } else if (starts_with(arg, "--output-fourcc=")) {
            if (!parse_fourcc(value_after(arg, "--output-fourcc"), &config->output_fourcc)) {
                if (error) *error = "--output-fourcc must be 4 chars";
                return false;
            }
        } else if (starts_with(arg, "--capture-fourcc=")) {
            if (!parse_fourcc(value_after(arg, "--capture-fourcc"), &config->capture_fourcc)) {
                if (error) *error = "--capture-fourcc must be 4 chars";
                return false;
            }
        } else if (arg == "--require-device") {
            config->require_device = true;
        } else if (arg == "--no-require-device") {
            config->require_device = false;
        } else if (arg == "--require-rkmpp") {
            config->require_rkmpp = true;
        } else if (arg == "--no-recover") {
            config->recover = false;
        } else if (arg == "--quiet") {
            config->verbose = false;
        } else {
            if (error) *error = "unknown argument: " + arg;
            return false;
        }
    }

    /*
     * 故障注入开关提供更贴近工作的复现场景。
     * CLI 支持直接指定 --timeout-at 等，也支持 --inject=xxx 的快速模式。
     */
    if (config->inject == "timeout") {
        config->timeout_at = config->timeout_at < 0 ? 3 : config->timeout_at;
        config->allowed_timeouts = config->recover ? 1 : 0;
    } else if (config->inject == "bytesused_zero") {
        config->bytesused_zero_at = config->bytesused_zero_at < 0 ? 2 : config->bytesused_zero_at;
    } else if (config->inject == "source_change") {
        config->source_change_at = config->source_change_at < 0 ? 3 : config->source_change_at;
    } else if (config->inject == "source_change_no_reconfigure") {
        config->source_change_at = config->source_change_at < 0 ? 3 : config->source_change_at;
        config->recover = false;
    } else if (config->inject == "unsupported_format") {
        config->output_fourcc = V4L2_PIX_FMT_H264;
        config->min_decoded_frames = 0;
    } else if (config->inject != "none") {
        if (error) *error = "unsupported --inject value: " + config->inject;
        return false;
    }

    if (config->mode != "vm-vim2m" && config->mode != "rk-rkmpp") {
        if (error) *error = "--mode must be vm-vim2m or rk-rkmpp";
        return false;
    }
    if (config->frames <= 0 || config->output_depth <= 0 || config->capture_depth <= 0) {
        if (error) *error = "frames/output-depth/capture-depth must be positive";
        return false;
    }
    if (config->min_decoded_frames < 0 || config->allowed_timeouts < 0 || config->timeout_ms < 0) {
        if (error) *error = "min-decoded-frames/allowed-timeouts/timeout-ms must not be negative";
        return false;
    }
    return true;
}

void print_usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [options]\n"
              << "  --mode=vm-vim2m|rk-rkmpp\n"
              << "  --device=/dev/video0\n"
              << "  --output-dir=logs/run_default\n"
              << "  --inject=none|timeout|bytesused_zero|source_change|source_change_no_reconfigure|unsupported_format\n"
              << "  --frames=12 --output-depth=3 --capture-depth=4\n"
              << "  --timeout-at=5 --source-change-at=4 --bytesused-zero-at=3\n"
              << "  --timeout-ms=1000 --min-decoded-frames=4 --allowed-timeouts=0\n"
              << "  --output-fourcc=RGBP --capture-fourcc=RGBP --width=640 --height=480\n"
              << "  --input=/path/to/input.h264 --decoder=h264_rkmpp --require-rkmpp\n"
              << "  --require-device --no-require-device --no-recover --quiet\n";
}

std::string config_summary(const CliConfig& c) {
    std::ostringstream oss;
    oss << "mode=" << c.mode
        << ", device=" << c.device
        << ", output_dir=" << c.output_dir
        << ", inject=" << c.inject
        << ", input=" << (c.input.empty() ? "(none)" : c.input)
        << ", decoder=" << c.decoder
        << ", output_fourcc=" << fourcc_to_string(c.output_fourcc)
        << ", capture_fourcc=" << fourcc_to_string(c.capture_fourcc)
        << ", size=" << c.width << "x" << c.height
        << ", frames=" << c.frames
        << ", output_depth=" << c.output_depth
        << ", capture_depth=" << c.capture_depth
        << ", timeout_at=" << c.timeout_at
        << ", source_change_at=" << c.source_change_at
        << ", bytesused_zero_at=" << c.bytesused_zero_at
        << ", timeout_ms=" << c.timeout_ms
        << ", recover=" << yes_no(c.recover)
        << ", require_device=" << yes_no(c.require_device)
        << ", require_rkmpp=" << yes_no(c.require_rkmpp);
    return oss.str();
}

}  // namespace stage06_enterprise
