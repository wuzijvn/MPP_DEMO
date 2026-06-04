# Stage07 Enterprise Project - GStreamer Pipeline Diagnostic Service

这个企业项目把 Stage07 的基础知识组合成一个可复现的 GStreamer pipeline 诊断服务。

基础 demo 教同一条链路的局部知识；企业项目是在复杂度、可观测性、恢复策略上的扩展。

## 项目目标

真实 SoC codec stack 工作里，GStreamer 问题通常不是“跑一条命令”就结束，而是要输出可交接证据：

- 完整 pipeline；
- element/backend 选择；
- 原始 `gst-launch-1.0` 日志；
- failure layer；
- caps/error/EOS counters；
- gate pass/fail；
- JSON metrics；
- fault matrix；
- driver-facing 证据边界。

## 模块职责

| 文件 | 职责 |
| --- | --- |
| `include/00_enterprise_common.hpp` | 公共结构、命令执行、failure 分类、JSON/日志辅助 |
| `include/01_cli_config.hpp` / `src/01_cli_config.cpp` | CLI 参数、scenario 快捷规则、阈值 |
| `include/02_state_machine.hpp` / `src/02_state_machine.cpp` | Validate/Probe/Build/Run/Parse/Gate/Export 状态机 |
| `include/03_logger.hpp` / `src/03_logger.cpp` | 结构化日志 `enterprise_pipeline.log` |
| `include/04_metrics_sink.hpp` / `src/04_metrics_sink.cpp` | 输出 `enterprise_metrics.json` |
| `include/05_gate_evaluator.hpp` / `src/05_gate_evaluator.cpp` | 客观 gate 规则 |
| `include/06_pipeline_service.hpp` / `src/06_pipeline_service.cpp` | 构造 pipeline、运行、解析证据、调用 gate |
| `src/07_enterprise_gst_pipeline_main.cpp` | CLI 入口和 stdout summary |

## 构建

```bash
cd /usr/local/MPP_DEMO/rk3568_study_demo/v4l2/stages/stage07_gstreamer_pipeline_thinking/enterprise_project
./build.sh all
```

## 运行主路径

```bash
./scripts/run_07_enterprise_gst_pipeline_service.sh
```

输出：

```text
enterprise_pipeline.log
gst_run.log
enterprise_metrics.json
```

## 故障矩阵

```bash
./scripts/run_07_enterprise_fault_matrix.sh
```

默认 case：

| case | 场景 | 预期 |
| --- | --- | --- |
| `normal_debug_caps` | 正常 raw pipeline | gate pass |
| `caps_failure_expected` | raw source 强行接 H.264 caps | expected failure pass |
| `missing_element_expected` | 缺失 element | expected failure pass |
| `slow_queue_observation` | queue + 慢节点 | gate pass，观察 latency |
| `hardware_probe_rkmpp` | 硬件后端候选 | 若要求的 element installed 则 pass |

## Gate 规则

normal path：

- `gst-launch-1.0` 可用；
- pipeline exit code 为 0；
- 看到 EOS；
- elapsed 不超过阈值；
- 如果要求 backend，则 backend element 必须 installed。

expected failure path：

- pipeline 必须失败；
- failure layer 必须能分类；
- 不能把任何失败都无条件当成功。

## Driver Shadow

本企业项目输出的 `failure_layer` 用于决定下一步交给谁：

| failure_layer | 优先排查 |
| --- | --- |
| `missing_element_or_plugin` | rootfs、plugin package、registry |
| `link_or_caps_negotiation_failure` | pipeline、pad template、caps |
| `runtime_caps_not_negotiated` | parser/backend/sink memory type |
| `runtime_stream_error` | backend/device/driver/bitstream |
| `success` | 继续做硬件证据、性能、zero-copy 检查 |

硬件证明仍需额外补充：

- 真实压缩码流；
- 显式硬件 decoder；
- 后端日志/dmesg/device node；
- CPU/fps/fallback；
- memory type/hidden copy；
- what this proves / what this does not prove。

## 生产差距

本项目是教学型诊断服务，尚未覆盖：

- GStreamer C API bus/pad probe；
- appsink/appsrc；
- 真实文件 demux/parse/decode；
- DMA-BUF/DRM PRIME memory caps；
- CI 中的长期稳定性统计；
- 板端功耗/温度/DVFS 采样。

这些会在后续 Stage08/Stage09/Stage11/Stage12 继续展开。
