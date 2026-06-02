#include "00_ffmpeg_hwaccel_common.hpp"

#include <string>
#include <vector>

namespace {

struct Args {
    /*
     * RK3568 当前学习主线是 RKMPP decoder wrapper。
     * 这个 demo 只在显式传入 --hw-type 时检查 AVHWDeviceContext 型 hwfmt；
     * 默认不再假设 vaapi，因为板端 VAAPI runtime 不可用。
     */
    std::string decoder = "h264_rkmpp";
    std::string hw_type;
    bool hw_type_set = false;
};

bool parse_args(int argc, char** argv, Args* a) {
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--decoder=", 10) == 0) {
            a->decoder = argv[i] + 10;
        } else if (strncmp(argv[i], "--hw-type=", 10) == 0) {
            a->hw_type = argv[i] + 10;
            a->hw_type_set = true;
        }
    }
    return true;
}

}  // namespace

/*
 * demo03 目标：验证“decoder + hw_type”是否有硬件像素格式候选。
 * 这一步等价于 get_format 决策前的静态预检查。
 */
int main(int argc, char** argv) {
    using namespace ffmpeg_stage05;

    Args a;
    parse_args(argc, argv, &a);

    const AVCodec* dec = avcodec_find_decoder_by_name(a.decoder.c_str());
    if (dec == nullptr) {
        fprintf(stderr, "[demo03] decoder not found: %s\n", a.decoder.c_str());
        return 2;
    }

    if (!a.hw_type_set) {
        printf("[demo03] decoder=%s\n", dec->name);
        printf("[demo03] no --hw-type specified; skip AVHWDeviceContext hwfmt probe\n");
        printf("[demo03] note: rkmpp decoder wrapper can be valid even when decoder hw_config is empty.\n");
        printf("[demo03] PASS(wrapper-mode)\n");
        return 0;
    }

    AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
    if (!parse_hw_type(a.hw_type, &type)) {
        fprintf(stderr, "[demo03] unknown hw type: %s\n", a.hw_type.c_str());
        return 3;
    }

    // collect_decoder_hw_configs 会过滤到目标 hw_type 的配置项。
    const std::vector<AVPixelFormat> fmts = collect_decoder_hw_configs(dec, type);
    printf("[demo03] decoder=%s target_hw=%s\n", dec->name, av_hwdevice_get_type_name(type));
    if (fmts.empty()) {
        // 无候选不等于“绝对不能解码”，但意味着该组合更可能走软件路径。
        printf("[demo03] no hw pixel format for this decoder+hwtype pair\n");
        printf("[demo03] PASS(without-hw-format)\n");
        return 0;
    }

    for (size_t i = 0; i < fmts.size(); ++i) {
        printf("[demo03] candidate_hw_pix_fmt[%zu]=%s\n", i, pix_fmt_name(fmts[i]));
    }

    printf("[demo03] first_hw_pix_fmt=%s\n", pix_fmt_name(fmts[0]));
    printf("[demo03] PASS\n");
    return 0;
}
