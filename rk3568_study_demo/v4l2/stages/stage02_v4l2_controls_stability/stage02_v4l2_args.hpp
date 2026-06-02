#ifndef STAGE02_V4L2_ARGS_HPP_
#define STAGE02_V4L2_ARGS_HPP_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "stage02_v4l2_types.hpp"

namespace stage02_v4l2 {

/*
===============================================================================
Stage02 Argument Parsing
===============================================================================

相比 Stage01，本阶段参数更多，核心是三类：
1) controls 类：--list-ctrls / --set-ctrl
2) 队列与写盘类：--queue-depth / --queue-policy / --writer-delay-ms
3) 稳定性类：--duration-sec / --recover-on-timeout / --max-recoveries

学习重点：
1) 先看默认值（init_default_config）；
2) 再看 parse_args 的选项分发；
3) 最后看尾部校验如何把非法配置提前拦截。
*/

// starts_with:
//   判断字符串是否以某前缀开头。
//
// 用途：
// - 解析 `--key=value` 风格参数；
// - 避免写大量繁琐的 substr 比较代码。
inline bool starts_with(const std::string& s, const char* prefix) {
    size_t n = strlen(prefix);
    return s.size() >= n && s.compare(0, n, prefix) == 0;
}

// parse_int:
//   把字符串解析为 int，并做基础合法性检查。
//
// 返回 false 的常见原因：
// 1) 空串
// 2) 包含非数字字符
// 3) 溢出 int 范围
inline bool parse_int(const char* s, int* out) {
    if (!s || !*s) return false;
    char* end = NULL;
    long v = strtol(s, &end, 10);
    if (!end || *end != '\0') return false;
    if (v < -2147483647L || v > 2147483647L) return false;
    *out = (int)v;
    return true;
}

// parse_ctrl_kv:
//   解析 `KEY=VAL` 形式的控制项设置参数。
//
// 示例：
// - brightness=128
// - 0x00980900=1
//
// 注意：
// - 这里只做语法解析，不判断 key 是否存在；
// - key 存在性校验在 controls 模块里做。
inline bool parse_ctrl_kv(const std::string& kv, ControlSetRequest* out) {
    if (!out) return false;
    size_t p = kv.find('=');
    if (p == std::string::npos || p == 0 || p + 1 >= kv.size()) return false;
    std::string key = kv.substr(0, p);
    std::string val = kv.substr(p + 1);
    // value 目前按 int 解析，覆盖绝大多数标准 V4L2 控件。
    int v = 0;
    if (!parse_int(val.c_str(), &v)) return false;
    out->key = key;
    out->value = v;
    return true;
}

// init_default_config:
//   初始化 Stage2 默认配置。
//
// 默认值设计理念：
// 1) 保守稳定（640x480, 30fps, reqbufs=4）
// 2) 可直接运行（不需要额外参数也能起）
// 3) 兼顾教学（提供 queue/recovery 可调开关）
inline void init_default_config(AppConfig* cfg) {
    cfg->dev = "/dev/video10";
    cfg->req_width = 640;
    cfg->req_height = 480;
    cfg->req_pixfmt = "YUYV";
    cfg->req_fps = 30;
    cfg->timeout_ms = 2000;
    cfg->req_buf_count = 4;
    cfg->duration_sec = 0;
    cfg->total_frames = 600;
    cfg->list_ctrls = false;
    cfg->set_ctrls.clear();
    cfg->queue_depth = 64;
    cfg->queue_policy = "drop-oldest";
    cfg->writer_delay_ms = 0;
    cfg->no_save = false;
    cfg->dump_every = 0;
    cfg->out_dir = "../../../artifacts/stage02_outputs";
    cfg->log_every = 100;
    cfg->recover_on_timeout = true;
    cfg->max_recoveries = 3;
}

// print_usage:
//   打印帮助信息。
//
// 建议学习路径：
// 1) 先 `--list-ctrls`
// 2) 再 `--set-ctrl=...`
// 3) 再跑 `--duration-sec` 稳定性测试
// 4) 最后做 queue/backpressure 对照实验
inline void print_usage(const char* prog) {
    fprintf(stderr,
            "Usage:\n"
            "  %s [dev] [width] [height] [options]\n\n"
            "Positionals:\n"
            "  dev       default: /dev/video10\n"
            "  width     default: 640\n"
            "  height    default: 480\n\n"
            "Options:\n"
            "  --pixfmt=FOURCC          default: YUYV\n"
            "  --fps=N                  default: 30\n"
            "  --timeout-ms=N           default: 2000\n"
            "  --req-bufs=N             default: 4\n"
            "  --duration-sec=N         stop by duration (prefer stability run)\n"
            "  --frames=N               stop by frame count when duration-sec=0\n"
            "  --list-ctrls             list controls and exit\n"
            "  --set-ctrl=KEY=VAL       can repeat (exposure_auto=1 etc.)\n"
            "  --queue-depth=N          default: 64\n"
            "  --queue-policy=MODE      MODE: drop-oldest|block\n"
            "  --writer-delay-ms=N      simulate slow writer\n"
            "  --dump-every=N           dump every Nth frame to raw file\n"
            "  --out-dir=DIR            default: ../../../artifacts/stage02_outputs\n"
            "  --no-save                disable writer thread file dump\n"
            "  --log-every=N            default: 100\n"
            "  --recover-on-timeout=0|1 default: 1\n"
            "  --max-recoveries=N       default: 3\n"
            "  -h, --help\n\n"
            "Examples:\n"
            "  %s --list-ctrls\n"
            "  %s /dev/video0 640 480 --duration-sec=60 --dump-every=120\n"
            "  %s /dev/video0 1280 720 --set-ctrl=brightness=128 --set-ctrl=contrast=64\n"
            "  %s /dev/video0 640 480 --duration-sec=120 --queue-depth=8 --queue-policy=drop-oldest --writer-delay-ms=20\n",
            prog, prog, prog, prog, prog);
}

// parse_args:
//   命令行解析入口。
//
// 设计原则：
// 1) 位置参数短，便于常用运行；
// 2) 复杂行为用长选项配置；
// 3) 参数校验尽量前置，避免运行中才发现配置无效。
//
// 返回：
// - true: 解析成功
// - false: 参数错误或用户请求 help
inline bool parse_args(int argc, char** argv, AppConfig* cfg, bool* show_help) {
    init_default_config(cfg);
    if (show_help) *show_help = false;

    int positional_idx = 0;
    for (int i = 1; i < argc; ++i) {
        std::string a(argv[i]);
        if (a == "-h" || a == "--help") {
            if (show_help) *show_help = true;
            return false;
        }

        // 先处理长选项，减少位置参数误判。
        if (starts_with(a, "--")) {
            if (starts_with(a, "--pixfmt=")) {
                cfg->req_pixfmt = a.substr(9);
            } else if (starts_with(a, "--fps=")) {
                int v = 0;
                if (!parse_int(a.c_str() + 6, &v)) return false;
                cfg->req_fps = v;
            } else if (starts_with(a, "--timeout-ms=")) {
                int v = 0;
                if (!parse_int(a.c_str() + 13, &v)) return false;
                cfg->timeout_ms = v;
            } else if (starts_with(a, "--req-bufs=")) {
                int v = 0;
                if (!parse_int(a.c_str() + 11, &v)) return false;
                if (v > 0) cfg->req_buf_count = (unsigned int)v;
            } else if (starts_with(a, "--duration-sec=")) {
                int v = 0;
                if (!parse_int(a.c_str() + 15, &v)) return false;
                // duration_sec>0 时，采集循环以“绝对时间截止”退出。
                cfg->duration_sec = v;
            } else if (starts_with(a, "--frames=")) {
                int v = 0;
                if (!parse_int(a.c_str() + 9, &v)) return false;
                // duration_sec==0 时，frames 才是主停止条件。
                cfg->total_frames = v;
            } else if (a == "--list-ctrls") {
                cfg->list_ctrls = true;
            } else if (starts_with(a, "--set-ctrl=")) {
                ControlSetRequest r;
                if (!parse_ctrl_kv(a.substr(11), &r)) {
                    fprintf(stderr, "invalid --set-ctrl format: %s\n", a.c_str());
                    return false;
                }
                cfg->set_ctrls.push_back(r);
            } else if (starts_with(a, "--queue-depth=")) {
                int v = 0;
                if (!parse_int(a.c_str() + 14, &v)) return false;
                cfg->queue_depth = v;
            } else if (starts_with(a, "--queue-policy=")) {
                // 队列策略是本阶段重点：
                // drop-oldest 更偏实时，block 更偏完整。
                cfg->queue_policy = a.substr(15);
            } else if (starts_with(a, "--writer-delay-ms=")) {
                int v = 0;
                if (!parse_int(a.c_str() + 18, &v)) return false;
                cfg->writer_delay_ms = v;
            } else if (starts_with(a, "--dump-every=")) {
                int v = 0;
                if (!parse_int(a.c_str() + 13, &v)) return false;
                cfg->dump_every = v;
            } else if (starts_with(a, "--out-dir=")) {
                cfg->out_dir = a.substr(10);
            } else if (a == "--no-save") {
                cfg->no_save = true;
            } else if (starts_with(a, "--log-every=")) {
                int v = 0;
                if (!parse_int(a.c_str() + 12, &v)) return false;
                cfg->log_every = v;
            } else if (starts_with(a, "--recover-on-timeout=")) {
                int v = 0;
                if (!parse_int(a.c_str() + 21, &v)) return false;
                // 非0视为 true，兼容 1/0 风格脚本参数。
                cfg->recover_on_timeout = (v != 0);
            } else if (starts_with(a, "--max-recoveries=")) {
                int v = 0;
                if (!parse_int(a.c_str() + 17, &v)) return false;
                cfg->max_recoveries = v;
            } else {
                fprintf(stderr, "unknown option: %s\n", a.c_str());
                return false;
            }
            continue;
        }

        // 再处理位置参数：
        // dev width height
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
            default:
                fprintf(stderr, "too many positional args: %s\n", a.c_str());
                return false;
        }
        positional_idx++;
    }

