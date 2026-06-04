#include "00_stage07_gst_common.hpp"

/*
 * Demo06: FFmpeg 与 GStreamer 对照。
 *
 * 学习目标：
 * - 同一件事在 FFmpeg 中常表现为“命令选项 + demux/decode/filter/sink”。
 * - 在 GStreamer 中表现为“element graph + pad/caps negotiation + bus messages”。
 * - 工作中要能把两套日志互相翻译：codec、pixel format、sink、硬件后端、fallback。
 */
int main(int argc, char** argv) {
    const int frames = stage07::get_arg_int(argc, argv, "--frames", 12);
    const bool run_ffmpeg = !stage07::has_arg(argc, argv, "--skip-ffmpeg");

    std::ostringstream gst_cmd;
    gst_cmd << "gst-launch-1.0 -q videotestsrc num-buffers=" << frames
            << " ! video/x-raw,format=NV12,width=320,height=240,framerate=30/1"
            << " ! videoconvert ! fakesink sync=false";

    std::ostringstream ff_cmd;
    ff_cmd << "ffmpeg -hide_banner -loglevel error "
           << "-f lavfi -i testsrc2=size=320x240:rate=30 "
           << "-frames:v " << frames << " -f null -";

    std::cout << "Stage07 Demo06: FFmpeg vs GStreamer mental model\n";
    std::cout << "[mapping]\n";
    std::cout << "FFmpeg input/filter/output model  <->  GStreamer element/pad/caps model\n";
    std::cout << "FFmpeg decoder selection          <->  GStreamer decoder element selection\n";
    std::cout << "FFmpeg hwaccel/hwdownload         <->  GStreamer hardware element + memory caps + sink\n";

    std::cout << "\n[gstreamer run]\n";
    stage07::CommandResult gst = stage07::run_command_capture(gst_cmd.str());
    stage07::print_command_result(gst, 1600);

    bool ff_ok = true;
    if (run_ffmpeg && stage07::tool_exists("ffmpeg")) {
        std::cout << "\n[ffmpeg run]\n";
        stage07::CommandResult ff = stage07::run_command_capture(ff_cmd.str());
        stage07::print_command_result(ff, 1600);
        ff_ok = ff.ok();
    } else {
        std::cout << "\n[ffmpeg run]\n";
        std::cout << "ffmpeg skipped or not installed; GStreamer comparison table still valid.\n";
    }

    std::cout << "\n[driver shadow]\n";
    std::cout << "If FFmpeg hardware path works but GStreamer fails, compare backend plugin, parser, caps, and sink memory type.\n";
    std::cout << "If both fail on the same compressed stream, suspect bitstream/profile/backend/driver/device support earlier.\n";
    std::cout << "verdict=" << ((gst.ok() && ff_ok) ? "PASS_FFMPEG_GSTREAMER_COMPARE" : "FAIL_COMPARE_RUN") << "\n";
    return (gst.ok() && ff_ok) ? 0 : 1;
}
