# Enterprise Architecture

## 双模式架构

```text
CLI args
  -> CliConfig
  -> M2mDiagnosticService
      -> mode=vm-vim2m
          -> real V4L2 ioctl/mmap/qbuf/dqbuf/poll
      -> mode=rk-rkmpp
          -> FFmpeg/RKMPP/device/dmesg evidence
      -> StateMachine
      -> Logger
      -> PipelineMetrics JSON
      -> GateEvaluator
```

## VM/vim2m 状态机

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

## RK/RKMPP 状态机

```text
INIT
  -> OPEN_DEVICE          # collect board evidence, not V4L2 codec open
  -> RUNNING              # optional FFmpeg RKMPP decode command
  -> STOPPED
```

## Gate 差异

| 模式 | 必须通过的证据 |
| --- | --- |
| `vm-vim2m` | `real_ioctl_path=true`、M2M+streaming capability、mmap/qbuf/dqbuf counters |
| `rk-rkmpp` | 板端证据文件生成；若 `--require-rkmpp`，decoder 必须存在；若提供 `--input`，命令必须成功 |

## 故障注入

| 注入 | 真实动作 | gate |
| --- | --- | --- |
| `timeout` | `poll(0)` + real `STREAMOFF/QBUF/STREAMON` recovery | `PASS_WITH_RECOVERY_EVIDENCE` |
| `bytesused_zero` | real `VIDIOC_QBUF OUTPUT bytesused=0` | `FAIL_OUTPUT_BYTESUSED_ZERO` |
| `unsupported_format` | real `VIDIOC_S_FMT H264` and check returned fourcc | `PASS_FAULT_UNSUPPORTED_FORMAT_REJECTED` |
| `source_change` | real CAPTURE queue reconfiguration sequence | `PASS_WITH_RECOVERY_EVIDENCE` |
| `source_change_no_reconfigure` | stop without reconfigure | `FAIL_TIMEOUT_OVER_LIMIT` |

## 工作价值

企业版不再只训练“会打印流程”，而是把真实 V4L2 M2M queue 行为变成可回归的 JSON 指标；同时保留 RK 板硬件路径入口，让板端验证和 VM 学习互不冒充。
