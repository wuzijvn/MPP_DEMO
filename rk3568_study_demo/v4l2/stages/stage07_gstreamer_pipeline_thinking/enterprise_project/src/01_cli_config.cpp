#include "01_cli_config.hpp"

namespace stage07_enterprise {

namespace {

bool starts_with(const std::string& text, const std::string& prefix) {
    return text.compare(0, prefix.size(), prefix) == 0;
}

std::string value_after(const std::string& arg, const std::string& key) {
    return arg.substr(key.size() + 1);
}

int to_int(const std::string& value, int def) {
    if (value.empty()) {
        return def;
    }
    return std::atoi(value.c_str());
}

long long to_ll(const std::string& value, long long def) {
    if (value.empty()) {
        return def;
    }
    return std::atoll(value.c_str());
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
            std::exit(0);
        } else if (starts_with(arg, "--mode=")) {
            config->mode = value_after(arg, "--mode");
        } else if (starts_with(arg, "--scenario=")) {
            config->scenario = value_after(arg, "--scenario");
        } else if (starts_with(arg, "--output-dir=")) {
            config->output_dir = value_after(arg, "--output-dir");
        } else if (starts_with(arg, "--backend-element=")) {
            config->backend_element = value_after(arg, "--backend-element");
        } else if (starts_with(arg, "--gst-debug=")) {
            config->gst_debug = value_after(arg, "--gst-debug");
        } else if (starts_with(arg, "--frames=")) {
            config->frames = to_int(value_after(arg, "--frames"), config->frames);
        } else if (starts_with(arg, "--width=")) {
            config->width = to_int(value_after(arg, "--width"), config->width);
        } else if (starts_with(arg, "--height=")) {
            config->height = to_int(value_after(arg, "--height"), config->height);
        } else if (starts_with(arg, "--queue-depth=")) {
            config->queue_depth = to_int(value_after(arg, "--queue-depth"), config->queue_depth);
        } else if (starts_with(arg, "--slow-us=")) {
            config->slow_us = to_int(value_after(arg, "--slow-us"), config->slow_us);
        } else if (starts_with(arg, "--min-caps-mentions=")) {
            config->min_caps_mentions = to_int(value_after(arg, "--min-caps-mentions"),
                                               config->min_caps_mentions);
        } else if (starts_with(arg, "--max-elapsed-ms=")) {
            config->max_elapsed_ms = to_ll(value_after(arg, "--max-elapsed-ms"),
                                           config->max_elapsed_ms);
        } else if (arg == "--require-backend") {
            config->require_backend = true;
        } else if (arg == "--no-require-backend") {
            config->require_backend = false;
        } else if (arg == "--expect-failure") {
            config->expect_failure = true;
        } else if (arg == "--quiet") {
            config->quiet = true;
        } else {
            if (error) {
                *error = "unknown argument: " + arg;
            }
            return false;
        }
    }

    /*
     * scenario 快捷规则：
     * - caps-failure 和 missing-element 是预期失败；
     * - slow-queue 观察 latency；
     * - hardware-probe 不一定运行真实硬解，只验证 element 候选和证据边界。
     */
    if (config->scenario == "caps-failure" || config->scenario == "missing-element") {
        config->expect_failure = true;
    } else if (config->scenario == "slow-queue" && config->slow_us <= 0) {
        config->slow_us = 3000;
    } else if (config->scenario == "hardware-probe") {
        config->require_backend = true;
    }

    if (config->mode != "raw-basic" && config->mode != "debug-caps" &&
        config->mode != "hardware-candidate") {
        if (error) {
            *error = "--mode must be raw-basic, debug-caps, or hardware-candidate";
        }
        return false;
    }
    if (config->scenario != "normal" && config->scenario != "caps-failure" &&
        config->scenario != "missing-element" && config->scenario != "slow-queue" &&
        config->scenario != "hardware-probe") {
        if (error) {
            *error = "--scenario must be normal, caps-failure, missing-element, slow-queue, or hardware-probe";
        }
        return false;
    }
    if (config->frames <= 0 || config->width <= 0 || config->height <= 0 ||
        config->queue_depth <= 0 || config->max_elapsed_ms <= 0) {
        if (error) {
            *error = "frames/width/height/queue-depth/max-elapsed-ms must be positive";
        }
        return false;
    }
    return true;
}

void print_usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [options]\n"
              << "  --mode=raw-basic|debug-caps|hardware-candidate\n"
              << "  --scenario=normal|caps-failure|missing-element|slow-queue|hardware-probe\n"
              << "  --output-dir=logs/enterprise_default\n"
              << "  --backend-element=avdec_h264_rkmpp --require-backend\n"
              << "  --frames=30 --width=320 --height=240 --queue-depth=4 --slow-us=3000\n"
              << "  --gst-debug=GST_CAPS:3,GST_ELEMENT_PADS:3,pipeline:3\n"
              << "  --min-caps-mentions=0 --max-elapsed-ms=20000 --expect-failure --quiet\n";
}

std::string config_summary(const CliConfig& c) {
    std::ostringstream oss;
    oss << "mode=" << c.mode
        << ", scenario=" << c.scenario
        << ", output_dir=" << c.output_dir
        << ", backend_element=" << c.backend_element
        << ", frames=" << c.frames
        << ", size=" << c.width << "x" << c.height
        << ", queue_depth=" << c.queue_depth
        << ", slow_us=" << c.slow_us
        << ", require_backend=" << yes_no(c.require_backend)
        << ", expect_failure=" << yes_no(c.expect_failure)
        << ", max_elapsed_ms=" << c.max_elapsed_ms;
    return oss.str();
}

}  // namespace stage07_enterprise
