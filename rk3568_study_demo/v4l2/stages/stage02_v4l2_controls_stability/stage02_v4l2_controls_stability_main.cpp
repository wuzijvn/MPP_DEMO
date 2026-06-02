#include <stdio.h>

#include "stage02_v4l2_args.hpp"
#include "stage02_v4l2_capture.hpp"

/*
===============================================================================
Stage 02 - V4L2 Controls + Stability Engineering
===============================================================================

本阶段目标：
1) 学会 control 枚举/读写（QUERYCTRL/G_CTRL/S_CTRL）
2) 学会采集线程与写盘线程解耦（队列/backpressure）
3) 学会做 10~30 分钟稳定性跑测并量化异常

建议阅读顺序：
1) stage02_v4l2_args.hpp
2) stage02_v4l2_ctrls.hpp
3) stage02_v4l2_capture.hpp

main 的角色：
1) 收敛参数入口（解析/校验/帮助）；
2) 打印“本次实验配置快照”；
3) 调用 run_stage02 执行完整链路；
4) 统一输出返回码，便于脚本判定成功失败。
*/

int main(int argc, char** argv) {
    // cfg 收纳 Stage02 所有行为开关：
    // controls / queue / writer / recovery / stop 条件 等。
    stage02_v4l2::AppConfig cfg;
    // show_help 用于区分“用户请求帮助”与“参数错误”。
    bool show_help = false;

    // 第一步：参数解析。
    //
    // 参数解析失败分两类：
    // 1) 用户主动请求 help
    // 2) 参数非法
    // 统一在这里决定返回码，保证入口行为可预期。
    if (!stage02_v4l2::parse_args(argc, argv, &cfg, &show_help)) {
        if (show_help) {
            stage02_v4l2::print_usage(argv[0]);
            return 0;
        }
        stage02_v4l2::print_usage(argv[0]);
        return 1;
    }

    // 第二步：打印最终生效配置。
    //
    // 这一步对于“稳定性实验复盘”非常关键：
    // 你后续看 summary 时必须能对应到当时的 queue/recovery 参数。
    printf("stage02 config:\n");
    printf("  dev=%s req=%dx%d pixfmt=%s fps=%d timeout_ms=%d req_bufs=%u\n",
           cfg.dev.c_str(),
           cfg.req_width,
           cfg.req_height,
           cfg.req_pixfmt.c_str(),
           cfg.req_fps,
           cfg.timeout_ms,
           cfg.req_buf_count);
    printf("  duration_sec=%d frames=%d queue_depth=%d queue_policy=%s writer_delay_ms=%d dump_every=%d no_save=%d out_dir=%s log_every=%d\n",
           cfg.duration_sec,
           cfg.total_frames,
           cfg.queue_depth,
           cfg.queue_policy.c_str(),
           cfg.writer_delay_ms,
           cfg.dump_every,
           cfg.no_save ? 1 : 0,
           cfg.out_dir.c_str(),
           cfg.log_every);
    printf("  list_ctrls=%d set_ctrl_count=%zu recover_on_timeout=%d max_recoveries=%d\n",
           cfg.list_ctrls ? 1 : 0,
           cfg.set_ctrls.size(),
           cfg.recover_on_timeout ? 1 : 0,
           cfg.max_recoveries);

    // 第三步：执行主流程（含 controls、采集、线程队列、恢复、统计）。
    //
    // 返回码约定由 run_stage02 决定，main 只做透传。
    int rc = stage02_v4l2::run_stage02(cfg);
    return rc;
}
