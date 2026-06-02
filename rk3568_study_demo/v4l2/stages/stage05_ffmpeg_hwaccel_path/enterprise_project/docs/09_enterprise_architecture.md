# 09 Enterprise Architecture

## 架构分层

1. CLI 层：`01_cli_config.cpp`
2. 控制层：`07_enterprise_pipeline_main.cpp`
3. 状态机层：`02_state_machine.cpp`
4. 执行层：`06_hwaccel_pipeline_service.cpp`
5. 可观测性层：`03_logger.cpp` + `04_metrics_sink.cpp`
6. 门禁层：`05_gate_evaluator.cpp`

## 状态机

`Init -> DevicePrepared -> InputOpened -> StreamReady -> DecoderReady -> LoopRunning -> Draining -> Stopped`

失败分支：任一步失败都转 `Failed`。

## 数据流

1. 输入容器 -> packet
2. packet -> decoder
3. 默认 RKMPP wrapper：`h264_rkmpp/hevc_rkmpp` decoder -> frame
4. 显式 hwdevice 模式：decoder -> hw/sw 分支
5. hw frame 可选 transfer 到 sw frame
6. 指标统计 -> gate -> json

## 计数字段语义（避免误读）

1. `frame_hw`：仅在显式 `--hw-type` 且像素格式命中 `g_hw_pix_fmt` 时计数。
2. `frame_cpu_visible`：输出帧是 CPU 可见内存形态（例如 yuv420p/nv12）时计数。
3. `frame_cpu_visible` 不等于“软件解码”；默认 RKMPP wrapper 模式下它通常会增长。
4. 默认 wrapper 模式硬解证据来自：`decoder=*_rkmpp` + 成功 `frame_recv` + 性能/日志对比。

## 关键门禁

1. `packet_read/frame_recv` 必须 > 0。
2. 默认 RKMPP wrapper 模式下，decoder 必须是 `*_rkmpp`，并且必须成功输出 frame。
3. 显式 `--hw-type` 模式下，非强制 fallback 时 `frame_hw` 不得为 0。
4. 显式 `--hw-type` 模式下，非 transfer 注入模式若有 hw frame，`hw_transfer_ok` 不应为 0。
