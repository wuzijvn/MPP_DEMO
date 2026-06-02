#include "00_ffmpeg_hwaccel_common.hpp"

#include <string>

/*
 * demo01 目标：
 * 1) 观察当前 FFmpeg 构建暴露了哪些 hw device type；
 * 2) 观察某个 decoder 是否声明了可用硬件配置（device + hw pix fmt + methods）。
 * 
 * 驱动影子线：
 * - 这里只是“能力枚举层”，不会真正触发 /dev 节点访问。
 * - 但它能帮助你确认：后续失败是“编译能力缺失”还是“运行时设备问题”。
 */
int main(int argc, char** argv) {
    using namespace ffmpeg_stage05;

    std::string decoder_name = "h264_rkmpp";
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--decoder=", 10) == 0) {
            decoder_name = argv[i] + 10;
        }
    }

    // 第一步：枚举 FFmpeg 支持的硬件后端类型（编译时能力）。
    print_hw_types();

    // 第二步：按名称找 decoder。失败通常是 FFmpeg 构建未包含对应 decoder。
    const AVCodec* dec = avcodec_find_decoder_by_name(decoder_name.c_str());
    if (dec == nullptr) {
        fprintf(stderr, "[demo01] decoder not found: %s\n", decoder_name.c_str());
        return 2;
    }

    printf("[demo01] decoder=%s id=%d\n", dec->name, dec->id);
    printf("[demo01] hw configs for decoder:\n");

    int found = 0;
    for (int i = 0;; ++i) {
        // avcodec_get_hw_config 用于读取 decoder 声明的硬件配置表。
        const AVCodecHWConfig* cfg = avcodec_get_hw_config(dec, i);
        if (cfg == nullptr) {
            break;
        }
        ++found;

        // methods 按位标识当前配置的工作方式（如 HW_DEVICE_CTX）。
        printf("  - idx=%d device=%s pix_fmt=%s methods=0x%x\n", i,
               av_hwdevice_get_type_name(cfg->device_type),
               pix_fmt_name(cfg->pix_fmt), cfg->methods);
    }

    if (found == 0) {
        printf("[demo01] no hw config found for decoder (may still decode in software).\n");
    }

    printf("[demo01] PASS\n");
    return 0;
}
