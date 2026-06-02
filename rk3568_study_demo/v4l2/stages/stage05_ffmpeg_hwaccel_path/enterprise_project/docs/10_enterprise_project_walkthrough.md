# 10 Enterprise Project Walkthrough（项目总讲解）

## 1. 这个企业项目在做什么

这个项目把 Stage05 的“FFmpeg 硬解路径学习 demo”提升为一个可复盘的服务骨架，目标是：

1. 给出可运行的数据通路：`demux -> decode -> 统计 -> gate`。
2. 给出可观测证据：日志、JSON 指标、PASS/FAIL 门禁。
3. 给出可调试故障：设备创建失败、强制 fallback、transfer 失败、缺失 hwfmt。

你当前看到的 `sw`（现已改名 `cpu_visible`）增长，本质是“输出帧内存形态”现象，不是“软解一定在工作”。

## 2. 文件责任图（先看这个）

1. `src/07_enterprise_pipeline_main.cpp`：主入口，组装模块，打印 summary，落盘 metrics。
2. `src/06_hwaccel_pipeline_service.cpp`：核心解码流程（prepare + run_loop）。
3. `src/05_gate_evaluator.cpp`：按计数客观判定 PASS/FAIL。
4. `src/04_metrics_sink.cpp`：把运行证据写成 `enterprise_metrics.json`。
5. `src/03_logger.cpp`：结构化日志输出。
6. `src/02_state_machine.cpp`：状态迁移计数，保证流程完整性。
7. `src/01_cli_config.cpp`：参数解析与校验。

## 3. 你这次输出为什么“全是 sw/cpu_visible”

你的运行命令是默认 wrapper 模式（没有 `--hw-type`）：

1. `--decoder=h264_rkmpp`
2. `hw_type=(not-forced)`

该模式下代码逻辑是：

1. 不强行走 `AVHWDeviceContext + get_format + hw pix fmt` 路径。
2. `frame_hw` 统计口径不会被触发（常见是 0）。
3. 输出帧若是 CPU 可见格式，会计入 `frame_cpu_visible`。
4. gate 判定硬解证据看的是 `*_rkmpp decoder + frame_recv>0`，而不是 `frame_hw>0`。

所以你的日志里：

1. `decoder=h264_rkmpp`
2. `frame_recv=120`
3. `fallback_count=0`
4. `gate pass`

这在本项目语义里是“wrapper 硬解路径证据成立”，并不矛盾。

## 4. 如何验证“不是软解回退”

用同一输入做 A/B：

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage05_ffmpeg_hwaccel_path/enterprise_project

time -p ./bin/09_enterprise_ffmpeg_hwaccel_service --input=../samples/sample.mp4 --decoder=h264
time -p ./bin/09_enterprise_ffmpeg_hwaccel_service --input=../samples/sample.mp4 --decoder=h264_rkmpp
```

重点看：

1. `result=PASS/FAIL` 与 `gate reason`
2. `enterprise_metrics.json` 里的 `decoder`、`fallback_count`、`frame_recv`
3. real/user/sys 的差异（通常 `h264_rkmpp` 更低 CPU user）

## 5. 驱动影子线（你现在该知道的）

1. wrapper 模式：硬解证据更多来自“选择了 rkmpp decoder 并稳定出帧 + 性能/日志对比”。
2. 显式 hwdevice 模式：更强调 `frame_hw` 和 `hw_transfer_ok` 这些 hwcontext 证据。
3. 两种证据口径不同，不能混用同一指标下结论。

## 6. 常见误区

1. 把 `CPU 可见输出帧` 误认为 `CPU 在做软解`。
2. 只看一个字段（例如旧的 `sw`）就判定回退。
3. 不做 `h264` vs `h264_rkmpp` 对照。

## 7. 最小排查顺序（1 分钟）

1. 看命令是否传了 `--decoder=h264_rkmpp`。
2. 看是否显式传了 `--hw-type`（决定证据口径）。
3. 看 `enterprise_metrics.json`：`decoder`、`frame_recv`、`fallback_count`、`gate.pass`。
4. 跑一轮 `h264` 对照测时，确认性能侧证据。

