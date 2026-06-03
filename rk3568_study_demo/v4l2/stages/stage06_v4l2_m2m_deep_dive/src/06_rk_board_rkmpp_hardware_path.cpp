#include "00_stage06_m2m_common.hpp"

#include <sys/wait.h>

namespace {

std::string shell_quote(const std::string& text) {
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

int run_command_to_file(const std::string& command, const std::string& output_path) {
    const std::string full = "( " + command + " ) > " + shell_quote(output_path) + " 2>&1";
    return system(full.c_str());
}

bool command_available(const std::string& command) {
    const std::string test = "command -v " + command + " >/dev/null 2>&1";
    return system(test.c_str()) == 0;
}

}  // namespace

/*
 * Demo06 目标：
 * - RK 板上不强行跑 V4L2 M2M codec ioctl。
 * - 收集 RKMPP/FFmpeg/设备节点/dmesg 证据，给真实硬件路径一个独立入口。
 *
 * 设计边界：
 * - 本文件不发明 RKMPP SDK API。
 * - 如果没有输入文件，只做 capability/report gate。
 * - 如果提供 --input 且 FFmpeg 有 h264_rkmpp/hevc_rkmpp，可运行命令验证硬件路径。
 */
int main(int argc, char** argv) {
    const std::string output_dir = stage06::get_arg(argc, argv, "--output-dir", "logs/rk_rkmpp_probe");
    const std::string input = stage06::get_arg(argc, argv, "--input", "");
    const std::string decoder = stage06::get_arg(argc, argv, "--decoder", "h264_rkmpp");
    const bool require_rkmpp = stage06::get_arg_int(argc, argv, "--require-rkmpp", 0) != 0;

    stage06::ensure_dir(output_dir);
    std::cout << "Stage06 Demo06: RK board RKMPP hardware path evidence collector\n";
    stage06::print_line("output_dir", output_dir);
    stage06::print_line("decoder", decoder);
    stage06::print_line("boundary", "RK hardware proof uses RKMPP/FFmpeg evidence, not VM vim2m");

    const bool ffmpeg_ok = command_available("ffmpeg");
    stage06::print_line("ffmpeg_available", stage06::yes_no(ffmpeg_ok));
    run_command_to_file("uname -a", output_dir + "/uname.txt");
    run_command_to_file("ls -l /dev/video* /dev/dri/* 2>/dev/null || true",
                        output_dir + "/device_nodes.txt");
    run_command_to_file("ffmpeg -hide_banner -decoders 2>/dev/null | grep -Ei 'rkmpp|v4l2m2m|h264|hevc' || true",
                        output_dir + "/ffmpeg_decoders.txt");
    run_command_to_file("dmesg | grep -Ei 'rkvdec|mpp|vpu|v4l2|codec|firmware|iommu|dma|timeout|reset' | tail -n 120 || true",
                        output_dir + "/dmesg_media_hints.txt");

    bool decoder_seen = false;
    if (ffmpeg_ok) {
        const std::string grep_cmd = "ffmpeg -hide_banner -decoders 2>/dev/null | grep -q " + decoder;
        decoder_seen = system(grep_cmd.c_str()) == 0;
    }
    stage06::print_line("decoder_seen", stage06::yes_no(decoder_seen));

    bool decode_command_ok = false;
    if (!input.empty() && ffmpeg_ok && decoder_seen) {
        const std::string cmd = "ffmpeg -hide_banner -v verbose -c:v " +
                                shell_quote(decoder) + " -i " +
                                shell_quote(input) + " -frames:v 8 -f null -";
        const int rc = run_command_to_file(cmd, output_dir + "/ffmpeg_rkmpp_decode.log");
        decode_command_ok = (rc == 0);
        stage06::print_line("decode_command", decode_command_ok ? "ok" : "failed_or_unsupported");
    } else {
        stage06::write_text_file(output_dir + "/ffmpeg_rkmpp_decode.log",
                                 "decode command not run: provide --input and ensure ffmpeg decoder exists\n");
    }

    std::ostringstream report;
    report << "# RKMPP Hardware Path Report\n\n";
    report << "- ffmpeg_available: " << stage06::yes_no(ffmpeg_ok) << "\n";
    report << "- decoder: " << decoder << "\n";
    report << "- decoder_seen: " << stage06::yes_no(decoder_seen) << "\n";
    report << "- input: " << (input.empty() ? "(not provided)" : input) << "\n";
    report << "- decode_command_ok: " << stage06::yes_no(decode_command_ok) << "\n\n";
    report << "## Evidence Files\n";
    report << "- uname.txt\n";
    report << "- device_nodes.txt\n";
    report << "- ffmpeg_decoders.txt\n";
    report << "- dmesg_media_hints.txt\n";
    report << "- ffmpeg_rkmpp_decode.log\n\n";
    report << "## Driver Shadow Line\n";
    report << "RKMPP path may use vendor user-space middleware and kernel/media/VPU drivers. "
              "Do not infer V4L2 M2M codec support from vim2m or ISP nodes.\n";
    stage06::write_text_file(output_dir + "/rk_rkmpp_report.md", report.str());

    if (require_rkmpp && !decoder_seen) {
        std::cout << "verdict=FAIL_RKMPP_DECODER_NOT_FOUND\n";
        return 1;
    }
    if (!input.empty() && decoder_seen && !decode_command_ok) {
        std::cout << "verdict=FAIL_RKMPP_DECODE_COMMAND\n";
        return 1;
    }
    std::cout << "verdict=PASS_RK_HARDWARE_PATH_EVIDENCE_COLLECTED\n";
    return 0;
}
