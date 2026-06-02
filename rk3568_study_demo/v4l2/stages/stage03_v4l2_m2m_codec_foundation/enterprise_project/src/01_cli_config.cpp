#include "02_cli_config.hpp"

#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

namespace enterprise_m2m {
namespace {

bool parse_u32(const char* s, uint32_t* out) {
    if (s == nullptr || *s == '\0') {
        return false;
    }
    const long v = strtol(s, nullptr, 10);
    if (v < 0) {
        return false;
    }
    *out = static_cast<uint32_t>(v);
    return true;
}

bool parse_bool(const char* s, bool* out) {
    if (s == nullptr) {
        return false;
    }
    if (strcmp(s, "1") == 0 || strcmp(s, "true") == 0 || strcmp(s, "yes") == 0) {
        *out = true;
        return true;
    }
    if (strcmp(s, "0") == 0 || strcmp(s, "false") == 0 || strcmp(s, "no") == 0) {
        *out = false;
        return true;
    }
    return false;
}

bool parse_kv(const char* arg, const char* key, std::string* out) {
    const size_t n = strlen(key);
    if (strncmp(arg, key, n) != 0 || arg[n] != '=') {
        return false;
    }
    *out = arg + n + 1;
    return true;
}

bool parse_fourcc(const std::string& s, uint32_t* out) {
    if (s.size() != 4) {
        return false;
    }
    *out = v4l2_fourcc(s[0], s[1], s[2], s[3]);
    return true;
}

}  // namespace

void print_usage(const char* prog) {
    printf("%s: enterprise stage03 pipeline service\n", prog);
    printf("Usage:\n");
    printf("  %s [--dev=/dev/video0] [--in-fourcc=H264] [--out-fourcc=NV12]\\\n", prog);
    printf("     [--width=1280] [--height=720] [--out-count=4] [--cap-count=4]\\\n");
    printf("     [--timeout-ms=200] [--loops=8] [--mplane=0|1] [--verbose]\\\n");
    printf("     [--output-bytesused=16] [--input-annexb=./samples/sample.h264]\\\n");
    printf("     [--max-input-chunks=0]\\\n");
    printf("     [--inject-timeout=0|1] [--inject-source-change=0|1] [--inject-dqbuf-eagain=0|1]\\\n");
    printf("     [--log-dir=./logs/run_xxx]\n");
}

bool parse_cli(int argc, char** argv, PipelineConfig* cfg) {
    if (cfg == nullptr) {
        return false;
    }

    cfg->in_fourcc = v4l2_fourcc('H', '2', '6', '4');
    cfg->out_fourcc = v4l2_fourcc('N', 'V', '1', '2');

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return false;
        }
        if (strcmp(argv[i], "--verbose") == 0) {
            cfg->verbose = true;
            continue;
        }

        std::string v;
        if (parse_kv(argv[i], "--dev", &v)) {
            cfg->dev = v;
            continue;
        }
        if (parse_kv(argv[i], "--in-fourcc", &v)) {
            if (!parse_fourcc(v, &cfg->in_fourcc)) {
                fprintf(stderr, "invalid --in-fourcc: %s\n", v.c_str());
                return false;
            }
            continue;
        }
        if (parse_kv(argv[i], "--out-fourcc", &v)) {
            if (!parse_fourcc(v, &cfg->out_fourcc)) {
                fprintf(stderr, "invalid --out-fourcc: %s\n", v.c_str());
                return false;
            }
            continue;
        }
        if (parse_kv(argv[i], "--width", &v)) {
            if (!parse_u32(v.c_str(), &cfg->width)) {
                return false;
            }
            continue;
        }
        if (parse_kv(argv[i], "--height", &v)) {
            if (!parse_u32(v.c_str(), &cfg->height)) {
                return false;
            }
            continue;
        }
        if (parse_kv(argv[i], "--out-count", &v)) {
            if (!parse_u32(v.c_str(), &cfg->out_count)) {
                return false;
            }
            continue;
        }
        if (parse_kv(argv[i], "--cap-count", &v)) {
            if (!parse_u32(v.c_str(), &cfg->cap_count)) {
                return false;
            }
            continue;
        }
        if (parse_kv(argv[i], "--timeout-ms", &v)) {
            if (!parse_u32(v.c_str(), &cfg->timeout_ms)) {
                return false;
            }
            continue;
        }
        if (parse_kv(argv[i], "--loops", &v)) {
            if (!parse_u32(v.c_str(), &cfg->loops)) {
                return false;
            }
            continue;
        }
        if (parse_kv(argv[i], "--output-bytesused", &v)) {
            if (!parse_u32(v.c_str(), &cfg->output_bytesused)) {
                return false;
            }
            continue;
        }
        if (parse_kv(argv[i], "--input-annexb", &v)) {
            cfg->input_annexb = v;
            continue;
        }
        if (parse_kv(argv[i], "--max-input-chunks", &v)) {
            if (!parse_u32(v.c_str(), &cfg->max_input_chunks)) {
                return false;
            }
            continue;
        }
        if (parse_kv(argv[i], "--mplane", &v)) {
            bool b = false;
            if (!parse_bool(v.c_str(), &b)) {
                return false;
            }
            cfg->mplane = b;
            continue;
        }
        if (parse_kv(argv[i], "--inject-timeout", &v)) {
            bool b = false;
            if (!parse_bool(v.c_str(), &b)) {
                return false;
            }
            cfg->inject_timeout = b;
            continue;
        }
        if (parse_kv(argv[i], "--inject-source-change", &v)) {
            bool b = false;
            if (!parse_bool(v.c_str(), &b)) {
                return false;
            }
            cfg->inject_source_change = b;
            continue;
        }
        if (parse_kv(argv[i], "--inject-dqbuf-eagain", &v)) {
            bool b = false;
            if (!parse_bool(v.c_str(), &b)) {
                return false;
            }
            cfg->inject_dqbuf_eagain = b;
            continue;
        }
        if (parse_kv(argv[i], "--log-dir", &v)) {
            cfg->log_dir = v;
            continue;
        }

        fprintf(stderr, "unknown arg: %s\n", argv[i]);
        return false;
    }

    return true;
}

}  // namespace enterprise_m2m
