#include "00_stage07_gst_common.hpp"

/*
 * Demo04: GST_DEBUG 日志采集。
 *
 * 学习目标：
 * - 使用 GST_DEBUG 收集 caps/pipeline 相关日志。
 * - 把日志保存到文件，形成可复现 debug evidence。
 * - 学会区分“命令成功但日志少”和“命令失败且日志有明确 error”。
 *
 * 工作场景：
 * - 和驱动/平台同事沟通问题时，纯口头描述不够，要给 pipeline、日志、版本、后端候选。
 */
int main(int argc, char** argv) {
    const std::string output_dir = stage07::get_arg(argc, argv, "--output-dir", "logs/debug_demo");
    const int frames = stage07::get_arg_int(argc, argv, "--frames", 6);
    stage07::ensure_dir("logs");
    stage07::ensure_dir(output_dir);

    std::ostringstream cmd;
    cmd << "GST_DEBUG='GST_CAPS:3,GST_ELEMENT_PADS:3,pipeline:3' "
        << "gst-launch-1.0 videotestsrc num-buffers=" << frames
        << " ! video/x-raw,format=NV12,width=160,height=120,framerate=15/1"
        << " ! videoconvert ! video/x-raw,format=I420 ! fakesink sync=false";

    std::cout << "Stage07 Demo04: GST_DEBUG log capture\n";
    stage07::print_kv("output_dir", output_dir);

    stage07::CommandResult result = stage07::run_command_capture(cmd.str());
    const std::string log_path = output_dir + "/gst_debug_caps.log";
    stage07::write_text_file(log_path, result.output);
    stage07::print_command_result(result, 2600);

    const int caps_mentions = stage07::count_occurrences(result.output, "caps");
    const bool got_eos = stage07::contains(result.output, "Got EOS");
    stage07::print_kv("log_path", log_path);
    stage07::print_kv("caps_mentions", std::to_string(caps_mentions));
    stage07::print_kv("got_eos", stage07::yes_no(got_eos));

    std::cout << "\n[driver shadow]\n";
    std::cout << "When hardware element is used, raise debug level around that plugin and compare with dmesg/device-node evidence.\n";
    std::cout << "verdict=" << (result.ok() ? "PASS_GST_DEBUG_CAPTURE" : "FAIL_GST_DEBUG_CAPTURE") << "\n";
    return result.ok() ? 0 : 1;
}
