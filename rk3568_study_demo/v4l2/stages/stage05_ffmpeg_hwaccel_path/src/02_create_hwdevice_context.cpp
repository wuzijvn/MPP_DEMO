#include "00_ffmpeg_hwaccel_common.hpp"

#include <string>

namespace {

struct Args {
    /*
     * RK3568/Firefly 板端最稳定的基础验证通常是 DRM render node。
     * VAAPI 是否可用还取决于 libva、libva-drm、平台 VA driver，不能只看
     * `ffmpeg -hwaccels` 里有没有 vaapi。
     */
    std::string hw_type = "drm";
    std::string device;
    bool device_set = false;
};

/*
 * 参数解释：
 * --hw-type=drm/vaapi/vdpau/rkmpp/... 选择 FFmpeg 硬件后端。
 * --device=... 指定设备节点（如 /dev/dri/renderD128）。
 *
 * 学习边界：
 * - 本 demo 只验证 AVHWDeviceContext 能否创建。
 * - `h264_rkmpp` 解码器可以不经过这个 demo 手动创建 hwdevice，它是另一条
 *   decoder wrapper 路径，真实解码要看后续 decode demo 或 ffmpeg 命令证据。
 */
bool parse_args(int argc, char** argv, Args* a) {
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--hw-type=", 10) == 0) {
            a->hw_type = argv[i] + 10;
        } else if (strncmp(argv[i], "--device=", 9) == 0) {
            a->device = argv[i] + 9;
            a->device_set = true;
        }
    }
    return true;
}

std::string default_device_for_hw_type(const Args& a) {
    if (a.device_set) {
        return a.device;
    }
    if (a.hw_type == "drm") {
        return "/dev/dri/renderD128";
    }
    return "";
}

}  // namespace

int main(int argc, char** argv) {
    using namespace ffmpeg_stage05;

    Args a;
    parse_args(argc, argv, &a);

    AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
    if (!parse_hw_type(a.hw_type, &type)) {
        fprintf(stderr, "[demo02] unknown hw type: %s\n", a.hw_type.c_str());
        print_hw_types();
        return 2;
    }

    AVBufferRef* hw_device_ctx = nullptr;
    const std::string selected_device = default_device_for_hw_type(a);
    const char* dev = selected_device.empty() ? nullptr : selected_device.c_str();

    /*
     * 关键 API：av_hwdevice_ctx_create
     * 前置条件：
     * 1) hw_type 在 FFmpeg 中可识别；
     * 2) device 节点可访问（若 backend 需要）；
     * 调用后：
     * - 成功：hw_device_ctx 获得引用，后续需 av_buffer_unref 对称释放；
     * - 失败：返回负错误码，可映射到权限/设备不存在/驱动不支持等问题。
     */
    const int ret = av_hwdevice_ctx_create(&hw_device_ctx, type, dev, nullptr, 0);
    if (ret < 0) {
        fprintf(stderr, "[demo02] av_hwdevice_ctx_create failed: %s\n",
                ff_err2str(ret).c_str());
        fprintf(stderr, "[demo02] hint: check /dev/dri/renderD* or backend-specific device.\n");
        return 3;
    }

    printf("[demo02] hw device created: type=%s device=%s\n",
           av_hwdevice_get_type_name(type), dev ? dev : "(auto)");

    // 资源释放对称：创建成功后必须 unref。
    av_buffer_unref(&hw_device_ctx);
    printf("[demo02] PASS\n");
    return 0;
}
