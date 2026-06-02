#include <stdio.h>

#include <string>
#include <vector>

#include "00_m2m_demo_common.hpp"

namespace {

/*
 * 结构体作用：02 demo 参数集合。
 * 知识边界：只训练 QUERYCAP + ENUM_FMT，不做 S_FMT/REQBUFS。
 */
struct Args {
    std::string dev = "/dev/video0";
    int mplane = 0;
    bool verbose = false;
};

void print_usage(const char* prog) {
    printf("%s: Stage03 demo 02 - querycap + enum formats\n", prog);
    printf("Usage: %s --dev=/dev/video0 --mplane=0 --verbose\n", prog);
}

/*
 * 函数作用：解析参数。
 * 参数语义：
 * - mplane=0 走单平面类型；mplane=1 走多平面类型。
 */
bool parse_args(int argc, char** argv, Args* a) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return false;
        } else if (m2m_demo::parse_kv(argv[i], "--dev", &a->dev)) {
        } else if (m2m_demo::parse_kv_int(argv[i], "--mplane", &a->mplane)) {
        } else if (strcmp(argv[i], "--verbose") == 0) {
            a->verbose = true;
        } else {
            fprintf(stderr, "unknown arg: %s\n", argv[i]);
            return false;
        }
    }
    return true;
}

/*
 * 函数作用：枚举某个队列类型的格式。
 * 输入参数：
 * - type: OUTPUT 或 CAPTURE 类型。
 * 失败边界：
 * - enum 到驱动返回失败就停止；这通常表示已枚举完。
 * 驱动影子线：
 * - 对应 VIDIOC_ENUM_FMT -> 驱动 enum_fmt 回调。
 */
void enum_one_type(int fd, uint32_t type, const char* name) {
    printf("[demo02] %s formats:", name);
    bool any = false;

    // 教学上限制最多 64 个，防止异常驱动导致无限循环。
    for (uint32_t i = 0; i < 64; ++i) {
        struct v4l2_fmtdesc desc;
        memset(&desc, 0, sizeof(desc));
        desc.index = i;
        desc.type = type;

        if (m2m_demo::xioctl(fd, VIDIOC_ENUM_FMT, &desc) < 0) {
            // 常见是 EINVAL，表示 index 越界（枚举结束）。
            break;
        }

        any = true;
        printf(" %s", m2m_demo::fourcc_to_string(desc.pixelformat).c_str());
    }

    if (!any) {
        printf(" (none)");
    }
    printf("\n");
}

}  // namespace

/*
 * 函数作用：demo02 入口。
 * 主流程：
 * 1) open 节点；
 * 2) QUERYCAP；
 * 3) ENUM_FMT(OUTPUT/CAPTURE)；
 * 4) close。
 */
int main(int argc, char** argv) {
    Args args;
    if (!parse_args(argc, argv, &args)) {
        return 1;
    }

    int fd = -1;
    if (!m2m_demo::open_node(args.dev, &fd)) {
        return 1;
    }

    struct v4l2_capability cap;
    if (!m2m_demo::querycap(fd, &cap)) {
        close(fd);
        return 1;
    }

    // 有些驱动把真实能力放在 device_caps，需要按 V4L2 规范优先取它。
    const uint32_t caps =
        (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;
    const bool is_m2m = (caps & V4L2_CAP_VIDEO_M2M) || (caps & V4L2_CAP_VIDEO_M2M_MPLANE);

    printf("[demo02] driver=%s card=%s m2m=%d\n", reinterpret_cast<char*>(cap.driver),
           reinterpret_cast<char*>(cap.card), is_m2m ? 1 : 0);

    // 枚举解码器输入队列（OUTPUT）支持的压缩/输入格式。
    enum_one_type(fd, m2m_demo::output_type_from_mplane(args.mplane), "OUTPUT");
    // 枚举解码器输出队列（CAPTURE）支持的原始帧格式。
    enum_one_type(fd, m2m_demo::capture_type_from_mplane(args.mplane), "CAPTURE");

    printf("[demo02] PASS: querycap + enum formats done\n");

    close(fd);
    return 0;
}
