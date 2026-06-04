# Stage07 Code Walkthrough

## 文件职责

| 文件 | 职责 |
| --- | --- |
| `include/00_stage07_gst_common.hpp` | 命令执行、日志保存、failure 分类、element 探测公共工具 |
| `src/01_gst_environment_probe.cpp` | 探测 GStreamer 工具、基础 element、硬件后端候选 |
| `src/02_caps_negotiation_raw_video.cpp` | 运行 raw caps negotiation pipeline |
| `src/03_queue_backpressure_latency.cpp` | 比较有无 queue 的耗时和背压语义 |
| `src/04_gst_debug_log_capture.cpp` | 用 GST_DEBUG 采集 caps/pad/pipeline 日志 |
| `src/05_link_failure_fault_injection.cpp` | 故意制造 link/caps failure 并分类 |
| `src/06_ffmpeg_gstreamer_compare.cpp` | 对照 FFmpeg 与 GStreamer 命令模型 |
| `src/07_hardware_backend_probe.cpp` | 探测硬件后端候选并输出证据 checklist |

## 公共工具分析

`CommandResult` 记录一次命令运行：

- `command`：可复现命令。
- `output`：stdout/stderr 合并输出。
- `exit_code`：进程退出码。
- `elapsed_ms`：粗粒度耗时。

`classify_gst_failure()` 做第一层 failure 分类：

- `missing_element_or_plugin`
- `link_or_caps_negotiation_failure`
- `runtime_caps_not_negotiated`
- `runtime_stream_error`
- `unknown_gstreamer_failure`

这不是替代人工分析，而是让报告先把问题放到正确层，避免把所有失败都推给 driver。

## Demo02 数据流

```text
videotestsrc
  -> capsfilter: video/x-raw,format=NV12,width=...
  -> videoconvert
  -> capsfilter: video/x-raw,format=I420
  -> fakesink
```

所有权视角：

- GStreamer buffer 从 source 向 sink 推进。
- 每个 transform element 可以原地处理，也可能重新分配/拷贝 buffer。
- 基础 demo 不证明 zero-copy；它只证明 caps 连接和 raw conversion 路径。

## Demo03 性能观察

`identity sleep-time=N` 模拟慢节点。`queue` 插入线程边界：

```text
source -> capsfilter -> queue -> slow identity -> sink
```

观察点：

- `elapsed_ms` 会受 sleep、frames、sync、调度影响。
- queue 改变的是背压传播位置和缓存深度，不是保证吞吐提升。
- 真实 SoC 中对应 decoder/display 队列深度、DQBUF 间隔、IRQ completion、内存带宽。

## Demo05 故障注入

错误 pipeline：

```text
videotestsrc ! video/x-h264 ! fakesink
```

原因：

- `videotestsrc` 输出 raw video。
- `video/x-h264` 是 compressed bitstream caps。
- 没有 encoder/parser，因此 link 失败是用户态 caps 错误，通常还没进入驱动 ioctl。

## 企业项目走读

企业项目模块：

| 模块 | 作用 |
| --- | --- |
| `01_cli_config` | 解析 scenario、后端、阈值、输出目录 |
| `02_state_machine` | 记录 Validate/Probe/Build/Run/Parse/Gate/Export 状态 |
| `03_logger` | 输出结构化 pipeline log |
| `04_metrics_sink` | 输出 JSON metrics |
| `05_gate_evaluator` | 根据 scenario 做客观 gate |
| `06_pipeline_service` | 构建 pipeline、运行、解析、调用 gate |

企业项目不是为了写复杂 UI，而是训练真实工作闭环：

```text
可复现命令 -> 原始日志 -> 结构化 metrics -> gate -> failure layer -> driver-facing note
```
