# Stage07 Enterprise Architecture

## 架构图

```text
CLI
  -> CliConfig
  -> StateMachine
  -> PipelineService
       -> probe gst tools/backend
       -> build pipeline
       -> run gst-launch
       -> parse evidence
       -> GateEvaluator
       -> MetricsSink
  -> stdout summary
```

## 状态机

```text
Init
  -> ValidateConfig
  -> ProbeTools
  -> BuildPipeline
  -> RunPipeline
  -> ParseEvidence
  -> EvaluateGate
  -> ExportMetrics
  -> Done | Failed
```

状态机的意义：

- 失败停在 `ProbeTools`，通常是 rootfs/plugin 问题。
- 失败停在 `RunPipeline`，需要看 gst 原始日志。
- 失败停在 `EvaluateGate`，说明 pipeline 跑了，但证据没达到验收标准。

## Scenario 设计

| scenario | pipeline 行为 | 工作映射 |
| --- | --- | --- |
| `normal` | raw video -> convert -> fakesink | smoke test |
| `caps-failure` | raw source 强接 H.264 caps | link/caps 故障 |
| `missing-element` | 使用不存在 element | rootfs/plugin 故障 |
| `slow-queue` | queue + slow identity | 背压/latency |
| `hardware-probe` | raw smoke + backend element probe | 硬件候选证据边界 |

## Metrics 字段解释

| 字段 | 意义 |
| --- | --- |
| `exit_code` | gst-launch 退出码 |
| `elapsed_ms` | pipeline 粗粒度耗时 |
| `eos_count` | 是否正常到 EOS |
| `caps_mentions` | debug log 中 caps 相关证据数量 |
| `link_failure_count` | link/caps 失败提示数量 |
| `missing_element_count` | 缺插件提示数量 |
| `failure_layer` | 初步失败层分类 |
| `backend_installed` | 后端 element 是否可见 |
| `gate_pass` | 是否满足本 case 验收 |

## 关键边界

`hardware-probe` 不构造假硬解 pipeline。它只回答：

```text
这个 backend element 在当前 rootfs 中是否可见？
```

它不能回答：

```text
真实 VPU 是否工作？
是否发生 fallback？
输出 buffer 是否 zero-copy？
```

真实硬解验证必须扩展为真实码流 pipeline，并加入 backend/dmesg/device/perf 证据。
