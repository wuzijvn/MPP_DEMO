# Enterprise Architecture

## 架构图

```text
CLI args
  -> CliConfig
  -> M2mDiagnosticService
      -> StateMachine
      -> Logger
      -> simulated V4L2 M2M queue loop
      -> fault injection
      -> GateEvaluator
      -> MetricsSink JSON
```

## 状态机

```text
INIT
  -> OPEN_DEVICE
  -> QUERYCAP
  -> FORMAT_NEGOTIATION
  -> BUFFER_SETUP
  -> STREAMING
  -> RUNNING
      -> SOURCE_CHANGE -> RECOVERY -> RUNNING
      -> RECOVERY -> RUNNING
  -> DRAINING
  -> STOPPED
```

失败时进入：

```text
RUNNING/SOURCE_CHANGE/OPEN_DEVICE -> FAILED
```

## 数据流

```text
frame loop:
  QBUF OUTPUT compressed packet
  QBUF CAPTURE empty frame
  poll
  DQBUF OUTPUT consumed packet
  DQBUF CAPTURE decoded frame
  update metrics
```

## 故障注入

| 注入 | 触发 | 预期恢复 | gate |
| --- | --- | --- | --- |
| `timeout` | 指定帧 poll timeout | STREAMOFF/cleanup/STREAMON | 可通过，若 allowed_timeouts 足够 |
| `bytesused_zero` | OUTPUT payload 长度为 0 | 不恢复，直接失败 | `FAIL_OUTPUT_BYTESUSED_ZERO` |
| `source_change` | 分辨率/stride 变化 | CAPTURE 重配 | 可通过 |
| `source_change_no_reconfigure` | 事件后不重配 | timeout | `FAIL_TIMEOUT_OVER_LIMIT` |

## 驱动影子线映射

| 用户态状态 | 驱动侧概念 | 诊断证据 |
| --- | --- | --- |
| OPEN_DEVICE | file_operations.open/session | fd、driver/card |
| QUERYCAP | V4L2 capability | `m2m_capable` |
| FORMAT_NEGOTIATION | s_fmt/try_fmt | fourcc/size/stride |
| BUFFER_SETUP | vb2 queue setup | buffer count |
| STREAMING | streamon + m2m scheduler | stream state |
| RUNNING | QBUF/DQBUF + job completion | qbuf/dqbuf counters |
| SOURCE_CHANGE | decoder event/reconfig | source_change_count |
| RECOVERY | streamoff/reset/requeue | recovery_count |
| DRAINING | DPB flush/LAST buffer | eos_count/dqbuf_capture |

## 真实工作如何使用这套结构

1. 默认先跑模拟路径，确认工具和 gate 正常。
2. 指定候选 `/dev/videoX`，看 `m2m_capable`。
3. 找到真实 M2M 节点后，把 service 的模拟 buffer loop 替换成真实 ioctl。
4. 保留同样的 `PipelineMetrics`，这样真实路径和模拟路径可以共用报告格式。
