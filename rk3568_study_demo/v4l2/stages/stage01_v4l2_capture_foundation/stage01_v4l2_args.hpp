#ifndef STAGE01_V4L2_ARGS_HPP_
#define STAGE01_V4L2_ARGS_HPP_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/videodev2.h>

#include <string>

#include "stage01_v4l2_types.hpp"

namespace stage01_v4l2 {

/*
===============================================================================
Stage01 Argument Parsing
===============================================================================

本文件目标：
1) 把命令行参数转成 AppConfig；
2) 在“真正采集前”做尽量多的参数合法性检查；
3) 提供可复现、可批量执行的 CLI 入口。

为什么这一步很重要：
1) 90% 的实验失败其实是参数错误，而不是驱动 bug；
2) 前置校验能显著减少“跑一半才报错”的低效调试；
3) 清晰 CLI 是后续自动化回归的基础。
*/

// starts_with:
//   判断字符串是否以某前缀开头。
//
// 在这里主要用于识别形如 "--fps=30" 的长选项。
inline bool starts_with(const std::string& s, const char* prefix) {
    size_t n = strlen(prefix);
    return s.size() >= n && s.compare(0, n, prefix) == 0;
}

// parse_int:
//   将 C 字符串解析为 int，并做基本合法性检查。
//
// 返回：
//   true  : 解析成功，out 已写入。
//   false : 解析失败（空串、包含非数字、溢出等）。
inline bool parse_int(const char* s, int* out) {
    if (!s || !*s) return false;
    char* end = NULL;
    // strtol 能区分“解析失败”和“解析到部分”两种情况。
    long v = strtol(s, &end, 10);
    if (!end || *end != '\0') return false;
    if (v < -2147483647L || v > 2147483647L) return false;
    *out = (int)v;
    return true;
}

// default_ppm_path:
//   当用户未传 ppm 输出名时，从 raw 输出名推导。
//
// 示例：
//   frame.yuyv -> frame.ppm
//   shot       -> shot.ppm
inline std::string default_ppm_path(const std::string& raw_path) {
    std::string p = raw_path.empty() ? "frame.yuyv" : raw_path;
    // 仅替换最后一个扩展名，避免路径中间的点被误处理。
    size_t pos = p.find_last_of('.');
    if (pos == std::string::npos) {
        p += ".ppm";
    } else {
        p = p.substr(0, pos) + ".ppm";
    }
    return p;
}

inline bool parse_fourcc_text(const std::string& s, uint32_t* out) {
    if (!out) return false;
    if (s.size() != 4) return false;
    // fourcc 固定是 4 字符编码。
    // 注意：这里只校验长度，不校验字符是否可打印。
    // 真正可用性由 TRY_FMT/S_FMT 决定。
    *out = v4l2_fourcc((unsigned char)s[0], (unsigned char)s[1], (unsigned char)s[2], (unsigned char)s[3]);
    return true;
}

// init_default_config:
//   初始化默认参数。
//
// 这些默认值是“训练友好”配置，不一定是所有设备最佳值。
inline void init_default_config(AppConfig* cfg) {
    // 注意：/dev/video10 只是训练默认值，不保证你机器上存在。
    cfg->dev = "/dev/video10";
    cfg->req_width = 640;
    cfg->req_height = 480;
    cfg->out_raw = "frame.yuyv";
    cfg->out_ppm = "";
    cfg->total_frames = 300;
    cfg->warmup_frames = 3;
    cfg->req_fps = 30;
    cfg->req_pixfmt = "YUYV";
    cfg->timeout_ms = 2000;
    cfg->req_buf_count = 4;
    cfg->inject = "none";
    cfg->inject_frame = 120;
    cfg->save_preview = true;
    cfg->dump_formats = false;
    cfg->trace_csv = "";
    cfg->log_every = 50;
}

// print_usage:
//   打印命令行帮助。
inline void print_usage(const char* prog) {
    fprintf(stderr,
            "Usage:\n"
            "  %s [dev] [width] [height] [raw_out] [ppm_out] [frames] [options]\n\n"
            "Positionals (all optional, keep order):\n"
            "  dev       default: /dev/video10\n"
            "  width     default: 640\n"
            "  height    default: 480\n"
            "  raw_out   default: frame.yuyv\n"
            "  ppm_out   default: auto from raw_out\n"
            "  frames    default: 300\n\n"
            "Options:\n"
            "  --warmup=N        default: 3\n"
            "  --fps=N           default: 30\n"
            "  --pixfmt=FOURCC   default: YUYV (example: YUYV/NV12/MJPG)\n"
            "  --timeout-ms=N    default: 2000\n"
            "  --req-bufs=N      default: 4\n"
            "  --dump-formats    print ENUM_FMT/ENUM_FRAMESIZES/ENUM_FRAMEINTERVALS\n"
            "  --trace-csv=PATH  optional per-frame CSV trace output\n"
            "  --log-every=N     frame log cadence (default: 50)\n"
            "  --inject=MODE     MODE: none|bad-node|bad-fmt|skip-requeue\n"
            "  --inject-frame=N  default: 120 (for skip-requeue trigger)\n"
            "  --no-save         do not save raw/ppm preview frame\n"
            "  -h, --help\n\n"
            "Examples:\n"
            "  %s\n"
            "  %s /dev/video0 1280 720 ../artifacts/a.yuyv ../artifacts/a.ppm 300\n"
            "  %s /dev/video0 1280 720 out.yuyv out.ppm 120 --pixfmt=NV12 --dump-formats --trace-csv=trace.csv\n"
            "  %s /dev/video0 640 480 ../artifacts/b.yuyv ../artifacts/b.ppm 300 --inject=skip-requeue --inject-frame=30\n"
            "  %s /dev/video999 640 480 bad.yuyv bad.ppm 10\n",
            prog, prog, prog, prog, prog, prog);
}

// parse_args:
//   解析命令行到 AppConfig。
//
// 入参：
//   show_help: 当用户显式传 -h/--help 时置 true。
//
// 返回：
//   true  : 解析成功
//   false : 参数非法，或用户请求帮助
//
// 注意：
//   本函数会做一些“教学友好型”限制，例如 YUYV 宽度必须偶数。
inline bool parse_args(int argc, char** argv, AppConfig* cfg, bool* show_help) {
    init_default_config(cfg);
    if (show_help) *show_help = false;

    // positional_idx 用于追踪“当前正在填第几个位置参数”。
    int positional_idx = 0;
    for (int i = 1; i < argc; ++i) {
        std::string a(argv[i]);

        // 显式帮助：返回 false，让 main 统一打印 usage。
        if (a == "-h" || a == "--help") {
            if (show_help) *show_help = true;
            return false;
        }

        // 先解析 --long-options。
        if (starts_with(a, "--")) {
            // long-option 分支：参数顺序不敏感，便于脚本自动生成命令。
            if (starts_with(a, "--warmup=")) {
                int v = 0;
                if (!parse_int(a.c_str() + 9, &v)) return false;
                cfg->warmup_frames = v;
            } else if (starts_with(a, "--fps=")) {
                int v = 0;
                if (!parse_int(a.c_str() + 6, &v)) return false;
                cfg->req_fps = v;
            } else if (starts_with(a, "--pixfmt=")) {
                cfg->req_pixfmt = a.substr(9);
            } else if (starts_with(a, "--timeout-ms=")) {
                int v = 0;
                if (!parse_int(a.c_str() + 13, &v)) return false;
                cfg->timeout_ms = v;
            } else if (starts_with(a, "--req-bufs=")) {
                int v = 0;
                if (!parse_int(a.c_str() + 11, &v)) return false;
                // req-bufs<=0 没有意义，这里仅接受正值覆盖。
                if (v > 0) cfg->req_buf_count = (unsigned int)v;
            } else if (starts_with(a, "--inject=")) {
                cfg->inject = a.substr(9);
            } else if (starts_with(a, "--inject-frame=")) {
                int v = 0;
                if (!parse_int(a.c_str() + 15, &v)) return false;
                cfg->inject_frame = v;
            } else if (a == "--no-save") {
                cfg->save_preview = false;
            } else if (a == "--dump-formats") {
                cfg->dump_formats = true;
            } else if (starts_with(a, "--trace-csv=")) {
                cfg->trace_csv = a.substr(12);
            } else if (starts_with(a, "--log-every=")) {
                int v = 0;
                if (!parse_int(a.c_str() + 12, &v)) return false;
                cfg->log_every = v;
            } else {
                fprintf(stderr, "unknown option: %s\n", a.c_str());
                return false;
            }
            continue;
        }

        // 再按顺序填充位置参数。
        // 位置参数顺序固定，少传可走默认值，多传报错。
        switch (positional_idx) {
            case 0:
                cfg->dev = a;
                break;
            case 1: {
                int v = 0;
                if (!parse_int(a.c_str(), &v)) return false;
                cfg->req_width = v;
                break;
            }
            case 2: {
                int v = 0;
                if (!parse_int(a.c_str(), &v)) return false;
                cfg->req_height = v;
                break;
            }
            case 3:
                cfg->out_raw = a;
                break;
            case 4:
                cfg->out_ppm = a;
                break;
            case 5: {
                int v = 0;
                if (!parse_int(a.c_str(), &v)) return false;
                cfg->total_frames = v;
                break;
            }
            default:
                fprintf(stderr, "too many positional args, got: %s\n", a.c_str());
                return false;
        }
        positional_idx++;
    }

    // 基础范围校验。
    // 这一层属于“硬边界”校验：必须严格 > 0。
    if (cfg->req_width <= 0 || cfg->req_height <= 0 || cfg->total_frames <= 0 || cfg->req_fps <= 0 || cfg->timeout_ms <= 0) {
        fprintf(stderr, "invalid numeric argument\n");
        return false;
    }

    // fourcc 语义校验，防止传错长度导致后续协商混乱。
    uint32_t requested_fourcc = 0;
    if (!parse_fourcc_text(cfg->req_pixfmt, &requested_fourcc)) {
        fprintf(stderr, "invalid --pixfmt value (must be 4 chars): %s\n", cfg->req_pixfmt.c_str());
        return false;
    }

    // YUYV(4:2:2) 每 2 像素共享 U/V，宽度应为偶数。
    // 该限制只在请求 YUYV 时强制。
    if (requested_fourcc == V4L2_PIX_FMT_YUYV && (cfg->req_width % 2) != 0) {
        fprintf(stderr, "width must be even for YUYV 4:2:2, got=%d\n", cfg->req_width);
        return false;
    }

    // warmup 为负则归零，避免后续逻辑复杂化。
    // soft-normalize：负 warmup 没意义，归零处理。
    if (cfg->warmup_frames < 0) cfg->warmup_frames = 0;
    if (cfg->log_every <= 0) cfg->log_every = 1;

    // 故障注入模式校验。
    if (cfg->inject != "none" &&
        cfg->inject != "bad-node" &&
        cfg->inject != "bad-fmt" &&
        cfg->inject != "skip-requeue") {
        fprintf(stderr, "invalid --inject value: %s\n", cfg->inject.c_str());
        return false;
    }

    // 未传 ppm 路径则自动推导。
    if (cfg->out_ppm.empty()) {
        cfg->out_ppm = default_ppm_path(cfg->out_raw);
    }

    // bad-node 模式：直接替换成不存在节点，复现实验场景。
    if (cfg->inject == "bad-node") {
        // 这里强制改 dev，是为了让故障注入“稳定可复现”。
        cfg->dev = "/dev/video999";
    }

    return true;
}

}  // namespace stage01_v4l2

#endif  // STAGE01_V4L2_ARGS_HPP_
