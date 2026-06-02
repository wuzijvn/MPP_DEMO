#include "00_stage06_m2m_common.hpp"

#include <sstream>

int main(int argc, char** argv) {
    const std::string out = stage06::get_arg(argc, argv, "--out", "logs/06_timeout_debug_report.md");
    const std::string scenario = stage06::get_arg(argc, argv, "--scenario", "dqbuf_timeout");
    const int timeouts = stage06::get_arg_int(argc, argv, "--timeouts", 1);
    const int bytesused_zero = stage06::get_arg_int(argc, argv, "--bytesused-zero", 0);
    const int source_change_seen = stage06::get_arg_int(argc, argv, "--source-change", 0);

    std::ostringstream report;
    report << "# V4L2 M2M Debug Report - " << scenario << "\n\n";
    report << "## 问题现象\n";
    report << "- 现象：`poll` 或 `VIDIOC_DQBUF` 超时，CAPTURE 队列没有返回 decoded frame。\n";
    report << "- timeout_count：" << timeouts << "\n";
    report << "- bytesused_zero：" << bytesused_zero << "\n";
    report << "- source_change_seen：" << source_change_seen << "\n\n";

    report << "## 复现命令\n";
    report << "```bash\n";
    report << "v4l2-ctl --list-devices\n";
    report << "v4l2-ctl -d /dev/videoX --all\n";
    report << "v4l2-ctl -d /dev/videoX --list-formats-ext\n";
    report << "dmesg | grep -Ei 'v4l2|m2m|vpu|rkvdec|mpp|codec|timeout|reset|iommu|dma'\n";
    report << "```\n\n";

    report << "## 分层定位\n";
    report << "| 层级 | 可能原因 | 证据 | 下一步 |\n";
    report << "| --- | --- | --- | --- |\n";
    report << "| 命令/输入 | 码流不是 Annex B、缺 SPS/PPS、codec 选错 | ffprobe/码流 parser | 先用软件解码验证输入 |\n";
    report << "| V4L2 队列 | OUTPUT/CAPTURE QBUF 顺序错、bytesused=0 | qbuf/dqbuf counter | 打印每个 buffer index/bytesused |\n";
    report << "| 格式协商 | CAPTURE 格式/stride/sizeimage 未按驱动返回值更新 | TRY_FMT/S_FMT 返回值 | 记录驱动回填格式 |\n";
    report << "| source change | 分辨率变化后未重配 CAPTURE | SOURCE_CHANGE event | STREAMOFF CAPTURE 后重新 REQBUFS |\n";
    report << "| 驱动/硬件 | IRQ 未完成、firmware timeout、runtime PM | dmesg/trace | 给驱动同学完整日志 |\n\n";

    report << "## 驱动侧可能原因\n";
    report << "1. vb2 buffer 状态没有从 active 回到 done，用户态表现为 DQBUF timeout。\n";
    report << "2. VPU job 提交后没有 IRQ completion，可能是硬件 hang、firmware 错误或中断未到。\n";
    report << "3. runtime PM/autosuspend 让 VPU clock/power 在 job 期间异常关闭。\n";
    report << "4. source change 后 CAPTURE queue 生命周期处理不完整。\n\n";

    report << "## 验收结论模板\n";
    report << "- 已证明软件输入有效：是/否。\n";
    report << "- 已证明 V4L2 格式协商成功：是/否。\n";
    report << "- 已证明 OUTPUT bytesused 合理：是/否。\n";
    report << "- 已证明 CAPTURE buffer 足够且已 QBUF：是/否。\n";
    report << "- 驱动侧假设是否有 dmesg 支撑：是/否。\n";

    const std::string slash = out.find_last_of('/') == std::string::npos ? "" : out.substr(0, out.find_last_of('/'));
    if (!slash.empty()) {
        stage06::ensure_dir(slash);
    }
    if (!stage06::write_text_file(out, report.str())) {
        return 1;
    }

    std::cout << "Stage06 Demo06: timeout debug report template\n";
    stage06::print_line("scenario", scenario);
    stage06::print_line("report", out);
    stage06::print_line("meaning", "把 DQBUF timeout 从一句话故障变成可行动报告");
    std::cout << "verdict=DEBUG_REPORT_WRITTEN\n";
    return 0;
}
