# Stage04 最终验收清单

## A. 构建

- [ ] `./build.sh all` 成功。
- [ ] `bin/01_* ... 08_*` 可见。
- [ ] `enterprise_project/build.sh` 成功，`enterprise_project/bin/09_enterprise_ffmpeg_pipeline_service` 可见。

## B. 功能

- [ ] demo01 能识别流。
- [ ] demo03 能输出 frame。
- [ ] demo05 能输出 pts/dts/time_base。
- [ ] demo07 故障注入后仍能打印 cleanup 完成。
- [ ] demo08 输出 fps。
- [ ] demo09 生成 `enterprise_pipeline.log` 和 `enterprise_metrics.json`。

## C. 指标

- [ ] 记录 demo08 `frames/elapsed/fps`。
- [ ] 记录 demo09 `packet_in/frame_out/error_count/state_transition`。

## D. 解释

- [ ] 能解释 send/receive 语义。
- [ ] 能解释 `EAGAIN/EOF`。
- [ ] 能解释 packet/frame 生命周期。

## E. 通过门槛

A+B+C+D 全满足即通过。
