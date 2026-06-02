#include <stdio.h>

#include "stage01_v4l2_args.hpp"
#include "stage01_v4l2_capture.hpp"
#include "stage01_v4l2_common.hpp"
#include "stage01_v4l2_image.hpp"

/*
===============================================================================
Stage 01 - V4L2 Capture Foundation
===============================================================================

本文件只做“流程编排”，把细节留在模块里，便于你分块学习：
1) stage01_v4l2_args.hpp    参数解析与配置校验
2) stage01_v4l2_capture.hpp V4L2 采集主状态机
3) stage01_v4l2_stats.hpp   统计与日志解释
4) stage01_v4l2_image.hpp   YUYV 转 PPM
5) stage01_v4l2_common.hpp  xioctl/fourcc/计时工具
6) stage01_v4l2_types.hpp   结构体定义
  
建议阅读顺序：
main -> parse_args -> run_capture -> print_stats -> image save

main 的职责边界（非常重要）：
1) 只做“流程编排”和“错误码收口”；
2) 不在这里直接写 ioctl 细节；
3) 所有业务细节都下沉到模块函数，方便你单独学习每一层。
*/

int main(int argc, char** argv) {
    // cfg 保存“最终生效配置”，来源是默认值 + 命令行覆盖。
    stage01_v4l2::AppConfig cfg;
    // show_help 用来区分：
    // 1) 用户主动请求帮助（返回0）
    // 2) 参数非法（返回1）
    bool show_help = false;

    // 第一步：解析参数。
    //
    // 设计意图：
    // - 把“默认值、校验、规范化”统一放在 parse_args；
    // - main 只负责根据结果决定流程分支。
    if (!stage01_v4l2::parse_args(argc, argv, &cfg, &show_help)) {
        if (show_help) {
            stage01_v4l2::print_usage(argv[0]);
            return 0;
        }
        stage01_v4l2::print_usage(argv[0]);
        return 1;
    }

    // 第二步：打印最终配置。
    //
    // 这是“可复现实验”的关键：你后续回看日志时，
    // 必须知道当时到底跑了哪组参数。
    printf("start capture with config:\n");
    printf("  dev=%s req=%dx%d pixfmt=%s raw=%s ppm=%s frames=%d warmup=%d fps=%d timeout_ms=%d req_bufs=%u dump_formats=%d inject=%s inject_frame=%d save=%d log_every=%d trace_csv=%s\n",
           cfg.dev.c_str(),
           cfg.req_width,
           cfg.req_height,
           cfg.req_pixfmt.c_str(),
           cfg.out_raw.c_str(),
           cfg.out_ppm.c_str(),
           cfg.total_frames,
            cfg.warmup_frames,
            cfg.req_fps,
            cfg.timeout_ms,
            cfg.req_buf_count,
            cfg.dump_formats ? 1 : 0,
            cfg.inject.c_str(),
            cfg.inject_frame,
            cfg.save_preview ? 1 : 0,
            cfg.log_every,
            cfg.trace_csv.empty() ? "(disabled)" : cfg.trace_csv.c_str());
    // 刷新 stdout，避免在程序异常退出时丢失配置行。
    fflush(stdout);

    // 第三步：进入采集主状态机（open -> querycap -> fmt -> stream loop -> cleanup）。
    stage01_v4l2::CaptureRunResult rr = stage01_v4l2::run_capture(cfg);

    // 第四步：按配置导出“稳定预览帧”。
    //
    // 注意：
    // - preview 只是“快速肉眼确认”手段；
    // - 真正的稳定性分析还要结合 summary + trace CSV。
    if (cfg.save_preview) {
        if (rr.saved_frame.empty()) {
            // 常见触发：采集在 warmup 阶段就提前结束，尚未保存稳定帧。
            fprintf(stderr, "no stable frame available to save (maybe stopped before warmup finished)\n");
        } else {
            // 4.1 先保存 raw，确保保留“最原始证据”。
            FILE* fp = fopen(cfg.out_raw.c_str(), "wb");
            if (!fp) {
                perror("fopen raw output");
                return 1;
            }
            size_t w = fwrite(rr.saved_frame.data(), 1, rr.saved_frame.size(), fp);
            fclose(fp);
            if (w != rr.saved_frame.size()) {
                fprintf(stderr, "write raw failed: %zu/%zu\n", w, rr.saved_frame.size());
                return 1;
            }
            printf("saved raw preview: %s (%u bytes, seq=%u)\n",
                   cfg.out_raw.c_str(), rr.saved_bytes, rr.saved_seq);

            bool ppm_ok = false;
            if (rr.active_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {
                // 4.2 再做可视化导出（YUYV -> PPM）。
                const int width = (int)rr.active_fmt.fmt.pix.width;
                const int height = (int)rr.active_fmt.fmt.pix.height;
                const int bytesperline = (int)rr.active_fmt.fmt.pix.bytesperline;
                if (bytesperline > 0) {
                    // 优先使用 stride-aware 版本，适配“每行有对齐填充”的驱动。
                    ppm_ok = stage01_v4l2::save_ppm_from_yuyv_with_stride(cfg.out_ppm.c_str(),
                                                                           rr.saved_frame.data(),
                                                                           rr.saved_frame.size(),
                                                                           width,
                                                                           height,
                                                                           bytesperline);
                } else {
                    // 退化到紧凑布局版本（无显式 stride 信息时）。
                    ppm_ok = stage01_v4l2::save_ppm_from_yuyv(cfg.out_ppm.c_str(),
                                                              rr.saved_frame.data(),
                                                              rr.saved_frame.size(),
                                                              width,
                                                              height);
                }
            } else {
                // 当前转换器只实现了 YUYV；其他格式仍可完成采集与统计，只是无法直接转 PPM。
                fprintf(stderr,
                        "skip ppm export: fourcc=%s is not supported by current converter\n",
                        stage01_v4l2::fourcc_to_string(rr.active_fmt.fmt.pix.pixelformat).c_str());
            }

            if (ppm_ok) {
                printf("saved ppm preview: %s\n", cfg.out_ppm.c_str());
            } else {
                fprintf(stderr, "save ppm failed\n");
            }
        }
    }

    // 第五步：透传 run_capture 的结果码。
    //
    // 语义：
    // - 0: 达成目标帧数
    // - 1: 初始化阶段失败
    // - 2: 初始化成功但中途提前结束
    return rr.ret_code;
}
