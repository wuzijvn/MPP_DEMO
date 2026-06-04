#ifndef STAGE07_ENTERPRISE_COMMON_HPP_
#define STAGE07_ENTERPRISE_COMMON_HPP_

#include <sys/stat.h>
#include <sys/wait.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace stage07_enterprise {

/*
 * 企业项目公共定义。
 *
 * 本项目模拟真实 SoC codec stack bring-up 中常见的 GStreamer 诊断服务：
 * - 生成 pipeline；
 * - 运行 gst-launch-1.0；
 * - 收集 stdout/stderr/GST_DEBUG；
 * - 解析 caps/link/error/EOS/fps 等证据；
 * - 输出 JSON metrics 和 gate 结论。
 *
 * 教学简化：
 * - 不使用 GStreamer C API，也不开发 plugin。
 * - 通过命令行工具建立“能复现、能分类、能汇报”的工作能力。
 */

struct CommandResult {
    std::string command;
    std::string output;
    int exit_code = -1;
    long long elapsed_ms = 0;
};

struct PipelineMetrics {
    std::string mode;
    std::string scenario;
    std::string backend_element;
    std::string pipeline;
    int exit_code = -1;
    long long elapsed_ms = 0;
    int eos_count = 0;
    int error_count = 0;
    int warning_count = 0;
    int caps_mentions = 0;
    int link_failure_count = 0;
    int missing_element_count = 0;
    int not_negotiated_count = 0;
    int rendered_frames_hint = 0;
    int dropped_frames_hint = 0;
    bool gst_tools_available = false;
    bool backend_installed = false;
    bool expected_failure = false;
    bool gate_pass = false;
    std::string failure_layer;
    std::string gate_reason;
    std::string log_path;
};

inline bool ensure_dir(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    if (mkdir(path.c_str(), 0755) == 0 || errno == EEXIST) {
        return true;
    }
    std::cerr << "mkdir failed: " << path << "\n";
    return false;
}

inline bool write_text_file(const std::string& path, const std::string& body) {
    std::ofstream out(path.c_str());
    if (!out) {
        std::cerr << "write failed: " << path << "\n";
        return false;
    }
    out << body;
    return true;
}

inline std::string yes_no(bool value) {
    return value ? "yes" : "no";
}

inline bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

inline int count_occurrences(const std::string& text, const std::string& needle) {
    if (needle.empty()) {
        return 0;
    }
    int count = 0;
    std::string::size_type pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

inline void print_kv(const std::string& key, const std::string& value) {
    std::cout << std::left << std::setw(32) << key << " : " << value << "\n";
}

inline int decode_wait_status(int status) {
    if (status < 0) {
        return status;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return status;
}

inline CommandResult run_command_capture(const std::string& command) {
    CommandResult result;
    result.command = command;
    const auto begin = std::chrono::steady_clock::now();
    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
    if (pipe == NULL) {
        result.output = "popen failed\n";
        result.exit_code = -1;
        return result;
    }
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe) != NULL) {
        result.output += buf;
    }
    result.exit_code = decode_wait_status(pclose(pipe));
    const auto end = std::chrono::steady_clock::now();
    result.elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    return result;
}

inline bool tool_exists(const std::string& tool) {
    CommandResult r = run_command_capture("command -v " + tool);
    return r.exit_code == 0 && !r.output.empty();
}

inline bool element_exists(const std::string& element) {
    if (element.empty()) {
        return false;
    }
    CommandResult r = run_command_capture("gst-inspect-1.0 " + element);
    return r.exit_code == 0;
}

inline std::string shell_quote(const std::string& text) {
    std::string out = "'";
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\'') {
            out += "'\\''";
        } else {
            out += text[i];
        }
    }
    out += "'";
    return out;
}

inline std::string classify_failure(const std::string& output, int exit_code) {
    if (exit_code == 0) {
        return "success";
    }
    if (contains(output, "no element") || contains(output, "no such element")) {
        return "missing_element_or_plugin";
    }
    if (contains(output, "could not link") || contains(output, "can't handle caps")) {
        return "link_or_caps_negotiation_failure";
    }
    if (contains(output, "not-negotiated")) {
        return "runtime_caps_not_negotiated";
    }
    if (contains(output, "Internal data stream error")) {
        return "runtime_stream_error";
    }
    return "unknown_gstreamer_failure";
}

}  // namespace stage07_enterprise

#endif  // STAGE07_ENTERPRISE_COMMON_HPP_