    // 基础范围校验，提前拦截明显无效配置。
    if (cfg->req_width <= 0 || cfg->req_height <= 0 || cfg->req_fps <= 0 || cfg->timeout_ms <= 0) {
        fprintf(stderr, "invalid numeric arguments\n");
        return false;
    }
    // 稳定性与队列相关参数的边界校验。
    if (cfg->duration_sec < 0 || cfg->total_frames <= 0 || cfg->queue_depth <= 0 || cfg->max_recoveries < 0) {
        fprintf(stderr, "invalid duration/frames/queue/recovery arguments\n");
        return false;
    }
    // 策略枚举校验，避免拼写错误导致行为不可预期。
    if (cfg->queue_policy != "drop-oldest" && cfg->queue_policy != "block") {
        fprintf(stderr, "invalid --queue-policy: %s\n", cfg->queue_policy.c_str());
        return false;
    }
    if (cfg->log_every <= 0) cfg->log_every = 1;
    // dump_every<0 / writer_delay_ms<0 没有业务意义，归零处理。
    if (cfg->dump_every < 0) cfg->dump_every = 0;
    if (cfg->writer_delay_ms < 0) cfg->writer_delay_ms = 0;
    // fourcc 固定 4 字节，必须严格校验。
    if (cfg->req_pixfmt.size() != 4) {
        fprintf(stderr, "--pixfmt must be 4 chars, got=%s\n", cfg->req_pixfmt.c_str());
        return false;
    }
    return true;
}

}  // namespace stage02_v4l2

#endif  // STAGE02_V4L2_ARGS_HPP_
