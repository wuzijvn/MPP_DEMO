#ifndef STAGE07_GST_COMMON_HPP_
#define STAGE07_GST_COMMON_HPP_

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

namespace stage07 {

/*
 * Stage07 公共工具头。
 *
 * 设计边界：
 * - 本阶段重点是 GStreamer pipeline thinking，不是插件开发。
 * - 为了让当前 VM/RK 板都能直接跑，基础 demo 不依赖 GStreamer C 开发头文件。
 * - demo 通过 gst-launch-1.0/gst-inspect-1.0 观察 pipeline、caps、queue、GST_DEBUG 和硬件后端。
 *
 * 驱动影子线：
 * - gst-inspect 看到的 element 是用户态插件入口。
 * - 硬件 decoder element 可能继续走 libav+rkmpp、V4L2 M2M、VAAPI、OpenMAX 或厂商库。
 * - caps 协商失败在用户态表现为 link/negotiation error，驱动侧常对应格式、分辨率、stride、modifier、profile/level 不支持。
 */

struct CommandResult {
    std::string command;
    std::string output;
    int exit_code = -1;
    long long elapsed_ms = 0;

    bool ok() const {
        return exit_code == 0;
    }
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
        std::cerr << "open for write failed: " << path << "\n";
        return false;
    }
    out << body;
    return true;
}

inline bool file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

inline std::string get_arg(int argc, char** argv, const std::string& key,
                           const std::string& def) {
    const std::string prefix = key + "=";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.compare(0, prefix.size(), prefix) == 0) {
            return arg.substr(prefix.size());
        }
    }
    return def;
}

inline int get_arg_int(int argc, char** argv, const std::string& key, int def) {
    const std::string v = get_arg(argc, argv, key, "");
    if (v.empty()) {
        return def;
    }
    return std::atoi(v.c_str());
}

inline bool has_arg(int argc, char** argv, const std::string& key) {
    for (int i = 1; i < argc; ++i) {
        if (key == argv[i]) {
            return true;
        }
    }
    return false;
}

inline std::string yes_no(bool value) {
    return value ? "yes" : "no";
}

inline void print_kv(const std::string& key, const std::string& value) {
    std::cout << std::left << std::setw(30) << key << " : " << value << "\n";
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
    /*
     * popen 这里用于教学诊断工具，不用于处理不可信输入。
     * 真实量产工具如果接受外部路径/参数，要做 shell escaping 或改为 fork/exec argv 模式。
     */
    CommandResult result;
    result.command = command;
    const auto begin = std::chrono::steady_clock::now();
    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
    if (pipe == NULL) {
        result.output = "popen failed\n";
        result.exit_code = -1;
        return result;
    }

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result.output += buffer;
    }
    result.exit_code = decode_wait_status(pclose(pipe));
    const auto end = std::chrono::steady_clock::now();
    result.elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    return result;
}

inline bool tool_exists(const std::string& tool) {
    CommandResult r = run_command_capture("command -v " + tool);
    return r.ok() && !r.output.empty();
}

inline bool inspect_element_exists(const std::string& element) {
    CommandResult r = run_command_capture("gst-inspect-1.0 " + element);
    return r.ok();
}

inline std::string one_line_summary(const std::string& output) {
    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty()) {
            return line;
        }
    }
    return "(no output)";
}

inline void print_command_result(const CommandResult& r, int max_chars) {
    print_kv("command", r.command);
    print_kv("exit_code", std::to_string(r.exit_code));
    print_kv("elapsed_ms", std::to_string(r.elapsed_ms));
    std::cout << "output_begin\n";
    if (static_cast<int>(r.output.size()) <= max_chars) {
        std::cout << r.output;
    } else {
        std::cout << r.output.substr(0, static_cast<size_t>(max_chars));
        std::cout << "\n...[truncated " << (r.output.size() - max_chars) << " bytes]\n";
    }
    std::cout << "output_end\n";
}

inline std::string classify_gst_failure(const std::string& output, int exit_code) {
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

inline std::string hardware_backend_hint(const std::string& element) {
    if (contains(element, "rkmpp") || contains(element, "mpp")) {
        return "Rockchip MPP/VPU path candidate; verify with dmesg, CPU, logs, and output format.";
    }
    if (contains(element, "v4l2")) {
        return "V4L2 path candidate; map caps to VIDIOC_ENUM_FMT/TRY_FMT/S_FMT and device node.";
    }
    if (contains(element, "vaapi")) {
        return "VAAPI path candidate; map to libva, DRM render node, Mesa/driver support.";
    }
    if (contains(element, "omx")) {
        return "OpenMAX path candidate; map to vendor OMX component and buffer mode.";
    }
    return "software or generic plugin; not hardware proof by itself.";
}

}  // namespace stage07

#endif  // STAGE07_GST_COMMON_HPP_
