# Stage07 Experiment Matrix

| 实验 | 知识点 | 命令 | 预期输出 | Pass/Fail | 指标 | 指标含义 | 假信号 | 失败层 | 下一步 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 环境探测 | 工具/element/backend | `./scripts/run_01_gst_environment_probe.sh` | `verdict=PASS_GSTREAMER_ENVIRONMENT_PROBE` | 工具存在且基础 element 可见 | found_hw_candidates | 后端候选数量 | 候选存在不等于硬解 | rootfs/plugin | 跑真实码流和日志 |
| raw caps | caps negotiation | `./scripts/run_02_caps_negotiation_raw_video.sh` | `PASS_CAPS_NEGOTIATION` | exit code 0 | elapsed_ms、caps 参数 | raw pipeline 可 link | raw 成功不证明 codec | caps/plugin | inspect pad template |
| queue | 背压/latency | `./scripts/run_03_queue_backpressure_latency.sh` | `PASS_QUEUE_OBSERVATION` | 两条 pipeline 均成功 | no_queue/with_queue elapsed | 调度和缓存边界 | elapsed 更短不等于硬件加速 | scheduling/queue | 调 queue depth |
| GST_DEBUG | 日志证据 | `./scripts/run_04_gst_debug_log_capture.sh` | `PASS_GST_DEBUG_CAPTURE` | 生成 `gst_debug_caps.log` | caps_mentions | caps/pad 日志密度 | 日志多不等于定位完成 | debug/evidence | 提升 backend debug |
| link failure | 故障注入 | `./scripts/run_05_link_failure_fault_injection.sh` | `PASS_EXPECTED_LINK_FAILURE` | 分类为 link/caps failure | failure_layer | 用户态错误层 | 误判为 driver | caps/link | 修 pipeline |
| FFmpeg 对照 | 框架对照 | `./scripts/run_06_ffmpeg_gstreamer_compare.sh` | `PASS_FFMPEG_GSTREAMER_COMPARE` | GStreamer 成功，FFmpeg 可选成功 | exit code | 两框架最小路径 | 后端不同导致误判 | framework/backend | 对齐 decoder/parser |
| backend probe | 硬件候选 | `./scripts/run_07_hardware_backend_probe.sh` | `PASS_BACKEND_PROBE` | 输出 checklist | found_hw_candidates | rootfs 后端候选 | 插件存在当硬解 proof | backend/evidence | 真实码流验证 |
| 企业 normal | 可观测主路径 | `./enterprise_project/scripts/run_07_enterprise_gst_pipeline_service.sh` | `PASS_ENTERPRISE_GST_PIPELINE` | `gate_pass=true` | JSON metrics | 可进入 CI/report | 只看 stdout | observability | 看 metrics JSON |
| 企业 fault matrix | 故障矩阵 | `./enterprise_project/scripts/run_07_enterprise_fault_matrix.sh` | `summary.md` | 每 case 有 metrics | gate/failure_layer | 分类和恢复策略 | expected failure 被当真失败 | gate/report | 写 debug report |

## 性能观察说明

Stage07 的性能观察是 pipeline 级别的粗粒度观察，不等于硬件性能评测。

`elapsed_ms` 可能受这些因素影响：

- `identity sleep-time`
- `sync=false`
- source 是否 live
- queue depth
- CPU 调度
- shell/popen 开销

真实硬件性能要在后续结合：

- fps
- CPU
- memory bandwidth
- copy count
- queue depth
- DQBUF interval
- dmesg/IRQ/backend log
- thermal/DVFS
